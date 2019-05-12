#include "gamerule.h"
#include "serverplayer.h"
#include "room.h"
#include "standard.h"
#include "maneuvering.h"
#include "engine.h"
#include "settings.h"
#include "json.h"

#include <QTime>

GameRule::GameRule(QObject *)
    : TriggerSkill("game_rule")
{
    //@todo: this setParent is illegitimate in QT and is equivalent to calling
    // setParent(NULL). So taking it off at the moment until we figure out
    // a way to do it.
    //setParent(parent);

    events << GameStart << TurnStart
        << EventPhaseStart << EventPhaseProceeding << EventPhaseEnd << EventPhaseChanging
        << PreCardUsed<< TargetChosed << CardUsed << TargetConfirmed << CardFinished << CardEffected
        << HpChanged
        << EventLoseSkill << EventAcquireSkill
        << CardsMoveOneTime
        << AskForPeaches << AskForPeachesDone << BuryVictim << GameOverJudge
        << SlashHit << SlashEffected << SlashProceed
        << ConfirmDamage << DamageDone << DamageComplete
        << StartJudge << FinishRetrial << FinishJudge
        << ChoiceMade << PlayCard;
}

bool GameRule::triggerable(const ServerPlayer *) const
{
    return true;
}

int GameRule::getPriority(TriggerEvent) const
{
    return 0;
}

void GameRule::onPhaseProceed(ServerPlayer *player) const
{
    Room *room = player->getRoom();
    switch (player->getPhase()) {
    case Player::PhaseNone: {
        Q_ASSERT(false);
    }
    case Player::RoundStart:{
        break;
    }
    case Player::Start: {
        break;
    }
    case Player::Judge: {
        QList<const Card *> tricks = player->getJudgingArea();
        while (!tricks.isEmpty() && player->isAlive()) {
            const Card *trick = tricks.takeLast();
            CardMoveReason reason(CardMoveReason::S_REASON_NATURAL_MOVE, player->objectName(), trick->objectName(), QString());
            room->moveCardTo(trick, NULL, Player::PlaceTable, reason, true);
            bool on_effect = room->cardEffect(trick, NULL, player);
            if (!on_effect)
                trick->onNullified(player);
        }
        break;
    }
    case Player::Draw: {
        int num = 2;
        if (player->hasFlag("Global_FirstRound")) {
            room->setPlayerFlag(player, "-Global_FirstRound");
            if (room->getMode() == "02_1v1") num--;
        }

        QVariant data = num;
        room->getThread()->trigger(DrawNCards, room, player, data);
        int n = data.toInt();
        if (n > 0)
            player->drawCards(n, "draw_phase");
        QVariant _n = n;
        room->getThread()->trigger(AfterDrawNCards, room, player, _n);
        break;
    }
    case Player::Play: {
        while (player->isAlive()) {
			CardUseStruct card_use;
            room->activate(player, card_use);
            if (card_use.card != NULL){
				room->useCard(card_use, true);
			} else
                break;
		}
        break;
    }
    case Player::Discard: {
        QList<const MaxCardsSkill *> maxcards_skills = Sanguosha->getMaxCardsSkills();
        foreach(const MaxCardsSkill *skill, maxcards_skills) {
            if (skill->getExtra(player) != 0 && skill->isVisible()) {
                room->sendCompulsoryTriggerLog(player, skill->objectName());
                player->broadcastSkillInvoke(skill->objectName(), skill->getEffectIndex(player, "MaxCardsSkill"));
            }
        }

        int x = 0;
        foreach (const Card *c, player->getHandcards()) {
            if (!Sanguosha->isCardHided(player, c))
                x++;
        }

        int discard_num = x - player->getMaxCards();
        if (discard_num > 0)
            room->askForDiscard(player, "gamerule", discard_num, discard_num);
		break;
    }
    case Player::Finish: {
        break;
    }
    case Player::NotActive:{
        break;
    }
    }
}

