#include "yjcm.h"
#include "skill.h"
#include "standard.h"
#include "maneuvering.h"
#include "clientplayer.h"
#include "engine.h"
#include "settings.h"
#include "ai.h"
#include "general.h"

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

class Luoying : public TriggerSkill
{
public:
    Luoying() : TriggerSkill("luoying")
    {
        events << CardsMoveOneTime;
        frequency = Frequent;
    }

    virtual QStringList triggerable(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        QVariantList move_datas = data.toList();
        foreach(QVariant move_data, move_datas) {
            CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
            if (move.from != NULL && move.from != player && move.to_place == Player::DiscardPile
                && ((move.reason.m_reason & CardMoveReason::S_MASK_BASIC_REASON) == CardMoveReason::S_REASON_DISCARD
                || move.reason.m_reason == CardMoveReason::S_REASON_JUDGEDONE)) {

                int i = 0;
                foreach (int card_id, move.card_ids) {
                    if (Sanguosha->getCard(card_id)->getSuit() == Card::Club && room->getCardPlace(card_id) == Player::DiscardPile) {
                        if (move.reason.m_reason == CardMoveReason::S_REASON_JUDGEDONE
                                || (move.from_places[i] == Player::PlaceHand || move.from_places[i] == Player::PlaceEquip))
                        return QStringList(objectName());
                    }
                    i++;
                }


            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        if (!player->askForSkillInvoke(this, data)) return false;
        player->broadcastSkillInvoke(objectName());

        QList<int> card_ids;
        QVariantList move_datas = data.toList();
        foreach(QVariant move_data, move_datas) {
            CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
            if (move.from != NULL && move.from != player && move.to_place == Player::DiscardPile
                && ((move.reason.m_reason & CardMoveReason::S_MASK_BASIC_REASON) == CardMoveReason::S_REASON_DISCARD
                || move.reason.m_reason == CardMoveReason::S_REASON_JUDGEDONE)) {

                int i = 0;
                foreach (int card_id, move.card_ids) {
                    if (Sanguosha->getCard(card_id)->getSuit() == Card::Club && room->getCardPlace(card_id) == Player::DiscardPile) {
                        if (move.reason.m_reason == CardMoveReason::S_REASON_JUDGEDONE
                                || (move.from_places[i] == Player::PlaceHand || move.from_places[i] == Player::PlaceEquip))
                        card_ids << card_id;
                    }
                    i++;
                }
            }
        }

        if (!card_ids.isEmpty()) {
            DummyCard *dummy = new DummyCard(card_ids);
            room->obtainCard(player, dummy);
            delete dummy;
        }
        return false;
    }
};

JiushiCard::JiushiCard()
{
    target_fixed = true;
}

const Card *JiushiCard::validate(CardUseStruct &cardUse) const
{
    ServerPlayer *source = cardUse.from;
    Room *room = source->getRoom();

    source->broadcastSkillInvoke("jiushi");

    room->notifySkillInvoked(source, "jiushi");

    LogMessage log;
    log.from = source;
    log.type = "#UseCard";
    log.card_str = toString();
    room->sendLog(log);

    source->turnOver();

    Analeptic *analeptic = new Analeptic(Card::NoSuit, 0);
    analeptic->setSkillName("_jiushi");

    if (hasFlag("UsedBySecondWay"))
        room->setCardFlag(analeptic, "UsedBySecondWay");

    //room->useCard(CardUseStruct(analeptic, source, source));
    return analeptic;
}

class JiushiViewAsSkill : public ZeroCardViewAsSkill
{
public:
    JiushiViewAsSkill() : ZeroCardViewAsSkill("jiushi")
    {
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return Analeptic::IsAvailable(player) && player->faceUp();
    }

    virtual bool isEnabledAtResponse(const Player *player, const QString &pattern) const
    {
        return pattern.contains("analeptic") && player->faceUp();
    }

    virtual const Card *viewAs() const
    {
        return new JiushiCard;
    }

};

class Jiushi : public TriggerSkill
{
public:
    Jiushi() : TriggerSkill("jiushi")
    {
        events << Damaged;
        view_as_skill = new JiushiViewAsSkill;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (TriggerSkill::triggerable(player)) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.flags.contains("jiushi") && !player->faceUp()) {
                return QStringList(objectName());
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        if (player->askForSkillInvoke(this)) {
            player->broadcastSkillInvoke(objectName());
            player->turnOver();
        }
        return false;
    }

    virtual int getEffectIndex(const ServerPlayer *, const Card *card) const
    {
        return card->isKindOf("Analeptic") ? 0 : -1;
    }
};

class Enyuan : public TriggerSkill
{
public:
    Enyuan() : TriggerSkill("enyuan")
    {
        events << CardsMoveOneTime << Damaged;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        if (triggerEvent == CardsMoveOneTime && !room->getTag("FirstRound").toBool()) {
            QVariantList move_datas = data.toList();
            foreach(QVariant move_data, move_datas) {
                CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
                if (move.to == player && move.from && move.from->isAlive() && move.from != move.to && move.to_place == Player::PlaceHand) {
                    int n = 0;
                    for (int i = 0; i < move.card_ids.length(); i++) {
                        if (move.from_places[i] == Player::PlaceHand || move.from_places[i] == Player::PlaceEquip)
                            n++;
                    }
                    if (n > 1)
                        return QStringList(objectName());
                }
            }
        } else if (triggerEvent == Damaged) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.from && damage.from->isAlive() && damage.from != player) {
                QStringList trigger_list;
                for (int i = 1; i <= damage.damage; i++) {
                    trigger_list << objectName();
                }
                return trigger_list;
            }
        }
        return QStringList();
    }


    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        if (triggerEvent == CardsMoveOneTime) {
            QVariantList move_datas = data.toList();
            foreach(QVariant move_data, move_datas) {
                if (!TriggerSkill::triggerable(player)) break;
                CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
                if (move.to == player && move.from && move.from->isAlive() && move.from != move.to && move.to_place == Player::PlaceHand) {
                    int n = 0;
                    for (int i = 0; i < move.card_ids.length(); i++) {
                        if (move.from_places[i] == Player::PlaceHand || move.from_places[i] == Player::PlaceEquip)
                            n++;
                    }
                    if (n > 1) {
                        move.from->setFlags("EnyuanDrawTarget");
                        bool invoke = room->askForSkillInvoke(player, objectName(), QVariant::fromValue(move.from));
                        move.from->setFlags("-EnyuanDrawTarget");
                        if (invoke) {
                            player->broadcastSkillInvoke(objectName(), 1);
                            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), move.from->objectName());
                            room->drawCards((ServerPlayer *)move.from, 1, objectName());
                        }
                    }
                }
            }
        } else if (triggerEvent == Damaged) {
            DamageStruct damage = data.value<DamageStruct>();
            ServerPlayer *source = damage.from;
            if (room->askForSkillInvoke(player, objectName(), QVariant::fromValue(source))) {
                player->broadcastSkillInvoke(objectName(), 2);
                room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), source->objectName());
                const Card *card = NULL;
                if (!source->isKongcheng())
                    card = room->askForExchange(source, objectName(), 1, 1, false, "EnyuanGive::" + player->objectName(), true);
                if (card) {
                    CardMoveReason reason(CardMoveReason::S_REASON_GIVE, source->objectName(),
                        player->objectName(), objectName(), QString());
                    reason.m_playerId = player->objectName();
                    room->moveCardTo(card, source, player, Player::PlaceHand, reason);
                    delete card;
                } else {
                    room->loseHp(source);
                }
            }

        }
        return false;
    }
};

