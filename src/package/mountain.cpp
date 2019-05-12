#include "mountain.h"
#include "general.h"
#include "settings.h"
#include "skill.h"
#include "engine.h"
#include "standard.h"
#include "clientplayer.h"
#include "client.h"
#include "ai.h"
#include "json.h"

class dummyVS : public ZeroCardViewAsSkill
{
public:
    dummyVS() : ZeroCardViewAsSkill("dummy")
    {
    }

    virtual const Card *viewAs() const
    {
        return NULL;
    }
};

QiaobianMoveCard::QiaobianMoveCard()
{
    mute = true;
}

bool QiaobianMoveCard::targetsFeasible(const QList<const Player *> &targets, const Player *) const
{
    return targets.length() == 2;
}

bool QiaobianMoveCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *) const
{
    if (targets.isEmpty())
        return (!to_select->getJudgingArea().isEmpty() || !to_select->getEquips().isEmpty());
    else if (targets.length() == 1){
        for (int i = 0; i < S_EQUIP_AREA_LENGTH; i++) {
            if (targets.first()->getEquip(i) && to_select->canSetEquip(i))
                return true;
        }
        foreach(const Card *card, targets.first()->getJudgingArea()){
            if (!Sanguosha->isProhibited(NULL, to_select, card))
                return true;
        }

    }
    return false;
}

void QiaobianMoveCard::onUse(Room *room, const CardUseStruct &card_use) const
{
	CardUseStruct use = card_use;
    ServerPlayer *zhanghe = use.from;

    if (use.to.length() != 2) return;

    ServerPlayer *from = use.to.first();
    ServerPlayer *to = use.to.last();

    QList<int> all, ids, disabled_ids;
    for (int i = 0; i < S_EQUIP_AREA_LENGTH; i++) {
        if (from->getEquip(i)){
            if (!to->getEquip(i))
                ids << from->getEquip(i)->getEffectiveId();
            else
                disabled_ids << from->getEquip(i)->getEffectiveId();
            all << from->getEquip(i)->getEffectiveId();
        }
    }

    foreach(const Card *card, from->getJudgingArea()){
        if (!Sanguosha->isProhibited(NULL ,to, card))
            ids << card->getEffectiveId();
        else
            disabled_ids << card->getEffectiveId();
        all << card->getEffectiveId();
    }

    room->fillAG(all, zhanghe, disabled_ids);
    from->setFlags("QiaobianTarget");
    int card_id = room->askForAG(zhanghe, ids, true, "qiaobian");
    from->setFlags("-QiaobianTarget");
    room->clearAG(zhanghe);

    if (card_id != -1)
        room->moveCardTo(Sanguosha->getCard(card_id), from, to, room->getCardPlace(card_id), CardMoveReason(CardMoveReason::S_REASON_TRANSFER, zhanghe->objectName(), "qiaobian", QString()));

}

void QiaobianMoveCard::onEffect(const CardEffectStruct &effect) const
{
   Room *room = effect.from->getRoom();
    if (!effect.to->isKongcheng()) {
        int card_id = room->askForCardChosen(effect.from, effect.to, "h", "qiaobian");
        CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, effect.from->objectName());
        room->obtainCard(effect.from, Sanguosha->getCard(card_id), reason, false);
    }
}

class QiaobianMove : public ZeroCardViewAsSkill
{
public:
    QiaobianMove() : ZeroCardViewAsSkill("qiaobian_move")
    {
        response_pattern = "@@qiaobian_move";
    }

    virtual const Card *viewAs() const
    {
        return new QiaobianMoveCard;
    }
};

class Qiaobian : public TriggerSkill
{
public:
    Qiaobian() : TriggerSkill("qiaobian")
    {
        events << EventPhaseChanging;
    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        PhaseChangeStruct change = data.value<PhaseChangeStruct>();
        int index = 0;
        switch (change.to) {
            case Player::RoundStart:
            case Player::Start:
            case Player::Finish:
            case Player::NotActive: return QStringList();

            case Player::Judge: index = 1; break;
            case Player::Draw: index = 2; break;
            case Player::Play: index = 3; break;
            case Player::Discard: index = 4; break;
            case Player::PhaseNone: Q_ASSERT(false);
        }
        if (TriggerSkill::triggerable(player) && index > 0 && !player->isKongcheng() && !player->isSkipped(change.to))
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *zhanghe, QVariant &data, ServerPlayer *) const
    {
        PhaseChangeStruct change = data.value<PhaseChangeStruct>();
        int index = 0;
        switch (change.to) {
        case Player::RoundStart:
        case Player::Start:
        case Player::Finish:
        case Player::NotActive: return false;

        case Player::Judge: index = 1; break;
        case Player::Draw: index = 2; break;
        case Player::Play: index = 3; break;
        case Player::Discard: index = 4; break;
        case Player::PhaseNone: Q_ASSERT(false);
        }

        QString discard_prompt = QString("#qiaobian-%1").arg(index);
        QString use_prompt = QString("@qiaobian-%1").arg(index);
        if (index > 0 && room->askForCard(zhanghe, ".", discard_prompt, data, objectName())) {
            if (!zhanghe->isAlive()) return false;
            zhanghe->skip(change.to, true);
            if (index == 2) {
                QList<ServerPlayer *> to_choose;
                QList<ServerPlayer *> players = room->getOtherPlayers(zhanghe);
                foreach(ServerPlayer *p, players) {
                    if (zhanghe->canGetCard(p, "h"))
                        to_choose << p;
                }

                if (to_choose.isEmpty()) return false;
                QList<ServerPlayer *> choosees = room->askForPlayersChosen(zhanghe, to_choose, objectName(), 0, 2, use_prompt);

                if (choosees.length() > 0) {
                    room->sortByActionOrder(choosees);

                    foreach (ServerPlayer *target, choosees)
                        room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, zhanghe->objectName(), target->objectName());

                    foreach (ServerPlayer *target, choosees) {
                        if (!zhanghe->canGetCard(target, "h")) continue;
                        int card_id = room->askForCardChosen(zhanghe, target, "h", objectName(), false, Card::MethodGet);
                        CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, zhanghe->objectName());
                        room->obtainCard(zhanghe, Sanguosha->getCard(card_id), reason, false);
                    }
                }


            } else if (index == 3)
                room->askForUseCard(zhanghe, "@@qiaobian_move", use_prompt);
        }
        return false;
    }
};