bool GameRule::trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
{
    if (room->getTag("SkipGameRule").toBool()) {
        room->removeTag("SkipGameRule");
        return false;
    }

    // Handle global events
    if (player == NULL) {
        if (triggerEvent == GameStart) {
            if (room->getMode() == "04_boss") {
                int difficulty = Config.value("BossModeDifficulty", 0).toInt();
                if ((difficulty & (1 << GameRule::BMDIncMaxHp)) > 0) {
                    foreach (ServerPlayer *p, room->getPlayers()) {
                        if (p->isLord()) continue;
                        int m = p->getMaxHp() + 2;
                        p->setProperty("maxhp", m);
                        p->setProperty("hp", m);
                        room->broadcastProperty(p, "maxhp");
                        room->broadcastProperty(p, "hp");
                    }
                }
            }
            foreach (ServerPlayer *player, room->getPlayers()) {
                if (player->getGeneral()->getKingdom() == "god" && player->getGeneralName() != "anjiang"
                    && !player->getGeneralName().startsWith("boss_"))
                    room->setPlayerProperty(player, "kingdom", room->askForKingdom(player));
                foreach (const Skill *skill, player->getVisibleSkillList()) {
                    if (skill->getFrequency() == Skill::Limited && !skill->getLimitMark().isEmpty()
                        && (!skill->isLordSkill() || player->hasLordSkill(skill->objectName())))
                        room->setPlayerMark(player, skill->getLimitMark(), 1);
                }
            }
            room->setTag("FirstRound", true);
            room->setTag("TurnFirstRound", true);
            bool kof_mode = room->getMode() == "02_1v1" && Config.value("1v1/Rule", "2013").toString() != "Classical";
            QList<int> n_list;
            foreach (ServerPlayer *p, room->getPlayers()) {
                int n = kof_mode ? p->getMaxHp() : 4;
                QVariant data = n;
                room->getThread()->trigger(DrawInitialCards, room, p, data);
                n_list << data.toInt();
            }
            room->drawCards(room->getPlayers(), n_list, QString());
            if (Config.EnableLuckCard)
                room->askForLuckCard();
            int i = 0;
            foreach (ServerPlayer *p, room->getPlayers()) {
                QVariant _nlistati = n_list.at(i);
                room->getThread()->trigger(AfterDrawInitialCards, room, p, _nlistati);
                i++;
            }
        }
        return false;
    }

    switch (triggerEvent) {
    case TurnStart: {
        player = room->getCurrent();
        if (room->getTag("FirstRound").toBool()) {
            room->setTag("FirstRound", false);
            room->setPlayerFlag(player, "Global_FirstRound");
        }
        if (room->getTag("TurnFirstRound").toBool()) {
            room->setTag("TurnFirstRound", false);
            room->setPlayerFlag(player, "Global_TurnFirstRound");
        }
        room->addPlayerMark(player, "Global_TurnCount");
        room->setPlayerMark(player, "damage_point_round", 0);
        if (room->getMode() == "04_boss" && player->isLord()) {
            int turn = player->getMark("Global_TurnCount");
            if (turn == 1)
                room->doLightbox("BossLevelA\\ 1 \\BossLevelB", 2000, 100);

            LogMessage log2;
            log2.type = "#BossTurnCount";
            log2.from = player;
            log2.arg = QString::number(turn);
            room->sendLog(log2);

            int limit = Config.value("BossModeTurnLimit", 70).toInt();
            int level = room->getTag("BossModeLevel").toInt();
            if (limit >= 0 && level < Config.BossLevel && player->getMark("Global_TurnCount") > limit)
                room->gameOver("lord");
        }

        //for ai change skin
        if (player->getState() == "robot") {
            if (room->getTurn() == 1 || qrand()%10 > 7) {
                JsonArray args;
                args << (qrand()%(player->getGeneral()->skinCount()+1)) << true;
                room->changeSkinCommand(player, args);
            }
            if (player->getGeneral2() && (room->getTurn() == 1 || qrand()%10 > 7)) {
                JsonArray args;
                args << (qrand()%(player->getGeneral2()->skinCount()+1)) << false;
                room->changeSkinCommand(player, args);
            }
        }

        if (!player->faceUp()) {
            room->setPlayerFlag(player, "-Global_FirstRound");
            room->setPlayerFlag(player, "-Global_TurnFirstRound");
            player->turnOver();
        } else if (player->isAlive())
            player->play();

        break;
    }
    case EventPhaseStart: {
        if (player->getPhase() == Player::NotActive) {
            foreach (ServerPlayer * p, room->getAllPlayers(true)) {
                room->setPlayerFlag(p, ".");
                room->removeFixedDistance(player, p, 1);
                room->clearPlayerCardLimitation(p, true);
                //clear skills
                QVariantList turn_skills = room->getTag("TurnSkillsFor"+p->objectName()).toList();
                room->removeTag("TurnSkillsFor"+p->objectName());
                QStringList detachList;
                foreach (QVariant skill_data, turn_skills) {
                    QString skill_name = skill_data.toString();
                    if (Sanguosha->getSkill(skill_name) && p->hasSkill(skill_name, true))
                        detachList.append("-" + skill_name);
                }
                if (!detachList.isEmpty())
                    room->handleAcquireDetachSkills(p, detachList);
            }
        }
        break;
    }
    case EventPhaseProceeding: {
        onPhaseProceed(player);
        break;
    }
    case EventPhaseEnd: {
        if (player->getPhase() == Player::Play)
            room->addPlayerHistory(player, ".");
        break;
    }
    case EventPhaseChanging: {
        room->addPlayerHistory(NULL, "pushPile");
		PhaseChangeStruct change = data.value<PhaseChangeStruct>();
        if (change.to == Player::Play) {
            room->setPlayerMark(player, "damage_point_play_phase", 0);
            room->addPlayerHistory(player, ".");
        }
        break;
    }
    case PreCardUsed: {
        if (data.canConvert<CardUseStruct>()) {
            CardUseStruct card_use = data.value<CardUseStruct>();
            if (card_use.from->hasFlag("Global_ForbidSurrender")) {
                card_use.from->setFlags("-Global_ForbidSurrender");
                room->doNotify(card_use.from, QSanProtocol::S_COMMAND_ENABLE_SURRENDER, QVariant(true));
            }

            if (card_use.m_isOwnerUse && !card_use.card->isMute()) {
                if (card_use.card->getTypeId() != Card::TypeEquip)
                    card_use.from->broadcastSkillInvoke(card_use.card);
	        	if (!card_use.card->getSkillName().isNull() && card_use.card->getSkillName(true) == card_use.card->getSkillName(false)
                        && card_use.from->hasSkill(card_use.card->getSkillName()))
                    room->notifySkillInvoked(card_use.from, card_use.card->getSkillName());
	        }
        }
        break;
    }
    case CardUsed: {
        if (data.canConvert<CardUseStruct>()) {
            CardUseStruct card_use = data.value<CardUseStruct>();
            RoomThread *thread = room->getThread();

            if (card_use.from && !card_use.to.isEmpty()) {
                thread->trigger(TargetSpecifying, room, card_use.from, data);

                QList<ServerPlayer *> f_targets, all_players = room->getAllPlayers(true);

                while (true) {
                    foreach (ServerPlayer *p, all_players) {
                        while (f_targets.count(p) > card_use.to.count(p))
                            f_targets.removeOne(p);
                    }
                    ServerPlayer *to = NULL;
                    foreach (ServerPlayer *p, all_players) {
                        if (f_targets.count(p) < card_use.to.count(p)) {
                            to = p;
                            break;
                        }
                    }
                    if (to == NULL) break;

                    if (!thread->trigger(TargetConfirming, room, to, data))
                        f_targets << to;
                    card_use = data.value<CardUseStruct>();
                }
            }

            card_use = data.value<CardUseStruct>();

            if (card_use.from && !card_use.to.isEmpty()) {

                QVariantList intlist, boollist;
                for (int i = 0; i < card_use.to.length(); i++) {
                    intlist.append(QVariant(1));
                    boollist.append(QVariant(false));
                }
                card_use.card->setTag("Jink_List", QVariant::fromValue(intlist));
                card_use.card->setTag("Damage_List", QVariant::fromValue(intlist));
                card_use.card->setTag("Nullified_List", QVariant::fromValue(boollist));
                card_use.card->setTag("Qinggang_List", QVariant::fromValue(boollist));
                card_use.card->setTag("Wushuang1_List", QVariant::fromValue(boollist));
                card_use.card->setTag("Wushuang2_List", QVariant::fromValue(boollist));

                for (int i = 0; i < card_use.to.length(); i++) {
                    CardUseStruct new_use = data.value<CardUseStruct>();
                    new_use.index = i;
                    data = QVariant::fromValue(new_use);
                    thread->trigger(TargetSpecified, room, card_use.from, data);
                }

                for (int i = 0; i < card_use.to.length(); i++) {
                    CardUseStruct new_use = data.value<CardUseStruct>();
                    new_use.index = i;
                    data = QVariant::fromValue(new_use);
                    ServerPlayer *to = card_use.to.at(i);
                    thread->trigger(TargetConfirmed, room, to, data);
                }
            }
            card_use = data.value<CardUseStruct>();

            if (card_use.card->hasPreAction())
                card_use.card->doPreAction(room, card_use);

            card_use.card->setTag("CardUseNullifiedList", QVariant::fromValue(card_use.nullified_list));

            if (card_use.card->isKindOf("Nullification")) {
                if (card_use.to_card) {
                    CardEffectStruct effect;
                    effect.card = card_use.card;
                    effect.from = card_use.from;
                    effect.to_card = card_use.to_card;
                    effect.multiple = false;
                    effect.nullified = card_use.nullified_list.contains("_ALL_TARGETS");
                    room->cardEffect(effect);
                }
            } else
                card_use.card->use(room, card_use.from, card_use.to);
        }

        break;
    }
    case TargetConfirmed: {
        // for escape
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.to.contains(player) && use.from != player && (use.card->isKindOf("Slash") || use.card->getTypeId() == Card::TypeTrick)) {
            bool has_escape = false;
            foreach (const Card *card, player->getHandcards()) {
                if (card->isKindOf("Escape"))
                    has_escape = true;
                else {
                    has_escape = false;
                    break;
                }
            }
            if (has_escape && room->askForUseCard(player, "Escape", "@escape")) {
                use.nullified_list << player->objectName();
                data = QVariant::fromValue(use);
                player->drawCards(2, "escape");
            }
        }

        break;
    }
    case CardFinished: {
        CardUseStruct use = data.value<CardUseStruct>();

        room->clearCardFlag(use.card);

        if (use.card->isKindOf("AOE") || use.card->isKindOf("GlobalEffect")) {
            foreach(ServerPlayer *p, room->getAlivePlayers())
                room->doNotify(p, QSanProtocol::S_COMMAND_NULLIFICATION_ASKED, QVariant("."));
        }
        if (use.card->isKindOf("Slash"))
            use.from->tag.remove("Jink_" + use.card->toString());

		QList<int> table_cardids = room->getCardIdsOnTable(use.card);
		if (!table_cardids.isEmpty()) {
			DummyCard *dummy = new DummyCard(table_cardids);
			CardMoveReason reason(CardMoveReason::S_REASON_USE, use.from->objectName(), QString(), use.card->getSkillName(), QString());
			if (use.to.size() == 1) reason.m_targetId = use.to.first()->objectName();
            reason.m_extraData = QVariant::fromValue(use);
			room->moveCardTo(dummy, use.from, NULL, Player::DiscardPile, reason, true);
            dummy->deleteLater();
		}
        break;
    }
    case EventAcquireSkill:
    case EventLoseSkill: {
        QString skill_name = data.toString();
        const Skill *skill = Sanguosha->getSkill(skill_name);
        bool refilter = skill->inherits("FilterSkill");

        if (refilter)
            room->filterCards(player, player->getCards("he"), triggerEvent == EventLoseSkill);

        break;
    }
    case HpChanged: {
        if (player->getHp() > 0)
            break;
        if (data.isNull() || data.canConvert<RecoverStruct>())
            break;
        if (data.canConvert<DamageStruct>()) {
            DamageStruct damage = data.value<DamageStruct>();
            room->enterDying(player, &damage);
        } else {
            room->enterDying(player, NULL);
        }

        break;
    }
    case CardsMoveOneTime: {
//        CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
//        if (move.from && move.from->isAlive() && move.from->objectName() == player->objectName()
//            && move.from_places.contains(Player::PlaceHand) && move.to_place == Player::DiscardPile
//            && (move.reason.m_reason & CardMoveReason::S_MASK_BASIC_REASON) == CardMoveReason::S_REASON_DISCARD) {
//            foreach (int id, move.card_ids) {
//                if (Sanguosha->getCard(id)->isKindOf("Escape")) {
//                    player->drawCards(1, "escape");
//                }
//            }
//        }
        break;
    }
    case AskForPeaches: {
        DyingStruct dying = data.value<DyingStruct>();
        const Card *peach = NULL;

        while (dying.who->getHp() <= 0) {
            LogMessage log;
            log.type = "#AskForPeaches";
            log.from = dying.who;
            log.to.append(player);
            log.arg = QString::number(1 - dying.who->getHp());
            room->sendLog(log);

            peach = NULL;

            if (dying.who->isAlive())
                peach = room->askForSinglePeach(player, dying.who);

            if (peach == NULL) break;

            room->setCardFlag(peach, "UsedBySecondWay");
            room->useCard(CardUseStruct(peach, player, dying.who));
        }
        break;
    }
    case AskForPeachesDone: {
        if (player->getHp() <= 0 && player->isAlive()) {
            DyingStruct dying = data.value<DyingStruct>();
            room->killPlayer(player, dying.damage);
        }

        break;
    }
    case ConfirmDamage: {
        DamageStruct damage = data.value<DamageStruct>();
        if (damage.card && damage.to->getMark("SlashIsIntensify") > 0) {
            damage.damage += damage.to->getMark("SlashIsIntensify");
            damage.to->setMark("SlashIsIntensify", 0);
            data = QVariant::fromValue(damage);
        }

        break;
    }
    case DamageDone: {
        DamageStruct damage = data.value<DamageStruct>();
        if (damage.from && !damage.from->isAlive())
            damage.from = NULL;
        data = QVariant::fromValue(damage);

        LogMessage log;

		QString log_name = "#Damage";

        switch (damage.nature) {
        case DamageStruct::Fire: log_name = log_name + "Fire"; break;
        case DamageStruct::Thunder: log_name = log_name + "Thunder"; break;
		default:
		    break;
        }

        if (damage.from)
            log.from = damage.from;
        else
            log_name = log_name + "NoSource";

		log.type = log_name;
        log.to << damage.to;
        log.arg = QString::number(damage.damage);

        int new_hp = damage.to->getHp() - damage.damage;

		log.arg2 = QString::number(qMax(new_hp, 0));

        room->sendLog(log);

        QString change_str = QString("%1:%2").arg(damage.to->objectName()).arg(-damage.damage);
        switch (damage.nature) {
        case DamageStruct::Fire: change_str.append("F"); break;
        case DamageStruct::Thunder: change_str.append("T"); break;
        default: break;
        }

        JsonArray arg;
        arg << damage.to->objectName() << -damage.damage << damage.nature;
        room->doBroadcastNotify(QSanProtocol::S_COMMAND_CHANGE_HP, arg);

        room->setTag("HpChangedData", data);

        if (damage.nature != DamageStruct::Normal && player->isChained()) {
            room->setPlayerProperty(player, "chained", false);
			if (!damage.chain) {
                damage.flags << "is_chained";
                data = QVariant::fromValue(damage);
			}
        }

        room->setPlayerProperty(damage.to, "hp", new_hp);

        break;
    }
    case DamageComplete: {
        DamageStruct damage = data.value<DamageStruct>();
        if (damage.prevented) break;
        if (damage.flags.contains("is_chained")) {
            if (damage.nature != DamageStruct::Normal && !damage.chain) {
                // iron chain effect
                QList<ServerPlayer *> chained_players = room->getAllPlayers();
                foreach (ServerPlayer *chained_player, chained_players) {
                    if (chained_player != player && chained_player->isChained()) {
                        room->getThread()->delay(400);
						LogMessage log;
                        log.type = "#IronChainDamage";
                        log.from = chained_player;
                        room->sendLog(log);

                        DamageStruct chain_damage = damage;
                        chain_damage.to = chained_player;
                        chain_damage.chain = true;
                        chain_damage.transfer = false;
                        chain_damage.flags.clear();

                        room->damage(chain_damage);
                    }
                }
            }
        }
        if (room->getMode() == "02_1v1" || room->getMode() == "06_XMode") {
            foreach (ServerPlayer *p, room->getAllPlayers()) {
                if (p->hasFlag("Global_DebutFlag")) {
                    p->setFlags("-Global_DebutFlag");
                    if (room->getMode() == "02_1v1")
                        room->getThread()->trigger(Debut, room, p);
                }
            }
        }
        break;
    }
    case CardEffected: {
        if (data.canConvert<CardEffectStruct>()) {
            CardEffectStruct effect = data.value<CardEffectStruct>();
            if (effect.nullified) {
                LogMessage log;
                log.type = "#CardNullified";
                log.from = effect.to;
                log.arg = effect.card->objectName();
                room->sendLog(log);

                return true;
            } else if (effect.card->getTypeId() == Card::TypeTrick) {
                if (room->isCanceled(effect)) {
                    effect.to->setFlags("Global_NonSkillNullify");
                    return true;
                } else {
                    room->getThread()->trigger(TrickEffect, room, effect.to, data);
                }
            }
            if (effect.to->isAlive())
                effect.card->onEffect(effect);
        }

        break;
    }
    case SlashEffected: {
        SlashEffectStruct effect = data.value<SlashEffectStruct>();
        if (effect.nullified) {
            LogMessage log;
            log.type = "#CardNullified";
            log.from = effect.to;
            log.arg = effect.slash->objectName();
            room->sendLog(log);

            return true;
        }
        if (effect.jink_num > 0)
            room->getThread()->trigger(SlashProceed, room, effect.from, data);
        else
            room->slashResult(effect, NULL);
        break;
    }
    case SlashProceed: {


        break;
    }
    case SlashHit: {
        SlashEffectStruct effect = data.value<SlashEffectStruct>();
        DamageStruct damage(effect.slash, effect.from, effect.to, effect.cardinality, effect.nature);

        int index = effect.index;
        QVariantList qinggang_list = effect.slash->tag["Qinggang_List"].toList();
        if (qinggang_list[index].toBool())
            damage.flags << "qinggang";

        room->damage(damage);
        break;
    }
    case GameOverJudge: {
        if (room->getMode() == "04_boss" && player->isLord()
            && (Config.value("BossModeEndless", false).toBool() || room->getTag("BossModeLevel").toInt() < Config.BossLevel - 1))
            break;
        if (room->getMode() == "02_1v1") {
            QStringList list = player->tag["1v1Arrange"].toStringList();
            QString rule = Config.value("1v1/Rule", "2013").toString();
            if (list.length() > ((rule == "2013") ? 3 : 0)) break;
        }

        QString winner = getWinner(player);
        if (!winner.isNull()) {
            room->gameOver(winner);
            return true;
        }

        break;
    }
    case BuryVictim: {
        DeathStruct death = data.value<DeathStruct>();
        player->bury();

        if (room->getTag("SkipNormalDeathProcess").toBool())
            return false;

        ServerPlayer *killer = death.damage ? death.damage->from : NULL;
        if (killer)
            rewardAndPunish(killer, player);

        room->getThread()->trigger(DeathAfter, room, player, data);

        if (room->getMode() == "02_1v1") {
            QStringList list = player->tag["1v1Arrange"].toStringList();
            QString rule = Config.value("1v1/Rule", "2013").toString();
            if (list.length() <= ((rule == "2013") ? 3 : 0)) break;

            if (rule == "Classical") {
                player->tag["1v1ChangeGeneral"] = list.takeFirst();
                player->tag["1v1Arrange"] = list;
            } else {
                player->tag["1v1ChangeGeneral"] = list.first();
            }

            changeGeneral1v1(player);
            if (death.damage == NULL)
                room->getThread()->trigger(Debut, room, player);
            else
                player->setFlags("Global_DebutFlag");
            return false;
        } else if (room->getMode() == "06_XMode") {
            changeGeneralXMode(player);
            if (death.damage != NULL)
                player->setFlags("Global_DebutFlag");
            return false;
        } else if (room->getMode() == "04_boss" && player->isLord()) {
            int level = room->getTag("BossModeLevel").toInt();
            level++;
            room->setTag("BossModeLevel", level);
            doBossModeDifficultySettings(player);
            changeGeneralBossMode(player);
            if (death.damage != NULL)
                player->setFlags("Global_DebutFlag");
            room->doLightbox(QString("BossLevelA\\ %1 \\BossLevelB").arg(level + 1), 2000, 100);
            return false;
        }

        break;
    }
    case StartJudge: {
        JudgeStruct *judge = data.value<JudgeStruct *>();

        LogMessage log;
        log.type = "$InitialJudge";

        if (player->getPile("incantation").isEmpty())
            judge->card = Sanguosha->getCard(room->drawCard());
        else {
            judge->card = Sanguosha->getCard(player->getPile("incantation").first());
            log.type = "$ZhoufuJudge";
        }

        log.from = player;
        log.card_str = QString::number(judge->card->getEffectiveId());
        room->sendLog(log);

        room->moveCardTo(judge->card, NULL, judge->who, Player::PlaceJudge,
            CardMoveReason(CardMoveReason::S_REASON_JUDGE,
            judge->who->objectName(),
            QString(), QString(), judge->reason), true);
        judge->updateResult();
        break;
    }
    case FinishRetrial: {
        JudgeStruct *judge = data.value<JudgeStruct *>();

        LogMessage log;
        log.type = "$JudgeResult";
        log.from = player;
        log.card_str = QString::number(judge->card->getEffectiveId());
        room->sendLog(log);

        if(!judge->patterns.isEmpty()) {
            foreach (QString _pattern, judge->patterns) {
                if (ExpPattern(_pattern).match(player, judge->card)) {
                    judge->pattern = _pattern;
                    break;
                }
            }
        }

        if (judge->play_animation) {
            room->sendJudgeResult(judge);
            room->getThread()->delay(Config.S_JUDGE_LONG_DELAY);
        }

        break;
    }
    case FinishJudge: {
        JudgeStruct *judge = data.value<JudgeStruct *>();

        if (room->getCardPlace(judge->card->getEffectiveId()) == Player::PlaceJudge) {
            CardMoveReason reason(CardMoveReason::S_REASON_JUDGEDONE, judge->who->objectName(),
                judge->reason, QString());
            reason.m_extraData = data;
            room->moveCardTo(judge->card, judge->who, NULL, Player::DiscardPile, reason, true);
        }

        break;
    }
    case ChoiceMade: {
        foreach (ServerPlayer *p, room->getAlivePlayers()) {
            foreach (QString flag, p->getFlagList()) {
                if (flag.startsWith("Global_") && flag.endsWith("Failed"))
                    room->setPlayerFlag(p, "-" + flag);
            }
        }
        break;
    }
    case PlayCard: {
        room->addPlayerHistory(NULL, "pushPile");
        break;
    }
    default:
        break;
    }

    return false;
}

