#include "standard.h"
#include "serverplayer.h"
#include "room.h"
#include "skill.h"
#include "maneuvering.h"
#include "clientplayer.h"
#include "engine.h"
#include "client.h"
#include "exppattern.h"
#include "roomthread.h"

QString BasicCard::getType() const
{
    return "basic";
}

Card::CardType BasicCard::getTypeId() const
{
    return TypeBasic;
}

TrickCard::TrickCard(Suit suit, int number)
    : Card(suit, number), cancelable(true)
{
    handling_method = Card::MethodUse;
}

void TrickCard::setCancelable(bool cancelable)
{
    this->cancelable = cancelable;
}

QString TrickCard::getType() const
{
    return "trick";
}

Card::CardType TrickCard::getTypeId() const
{
    return TypeTrick;
}

bool TrickCard::isCancelable(const CardEffectStruct &effect) const
{
    Q_UNUSED(effect);
    return cancelable;
}

QString EquipCard::getType() const
{
    return "equip";
}

Card::CardType EquipCard::getTypeId() const
{
    return TypeEquip;
}

bool EquipCard::targetRated(const QList<const Player *> &targets, const Player *, const Player *) const
{
    return targets.isEmpty();
}

bool EquipCard::isAvailable(const Player *player) const
{
    if (!Card::isAvailable(player)) return false;
    if (gift) return true;
    QStringList area_names;
    area_names << "WeaponSealed" << "ArmorSealed" << "DefensiveHorseSealed" << "OffensiveHorseSealed" << "TreasureSealed";
    int index = (int) location();
    return !player->isProhibited(player, this) && player->getMark(area_names.at(index)) == 0;
}

void EquipCard::onUse(Room *room, const CardUseStruct &card_use) const
{
    CardUseStruct use = card_use;

    ServerPlayer *player = use.from;
    if (use.to.isEmpty())
        use.to << player;

    QVariant data = QVariant::fromValue(use);
    RoomThread *thread = room->getThread();
    thread->trigger(PreCardUsed, room, player, data);
	use = data.value<CardUseStruct>();

	LogMessage log;
    log.from = use.from;
    if (!use.card->targetFixed() || use.to.length() > 1 || !use.to.contains(use.from))
        log.to = use.to;
    log.type = "#UseCard";
    log.card_str = use.card->toString();
    room->sendLog(log);

	CardMoveReason reason(CardMoveReason::S_REASON_USE, use.from->objectName(), QString(), use.card->getSkillName(), QString());
    if (use.to.size() == 1 && !use.card->targetFixed())
        reason.m_targetId = use.to.first()->objectName();
    reason.m_extraData = QVariant::fromValue(use);
    room->moveCardTo(this, NULL, Player::PlaceTable, reason, true);

    thread->trigger(CardUsed, room, player, data);
    thread->trigger(CardFinished, room, player, data);
}

void EquipCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    if (room->getCardPlace(getEffectiveId()) != Player::PlaceTable) return;

	if (targets.isEmpty()) {
        CardMoveReason reason(CardMoveReason::S_REASON_USE, source->objectName(), QString(), this->getSkillName(), QString());
        room->moveCardTo(this, source, NULL, Player::DiscardPile, reason, true);
		return;
    }

    int equipped_id = Card::S_UNKNOWN_CARD_ID;
    ServerPlayer *target = targets.first();
    if (target->getEquip(location()))
        equipped_id = target->getEquip(location())->getEffectiveId();

    QList<CardsMoveStruct> exchangeMove;
    CardsMoveStruct move1(getEffectiveId(), target, Player::PlaceEquip,
        CardMoveReason(CardMoveReason::S_REASON_USE, target->objectName()));
    exchangeMove.push_back(move1);
    if (equipped_id != Card::S_UNKNOWN_CARD_ID) {
        CardsMoveStruct move2(equipped_id, NULL, Player::DiscardPile,
            CardMoveReason(CardMoveReason::S_REASON_CHANGE_EQUIP, target->objectName()));
        exchangeMove.push_back(move2);
    }
    LogMessage log;
    log.from = target;
    log.type = "$Install";
    log.card_str = QString::number(getEffectiveId());
    room->sendLog(log);

    room->moveCardsAtomic(exchangeMove, true);
}