class Beige : public TriggerSkill
{
public:
    Beige() : TriggerSkill("beige")
    {
        events << Damaged;
    }

    virtual TriggerList triggerable(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data) const
    {
        TriggerList skill_list;
        if (player == NULL || player->isDead()) return skill_list;

        DamageStruct damage = data.value<DamageStruct>();
        if (damage.card == NULL || !damage.card->isKindOf("Slash"))
            return skill_list;

        QList<ServerPlayer *> caiwenjis = room->findPlayersBySkillName(objectName());
        foreach(ServerPlayer *caiwenji, caiwenjis)
            if (!caiwenji->isNude())
                skill_list.insert(caiwenji, QStringList(objectName()));
        return skill_list;
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *ask_who) const
    {
        ServerPlayer *caiwenji = ask_who;
        if (room->askForCard(caiwenji, "..", "@beige:" + player->objectName(), data, objectName())) {
            DamageStruct damage = data.value<DamageStruct>();

            JudgeStruct judge;
            judge.good = true;
            judge.play_animation = false;
            judge.patterns << ".|heart" << ".|diamond" << ".|club" << ".|spade";
            judge.who = player;
            judge.reason = objectName();

            room->judge(judge);

            if (judge.pattern == ".|heart") {
                RecoverStruct recover;
                recover.who = caiwenji;
                room->recover(player, recover);
            } else if (judge.pattern == ".|diamond") {
                player->drawCards(2, objectName());
            } else if (judge.pattern == ".|club") {
                if (damage.from && damage.from->isAlive())
                    room->askForDiscard(damage.from, objectName(), 2, 2, false, true);
            } else if (judge.pattern == ".|spade") {
                if (damage.from && damage.from->isAlive())
                    damage.from->turnOver();
            }
        }
        return false;
    }
};

class Duanchang : public TriggerSkill
{
public:
    Duanchang() : TriggerSkill("duanchang")
    {
        events << Death;
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (player == NULL || !player->hasSkill(objectName())) return QStringList();
        DeathStruct death = data.value<DeathStruct>();
        if (death.who != player) return QStringList();

        if (death.damage && death.damage->from && death.damage->from->isAlive())
            return QStringList(objectName());

        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        DeathStruct death = data.value<DeathStruct>();
        if (death.damage && death.damage->from && death.damage->from->isAlive()) {
            ServerPlayer *target = death.damage->from;
            LogMessage log;
            log.type = "#DuanchangLoseSkills";
            log.from = player;
            log.to << target;
            log.arg = objectName();
            room->sendLog(log);
            player->broadcastSkillInvoke(objectName());
            room->notifySkillInvoked(player, objectName());
            room->addPlayerMark(target, "@duanchang");
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), target->objectName());

            QList<const Skill *> skills = target->getVisibleSkillList();
            QStringList detachList;
            foreach (const Skill *skill, skills) {
                if (!skill->isAttachedLordSkill())
                    detachList.append("-" + skill->objectName());
            }
            room->handleAcquireDetachSkills(target, detachList);
        }

        return false;
    }
};

class Tuntian : public TriggerSkill
{
public:
    Tuntian() : TriggerSkill("tuntian")
    {
        events << CardsMoveOneTime << FinishJudge;
        frequency = Frequent;
    }

    int getPriority(TriggerEvent triggerEvent) const
    {
        if (triggerEvent == FinishJudge)
            return 5;
        return TriggerSkill::getPriority(triggerEvent);
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (triggerEvent == CardsMoveOneTime && TriggerSkill::triggerable(player) && player->getPhase() == Player::NotActive) {
            QVariantList move_datas = data.toList();
            foreach(QVariant move_data, move_datas) {
                CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
                if (move.from == player && (move.from_places.contains(Player::PlaceHand) || move.from_places.contains(Player::PlaceEquip))
                    && !(move.to == player && (move.to_place == Player::PlaceHand || move.to_place == Player::PlaceEquip))) {
                    return QStringList(objectName());
                }
            }
        } else if (triggerEvent == FinishJudge) {
            JudgeStruct *judge = data.value<JudgeStruct *>();
            if (judge->reason == objectName() && judge->isGood() && room->getCardPlace(judge->card->getEffectiveId()) == Player::PlaceJudge)
                return QStringList("tuntian!");
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        if (triggerEvent == CardsMoveOneTime) {
            if (player->askForSkillInvoke(objectName(), data)) {
                player->broadcastSkillInvoke(objectName());
                JudgeStruct judge;
                judge.pattern = ".|heart";
                judge.good = false;
                judge.reason = objectName();
                judge.who = player;
                room->judge(judge);
            }
        } else if (triggerEvent == FinishJudge) {
            JudgeStruct *judge = data.value<JudgeStruct *>();
            if (judge->card && room->getCardPlace(judge->card->getEffectiveId()) == Player::PlaceJudge)
                player->addToPile("field", judge->card->getEffectiveId());
        }

        return false;
    }
};

class TuntianDistance : public DistanceSkill
{
public:
    TuntianDistance() : DistanceSkill("#tuntian-dist")
    {
    }