class Xuanhuo : public PhaseChangeSkill
{
public:
    Xuanhuo() : PhaseChangeSkill("xuanhuo")
    {
		view_as_skill = new dummyVS;
    }

    virtual bool onPhaseChange(ServerPlayer *fazheng) const
    {
        Room *room = fazheng->getRoom();
        if (fazheng->getPhase() == Player::Draw) {
            ServerPlayer *to = room->askForPlayerChosen(fazheng, room->getOtherPlayers(fazheng), objectName(), "xuanhuo-invoke", true, true);
            if (to) {
                fazheng->broadcastSkillInvoke(objectName(), 1);
                room->drawCards(to, 2, objectName());
                if (!fazheng->isAlive() || !to->isAlive())
                    return true;

                QList<ServerPlayer *> targets;
                foreach (ServerPlayer *vic, room->getOtherPlayers(to)) {
                    if (to->canSlash(vic))
                        targets << vic;
                }
                ServerPlayer *victim = NULL;
                if (!targets.isEmpty()) {
                    victim = room->askForPlayerChosen(fazheng, targets, "xuanhuo_slash", "@dummy-slash2:" + to->objectName());

                    LogMessage log;
                    log.type = "#CollateralSlash";
                    log.from = fazheng;
                    log.to << victim;
                    room->sendLog(log);
					
					room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, to->objectName(), victim->objectName());
                }

                if (victim == NULL || !room->askForUseSlashTo(to, victim, QString("xuanhuo-slash:%1:%2").arg(fazheng->objectName()).arg(victim->objectName()))) {
                    if (to->isNude())
                        return true;
                    fazheng->broadcastSkillInvoke(objectName(), 2);

                    QList<int> cards = room->askForCardsChosen(fazheng, to, "he", objectName(),2 , 2);

                    DummyCard dummy(cards);
                    room->moveCardTo(&dummy, fazheng, Player::PlaceHand, false);
                }

                return true;
            }
        }

        return false;
    }
};

class Xuanfeng : public TriggerSkill
{
public:
    Xuanfeng() : TriggerSkill("xuanfeng")
    {
        events << EventPhaseEnd << CardsMoveOneTime;
        view_as_skill = new dummyVS;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *lingtong, QVariant &data, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(lingtong)) return QStringList();
        bool can_trigger = false;
        if (triggerEvent == CardsMoveOneTime) {
            QVariantList move_datas = data.toList();
            foreach(QVariant move_data, move_datas) {
                CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
                if (move.from == lingtong && move.from_places.contains(Player::PlaceEquip)) {
                    can_trigger = true;
                    break;
                }
            }
        } else if (triggerEvent == EventPhaseEnd && lingtong->getPhase() == Player::Discard && lingtong->getMark("GlobalRuleDiscardCount") > 1)
            can_trigger = true;