void GameRule::changeGeneral1v1(ServerPlayer *player) const
{
    Config.AIDelay = Config.OriginAIDelay;

    Room *room = player->getRoom();
    bool classical = (Config.value("1v1/Rule", "2013").toString() == "Classical");
    QString new_general;
    if (classical) {
        new_general = player->tag["1v1ChangeGeneral"].toString();
        player->tag.remove("1v1ChangeGeneral");
    } else {
        QStringList list = player->tag["1v1Arrange"].toStringList();
        if (player->getAI())
            new_general = list.first();
        else
            new_general = room->askForGeneral(player, list);
        list.removeOne(new_general);
        player->tag["1v1Arrange"] = QVariant::fromValue(list);
    }

    if (player->getPhase() != Player::NotActive)
        player->changePhase(player->getPhase(), Player::NotActive);

    room->revivePlayer(player);
    room->changeHero(player, new_general, true, true);
    if (player->getGeneral()->getKingdom() == "god")
        room->setPlayerProperty(player, "kingdom", room->askForKingdom(player));
    room->addPlayerHistory(player, ".");

    if (player->getKingdom() != player->getGeneral()->getKingdom())
        room->setPlayerProperty(player, "kingdom", player->getGeneral()->getKingdom());

    QList<ServerPlayer *> notified = classical ? room->getOtherPlayers(player, true) : room->getPlayers();
    room->doBroadcastNotify(notified, QSanProtocol::S_COMMAND_REVEAL_GENERAL, JsonArray() << player->objectName() << new_general);

    if (!player->faceUp())
        player->turnOver();

    if (player->isChained())
        room->setPlayerProperty(player, "chained", false);

    room->setTag("FirstRound", true); //For Manjuan
    int draw_num = classical ? 4 : player->getMaxHp();
    QVariant data = draw_num;
    room->getThread()->trigger(DrawInitialCards, room, player, data);
    draw_num = data.toInt();
    try {
        player->drawCards(draw_num);
        room->setTag("FirstRound", false);
    }
    catch (TriggerEvent triggerEvent) {
        if (triggerEvent == TurnBroken || triggerEvent == StageChange)
            room->setTag("FirstRound", false);
        throw triggerEvent;
    }
    QVariant _drawnum = draw_num;
    room->getThread()->trigger(AfterDrawInitialCards, room, player, _drawnum);
}