void EquipCard::onInstall(ServerPlayer *player) const
{
    Room *room = player->getRoom();

    const Skill *skill = Sanguosha->getSkill(this);
    if (skill) {
        if (skill->inherits("ViewAsSkill")) {
            room->attachSkillToPlayer(player, objectName());
        } else if (skill->inherits("TriggerSkill")) {
            const TriggerSkill *trigger_skill = qobject_cast<const TriggerSkill *>(skill);
            room->getThread()->addTriggerSkill(trigger_skill);
            if (trigger_skill->getViewAsSkill() != NULL)
                room->attachSkillToPlayer(player, objectName());
        }
    }
}

void EquipCard::onUninstall(ServerPlayer *player) const
{
    Room *room = player->getRoom();
    if (Sanguosha->getSkill(this) && Sanguosha->getSkill(this)->inherits("ViewAsSkill"))
        room->detachSkillFromPlayer(player, this->objectName(), true);
}

QString GlobalEffect::getSubtype() const
{
    return "global_effect";
}

QList<ServerPlayer *> GlobalEffect::defaultTargets(Room *room, ServerPlayer *source) const
{
    QList<ServerPlayer *> targets, all_players = room->getAllPlayers();
    foreach (ServerPlayer *player, all_players) {
        const ProhibitSkill *skill = room->isProhibited(source, player, this);
        if (skill) {
            if (skill->isVisible()) {
                LogMessage log;
                log.type = "#SkillAvoid";
                log.from = player;
                log.arg = skill->objectName();
                log.arg2 = objectName();
                room->sendLog(log);

                room->notifySkillInvoked(player, skill->objectName());
                player->broadcastSkillInvoke(skill->objectName());
            }
        } else
            targets << player;
    }
    return targets;
}

bool GlobalEffect::isAvailable(const Player *player) const
{
    bool canUse = false;
    QList<const Player *> players = player->getAliveSiblings();
    players << player;
    foreach (const Player *p, players) {
        if (player->isProhibited(p, this))
            continue;

        canUse = true;
        break;
    }

    return canUse && TrickCard::isAvailable(player);
}

QString AOE::getSubtype() const
{
    return "aoe";
}

bool AOE::isAvailable(const Player *player) const
{
    bool canUse = false;
    QList<const Player *> players = player->getAliveSiblings();
    foreach (const Player *p, players) {
        if (player->isProhibited(p, this))
            continue;

        canUse = true;
        break;
    }

    return canUse && TrickCard::isAvailable(player);
}

QList<ServerPlayer *> AOE::defaultTargets(Room *room, ServerPlayer *source) const
{
    QList<ServerPlayer *> targets, all_players = room->getOtherPlayers(source);
    foreach (ServerPlayer *player, all_players) {
        const ProhibitSkill *skill = room->isProhibited(source, player, this);
        if (skill) {
            if (skill->isVisible()) {
                LogMessage log;
                log.type = "#SkillAvoid";
                log.from = player;
                log.arg = skill->objectName();
                log.arg2 = objectName();
                room->sendLog(log);

                room->notifySkillInvoked(player, skill->objectName());
                player->broadcastSkillInvoke(skill->objectName());
            }
        } else
            targets << player;
    }
    return targets;
}

QString SingleTargetTrick::getSubtype() const
{
    return "single_target_trick";
}

bool SingleTargetTrick::targetRated(const QList<const Player *> &, const Player *, const Player *) const
{
    return true;
}

DelayedTrick::DelayedTrick(Suit suit, int number, bool movable)
    : TrickCard(suit, number), movable(movable)
{
    judge.negative = true;
}