        if (can_trigger) {
            QList<ServerPlayer *> targets = room->getOtherPlayers(lingtong);
            foreach (ServerPlayer *target, targets) {
                if (!target->isNude())
                    return QStringList(objectName());
            }

        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *lingtong, QVariant &, ServerPlayer *) const
    {
        for (int i = 0; i < 2; i++) {
            if (lingtong->isDead()) break;
            bool no_target = true;
            QList<ServerPlayer *> targets, allplayers = room->getOtherPlayers(lingtong);
            foreach (ServerPlayer *p, allplayers) {
                if (!p->isNude()) no_target = false;
                if (lingtong->canDiscard(p, "he"))
                    targets << p;
            }
            if (no_target) break;

            ServerPlayer *target = room->askForPlayerChosen(lingtong, targets, objectName(), "xuanfeng-invoke", true, i==0);

            if (target == NULL) break;

            if (i==0) lingtong->broadcastSkillInvoke(objectName());
            else room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, lingtong->objectName(), target->objectName());

            room->throwCard(room->askForCardChosen(lingtong, target, "he", objectName(), false, Card::MethodDiscard), target, lingtong);

        }

        return false;
    }
};

XianzhenCard::XianzhenCard()
{
}

bool XianzhenCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && Self->canPindian(to_select);
}

void XianzhenCard::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.from->getRoom();
    if (effect.from->pindian(effect.to, "xianzhen", NULL)) {
        QStringList assignee_list = effect.from->property("xianzhen_targets").toString().split("+");
        assignee_list << effect.to->objectName();
        room->setPlayerProperty(effect.from, "xianzhen_targets", assignee_list.join("+"));
        room->addPlayerMark(effect.to, "Armor_Nullified");
    } else {
        room->setPlayerCardLimitation(effect.from, "use", "Slash", true);
    }
}

class XianzhenViewAsSkill : public ZeroCardViewAsSkill
{
public:
    XianzhenViewAsSkill() : ZeroCardViewAsSkill("xianzhen")
    {
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("XianzhenCard") && !player->isKongcheng();
    }

    virtual const Card *viewAs() const
    {
        return new XianzhenCard;
    }
};

class Xianzhen : public TriggerSkill
{
public:
    Xianzhen() : TriggerSkill("xianzhen")
    {
        events << EventPhaseStart;
        view_as_skill = new XianzhenViewAsSkill;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->getPhase() == Player::NotActive;
    }

    virtual bool trigger(TriggerEvent , Room *room, ServerPlayer *gaoshun, QVariant &) const
    {
        QStringList assignee_list = gaoshun->property("xianzhen_targets").toString().split("+");
        room->setPlayerProperty(gaoshun, "xianzhen_targets", QVariant());
        foreach (ServerPlayer *p, room->getAlivePlayers()) {
            if (assignee_list.contains(p->objectName()))
                room->removePlayerMark(p, "Armor_Nullified");
        }
        return false;
    }
};

class XianzhenTargetMod : public TargetModSkill
{
public:
    XianzhenTargetMod() : TargetModSkill("#xianzhen-target")
    {
        pattern = "^SkillCard";
    }

    int getResidueNum(const Player *from, const Card *, const Player *to) const
    {
        QStringList assignee_list = from->property("xianzhen_targets").toString().split("+");
        if (to && assignee_list.contains(to->objectName()))
            return 10000;
        return 0;
    }

    int getDistanceLimit(const Player *from, const Card *, const Player *to) const
    {
        QStringList assignee_list = from->property("xianzhen_targets").toString().split("+");
        if (to && assignee_list.contains(to->objectName()))
            return 10000;
        return 0;
    }

};

class Jinjiu : public FilterSkill
{
public:
    Jinjiu() : FilterSkill("jinjiu")
    {
    }

    virtual bool viewFilter(const Card *to_select) const
    {
        return to_select->objectName() == "analeptic";
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        Slash *slash = new Slash(originalCard->getSuit(), originalCard->getNumber());
        slash->setSkillName(objectName());
        WrappedCard *card = Sanguosha->getWrappedCard(originalCard->getId());
        card->takeOver(slash);
        return card;
    }
};

MingceCard::MingceCard()
{
    will_throw = false;
	will_sort = false;
    handling_method = Card::MethodNone;
}

bool MingceCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return (targets.isEmpty() && to_select != Self) || (targets.length() == 1 && targets.first()->canSlash(to_select, NULL, false) && targets.first()->inMyAttackRange(to_select));
}

bool MingceCard::targetsFeasible(const QList<const Player *> &targets, const Player *) const
{
    if (targets.length() == 1) {
		foreach(const Player *sib, targets.first()->getAliveSiblings())
			if (targets.first()->canSlash(sib, NULL, false) && targets.first()->inMyAttackRange(sib))
				return false;
        return true;
	}
	return targets.length() == 2;
}