void GameRule::changeGeneralXMode(ServerPlayer *player) const
{
    Config.AIDelay = Config.OriginAIDelay;

    Room *room = player->getRoom();
    ServerPlayer *leader = player->tag["XModeLeader"].value<ServerPlayer *>();
    Q_ASSERT(leader);
    QStringList backup = leader->tag["XModeBackup"].toStringList();
    QString general = room->askForGeneral(leader, backup);
    if (backup.contains(general))
        backup.removeOne(general);
    else
        backup.takeFirst();
    leader->tag["XModeBackup"] = QVariant::fromValue(backup);

    if (player->getPhase() != Player::NotActive)
        player->changePhase(player->getPhase(), Player::NotActive);

    room->revivePlayer(player);
    room->changeHero(player, general, true, true);
    if (player->getGeneral()->getKingdom() == "god")
        room->setPlayerProperty(player, "kingdom", room->askForKingdom(player));
    room->addPlayerHistory(player, ".");

    if (player->getKingdom() != player->getGeneral()->getKingdom())
        room->setPlayerProperty(player, "kingdom", player->getGeneral()->getKingdom());

    if (!player->faceUp())
        player->turnOver();

    if (player->isChained())
        room->setPlayerProperty(player, "chained", false);

    room->setTag("FirstRound", true); //For Manjuan
    QVariant data(4);
    room->getThread()->trigger(DrawInitialCards, room, player, data);
    int num = data.toInt();
    try {
        player->drawCards(num);
        room->setTag("FirstRound", false);
    }
    catch (TriggerEvent triggerEvent) {
        if (triggerEvent == TurnBroken || triggerEvent == StageChange)
            room->setTag("FirstRound", false);
        throw triggerEvent;
    }

    QVariant _num = num;
    room->getThread()->trigger(AfterDrawInitialCards, room, player, _num);
}