    virtual int getCorrect(const Player *from, const Player *) const
    {
        if (from->hasSkill("tuntian"))
            return -from->getPile("field").length();
        else
            return 0;
    }
};

class Zaoxian : public PhaseChangeSkill
{
public:
    Zaoxian() : PhaseChangeSkill("zaoxian")
    {
        frequency = Wake;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target)
            && target->getPhase() == Player::Start
            && target->getMark("zaoxian") == 0
            && target->getPile("field").length() >= 3;
    }

    virtual bool onPhaseChange(ServerPlayer *dengai) const
    {
        Room *room = dengai->getRoom();
        room->sendCompulsoryTriggerLog(dengai, objectName());

        dengai->broadcastSkillInvoke(objectName());
        //room->doLightbox("$ZaoxianAnimate", 4000);

        room->setPlayerMark(dengai, "zaoxian", 1);
        if (room->changeMaxHpForAwakenSkill(dengai) && dengai->getMark("zaoxian") == 1)
            room->acquireSkill(dengai, "jixi");

        return false;
    }
};

class Jixi : public OneCardViewAsSkill
{
public:
    Jixi() : OneCardViewAsSkill("jixi")
    {
        filter_pattern = ".|.|.|field";
        expand_pile = "field";
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->getPile("field").isEmpty();
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        Snatch *snatch = new Snatch(originalCard->getSuit(), originalCard->getNumber());
        snatch->addSubcard(originalCard);
        snatch->setSkillName(objectName());
        return snatch;
    }
};

class Jiang : public TriggerSkill
{
public:
    Jiang() : TriggerSkill("jiang")
    {
        events << TargetSpecified << TargetConfirmed;
        frequency = Frequent;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card->isKindOf("Duel") || (use.card->isKindOf("Slash") && use.card->isRed())) {
            if (triggerEvent == TargetSpecified && use.index > 0)  return QStringList();
            return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        if (player->askForSkillInvoke(this)) {
            player->broadcastSkillInvoke(objectName());
            player->drawCards(1, objectName());
        }
        return false;
    }
};

class Hunzi : public PhaseChangeSkill
{
public:
    Hunzi() : PhaseChangeSkill("hunzi")
    {
        frequency = Wake;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target)
            && target->getMark("hunzi") == 0
            && target->getPhase() == Player::Start
            && target->getHp() == 1;
    }

    virtual bool onPhaseChange(ServerPlayer *sunce) const
    {
        Room *room = sunce->getRoom();
        room->sendCompulsoryTriggerLog(sunce, objectName());

        sunce->broadcastSkillInvoke(objectName());
        //room->doLightbox("$HunziAnimate", 5000);

        room->setPlayerMark(sunce, "hunzi", 1);
        if (room->changeMaxHpForAwakenSkill(sunce) && sunce->getMark("hunzi") == 1)
            room->handleAcquireDetachSkills(sunce, "yingzi|yinghun");
        return false;
    }
};

ZhibaCard::ZhibaCard()
{
    mute = true;
    m_skillName = "zhiba_pindian";
}

bool ZhibaCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select->hasLordSkill("zhiba") && Self->canPindian(to_select) && !to_select->hasFlag("ZhibaInvoked");
}

void ZhibaCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    ServerPlayer *sunce = targets.first();
    LogMessage log;
    log.type = "#InvokeOthersSkill";
    log.from = source;
    log.to << sunce;
    log.arg = "zhiba";
    room->sendLog(log);
    room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, source->objectName(), sunce->objectName());
    room->setPlayerFlag(sunce, "ZhibaInvoked");
    room->notifySkillInvoked(sunce, "zhiba");
    sunce->broadcastSkillInvoke("zhiba");
    if (sunce->getMark("hunzi") > 0 && room->askForChoice(sunce, "zhiba_pindian", "yes+no", QVariant(), "@zhiba-pindianstart" + source->objectName()) == "no") {
        LogMessage log;
        log.type = "#ZhibaReject";
        log.from = sunce;
        log.to << source;
        log.arg = "zhiba";
        room->sendLog(log);

        return;
    }

    source->pindian(sunce, "zhiba");
}

class ZhibaPindian : public ZeroCardViewAsSkill
{
public:
    ZhibaPindian() : ZeroCardViewAsSkill("zhiba_pindian")
    {
        attached_lord_skill = true;
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        if (!shouldBeVisible(player) || player->isKongcheng()) return false;
        foreach(const Player *sib, player->getAliveSiblings())
            if (sib->hasLordSkill("zhiba") && !sib->hasFlag("ZhibaInvoked") && !sib->isKongcheng())
                return true;
        return false;
    }