void MingceCard::extraCost(Room *room, const CardUseStruct &card_use) const
{
	CardMoveReason reason(CardMoveReason::S_REASON_GIVE, card_use.from->objectName(), card_use.to.first()->objectName(), "mingce", QString());
    room->obtainCard(card_use.to.first(), this, reason);
}

void MingceCard::use(Room *room, ServerPlayer *, QList<ServerPlayer *> &targets) const
{
	QList<ServerPlayer *> copy = targets;
	ServerPlayer *target = copy.takeFirst();
    if (copy.isEmpty())
		target->drawCards(1, "mingce");
	else {
	    ServerPlayer *victim = copy.takeFirst();
		victim->setFlags("MingceTarget"); // For AI
        QString choice = room->askForChoice(target, "mingce", "use+draw", QVariant(), "@mingce-choose::"+victim->objectName());
        if (victim && victim->hasFlag("MingceTarget")) victim->setFlags("-MingceTarget");

        if (choice == "use") {
            if (target->canSlash(victim, NULL, false)) {
                Slash *slash = new Slash(Card::NoSuit, 0);
                slash->setSkillName("_mingce");
                room->useCard(CardUseStruct(slash, target, victim));
            }
        } else if (choice == "draw") {
            target->drawCards(1, "mingce");
        }
	}
}

class Mingce : public OneCardViewAsSkill
{
public:
    Mingce() : OneCardViewAsSkill("mingce")
    {
        filter_pattern = "EquipCard,Slash";
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("MingceCard");
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        MingceCard *mingceCard = new MingceCard;
        mingceCard->addSubcard(originalCard);
        return mingceCard;
    }
};

class Zhichi : public TriggerSkill
{
public:
    Zhichi() : TriggerSkill("zhichi")
    {
        events << Damaged << CardEffected << EventPhaseStart;
        frequency = Compulsory;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (triggerEvent == EventPhaseStart && player->getPhase() == Player::NotActive) {
            QList<ServerPlayer *> players = room->getAlivePlayers();
            foreach (ServerPlayer *p, players) {
                room->setPlayerMark(p, "#zhichi", 0);
            }
        }
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (triggerEvent == Damaged && TriggerSkill::triggerable(player) && player->getPhase() == Player::NotActive) {
            return QStringList(objectName());
        } else if (triggerEvent == CardEffected && player->isAlive() && player->getMark("#zhichi") > 0) {
            CardEffectStruct effect = data.value<CardEffectStruct>();
            if (effect.card->isKindOf("Slash") || effect.card->isNDTrick())
                return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        if (triggerEvent == Damaged) {
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            room->addPlayerTip(player, "#zhichi");
        } else if (triggerEvent == CardEffected) {
            LogMessage log;
            log.type = "#SkillForce";
            log.from = player;
            log.arg = objectName();
            room->sendLog(log);
            CardEffectStruct effect = data.value<CardEffectStruct>();
            effect.nullified = true;
            data = QVariant::fromValue(effect);
        }
        return false;
    }
};

GanluCard::GanluCard()
{
}

bool GanluCard::targetsFeasible(const QList<const Player *> &targets, const Player *) const
{
    return targets.length() == 2;
}

bool GanluCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    switch (targets.length()) {
    case 0: return true;
    case 1: {
        int n1 = targets.first()->getEquips().length();
        int n2 = to_select->getEquips().length();
        return qAbs(n1 - n2) <= Self->getLostHp();
    }
    default:
        return false;
    }
}

void GanluCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    LogMessage log;
    log.type = "#GanluSwap";
    log.from = source;
    log.to = targets;
    room->sendLog(log);

    room->swapCards(targets.at(0), targets.at(1), "e", "ganlu");
}

class Ganlu : public ZeroCardViewAsSkill
{
public:
    Ganlu() : ZeroCardViewAsSkill("ganlu")
    {
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("GanluCard");
    }

    virtual const Card *viewAs() const
    {
        return new GanluCard;
    }
};

class Buyi : public TriggerSkill
{
public:
    Buyi() : TriggerSkill("buyi")
    {
        events << Dying;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (TriggerSkill::triggerable(player)) {
            DyingStruct dying = data.value<DyingStruct>();
            ServerPlayer *target = dying.who;
            if (target && target->isAlive() && !target->isKongcheng() && target->getHp() < 1) {
                return QStringList(objectName());
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *wuguotai, QVariant &data, ServerPlayer *) const
    {
        DyingStruct dying = data.value<DyingStruct>();
        ServerPlayer *player = dying.who;
        if (wuguotai->askForSkillInvoke(this, QVariant::fromValue(player))) {
			room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, wuguotai->objectName(), player->objectName());
            wuguotai->broadcastSkillInvoke(objectName());
            if (player->isKongcheng()) return false;
            const Card *card = Sanguosha->getCard(room->askForCardChosen(wuguotai, player, "h", objectName()));
            room->showCard(player, card->getEffectiveId());

            if (card->getTypeId() != Card::TypeBasic) {
                if (!player->isJilei(card)) {
                    room->throwCard(card, player);
                    room->recover(player, RecoverStruct(wuguotai));
                }
            }
        }
        return false;
    }
};

class Quanji : public MasochismSkill
{
public:
    Quanji() : MasochismSkill("quanji")
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