void GameRule::changeGeneralBossMode(ServerPlayer *player) const
{
    Config.AIDelay = Config.OriginAIDelay;

    Room *room = player->getRoom();
    int level = room->getTag("BossModeLevel").toInt();
    room->doBroadcastNotify(QSanProtocol::S_COMMAND_UPDATE_BOSS_LEVEL, QVariant(level));
    QString general;
    if (level <= Config.BossLevel - 1) {
        QStringList boss_generals = Config.BossGenerals.at(level).split("+");
        if (boss_generals.length() == 1)
            general = boss_generals.first();
        else {
            if (Config.value("OptionalBoss", false).toBool())
                general = room->askForGeneral(player, boss_generals);
            else
                general = boss_generals.at(qrand() % boss_generals.length());
        }
    } else {
        general = (qrand() % 2 == 0) ? "sujiang" : "sujiangf";
    }

    if (player->getPhase() != Player::NotActive)
        player->changePhase(player->getPhase(), Player::NotActive);

    room->revivePlayer(player);
    room->changeHero(player, general, true, true);
    room->setPlayerMark(player, "BossMode_Boss", 1);
    int actualmaxhp = player->getMaxHp();
    if (level >= Config.BossLevel)
        actualmaxhp = level * 5 + 5;
    int difficulty = Config.value("BossModeDifficulty", 0).toInt();
    if ((difficulty & (1 << BMDDecMaxHp)) > 0) {
        if (level == 0);
        else if (level == 1) actualmaxhp -= 2;
        else if (level == 2) actualmaxhp -= 4;
        else actualmaxhp -= 5;
    }
    if (actualmaxhp != player->getMaxHp()) {
        player->setProperty("maxhp", actualmaxhp);
        player->setProperty("hp", actualmaxhp);
        room->broadcastProperty(player, "maxhp");
        room->broadcastProperty(player, "hp");
    }
    if (level >= Config.BossLevel)
        acquireBossSkills(player, level);
    room->addPlayerHistory(player, ".");

    if (player->getKingdom() != player->getGeneral()->getKingdom())
        room->setPlayerProperty(player, "kingdom", player->getGeneral()->getKingdom());

    if (!player->faceUp())
        player->turnOver();

    if (player->isChained())
        room->setPlayerProperty(player, "chained", false);

    room->setTag("FirstRound", true); //For Manjuan
    QVariant data(4);
    room->getThread()->trigger(DrawInitialCards, room, player, data);
    int num = data.toInt();
    try {
        player->drawCards(num);
        room->setTag("FirstRound", false);
    }
    catch (TriggerEvent triggerEvent) {
        if (triggerEvent == TurnBroken || triggerEvent == StageChange)
            room->setTag("FirstRound", false);
        throw triggerEvent;
    }

    QVariant _num = num;
    room->getThread()->trigger(AfterDrawInitialCards, room, player, _num);
}