    virtual bool shouldBeVisible(const Player *Self) const
    {
        return Self && Self->getKingdom() == "wu";
    }

    virtual const Card *viewAs() const
    {
        return new ZhibaCard;
    }
};

class Zhiba : public TriggerSkill
{
public:
    Zhiba() : TriggerSkill("zhiba$")
    {
        events << GameStart << EventAcquireSkill << EventLoseSkill << Pindian << EventPhaseChanging;
		view_as_skill = new dummyVS;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if ((triggerEvent == GameStart && player->isLord())
            || (triggerEvent == EventAcquireSkill && data.toString() == "zhiba")) {
            QList<ServerPlayer *> lords;
            foreach (ServerPlayer *p, room->getAlivePlayers()) {
                if (p->hasLordSkill(this))
                    lords << p;
            }
            if (lords.isEmpty()) return false;

            QList<ServerPlayer *> players;
            if (lords.length() > 1)
                players = room->getAlivePlayers();
            else
                players = room->getOtherPlayers(lords.first());
            foreach (ServerPlayer *p, players) {
                if (!p->hasSkill("zhiba_pindian"))
                    room->attachSkillToPlayer(p, "zhiba_pindian");
            }
        } else if (triggerEvent == EventLoseSkill && data.toString() == "zhiba") {
            QList<ServerPlayer *> lords;
            foreach (ServerPlayer *p, room->getAlivePlayers()) {
                if (p->hasLordSkill(this))
                    lords << p;
            }
            if (lords.length() > 2) return false;

            QList<ServerPlayer *> players;
            if (lords.isEmpty())
                players = room->getAlivePlayers();
            else
                players << lords.first();
            foreach (ServerPlayer *p, players) {
                if (p->hasSkill("zhiba_pindian"))
                    room->detachSkillFromPlayer(p, "zhiba_pindian", true);
            }
        } else if (triggerEvent == Pindian) {
            PindianStruct *pindian = data.value<PindianStruct *>();
            if (pindian->reason != "zhiba" || !pindian->to->hasLordSkill(this))
                return false;
            if (!pindian->isSuccess()) {
				DummyCard *dummy = new DummyCard;
                if (room->getCardPlace(pindian->from_card->getEffectiveId()) == Player::PlaceTable)
                    dummy->addSubcard(pindian->from_card);
                if (room->getCardPlace(pindian->to_card->getEffectiveId()) == Player::PlaceTable)
                    dummy->addSubcard(pindian->to_card);
                if (dummy->subcardsLength() > 0 && room->askForChoice(pindian->to, "zhiba", "yes+no", data, "@zhiba-pindianfinish") == "yes")
                    pindian->to->obtainCard(dummy);
                delete dummy;
            } 
                
        } else if (triggerEvent == EventPhaseChanging) {
            PhaseChangeStruct phase_change = data.value<PhaseChangeStruct>();
            if (phase_change.from != Player::Play)
                return false;
            QList<ServerPlayer *> players = room->getOtherPlayers(player);
            foreach (ServerPlayer *p, players) {
                if (p->hasFlag("ZhibaInvoked"))
                    room->setPlayerFlag(p, "-ZhibaInvoked");
            }
        }

        return false;
    }
};

TiaoxinCard::TiaoxinCard()
{
}

bool TiaoxinCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select->inMyAttackRange(Self);
}

void TiaoxinCard::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.from->getRoom();
	bool use_slash = false;
    if (effect.to->canSlash(effect.from, NULL, false))
        use_slash = room->askForUseSlashTo(effect.to, effect.from, "@tiaoxin-slash:" + effect.from->objectName());
    if (!use_slash && effect.from->canDiscard(effect.to, "he"))
        room->throwCard(room->askForCardChosen(effect.from, effect.to, "he", "tiaoxin", false, Card::MethodDiscard), effect.to, effect.from);
}

class Tiaoxin : public ZeroCardViewAsSkill
{
public:
    Tiaoxin() : ZeroCardViewAsSkill("tiaoxin")
    {
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("TiaoxinCard");
    }

    virtual const Card *viewAs() const
    {
        return new TiaoxinCard;
    }
};

class Zhiji : public PhaseChangeSkill
{
public:
    Zhiji() : PhaseChangeSkill("zhiji")
    {
        frequency = Wake;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target)
            && target->getMark("zhiji") == 0
            && target->getPhase() == Player::Start
            && target->isKongcheng();
    }

    virtual bool onPhaseChange(ServerPlayer *jiangwei) const
    {
        Room *room = jiangwei->getRoom();
        room->sendCompulsoryTriggerLog(jiangwei, objectName());
        jiangwei->broadcastSkillInvoke(objectName());
        //room->doLightbox("$ZhijiAnimate", 4000);

        room->setPlayerMark(jiangwei, "zhiji", 1);
		if (jiangwei->isWounded() && room->askForChoice(jiangwei, objectName(), "recover+draw") == "recover")
            room->recover(jiangwei, RecoverStruct(jiangwei));
        else
            room->drawCards(jiangwei, 2, objectName());
		
        if (room->changeMaxHpForAwakenSkill(jiangwei)) {
            if (jiangwei->getMark("zhiji") == 1)
                room->acquireSkill(jiangwei, "guanxing");
        }

        return false;
    }
};