    virtual void onDamaged(ServerPlayer *zhonghui, const DamageStruct &) const
    {
        Room *room = zhonghui->getRoom();
        if (zhonghui->askForSkillInvoke(objectName())) {
            zhonghui->broadcastSkillInvoke(objectName());
            zhonghui->drawCards(1, objectName());
            if (!zhonghui->isKongcheng()) {
                const Card *card = room->askForExchange(zhonghui, objectName(), 1, 1, false, "QuanjiPush");
                zhonghui->addToPile("power", card);
            }
        }
    }
};

class QuanjiKeep : public MaxCardsSkill
{
public:
    QuanjiKeep() : MaxCardsSkill("#quanji")
    {
        frequency = Frequent;
    }

    virtual int getExtra(const Player *target) const
    {
        if (target->hasSkill("quanji"))
            return target->getPile("power").length();
        else
            return 0;
    }
};

class Zili : public PhaseChangeSkill
{
public:
    Zili() : PhaseChangeSkill("zili")
    {
        frequency = Wake;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target)
            && target->getPhase() == Player::Start
            && target->getMark("zili") == 0
            && target->getPile("power").length() >= 3;
    }

    virtual bool onPhaseChange(ServerPlayer *zhonghui) const
    {
        Room *room = zhonghui->getRoom();
        room->sendCompulsoryTriggerLog(zhonghui, objectName());

        zhonghui->broadcastSkillInvoke(objectName());
        //room->doLightbox("$ZiliAnimate", 4000);

        room->setPlayerMark(zhonghui, "zili", 1);
        if (room->changeMaxHpForAwakenSkill(zhonghui)) {
            if (zhonghui->isWounded() && room->askForChoice(zhonghui, objectName(), "recover+draw") == "recover")
                room->recover(zhonghui, RecoverStruct(zhonghui));
            else
                room->drawCards(zhonghui, 2, objectName());
            if (zhonghui->getMark("zili") == 1)
                room->acquireSkill(zhonghui, "paiyi");
        }

        return false;
    }
};

PaiyiCard::PaiyiCard()
{
    will_throw = true;
    handling_method = Card::MethodNone;
}

bool PaiyiCard::targetFilter(const QList<const Player *> &targets, const Player *, const Player *) const
{
    return targets.isEmpty();
}

void PaiyiCard::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *zhonghui = effect.from;
    ServerPlayer *target = effect.to;
    Room *room = zhonghui->getRoom();

    room->drawCards(target, 2, "paiyi");
    if (target->getHandcardNum() > zhonghui->getHandcardNum())
        room->damage(DamageStruct("paiyi", zhonghui, target));
}

class Paiyi : public OneCardViewAsSkill
{
public:
    Paiyi() : OneCardViewAsSkill("paiyi")
    {
        expand_pile = "power";
        filter_pattern = ".|.|.|power";
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->getPile("power").isEmpty() && !player->hasUsed("PaiyiCard");
    }

    virtual const Card *viewAs(const Card *c) const
    {
        PaiyiCard *py = new PaiyiCard;
        py->addSubcard(c);
        return py;
    }
};

class Jueqing : public TriggerSkill
{
public:
    Jueqing() : TriggerSkill("jueqing")
    {
        frequency = Compulsory;
        events << Predamage;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &ask_who) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (TriggerSkill::triggerable(damage.from) && player->isAlive()){
            ask_who = damage.from;
            return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *zhangchunhua) const
    {
        room->sendCompulsoryTriggerLog(zhangchunhua, objectName());
        zhangchunhua->broadcastSkillInvoke(objectName());
        DamageStruct damage = data.value<DamageStruct>();
        room->preventDamage(damage);
        room->loseHp(player, damage.damage);
        return true;
    }
};

class Shangshi : public TriggerSkill
{
public:
    Shangshi() : TriggerSkill("shangshi")
    {
        events << HpChanged << HpRecover << MaxHpChanged << CardsMoveOneTime;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (TriggerSkill::triggerable(player) && player->getHandcardNum() < player->getLostHp()) {
            if (triggerEvent == CardsMoveOneTime) {
                QVariantList move_datas = data.toList();
                foreach(QVariant move_data, move_datas) {
                    CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
                    if (move.from == player && move.from_places.contains(Player::PlaceHand))
                        return QStringList(objectName());
                    if (move.to == player && move.to_place == Player::PlaceHand)
                        return QStringList(objectName());
                }
            } else if (triggerEvent == HpChanged) {
                if (!data.isNull() && !data.canConvert<RecoverStruct>()) {
                    return QStringList(objectName());
                }
            } else
                return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        if (player->askForSkillInvoke(this)) {
            player->broadcastSkillInvoke(objectName());
            int x = player->getLostHp() - player->getHandcardNum();
            if (x > 0)
                player->drawCards(x, objectName());
        }
        return false;
    }
};

SanyaoCard::SanyaoCard()
{
}

bool SanyaoCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    if (!targets.isEmpty()) return false;
    QList<const Player *> players = Self->getAliveSiblings();
    players << Self;
    int max = -1000;
    foreach (const Player *p, players) {
        if (max < p->getHp()) max = p->getHp();
    }
    return to_select->getHp() == max;
}