void DelayedTrick::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    QStringList nullified_list = tag["CardUseNullifiedList"].toStringList();
    bool all_nullified = nullified_list.contains("_ALL_TARGETS");
    if (room->getCardPlace(getEffectiveId()) == Player::PlaceTable) {
		if (all_nullified || targets.isEmpty()) {
            CardMoveReason reason(CardMoveReason::S_REASON_USE, source->objectName(), QString(), getSkillName(), QString());
            room->moveCardTo(this, room->getCardOwner(getEffectiveId()), NULL, Player::DiscardPile, reason, true);
        } else {
			CardMoveReason reason(CardMoveReason::S_REASON_USE, source->objectName(), targets.first()->objectName(), getSkillName(), QString());
            room->moveCardTo(this, NULL, targets.first(), Player::PlaceDelayedTrick, reason, true);
		}
    }
}

QString DelayedTrick::getSubtype() const
{
    return "delayed_trick";
}

void DelayedTrick::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.to->getRoom();

    LogMessage log;
    log.from = effect.to;
    log.type = "#DelayedTrick";
    log.arg = effect.card->objectName();
    room->sendLog(log);

    JudgeStruct judge_struct = judge;
    judge_struct.who = effect.to;
    judge_struct.good = effect.to->hasSkill("bossshenyi") ? !judge.good : judge.good;
    room->judge(judge_struct);

    if (judge_struct.isBad()) {
        takeEffect(effect.to);
        if (room->getCardOwner(getEffectiveId()) == NULL) {
            CardMoveReason reason(CardMoveReason::S_REASON_NATURAL_ENTER, QString());
            room->throwCard(this, reason, NULL);
        }
    } else if (movable) {
        onNullified(effect.to);
    } else {
        if (room->getCardOwner(getEffectiveId()) == NULL) {
            CardMoveReason reason(CardMoveReason::S_REASON_NATURAL_ENTER, QString());
            room->throwCard(this, reason, NULL);
        }
    }
}

void DelayedTrick::onNullified(ServerPlayer *target) const
{
    Room *room = target->getRoom();
    if (target->isAlive() && movable) {
        QList<ServerPlayer *> players = room->getOtherPlayers(target);
        players << target;

        foreach (ServerPlayer *player, players) {
            const ProhibitSkill *skill = room->isProhibited(NULL, player, this);
            if (skill) {
                if (skill->isVisible()) {
                    LogMessage log;
                    log.type = "#SkillAvoid";
                    log.from = player;
                    log.arg = skill->objectName();
                    log.arg2 = objectName();
                    room->sendLog(log);

                    room->notifySkillInvoked(player, skill->objectName());
                    player->broadcastSkillInvoke(skill->objectName());
                }
                continue;
            }

            CardMoveReason reason(CardMoveReason::S_REASON_TRANSFER, target->objectName(), QString(), this->getSkillName(), QString());
            room->moveCardTo(this, player, Player::PlaceDelayedTrick, reason, true);
			return;
        }
    }
    CardMoveReason reason(CardMoveReason::S_REASON_NATURAL_ENTER, target->objectName());
    room->throwCard(this, reason, NULL);
}

Weapon::Weapon(Suit suit, int number, int range)
    : EquipCard(suit, number), range(range)
{
    can_recast = true;
}

bool Weapon::isAvailable(const Player *player) const
{
    QString mode = player->getGameMode();
    if (mode == "04_1v3" && !player->isCardLimited(this, Card::MethodRecast))
        return true;
    return !player->isCardLimited(this, Card::MethodUse) && EquipCard::isAvailable(player);
}

int Weapon::getRange() const
{
    return range;
}

QString Weapon::getSubtype() const
{
    return "weapon";
}