ZhijianCard::ZhijianCard()
{
    will_throw = false;
    handling_method = Card::MethodNone;
}

bool ZhijianCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select != Self && to_select->canSetEquip(Sanguosha->getCard(getEffectiveId()));
}

void ZhijianCard::extraCost(Room *room, const CardUseStruct &card_use) const
{
	ServerPlayer *erzhang = card_use.from;
    LogMessage log;
    log.type = "$PutEquip";
    log.from = erzhang;
	log.to = card_use.to;
    log.card_str = QString::number(getEffectiveId());
    room->sendLog(log);
	
    room->moveCardTo(this, erzhang, card_use.to.first(), Player::PlaceEquip,
        CardMoveReason(CardMoveReason::S_REASON_PUT, erzhang->objectName(), "zhijian", QString()));
}

void ZhijianCard::onEffect(const CardEffectStruct &effect) const
{
    effect.from->drawCards(1, "zhijian");
}

class Zhijian : public OneCardViewAsSkill
{
public:
    Zhijian() :OneCardViewAsSkill("zhijian")
    {
        filter_pattern = "EquipCard|.|.|hand";
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        ZhijianCard *zhijian_card = new ZhijianCard();
        zhijian_card->addSubcard(originalCard);
        return zhijian_card;
    }
};

class Guzheng : public TriggerSkill
{
public:
    Guzheng() : TriggerSkill("guzheng")
    {
        events << CardsMoveOneTime << EventPhaseEnd << EventPhaseChanging;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return false;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == CardsMoveOneTime && TriggerSkill::triggerable(player)) {
//            ServerPlayer *current = room->getCurrent();
//            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();

//            if (!current || player == current || current->getPhase() != Player::Discard)
//                return false;

//            QVariantList guzhengToGet = player->tag["GuzhengToGet"].toList();
//            QVariantList guzhengOther = player->tag["GuzhengOther"].toList();

//            if ((move.reason.m_reason & CardMoveReason::S_MASK_BASIC_REASON) == CardMoveReason::S_REASON_DISCARD) {
//                int i = 0;
//                foreach (int card_id, move.card_ids) {
//                    if (move.from == current && move.from_places[i] == Player::PlaceHand)
//                        guzhengToGet << card_id;
//                    else if (!guzhengToGet.contains(card_id))
//                        guzhengOther << card_id;
//                    i++;
//                }
//            }

//            player->tag["GuzhengToGet"] = guzhengToGet;
//            player->tag["GuzhengOther"] = guzhengOther;
        } else if (triggerEvent == EventPhaseEnd && player->getPhase() == Player::Discard) {
            ServerPlayer *erzhang = room->findPlayerBySkillName(objectName());
            if (erzhang == NULL)
                return false;

            QVariantList guzheng_cardsToGet = erzhang->tag["GuzhengToGet"].toList();
            QVariantList guzheng_cardsOther = erzhang->tag["GuzhengOther"].toList();
            erzhang->tag.remove("GuzhengToGet");
            erzhang->tag.remove("GuzhengOther");

            if (player->isDead())
                return false;

            QList<int> cardsToGet;
            foreach (QVariant card_data, guzheng_cardsToGet) {
                int card_id = card_data.toInt();
                if (room->getCardPlace(card_id) == Player::DiscardPile)
                    cardsToGet << card_id;
            }
            QList<int> cardsOther;
            foreach (QVariant card_data, guzheng_cardsOther) {
                int card_id = card_data.toInt();
                if (room->getCardPlace(card_id) == Player::DiscardPile)
                    cardsOther << card_id;
            }


            if (cardsToGet.isEmpty())
                return false;

            QList<int> cards = cardsToGet + cardsOther;

            room->setPlayerMark(erzhang, "guzheng_length", cards.length());
			if (room->askForSkillInvoke(erzhang, objectName(), QVariant::fromValue(player))){
                erzhang->broadcastSkillInvoke(objectName());
                room->fillAG(cards, erzhang, cardsOther);
                int to_back = room->askForAG(erzhang, cardsToGet, false, objectName());
                room->clearAG(erzhang);
                player->obtainCard(Sanguosha->getCard(to_back));
                cards.removeOne(to_back);
				if (!cards.isEmpty() && room->askForSkillInvoke(erzhang, "guzheng_obtain", "prompt")){
                    DummyCard *dummy = new DummyCard(cards);
                    room->obtainCard(erzhang, dummy);
					
				    delete dummy;
			    }
            }
        } else if (triggerEvent == EventPhaseChanging) {
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (change.to == Player::Discard) {
                ServerPlayer *erzhang = room->findPlayerBySkillName(objectName());
                if (erzhang == NULL)
                    return false;
                erzhang->tag.remove("GuzhengToGet");
                erzhang->tag.remove("GuzhengOther");
            }
        }

        return false;
    }
};