void SanyaoCard::onEffect(const CardEffectStruct &effect) const
{
    effect.from->getRoom()->damage(DamageStruct("sanyao", effect.from, effect.to));
}

class Sanyao : public OneCardViewAsSkill
{
public:
    Sanyao() : OneCardViewAsSkill("sanyao")
    {
        filter_pattern = ".!";
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return player->canDiscard(player, "he") && !player->hasUsed("SanyaoCard");
    }

    const Card *viewAs(const Card *originalcard) const
    {
        SanyaoCard *first = new SanyaoCard;
        first->addSubcard(originalcard->getId());
        first->setSkillName(objectName());
        return first;
    }
};

class Zhiman : public TriggerSkill
{
public:
    Zhiman() : TriggerSkill("zhiman")
    {
        events << DamageCaused;
    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        DamageStruct damage = data.value<DamageStruct>();
        if (player != damage.to)
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        ServerPlayer *target = damage.to;
        if (player->askForSkillInvoke(this, QVariant::fromValue(target))) {
            player->broadcastSkillInvoke(objectName());
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), target->objectName());
            room->preventDamage(damage);
            if (player->canGetCard(target, "ej")) {
                int card_id = room->askForCardChosen(player, target, "ej", objectName(), false, Card::MethodGet);
                CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, player->objectName());
                room->obtainCard(player, Sanguosha->getCard(card_id), reason, false);
            }
            return true;
        }
        return false;
    }
};

class ZhenjunDiscard : public ViewAsSkill
{
public:
    ZhenjunDiscard() : ViewAsSkill("zhenjun_discard")
    {
        response_pattern = "@@zhenjun_discard";
    }


    bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
        return selected.length() < Self->getMark("zhenjun_num") && !Self->isJilei(to_select);
    }

    const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.length() == Self->getMark("zhenjun_num")) {
            DummyCard *xt = new DummyCard;
            xt->addSubcards(cards);
            return xt;
        }
        return NULL;
    }
};

class Zhenjun : public PhaseChangeSkill
{
public:
    Zhenjun() : PhaseChangeSkill("zhenjun")
    {
        view_as_skill = new dummyVS;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        if (PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Start) {
            QList<ServerPlayer *> targets = target->getRoom()->getAlivePlayers();
            foreach (ServerPlayer *p, targets) {
                if (p->getHandcardNum() > p->getHp())
                    return true;
            }
        }
        return false;
    }

    virtual bool onPhaseChange(ServerPlayer *player) const
    {
        Room *room = player->getRoom();
        QList<ServerPlayer *> targets;
        foreach (ServerPlayer *p, room->getAlivePlayers()) {
            if (p->getHandcardNum() > p->getHp())
                targets << p;
        }
        ServerPlayer *target = room->askForPlayerChosen(player, targets, objectName(), "zhenjun-invoke", true, true);
        if (target) {
            player->broadcastSkillInvoke(objectName());
            int x = target->getHandcardNum() - target->getHp();
            QList<int> cards = room->askForCardsChosen(player, target, "he", objectName(), x, x, false, Card::MethodDiscard);
            int y = 0;
            foreach (int id, cards) {
                if (Sanguosha->getCard(id)->getTypeId() != Card::TypeEquip)
                    y++;
            }
            DummyCard *dummy = new DummyCard(cards);
            room->throwCard(dummy, target, player);
            delete dummy;
            room->setPlayerMark(player, "zhenjun_num", y);
            const Card *to_discard = room->askForCard(player, "@@zhenjun_discard", "zhenjun-discard::"+target->objectName()+
                    ":"+QString::number(y)+":"+QString::number(x), QVariant(), Card::MethodNone);
            room->setPlayerMark(player, "zhenjun_num", 0);
            if (to_discard) {
                if (to_discard->subcardsLength() > 0) {
                    CardMoveReason mreason(CardMoveReason::S_REASON_THROW, player->objectName(), QString(), objectName(), QString());
                    room->throwCard(to_discard, mreason, player);
                }
            } else
                target->drawCards(x, "zhenjun");
        }
        return false;
    }
};

class Pojun : public TriggerSkill
{
public:
    Pojun() : TriggerSkill("pojun")
    {
        events << TargetSpecified << EventPhaseChanging;
    }