void GameRule::acquireBossSkills(ServerPlayer *player, int level) const
{
    QStringList skills = Config.BossEndlessSkills;
    int num = qBound(qMin(5, skills.length()), 5 + level - Config.BossLevel, qMin(10, skills.length()));
    for (int i = 0; i < num; i++) {
        QString skill = skills.at(qrand() % skills.length());
        skills.removeOne(skill);
        if (skill.contains("+")) {
            QStringList subskills = skill.split("+");
            skill = subskills.at(qrand() % subskills.length());
        }
        player->getRoom()->acquireSkill(player, skill);
    }
}

void GameRule::doBossModeDifficultySettings(ServerPlayer *lord) const
{
    Room *room = lord->getRoom();
    QList<ServerPlayer *> unions = room->getOtherPlayers(lord, true);
    int difficulty = Config.value("BossModeDifficulty", 0).toInt();
    if ((difficulty & (1 << BMDRevive)) > 0) {
        foreach (ServerPlayer *p, unions) {
            if (p->isDead() && p->getMaxHp() > 0) {
                room->revivePlayer(p, true);
                room->addPlayerHistory(p, ".");
                if (!p->faceUp())
                    p->turnOver();
                if (p->isChained())
                    room->setPlayerProperty(p, "chained", false);
                p->setProperty("hp", qMin(p->getMaxHp(), 4));
                room->broadcastProperty(p, "hp");
                QStringList acquired = p->tag["BossModeAcquiredSkills"].toStringList();
                foreach (QString skillname, acquired) {
                    if (p->hasSkill(skillname, true))
                        acquired.removeOne(skillname);
                }
                p->tag["BossModeAcquiredSkills"] = QVariant::fromValue(acquired);
                if (!acquired.isEmpty())
                    room->handleAcquireDetachSkills(p, acquired, true);
                foreach (const Skill *skill, p->getSkillList()) {
                    if (skill->getFrequency() == Skill::Limited && !skill->getLimitMark().isEmpty())
                        room->setPlayerMark(p, skill->getLimitMark(), 1);
                }
            }
        }
    }
    if ((difficulty & (1 << BMDRecover)) > 0) {
        foreach (ServerPlayer *p, unions) {
            if (p->isAlive() && p->isWounded()) {
                p->setProperty("hp", p->getMaxHp());
                room->broadcastProperty(p, "hp");
            }
        }
    }
    if ((difficulty & (1 << BMDDraw)) > 0) {
        foreach (ServerPlayer *p, unions) {
            if (p->isAlive() && p->getHandcardNum() < 4) {
                room->setTag("FirstRound", true); //For Manjuan
                try {
                    p->drawCards(4 - p->getHandcardNum());
                    room->setTag("FirstRound", false);
                }
                catch (TriggerEvent triggerEvent) {
                    if (triggerEvent == TurnBroken || triggerEvent == StageChange)
                        room->setTag("FirstRound", false);
                    throw triggerEvent;
                }
            }
        }
    }
    if ((difficulty & (1 << BMDReward)) > 0) {
        foreach (ServerPlayer *p, unions) {
            if (p->isAlive()) {
                room->setTag("FirstRound", true); //For Manjuan
                try {
                    p->drawCards(2);
                    room->setTag("FirstRound", false);
                }
                catch (TriggerEvent triggerEvent) {
                    if (triggerEvent == TurnBroken || triggerEvent == StageChange)
                        room->setTag("FirstRound", false);
                    throw triggerEvent;
                }
            }
        }
    }
}

void GameRule::rewardAndPunish(ServerPlayer *killer, ServerPlayer *victim) const
{
    Room *room = killer->getRoom();
    if (killer->isDead() || room->getMode() == "06_XMode"
        || room->getMode() == "04_boss"
        || room->getMode() == "08_defense")
        return;

    if (room->getMode() == "06_3v3") {
        if (Config.value("3v3/OfficialRule", "2013").toString().startsWith("201"))
            killer->drawCards(2, "kill");
        else
            killer->drawCards(3, "kill");
    } else {
        if (victim->getRole() == "rebel" && killer != victim)
            killer->drawCards(3, "kill");
        else if (victim->getRole() == "loyalist" && killer->isLord())
            killer->throwAllHandCardsAndEquips();
    }
}