class Xiangle : public TriggerSkill
{
public:
    Xiangle() : TriggerSkill("xiangle")
    {
        events << TargetConfirmed;
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card->isKindOf("Slash") && use.from && use.from->isAlive()) {
            return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *liushan, QVariant &data, ServerPlayer *) const
    {
        room->sendCompulsoryTriggerLog(liushan, objectName());
        liushan->broadcastSkillInvoke(objectName());
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.from && use.from->isAlive()) {
            QVariant dataforai = QVariant::fromValue(liushan);
            if (!room->askForCard(use.from, ".Basic", "@xiangle-discard", dataforai)) {
                int index = use.index;
                QVariantList nullified_list = use.card->tag["Nullified_List"].toList();
                nullified_list[index] = true;
                use.card->setTag("Nullified_List", nullified_list);
            }
        }
        return false;
    }
};

FangquanAskCard::FangquanAskCard()
{
	
}

bool FangquanAskCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select != Self;
}

void FangquanAskCard::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.from->getRoom();
    ServerPlayer *liushan = effect.from, *player = effect.to;

    LogMessage log;
    log.type = "#Fangquan";
    log.from = liushan;
    log.to << player;
    room->sendLog(log);

    player->gainAnExtraTurn();
}

class FangquanAsk : public OneCardViewAsSkill
{
public:
    FangquanAsk() : OneCardViewAsSkill("fangquanask")
    {
        filter_pattern = ".|.|.|hand!";
        response_pattern = "@@fangquanask";
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        FangquanAskCard *fangquan = new FangquanAskCard;
        fangquan->addSubcard(originalCard);
        return fangquan;
    }
};

class Fangquan : public TriggerSkill
{
public:
    Fangquan() : TriggerSkill("fangquan")
    {
        events << EventPhaseChanging;
	}

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        PhaseChangeStruct change = data.value<PhaseChangeStruct>();
        if (change.to == Player::Play && TriggerSkill::triggerable(player) && !player->isSkipped(Player::Play)) {
            return QStringList(objectName());
        } else if (change.to == Player::NotActive && player->hasFlag("fangquanInvoked") && !player->isKongcheng()) {
            return QStringList("fangquan!");
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *liushan, QVariant &data, ServerPlayer *) const
    {
        PhaseChangeStruct change = data.value<PhaseChangeStruct>();
        if (change.to == Player::Play && liushan->askForSkillInvoke(this)) {
            liushan->broadcastSkillInvoke(objectName(), 1);
            liushan->skip(Player::Play, true);
            liushan->setFlags("fangquanInvoked");
        } else if (change.to == Player::NotActive) {
            room->sendCompulsoryTriggerLog(liushan, objectName());
            liushan->broadcastSkillInvoke(objectName(), 2);
            room->askForUseCard(liushan, "@@fangquanask", "@fangquan-give", data, Card::MethodDiscard);
        }
        return false;
    }
};

class Ruoyu : public PhaseChangeSkill
{
public:
    Ruoyu() : PhaseChangeSkill("ruoyu$")
    {
        frequency = Wake;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        if (target != NULL && target->isAlive() && target->hasLordSkill("ruoyu") && target->getMark("ruoyu") == 0 && target->getPhase() == Player::Start) {
            QList<ServerPlayer *> players = target->getRoom()->getAlivePlayers();
            foreach (ServerPlayer *p, players) {
                if (target->getHp() > p->getHp()) return false;
            }
            return true;
        }
        return false;
    }

    virtual bool onPhaseChange(ServerPlayer *liushan) const
    {
        Room *room = liushan->getRoom();

        room->sendCompulsoryTriggerLog(liushan, objectName());

        liushan->broadcastSkillInvoke(objectName());

        room->setPlayerMark(liushan, "ruoyu", 1);
        if (room->changeMaxHpForAwakenSkill(liushan, 1)) {
            room->recover(liushan, RecoverStruct(liushan));
            room->acquireSkill(liushan, "jijiang");
        }

        return false;
    }
};

class Huashen : public TriggerSkill
{
public:
    Huashen() : TriggerSkill("huashen")
    {
        events << TurnStart << EventPhaseStart << EventPhaseChanging;
    }

    static void AcquireGenerals(ServerPlayer *zuoci, int n)
    {
        Room *room = zuoci->getRoom();
        QVariantList huashens = zuoci->tag["Huashens"].toList();
        QStringList list = GetAvailableGenerals(zuoci);
        qShuffle(list);
        if (list.isEmpty()) return;
        n = qMin(n, list.length());

        QStringList acquired = list.mid(0, n);
        foreach (QString name, acquired) {
            huashens << name;
            const General *general = Sanguosha->getGeneral(name);
            if (general) {
                foreach (const TriggerSkill *skill, general->getTriggerSkills()) {
                    if (skill->isVisible())
                        room->getThread()->addTriggerSkill(skill);
                }
            }
        }
        zuoci->tag["Huashens"] = huashens;

        QStringList hidden;
        for (int i = 0; i < n; i++) hidden << "unknown";
        foreach (ServerPlayer *p, room->getAllPlayers()) {
            if (p == zuoci)
                room->doAnimate(QSanProtocol::S_ANIMATE_HUASHEN, zuoci->objectName(), acquired.join(":"), QList<ServerPlayer *>() << p);
            else
                room->doAnimate(QSanProtocol::S_ANIMATE_HUASHEN, zuoci->objectName(), hidden.join(":"), QList<ServerPlayer *>() << p);
        }

        LogMessage log;
        log.type = "#GetHuashen";
        log.from = zuoci;
        log.arg = QString::number(n);
        room->sendLog(log, room->getOtherPlayers(zuoci));

        LogMessage log2;
        log2.type = "#GetHuashenDetail";
        log2.from = zuoci;
        log2.arg = acquired.join("\\, \\");
        room->sendLog(log2, zuoci);

        room->setPlayerMark(zuoci, "#huashen", huashens.length());
    }