void Weapon::onUse(Room *room, const CardUseStruct &card_use) const
{
    CardUseStruct use = card_use;
    ServerPlayer *player = card_use.from;
    if (room->getMode() == "04_1v3"
        && use.card->isKindOf("Weapon")
        && (player->isCardLimited(use.card, Card::MethodUse)
        || (!player->getHandPile().contains(getEffectiveId())
        && player->askForSkillInvoke("weapon_recast", QVariant::fromValue(use))))) {
        CardMoveReason reason(CardMoveReason::S_REASON_RECAST, player->objectName());
        reason.m_eventName = "weapon_recast";
        room->moveCardTo(use.card, player, NULL, Player::DiscardPile, reason);

        LogMessage log;
        log.type = "#UseCard_Recast";
        log.from = player;
        log.card_str = use.card->toString();
        room->sendLog(log);

        player->drawCards(1, "weapon_recast");
        return;
    }
    EquipCard::onUse(room, use);
}

EquipCard::Location Weapon::location() const
{
    return WeaponLocation;
}

QString Weapon::getCommonEffectName() const
{
    return "weapon";
}

QString Armor::getSubtype() const
{
    return "armor";
}

EquipCard::Location Armor::location() const
{
    return ArmorLocation;
}

QString Armor::getCommonEffectName() const
{
    return "armor";
}

Horse::Horse(Suit suit, int number, int correct)
    : EquipCard(suit, number), correct(correct)
{
}

int Horse::getCorrect() const
{
    return correct;
}

void Horse::onInstall(ServerPlayer *) const
{
}

void Horse::onUninstall(ServerPlayer *) const
{
}

QString Horse::getCommonEffectName() const
{
    return "horse";
}

OffensiveHorse::OffensiveHorse(Card::Suit suit, int number, int correct)
    : Horse(suit, number, correct)
{
}

QString OffensiveHorse::getSubtype() const
{
    return "offensive_horse";
}

DefensiveHorse::DefensiveHorse(Card::Suit suit, int number, int correct)
    : Horse(suit, number, correct)
{
}

QString DefensiveHorse::getSubtype() const
{
    return "defensive_horse";
}

EquipCard::Location Horse::location() const
{
    if (correct > 0)
        return DefensiveHorseLocation;
    else
        return OffensiveHorseLocation;
}

QString Treasure::getSubtype() const
{
    return "treasure";
}

EquipCard::Location Treasure::location() const
{
    return TreasureLocation;
}

QString Treasure::getCommonEffectName() const
{
    return "treasure";
}

class GlobalProhibit : public ProhibitSkill
{
public:
    GlobalProhibit() : ProhibitSkill("#global-prohibit")
    {
    }

    virtual bool isProhibited(const Player *from, const Player *to, const Card *card, const QList<const Player *> &) const
    {
        if (card->getTypeId() != Card::TypeSkill && from && from->hasFlag("DisabledOtherTargets") && to != from)
            return true;
        if (card->isKindOf("DelayedTrick") && (to->containsTrick(card->objectName()) || to->getMark("JudgeSealed") > 0))
            return true;
        return false;
    }
};

class GlobalRecord : public TriggerSkill
{
public:
    GlobalRecord() : TriggerSkill("#global-record")
    {
        events << CardsMoveOneTime << Death << PreDamageDone << CardUsed << PreCardResponded << CardFinished << CardResponded << TargetSpecified;
        global = true;
    }