    virtual TriggerList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        TriggerList skill_list;
        if (triggerEvent == TargetSpecified && TriggerSkill::triggerable(player) && player->getPhase() == Player::Play) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card != NULL && use.card->isKindOf("Slash")) {
                ServerPlayer *to = use.to.at(use.index);
                if (to && to->isAlive() && to->getHp() > 0 && !to->isNude())
                    skill_list.insert(player, QStringList(objectName()));
            }
        } else if (triggerEvent == EventPhaseChanging) {
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (change.to != Player::NotActive) return skill_list;
            QList<ServerPlayer *> all = room->getAlivePlayers();

            foreach (ServerPlayer *p, all) {
                if (p->getPile("army").length() > 0) {
                    skill_list.insert(p, QStringList("pojun!"));
                }
            }
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *, QVariant &data, ServerPlayer *player) const
    {
        if (triggerEvent == TargetSpecified) {
            CardUseStruct use = data.value<CardUseStruct>();
            ServerPlayer *to = use.to.at(use.index);
            if (player->askForSkillInvoke(this, QVariant::fromValue(to))) {
                player->broadcastSkillInvoke(objectName());
                room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), to->objectName());

                int n = qMin(to->getCards("he").length(), to->getHp());
                if (n > 0) {
                    QList<int> cards = room->askForCardsChosen(player, to, "he", objectName(), 1, n);
                    to->addToPile("army", cards, false);
                }
            }
        } else if (triggerEvent == EventPhaseChanging) {
            DummyCard *dummy = new DummyCard(player->getPile("army"));
            CardMoveReason reason(CardMoveReason::S_REASON_EXCHANGE_FROM_PILE, player->objectName(), objectName(), QString());
            room->obtainCard(player, dummy, reason, false);
            delete dummy;
        }
        return false;
    }
};

class Zhuhai : public TriggerSkill
{
public:
    Zhuhai() : TriggerSkill("zhuhai")
    {
        events << EventPhaseStart << ChoiceMade;
        view_as_skill = new dummyVS;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        //for notify skill
        if (triggerEvent == ChoiceMade && player->hasFlag("ZhuhaiSlash") && data.canConvert<CardUseStruct>()) {
            player->broadcastSkillInvoke(objectName());
            room->notifySkillInvoked(player, objectName());

            LogMessage log;
            log.type = "#InvokeSkill";
            log.from = player;
            log.arg = objectName();
            room->sendLog(log);

            player->setFlags("-ZhuhaiSlash");
        }
    }

    virtual TriggerList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *current, QVariant &) const
    {
        TriggerList skill_list;
        if (triggerEvent != EventPhaseStart || current->getPhase() != Player::Finish) return skill_list;
        if (current->isDead() || current->getMark("damage_point_round") == 0) return skill_list;
        QList<ServerPlayer *> xushus = room->findPlayersBySkillName(objectName());
        foreach (ServerPlayer *xushu, xushus) {
            if (xushu->canSlash(current, false))
                skill_list.insert(xushu, QStringList(objectName()));
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *current, QVariant &, ServerPlayer *xushu) const
    {
        xushu->setFlags("ZhuhaiSlash");
        QString prompt = QString("@zhuhai-slash:%1:%2").arg(xushu->objectName()).arg(current->objectName());
        if (!room->askForUseSlashTo(xushu, current, prompt, false))
            xushu->setFlags("-ZhuhaiSlash");
        return false;
    }
};

class Qianxin : public TriggerSkill
{
public:
    Qianxin() : TriggerSkill("qianxin")
    {
        events << Damage;
        frequency = Wake;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && TriggerSkill::triggerable(target)
            && target->getMark("qianxin") == 0 && target->isWounded();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());

        room->setPlayerMark(player, "qianxin", 1);
        if (room->changeMaxHpForAwakenSkill(player) && player->getMark("qianxin") == 1)
            room->acquireSkill(player, "jianyan");

        return false;
    }
};

JianyanCard::JianyanCard()
{
    target_fixed = true;
}

void JianyanCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const
{
    QStringList choice_list, pattern_list;
    choice_list << "basic" << "trick" << "equip" << "red" << "black";
    pattern_list << "BasicCard" << "TrickCard" << "EquipCard" << ".|red" << ".|black";

    QString choice = user_string;
    QString pattern = pattern_list.at(choice_list.indexOf(choice));

    LogMessage log;
    log.type = "#JianyanChoice";
    log.from = source;
    log.arg = choice;
    room->sendLog(log);


    int card_id = -1;
    for (int i = room->getDrawPile().length()-1; i >= 0; i--) {
        int id = room->getDrawPile().at(i);
        if (Sanguosha->matchExpPattern(pattern, NULL, Sanguosha->getCard(id))) {
            card_id = id;
            break;
        }
    }
    if (card_id < 0) {
        bool swappile = false;
        foreach (int card_id, room->getDiscardPile()) {
            if (Sanguosha->matchExpPattern(pattern, NULL, Sanguosha->getCard(card_id))) {
                swappile = true;
                break;
            }
        }
        if (swappile) {
            room->swapPile();
            for (int i = room->getDrawPile().length()-1; i >= 0; i--) {
                int id = room->getDrawPile().at(i);
                if (Sanguosha->matchExpPattern(pattern, NULL, Sanguosha->getCard(id))) {
                    card_id = id;
                    break;
                }
            }
        }
    }
    if (card_id < 0) {
        LogMessage log;
        log.type = "$SearchFailed";
        log.from = source;
        log.arg = pattern;
        room->sendLog(log);
    } else {
        const Card *card = Sanguosha->getCard(card_id);
        CardMoveReason reason(CardMoveReason::S_REASON_TURNOVER, source->objectName(), "jianyan", QString());
        room->moveCardTo(card, NULL, Player::PlaceTable, reason, true);

        QList<ServerPlayer *> males;
        foreach (ServerPlayer *player, room->getAlivePlayers()) {
            if (player->isMale())
                males << player;
        }
        if (!males.isEmpty()) {
            source->setMark("jianyan", card_id); // For AI
            ServerPlayer *target = room->askForPlayerChosen(source, males, "jianyan",
                QString("@jianyan-give:::%1:%2\\%3").arg(card->objectName())
                .arg(card->getSuitString() + "_char")
                .arg(card->getNumberString()));
            room->obtainCard(target, card);
        } else {
            CardMoveReason reason2(CardMoveReason::S_REASON_NATURAL_ENTER, source->objectName(), "jianyan", QString());
            room->throwCard(card, reason2, NULL);
        }
    }
}