    static QStringList GetAvailableGenerals(ServerPlayer *zuoci)
    {
        QSet<QString> all = Sanguosha->getLimitedGeneralNames().toSet();
        Room *room = zuoci->getRoom();
        if (isNormalGameMode(room->getMode())
            || room->getMode().contains("_mini_")
            || room->getMode() == "custom_scenario")
            all.subtract(Config.value("Banlist/Roles", "").toStringList().toSet());
        else if (room->getMode() == "06_XMode") {
            foreach(ServerPlayer *p, room->getAlivePlayers())
                all.subtract(p->tag["XModeBackup"].toStringList().toSet());
        } else if (room->getMode() == "02_1v1") {
            all.subtract(Config.value("Banlist/1v1", "").toStringList().toSet());
            foreach(ServerPlayer *p, room->getAlivePlayers())
                all.subtract(p->tag["1v1Arrange"].toStringList().toSet());
        }
        QSet<QString> huashen_set, room_set;

        foreach (ServerPlayer *player, room->getAlivePlayers()) {
            QVariantList huashens = player->tag["Huashens"].toList();
            foreach(QVariant huashen, huashens)
                huashen_set << huashen.toString();
            QString name = player->getGeneralName();
            if (Sanguosha->isGeneralHidden(name)) {
                QString fname = Sanguosha->getMainGeneral(name);
                if (!fname.isEmpty()) name = fname;
            }
            room_set << name;

            if (!player->getGeneral2()) continue;

            name = player->getGeneral2Name();
            if (Sanguosha->isGeneralHidden(name)) {
                QString fname = Sanguosha->getMainGeneral(name);
                if (!fname.isEmpty()) name = fname;
            }
            room_set << name;
        }

        static QSet<QString> banned;
        if (banned.isEmpty())
            banned << "zuoci" << "zhoutai" << "yuji";

        return (all - banned - huashen_set - room_set).toList();
    }

    static void SelectSkill(ServerPlayer *zuoci)
    {
        Room *room = zuoci->getRoom();
        QStringList ac_dt_list;

        QString huashen_skill = zuoci->tag["HuashenSkill"].toString();
        if (!huashen_skill.isEmpty())
            ac_dt_list.append("-" + huashen_skill);

        QVariantList huashens = zuoci->tag["Huashens"].toList();
        if (huashens.isEmpty()) return;

        QStringList huashen_generals;
        foreach(QVariant huashen, huashens)
            huashen_generals << huashen.toString();

        QStringList skill_names;
        QString skill_name;
        const General *general = NULL;
        AI* ai = zuoci->getAI();
        if (ai) {
            QHash<QString, const General *> hash;
            foreach (QString general_name, huashen_generals) {
                const General *general = Sanguosha->getGeneral(general_name);
                foreach (const Skill *skill, general->getVisibleSkillList()) {
                    if (skill->isLordSkill()
                        || skill->getFrequency() == Skill::Limited
                        || skill->getFrequency() == Skill::Wake)
                        continue;

                    if (!skill_names.contains(skill->objectName())) {
                        hash[skill->objectName()] = general;
                        skill_names << skill->objectName();
                    }
                }
            }
            if (skill_names.isEmpty()) return;
            skill_name = ai->askForChoice("huashen", skill_names.join("+"), QVariant());
            general = hash[skill_name];
            Q_ASSERT(general != NULL);
        } else {
            QString general_name = room->askForGeneral(zuoci, huashen_generals, true, "huashen");
            general = Sanguosha->getGeneral(general_name);

            foreach (const Skill *skill, general->getVisibleSkillList()) {
                if (skill->isLordSkill()
                    || skill->getFrequency() == Skill::Limited
                    || skill->getFrequency() == Skill::Wake)
                    continue;

                skill_names << skill->objectName();
            }

            if (!skill_names.isEmpty())
                skill_name = room->askForChoice(zuoci, "huashen", skill_names.join("+"));
        }
        //Q_ASSERT(!skill_name.isNull() && !skill_name.isEmpty());

        QString kingdom = general->getKingdom();
        if (zuoci->getKingdom() != kingdom) {
            if (kingdom == "god")
                kingdom = room->askForKingdom(zuoci);
            room->setPlayerProperty(zuoci, "kingdom", kingdom);
        }

        if (zuoci->getGender() != general->getGender())
            zuoci->setGender(general->getGender());

        JsonArray arg;
        arg << QSanProtocol::S_GAME_EVENT_HUASHEN << zuoci->objectName() << general->objectName() << skill_name;
        room->doBroadcastNotify(QSanProtocol::S_COMMAND_LOG_EVENT, arg);

        zuoci->tag["HuashenSkill"] = skill_name;
		zuoci->tag["HuashenGeneral"] = general->objectName();
        if (!skill_name.isEmpty())
            ac_dt_list.append(skill_name);
        room->handleAcquireDetachSkills(zuoci, ac_dt_list, true);
    }