    virtual bool triggerable(const ServerPlayer *) const
    {
        return false;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == CardsMoveOneTime && !room->getTag("FirstRound").toBool()) {
            QVariantList move_datas = data.toList();
            foreach(QVariant move_data, move_datas) {
                CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
                //record of get cards
                if (move.to == player && move.to_place == Player::PlaceHand) {
                    QStringList card_list;
                    if (player->property("GlobalGetCards").toString() != "")
                        card_list = player->property("GlobalGetCards").toString().split("+");
                    card_list << IntList2StringList(move.card_ids);
                    room->setPlayerProperty(player, "GlobalGetCards", card_list.join("+"));
                }

                //record of dis cards
                if (move.from == player && move.reason.m_playerId == player->objectName() &&
                        (move.reason.m_reason & CardMoveReason::S_MASK_BASIC_REASON) == CardMoveReason::S_REASON_DISCARD) {
                    int x=0, y=0;
                    foreach (Player::Place place, move.from_places) {
                        if (place == Player::PlaceHand)
                            x++;
                        if (place == Player::PlaceEquip)
                            y++;
                    }
                    if (move.reason.m_reason == CardMoveReason::S_REASON_RULEDISCARD)
                        room->addPlayerMark(player, "GlobalRuleDiscardCount", x);
                    room->addPlayerMark(player, "GlobalDiscardCount", x+y);
                }
            }
        } else if (triggerEvent == Death) {
            DeathStruct death = data.value<DeathStruct>();
            if (death.who != player) return;
            ServerPlayer *killer = death.damage ? death.damage->from : NULL;
            ServerPlayer *current = room->getCurrent();

            if (killer && current && current->getPhase() != Player::NotActive)
                room->addPlayerMark(killer, "GlobalKilledCount");
            if (killer && current && current->getPhase() == Player::Play)
                room->addPlayerMark(killer, "GlobalKilledCountinPlay");

        } else if (triggerEvent == PreDamageDone) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.card) {
                QStringList damaged_tag = damage.card->tag["GlobalCardDamagedTag"].toStringList();
                damaged_tag << player->objectName();
                damage.card->setTag("GlobalCardDamagedTag", damaged_tag);
            }
            if (damage.from && damage.from->distanceTo(player) < 2)
                damage.flags << "kuanggu";
            if (!player->faceUp())
                damage.flags << "jiushi";
            ServerPlayer *current = room->getCurrent();
            if (current && current->getPhase() != Player::NotActive) {
                room->setTag("ZuodingCannot", true);

                if (player->getMark("GlobalInjuredCount") == 0)
                    damage.flags << "FirstDamge";
                else
                    damage.flags << "NotFirstDamge";
                room->addPlayerMark(player, "GlobalInjuredCount", damage.damage);
            }
            data = QVariant::fromValue(damage);
        } else if (triggerEvent == CardUsed || triggerEvent == PreCardResponded) {
            //to record used/responded cards
            const Card *card = NULL;
            bool is_use = true;
            if (triggerEvent == CardUsed)
                card = data.value<CardUseStruct>().card;
            else {
                CardResponseStruct response = data.value<CardResponseStruct>();
                if (!response.m_isUse)
                   is_use = false;
                card = response.m_card;
            }
            if (card && card->getTypeId() != Card::TypeSkill) {
                ServerPlayer *current = room->getCurrent();
                if (current && current->getPhase() != Player::NotActive) {
                    QString type_name[4] = { QString(), "BasicCard", "TrickCard", "EquipCard" };
                    if (player->getCardUsedTimes(type_name[card->getTypeId()]) == 0)
                        room->setCardFlag(card, "ShicaiCanInvoke");



                    if (current->getPhase() == Player::Play) {

                    }
                }


                if (card->isKindOf("Analeptic") && !card->hasFlag("UsedBySecondWay"))
                    room->addPlayerMark(player, "AnalepticUsedTimes");



                QString tag1_name = is_use? "GameUsedCards": "GameRespondedCards";
                QString tag2_name = is_use? "RoundUsedCards": "RoundRespondedCards";
                QString tag3_name = is_use? "PhaseUsedCards": "PhaseRespondedCards";




                QVariantList card_list = player->tag[tag1_name].toList();
                card_list << QVariant::fromValue(card);
                player->tag[tag1_name] = card_list;
                if (current && current->getPhase() != Player::NotActive) {



                    QVariantList card_list = player->tag[tag2_name].toList();
                    card_list << QVariant::fromValue(card);
                    player->tag[tag2_name] = card_list;

                    if (current->getPhase() == Player::Play) {



                        QVariantList card_list = player->tag[tag3_name].toList();
                        card_list << QVariant::fromValue(card);
                        player->tag[tag3_name] = card_list;
                    }

                }
            }
        } else if (triggerEvent == TargetSpecified) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card && use.card->getTypeId() != Card::TypeSkill && use.to.size() == 1) {
                room->setCardFlag(use.card, "XingluanCanInvoke");
            }
        } else if (triggerEvent == CardFinished) {



        }
    }
};