class JianyanViewAsSkill : public ZeroCardViewAsSkill
{
public:
    JianyanViewAsSkill() : ZeroCardViewAsSkill("jianyan")
    {
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("JianyanCard");
    }

    virtual const Card *viewAs() const
    {
        QString card_name = Self->tag["jianyan"].toString();
        if (card_name.isEmpty()) return NULL;

        JianyanCard *skill_card = new JianyanCard;
        skill_card->setUserString(card_name);
        skill_card->setSkillName("jianyan");
        return skill_card;
    }
};

class Jianyan : public TriggerSkill
{
public:
    Jianyan() : TriggerSkill("jianyan")
    {
        view_as_skill = new JianyanViewAsSkill;
    }

    virtual bool trigger(TriggerEvent, Room *, ServerPlayer *, QVariant &) const
    {
        return false;
    }

    QString getSelectBox() const
    {
        return "basic+trick+equip+red+black";
    }
};

YJCMPackage::YJCMPackage()
    : Package("YJCM")
{
    General *caozhi = new General(this, "caozhi", "wei", 3); // YJ 001
    caozhi->addSkill(new Luoying);
    caozhi->addSkill(new Jiushi);

    General *chengong = new General(this, "chengong", "qun", 3); // YJ 002
    chengong->addSkill(new Mingce);
    chengong->addSkill(new Zhichi);

    General *fazheng = new General(this, "fazheng", "shu", 3); // YJ 003
    fazheng->addSkill(new Enyuan);
    fazheng->addSkill(new Xuanhuo);

    General *gaoshun = new General(this, "gaoshun", "qun"); // YJ 004
    gaoshun->addSkill(new Xianzhen);
    gaoshun->addSkill(new XianzhenTargetMod);
    gaoshun->addSkill(new Jinjiu);
    related_skills.insertMulti("xianzhen", "#xianzhen-target");

    General *lingtong = new General(this, "lingtong", "wu"); // YJ 005
    lingtong->addSkill(new Xuanfeng);

    General *masu = new General(this, "masu", "shu", 3); // YJ 006
    masu->addSkill(new Sanyao);
    masu->addSkill(new Zhiman);

    General *wuguotai = new General(this, "wuguotai", "wu", 3, false); // YJ 007
    wuguotai->addSkill(new Ganlu);
    wuguotai->addSkill(new Buyi);

    General *xusheng = new General(this, "xusheng", "wu"); // YJ 008
    xusheng->addSkill(new Pojun);

    General *xushu = new General(this, "xushu", "shu"); // SHU 017
    xushu->addSkill(new Zhuhai);
    xushu->addSkill(new Qianxin);
    xushu->addRelateSkill("jianyan");

    General *yujin = new General(this, "yujin", "wei"); // YJ 010
    yujin->addSkill(new Zhenjun);

    General *zhangchunhua = new General(this, "zhangchunhua", "wei", 3, false); // YJ 011
    zhangchunhua->addSkill(new Jueqing);
    zhangchunhua->addSkill(new Shangshi);

    General *zhonghui = new General(this, "zhonghui", "wei"); // YJ 012
    zhonghui->addSkill(new QuanjiKeep);
    zhonghui->addSkill(new Quanji);
	zhonghui->addSkill(new DetachEffectSkill("quanji", "power"));
    related_skills.insertMulti("quanji", "#quanji");
	related_skills.insertMulti("quanji", "#quanji-clear");
    zhonghui->addSkill(new Zili);
    zhonghui->addRelateSkill("paiyi");

    addMetaObject<JiushiCard>();
    addMetaObject<MingceCard>();
    addMetaObject<GanluCard>();
    addMetaObject<XianzhenCard>();
    addMetaObject<PaiyiCard>();
    addMetaObject<SanyaoCard>();
    addMetaObject<JianyanCard>();

    skills << new ZhenjunDiscard << new Paiyi << new Jianyan;
}

ADD_PACKAGE(YJCM)