QString GameRule::getWinner(ServerPlayer *victim) const
{
    Room *room = victim->getRoom();
    QString winner;

    if (room->getMode() == "06_3v3") {
        switch (victim->getRoleEnum()) {
        case Player::Lord: winner = "renegade+rebel"; break;
        case Player::Renegade: winner = "lord+loyalist"; break;
        default:
            break;
        }
    } else if (room->getMode() == "06_XMode") {
        QString role = victim->getRole();
        ServerPlayer *leader = victim->tag["XModeLeader"].value<ServerPlayer *>();
        if (leader->tag["XModeBackup"].toStringList().isEmpty()) {
            if (role.startsWith('r'))
                winner = "lord+loyalist";
            else
                winner = "renegade+rebel";
        }
    } else if (room->getMode() == "08_defense") {
        QStringList alive_roles = room->aliveRoles(victim);
        if (!alive_roles.contains("loyalist"))
            winner = "rebel";
        else if (!alive_roles.contains("rebel"))
            winner = "loyalist";
    } else if (Config.EnableHegemony) {
        bool has_anjiang = false, has_diff_kingdoms = false;
        QString init_kingdom;
        foreach (ServerPlayer *p, room->getAlivePlayers()) {
            if (!p->property("basara_generals").toString().isEmpty())
                has_anjiang = true;

            if (init_kingdom.isEmpty())
                init_kingdom = p->getKingdom();
            else if (init_kingdom != p->getKingdom())
                has_diff_kingdoms = true;
        }

        if (!has_anjiang && !has_diff_kingdoms) {
            QStringList winners;
            QString aliveKingdom = room->getAlivePlayers().first()->getKingdom();
            foreach (ServerPlayer *p, room->getPlayers()) {
                if (p->isAlive()) winners << p->objectName();
                if (p->getKingdom() == aliveKingdom) {
                    QStringList generals = p->property("basara_generals").toString().split("+");
                    if (generals.size() && !Config.Enable2ndGeneral) continue;
                    if (generals.size() > 1) continue;

                    //if someone showed his kingdom before death,
                    //he should be considered victorious as well if his kingdom survives
                    winners << p->objectName();
                }
            }
            winner = winners.join("+");
        }
        if (!winner.isNull()) {
            foreach (ServerPlayer *player, room->getAllPlayers()) {
                if (player->getGeneralName() == "anjiang") {
                    QStringList generals = player->property("basara_generals").toString().split("+");
                    room->changePlayerGeneral(player, generals.at(0));

                    room->setPlayerProperty(player, "kingdom", player->getGeneral()->getKingdom());
                    room->setPlayerProperty(player, "role", BasaraMode::getMappedRole(player->getKingdom()));

                    generals.takeFirst();
                    player->setProperty("basara_generals", generals.join("+"));
                    room->notifyProperty(player, player, "basara_generals");
                }
                if (Config.Enable2ndGeneral && player->getGeneral2Name() == "anjiang") {
                    QStringList generals = player->property("basara_generals").toString().split("+");
                    room->changePlayerGeneral2(player, generals.at(0));
                }
            }
        }
    } else {
        QStringList alive_roles = room->aliveRoles(victim);
        switch (victim->getRoleEnum()) {
        case Player::Lord: {
            if (alive_roles.length() == 1 && alive_roles.first() == "renegade")
                winner = room->getAlivePlayers().first()->objectName();
            else
                winner = "rebel";
            break;
        }
        case Player::Rebel:
        case Player::Renegade: {
            if (!alive_roles.contains("rebel") && !alive_roles.contains("renegade"))
                winner = "lord+loyalist";
            break;
        }
        default:
            break;
        }
    }

    return winner;
}

HulaoPassMode::HulaoPassMode(QObject *parent)
    : GameRule(parent)
{
    setObjectName("hulaopass_mode");
    events << HpChanged << StageChange;
}

bool HulaoPassMode::trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
{
    switch (triggerEvent) {
    case StageChange: {
        ServerPlayer *lord = room->getLord();
        room->setPlayerMark(lord, "secondMode", 1);
        QString lvbus = "shenlvbu2+shenlvbu3";
        QString general = room->askForGeneral(lord, lvbus.split("+"), true, QString(), false);
        int x = lord->getHp();
        room->changeHero(lord, general, true, true, false, false);
        if (x > 4) {
            room->setPlayerProperty(lord, "maxhp", x);
            room->setPlayerProperty(lord, "hp", x);
        }
        LogMessage log;
        log.type = "$AppendSeparator";
        room->sendLog(log);

        log.type = "#HulaoTransfigure";
        log.arg = "shenlvbu1";
        log.arg2 = general;
        room->sendLog(log);

        QList<const Card *> tricks = lord->getJudgingArea();
        if (!tricks.isEmpty()) {
            DummyCard *dummy = new DummyCard;
            foreach(const Card *trick, tricks)
                dummy->addSubcard(trick);
            CardMoveReason reason(CardMoveReason::S_REASON_NATURAL_ENTER, QString());
            room->throwCard(dummy, reason, NULL);
            dummy->deleteLater();
        }
        if (!lord->faceUp())
            lord->turnOver();
        if (lord->isChained())
            room->setPlayerProperty(lord, "chained", false);
        break;
    }
    case GameStart: {
        // Handle global events
        if (player == NULL) {
            foreach (ServerPlayer *p, room->getPlayers()) {
                foreach (const Skill *skill, p->getVisibleSkillList()) {
                    if (skill->getFrequency() == Skill::Limited && !skill->getLimitMark().isEmpty()
                        && (!skill->isLordSkill() || p->hasLordSkill(skill->objectName())))
                        room->setPlayerMark(p, skill->getLimitMark(), 1);
                }
            }
            room->setTag("FirstRound", true);
            room->setTag("TurnFirstRound", true);
            ServerPlayer *lord = room->getLord();
            lord->drawCards(8);
            foreach (ServerPlayer *player, room->getPlayers()) {
                if (!player->isLord())
                    player->drawCards(player->getSeat() + 1);
            }
            return false;
        }
        break;
    }
    case HpChanged: {
        if (player->isLord() && player->getHp() <= 4 && player->getMark("secondMode") == 0)
            throw StageChange;
        break;
    }
    case PlayCard: {
        if (room->getLord() && room->getLord()->getMark("secondMode") == 0 && room->getTag("SwapPile").toInt() > 0)
            throw StageChange;
        break;
    }
    case EventPhaseChanging: {
        if (room->getLord() && room->getLord()->getMark("secondMode") == 0 && room->getTag("SwapPile").toInt() > 0)
            throw StageChange;
        break;
    }
    case EventPhaseStart: {
        if (player->getPhase() == Player::NotActive && room->getTag("SwapPile").toInt() > 1)
            room->gameOver("lord");
        break;
    }
    case GameOverJudge: {
        if (player->isLord())
            room->gameOver("rebel");
        else if (room->aliveRoles(player).length() == 1)
            room->gameOver("lord");

        return false;
    }
    case BuryVictim: {
        if (player->hasFlag("actioned")) room->setPlayerFlag(player, "-actioned");

        LogMessage log;
        log.type = "#Reforming";
        log.from = player;
        room->sendLog(log);

        player->bury();
        room->setPlayerProperty(player, "hp", 0);

        return false;
    }
    case TurnStart: {
        LogMessage log;
        log.type = "$AppendSeparator";
        room->sendLog(log);
        room->addPlayerMark(player, "Global_TurnCount");

        if (!player->faceUp())
            player->turnOver();
        else
            player->play();

        return false;
    }
    default:
        break;
    }

    return GameRule::trigger(triggerEvent, room, player, data);
}