class GlobalClear : public TriggerSkill
{
public:
    GlobalClear() : TriggerSkill("#global-clear")
    {
        events << EventPhaseStart;
        global = true;
    }

    virtual bool triggerable(const ServerPlayer *) const
    {
        return false;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (triggerEvent == EventPhaseStart) {
            if (player->getPhase() == Player::NotActive) {
                room->removeTag("ZuodingCannot");
                foreach (ServerPlayer *p, room->getAlivePlayers()) {
                    room->setPlayerMark(p, "GlobalRuleDiscardCount", 0);
                    room->setPlayerMark(p, "GlobalDiscardCount", 0);
                    room->setPlayerMark(p, "GlobalKilledCount", 0);
                    room->setPlayerMark(p, "GlobalKilledCountinPlay", 0);
                    room->setPlayerMark(p, "GlobalInjuredCount", 0);
                    room->setPlayerMark(p, "Global_MaxcardsIncrease", 0);
                    room->setPlayerMark(p, "Global_MaxcardsDecrease", 0);
                    room->setPlayerMark(p, "AnalepticUsedTimes", 0);
                    p->tag.remove("RoundUsedCards");
                    p->tag.remove("RoundRespondedCards");
                    p->tag.remove("PhaseUsedCards");
                    p->tag.remove("PhaseRespondedCards");
                    room->setPlayerProperty(p, "GlobalGetCards", QVariant());





                }
            }
        }
    }
};

class GlobalMaxCards : public MaxCardsSkill
{
public:
    GlobalMaxCards() : MaxCardsSkill("#global-maxcards")
    {
    }

    virtual int getExtra(const Player *target) const
    {
        return target->getMark("Global_MaxcardsIncrease") - target->getMark("Global_MaxcardsDecrease");
    }
};




















StandardPackage::StandardPackage()
    : Package("standard")
{
    addGenerals();

    patterns["."] = new ExpPattern(".|.|.|hand");
    patterns[".S"] = new ExpPattern(".|spade|.|hand");
    patterns[".C"] = new ExpPattern(".|club|.|hand");
    patterns[".H"] = new ExpPattern(".|heart|.|hand");
    patterns[".D"] = new ExpPattern(".|diamond|.|hand");

    patterns[".black"] = new ExpPattern(".|black|.|hand");
    patterns[".red"] = new ExpPattern(".|red|.|hand");

    patterns[".."] = new ExpPattern(".");
    patterns["..S"] = new ExpPattern(".|spade");
    patterns["..C"] = new ExpPattern(".|club");
    patterns["..H"] = new ExpPattern(".|heart");
    patterns["..D"] = new ExpPattern(".|diamond");

    patterns[".Basic"] = new ExpPattern("BasicCard");
    patterns[".Trick"] = new ExpPattern("TrickCard");
    patterns[".Equip"] = new ExpPattern("EquipCard");

    patterns[".Weapon"] = new ExpPattern("Weapon");
    patterns["slash"] = new ExpPattern("Slash");
    patterns["jink"] = new ExpPattern("Jink");
    patterns["peach"] = new  ExpPattern("Peach");
    patterns["nullification"] = new ExpPattern("Nullification");
    patterns["peach+analeptic"] = new ExpPattern("Peach,Analeptic");

    skills << new GlobalProhibit << new GlobalRecord << new GlobalClear << new GlobalMaxCards;
}

ADD_PACKAGE(Standard)