    virtual TriggerList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        TriggerList skill_list;
        if (triggerEvent == TurnStart && room->getTag("FirstRound").toBool()) {
            QList<ServerPlayer *> zuocis = room->findPlayersBySkillName(objectName());
            foreach (ServerPlayer *zuoci, zuocis)
                skill_list.insert(zuoci, QStringList("huashen!"));

        } else if (triggerEvent == EventPhaseStart && TriggerSkill::triggerable(player) && player->getPhase() == Player::RoundStart)
            skill_list.insert(player, QStringList(objectName()));
        else if (triggerEvent == EventPhaseChanging && TriggerSkill::triggerable(player) && data.value<PhaseChangeStruct>().to == Player::NotActive)
            skill_list.insert(player, QStringList(objectName()));

        return skill_list;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *, QVariant &, ServerPlayer *zuoci) const
    {
        if (triggerEvent == TurnStart) {
            room->sendCompulsoryTriggerLog(zuoci, objectName());
            zuoci->broadcastSkillInvoke(objectName());
            AcquireGenerals(zuoci, 2);
            SelectSkill(zuoci);
        } else if (zuoci->askForSkillInvoke(objectName())) {
            zuoci->broadcastSkillInvoke(objectName());
            SelectSkill(zuoci);
        }
        return false;
    }
};

class HuashenClear : public DetachEffectSkill
{
public:
    HuashenClear() : DetachEffectSkill("huashen")
    {
    }

    virtual void onSkillDetached(Room *room, ServerPlayer *player) const
    {
        if (player->getKingdom() != player->getGeneral()->getKingdom())
            room->setPlayerProperty(player, "kingdom", player->getGeneral()->getKingdom());
        if (player->getGender() != player->getGeneral()->getGender())
            player->setGender(player->getGeneral()->getGender());
        QString huashen_skill = player->tag["HuashenSkill"].toString();
        if (!huashen_skill.isEmpty())
            room->detachSkillFromPlayer(player, huashen_skill, false, true);
        player->tag.remove("HuashenSkill");
        player->tag.remove("Huashens");
        room->setPlayerMark(player, "#huashen", 0);
    }
};

class Xinsheng : public MasochismSkill
{
public:
    Xinsheng() : MasochismSkill("xinsheng")
    {
        frequency = Frequent;
    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (TriggerSkill::triggerable(player)) {
            DamageStruct damage = data.value<DamageStruct>();
            QStringList trigger_list;
            for (int i = 1; i <= damage.damage; i++) {
                trigger_list << objectName();
            }

            return trigger_list;
        }

        return QStringList();
    }

    virtual void onDamaged(ServerPlayer *zuoci, const DamageStruct &) const
    {
        if (zuoci->askForSkillInvoke(this)) {
            zuoci->broadcastSkillInvoke(objectName());
            Huashen::AcquireGenerals(zuoci, 1);
        }
    }
};

MountainPackage::MountainPackage()
    : Package("mountain")
{
    General *zhanghe = new General(this, "zhanghe", "wei"); // WEI 009
    zhanghe->addSkill(new Qiaobian);

    General *dengai = new General(this, "dengai", "wei", 4); // WEI 015
    dengai->addSkill(new Tuntian);
    dengai->addSkill(new TuntianDistance);
	dengai->addSkill(new DetachEffectSkill("tuntian", "field"));
    related_skills.insertMulti("tuntian", "#tuntian-dist");
	related_skills.insertMulti("tuntian", "#tuntian-clear");
    dengai->addSkill(new Zaoxian);
    dengai->addRelateSkill("jixi");

    General *jiangwei = new General(this, "jiangwei", "shu"); // SHU 012
    jiangwei->addSkill(new Tiaoxin);
    jiangwei->addSkill(new Zhiji);
	jiangwei->addRelateSkill("guanxing");

    General *liushan = new General(this, "liushan$", "shu", 3); // SHU 013
    liushan->addSkill(new Xiangle);
    liushan->addSkill(new Fangquan);
    liushan->addSkill(new Ruoyu);
    liushan->addRelateSkill("jijiang");

    General *sunce = new General(this, "sunce$", "wu"); // WU 010
    sunce->addSkill(new Jiang);
    sunce->addSkill(new Hunzi);
    sunce->addRelateSkill("yingzi");
	sunce->addRelateSkill("yinghun");
    sunce->addSkill(new Zhiba);

    General *erzhang = new General(this, "erzhang", "wu", 3); // WU 015
    erzhang->addSkill(new Zhijian);
    erzhang->addSkill(new Guzheng);

    General *zuoci = new General(this, "zuoci", "qun", 3); // QUN 009
    zuoci->addSkill(new Huashen);
    zuoci->addSkill(new HuashenClear);
    zuoci->addSkill(new Xinsheng);
    related_skills.insertMulti("huashen", "#huashen-clear");

    General *caiwenji = new General(this, "caiwenji", "qun", 3, false); // QUN 012
    caiwenji->addSkill(new Beige);
    caiwenji->addSkill(new Duanchang);

    addMetaObject<QiaobianMoveCard>();
    addMetaObject<TiaoxinCard>();
    addMetaObject<ZhijianCard>();
    addMetaObject<ZhibaCard>();
    addMetaObject<FangquanAskCard>();

    skills << new ZhibaPindian << new Jixi << new FangquanAsk << new QiaobianMove;
}

ADD_PACKAGE(Mountain)