BasaraMode::BasaraMode(QObject *parent)
    : GameRule(parent)
{
    setObjectName("basara_mode");
    events << EventPhaseStart << DamageInflicted << BeforeGameOverJudge;
}

QString BasaraMode::getMappedRole(const QString &role)
{
    static QMap<QString, QString> roles;
    if (roles.isEmpty()) {
        roles["wei"] = "lord";
        roles["shu"] = "loyalist";
        roles["wu"] = "rebel";
        roles["qun"] = "renegade";
    }
    return roles[role];
}

int BasaraMode::getPriority(TriggerEvent) const
{
    return 15;
}

void BasaraMode::playerShowed(ServerPlayer *player) const
{
    Room *room = player->getRoom();
    QString name = player->property("basara_generals").toString();
    if (name.isEmpty())
        return;
    QStringList names = name.split("+");

    if (Config.EnableHegemony) {
        QMap<QString, int> kingdom_roles;
        foreach(ServerPlayer *p, room->getOtherPlayers(player))
            kingdom_roles[p->getKingdom()]++;

        if (kingdom_roles[Sanguosha->getGeneral(names.first())->getKingdom()] >= Config.value("HegemonyMaxShown", 2).toInt()
            && player->getGeneralName() == "anjiang")
            return;
    }

    QString answer = room->askForChoice(player, "RevealGeneral", "yes+no");
    if (answer == "yes") {
        QString general_name = room->askForGeneral(player, names);

        generalShowed(player, general_name);
        if (Config.EnableHegemony) room->getThread()->trigger(GameOverJudge, room, player);
        playerShowed(player);
    }
}

void BasaraMode::generalShowed(ServerPlayer *player, QString general_name) const
{
    Room *room = player->getRoom();
    QString name = player->property("basara_generals").toString();
    if (name.isEmpty())
        return;
    QStringList names = name.split("+");

    if (player->getGeneralName() == "anjiang") {
        room->changeHero(player, general_name, false, false, false, false);
        room->setPlayerProperty(player, "kingdom", player->getGeneral()->getKingdom());

        if (player->getGeneral()->getKingdom() == "god")
            room->setPlayerProperty(player, "kingdom", room->askForKingdom(player));

        if (Config.EnableHegemony)
            room->setPlayerProperty(player, "role", getMappedRole(player->getKingdom()));
    } else {
        room->changeHero(player, general_name, false, false, true, false);
    }

    names.removeOne(general_name);
    player->setProperty("basara_generals", names.join("+"));
    room->notifyProperty(player, player, "basara_generals");

    LogMessage log;
    log.type = "#BasaraReveal";
    log.from = player;
    log.arg = player->getGeneralName();
    if (player->getGeneral2()) {
        log.type = "#BasaraRevealDual";
        log.arg2 = player->getGeneral2Name();
    }
    room->sendLog(log);
}

bool BasaraMode::trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
{
    // Handle global events
    if (player == NULL) {
        if (triggerEvent == GameStart) {
            if (Config.EnableHegemony)
                room->setTag("SkipNormalDeathProcess", true);
            foreach (ServerPlayer *sp, room->getAlivePlayers()) {
                room->setPlayerProperty(sp, "general", "anjiang");
                sp->setGender(General::Sexless);
                room->setPlayerProperty(sp, "kingdom", "god");

                LogMessage log;
                log.type = "#BasaraGeneralChosen";
                log.arg = sp->property("basara_generals").toString().split("+").first();

                if (Config.Enable2ndGeneral) {
                    room->setPlayerProperty(sp, "general2", "anjiang");
                    log.type = "#BasaraGeneralChosenDual";
                    log.arg2 = sp->property("basara_generals").toString().split("+").last();
                }

                room->sendLog(log, sp);
            }
        }
        return false;
    }

    player->tag["triggerEvent"] = triggerEvent;
    player->tag["triggerEventData"] = data; // For AI

    switch (triggerEvent) {
    case CardEffected: {
        if (player->getPhase() == Player::NotActive) {
            CardEffectStruct ces = data.value<CardEffectStruct>();
            if (ces.card)
                if (ces.card->isKindOf("TrickCard") || ces.card->isKindOf("Slash"))
                    playerShowed(player);

            const ProhibitSkill *prohibit = room->isProhibited(ces.from, ces.to, ces.card);
            if (prohibit && ces.to->hasSkill(prohibit)) {
                if (prohibit->isVisible()) {
                    LogMessage log;
                    log.type = "#SkillAvoid";
                    log.from = ces.to;
                    log.arg = prohibit->objectName();
                    log.arg2 = ces.card != NULL ? ces.card->objectName() : "";
                    room->sendLog(log);

                    ces.to->broadcastSkillInvoke(prohibit->objectName());
                    room->notifySkillInvoked(ces.to, prohibit->objectName());
                }

                return true;
            }
        }
        break;
    }
    case EventPhaseStart: {
        if (player->getPhase() == Player::RoundStart)
            playerShowed(player);

        break;
    }
    case DamageInflicted: {
        playerShowed(player);
        break;
    }
    case BeforeGameOverJudge: {
        if (player->getGeneralName() == "anjiang") {
            QStringList generals = player->property("basara_generals").toString().split("+");
            room->changePlayerGeneral(player, generals.at(0));

            room->setPlayerProperty(player, "kingdom", player->getGeneral()->getKingdom());
            if (Config.EnableHegemony)
                room->setPlayerProperty(player, "role", getMappedRole(player->getKingdom()));

            generals.takeFirst();
            player->setProperty("basara_generals", generals.join("+"));
            room->notifyProperty(player, player, "basara_generals");
        }
        if (Config.Enable2ndGeneral && player->getGeneral2Name() == "anjiang") {
            QStringList generals = player->property("basara_generals").toString().split("+");
            room->changePlayerGeneral2(player, generals.at(0));
            player->setProperty("basara_generals", QString());
            room->notifyProperty(player, player, "basara_generals");
        }
        break;
    }
    case BuryVictim: {
        DeathStruct death = data.value<DeathStruct>();
        player->bury();
        if (Config.EnableHegemony) {
            ServerPlayer *killer = death.damage ? death.damage->from : NULL;
            if (killer && killer->getKingdom() != "god") {
                if (killer->getKingdom() == player->getKingdom())
                    killer->throwAllHandCardsAndEquips();
                else if (killer->isAlive())
                    killer->drawCards(3, "kill");
            }
            return true;
        }

        break;
    }
    default:
        break;
    }
    return false;
}

