#include "bestloyalist.h"
#include "engine.h"
#include "clientplayer.h"
#include "json.h"

AllArmy::AllArmy(Card::Suit suit, int number)
    : DelayedTrick(suit, number)
{
    setObjectName("all_army");

    judge.pattern = ".|club";
    judge.good = true;
    judge.reason = objectName();
}

bool AllArmy::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select != Self;
}

void AllArmy::takeEffect(ServerPlayer *target) const
{
    target->broadcastSkillInvoke("@all_army");
    target->getRoom()->addPlayerMark(target, "all_army");
}

BeatAnother::BeatAnother(Card::Suit suit, int number)
    : SingleTargetTrick(suit, number)
{
    setObjectName("beat_another");
}

bool BeatAnother::isAvailable(const Player *player) const
{
    return SingleTargetTrick::isAvailable(player);
}

bool BeatAnother::targetsFeasible(const QList<const Player *> &targets, const Player *) const
{
    return targets.length() == 2;
}

bool BeatAnother::targetFilter(const QList<const Player *> &targets,
    const Player *to_select, const Player *Self) const
{
    if (!targets.isEmpty()) {
        Q_ASSERT(targets.length() <= 2);
        if (targets.length() == 2) return false;
        return Self != to_select;
    } else
        return Self->distanceTo(to_select) == 1;
    return false;
}

void BeatAnother::onUse(Room *room, const CardUseStruct &card_use) const
{
    Q_ASSERT(card_use.to.length() == 2);
    ServerPlayer *target = card_use.to.at(0);
    ServerPlayer *victim = card_use.to.at(1);

    CardUseStruct new_use = card_use;
    new_use.to.removeAt(1);
    target->tag["beatAnotherTo"] = QVariant::fromValue(victim);

    SingleTargetTrick::onUse(room, new_use);
}

void BeatAnother::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *source = effect.from;
    Room *room = source->getRoom();
    ServerPlayer *target = effect.to;
    ServerPlayer *victim = target->tag["beatAnotherTo"].value<ServerPlayer *>();
    target->tag.remove("beatAnotherTo");
    if (victim == NULL) return;

    if (source->isKongcheng()) return;
    const Card *card1 = room->askForExchange(source, objectName(), 1, 1, false, "@beatanother-give:" + target->objectName());
    CardMoveReason reason1(CardMoveReason::S_REASON_GIVE, source->objectName(), target->objectName(), objectName(), QString());
    room->moveCardTo(card1, source, target, Player::PlaceHand, reason1);
    if (target->getHandcardNum() < 2 || victim->isDead()) return;
    const Card *card2 = room->askForExchange(target, objectName(), 2, 2, true, "@beatanother-give:" + victim->objectName());
    CardMoveReason reason2(CardMoveReason::S_REASON_GIVE, target->objectName(), victim->objectName(), objectName(), QString());
    room->moveCardTo(card2, target, victim, Player::PlaceHand, reason2);
}

class BeatAnotherGive : public ViewAsSkill
{
public:
    BeatAnotherGive() : ViewAsSkill("beatanothergive")
    {
    }

    virtual bool isEnabledAtPlay(const Player *) const
    {
        return false;
    }

    virtual bool isEnabledAtResponse(const Player *, const QString &pattern) const
    {
        return pattern == "@@beatanothergive!";
    }

    virtual bool viewFilter(const QList<const Card *> &selected, const Card *) const
    {
        return selected.length() < 2;
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const
    {
        DummyCard *dummy = new DummyCard;
        dummy->addSubcards(cards);
        return dummy;
    }
};

MoreTroops::MoreTroops(Card::Suit suit, int number)
    : SingleTargetTrick(suit, number)
{
    setObjectName("more_troops");
}

bool MoreTroops::targetFilter(const QList<const Player *> &targets, const Player *, const Player *) const
{
    return targets.isEmpty();
}

void MoreTroops::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.from->getRoom();
    effect.to->drawCards(3);
    if (!room->askForCard(effect.to, "@@by_stove!", "@by_stove")) {
        QList<const Card *> cards = effect.to->getCards("he");
        DummyCard *dummy = new DummyCard;
        dummy->deleteLater();
        qShuffle(cards);
        int cardId = -1;
        foreach(const Card *c, cards)
        {
            if (c->getTypeId() != Card::TypeBasic) {
                cardId = c->getId();
                break;
            }
        }
        if (cardId != -1)
            dummy->addSubcard(cardId);
        else {
            dummy->addSubcard(cards.first());
            dummy->addSubcard(cards.last());
        }

        room->throwCard(dummy, effect.to);
    }
}

class ByStove : public ViewAsSkill
{
public:
    ByStove() : ViewAsSkill("by_stove")
    {
    }

    virtual bool isEnabledAtPlay(const Player *) const
    {
        return false;
    }

    virtual bool isEnabledAtResponse(const Player *, const QString &pattern) const
    {
        return pattern == "@@by_stove!";
    }

    virtual bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
        if (Self->isJilei(to_select))
            return false;

        if (selected.length() < 2)
            return true;

        return false;
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const
    {
        bool ok = false;
        if (cards.length() == 1)
            ok = cards.first()->getTypeId() != Card::TypeBasic;
        else if (cards.length() == 2)
            ok = true;

        if (!ok)
            return NULL;

        DummyCard *dummy = new DummyCard;
        dummy->addSubcards(cards);
        return dummy;
    }
};

class ShowLord : public TriggerSkill
{
public:
    ShowLord() : TriggerSkill("showlord")
    {
        global = true;
        events << BuryVictim;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target->getRoom()->getMode() == "08_zdyj" && target != NULL && target->getMark("shown_loyalist");
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *, QVariant &) const
    {
        ServerPlayer *lord = room->getLord(true);
        room->broadcastProperty(lord, "role", lord->getRole());
        LogMessage log;
        log.type = "#Showlord";
        log.to << lord;
        foreach (ServerPlayer *loyalist, room->getOtherPlayers(lord, true))
            if (loyalist->getMark("shown_loyalist"))
                log.from = loyalist;
        room->sendLog(log);
        QStringList skill_names;
        const General *general = lord->getGeneral();
        foreach(const Skill *skill, general->getVisibleSkillList())
        {
            if (skill->isLordSkill())
                skill_names << skill->objectName();
        }
        general = lord->getGeneral2();
        if (general != NULL) {
            foreach (const Skill *skill, general->getVisibleSkillList()) {
                if (skill->isLordSkill())
                    skill_names << skill->objectName();
            }
        }
        if (!skill_names.isEmpty())
            room->handleAcquireDetachSkills(lord, skill_names, true);
        return false;
    }
};

class Yawang : public TriggerSkill
{
public:
    Yawang() : TriggerSkill("yawang")
    {
        events << EventPhaseStart << EventPhaseChanging;
        frequency = Compulsory;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->isAlive();
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseStart) {
            if (TriggerSkill::triggerable(player) && player->getPhase() == Player::Draw) {
                room->sendCompulsoryTriggerLog(player, objectName());
                player->broadcastSkillInvoke(objectName());
                int n = 0;
                foreach(ServerPlayer *p, room->getAllPlayers())
                {
                    if (p->getHp() == player->getHp())
                        ++n;
                }
                player->setFlags(objectName());
                if (n > 0)
                    player->drawCards(n, objectName());
                room->setPlayerMark(player, "#yawang", n);
                return true;
            }
        } else if (triggerEvent == EventPhaseChanging) {
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (change.to != Player::NotActive)
                return false;
            room->setPlayerMark(player, "#yawang", 0);
        }
        return false;
    }
};

class YawangLimitation : public TriggerSkill
{
public:
    YawangLimitation() : TriggerSkill("#yawang")
    {
        events << EventPhaseStart << EventPhaseEnd << CardUsed << CardResponded;
    }

    int getPriority(TriggerEvent triggerEvent) const
    {
        if (triggerEvent == EventPhaseEnd)
            return -6;
        else
            return 6;
        return TriggerSkill::getPriority(triggerEvent);
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->isAlive();
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (player->getPhase() != Player::Play || !player->hasFlag("yawang"))
            return false;
        if (triggerEvent == EventPhaseStart) {
            if (player->getMark("#yawang") < 1) {
                room->setPlayerCardLimitation(player, "use", ".|.|.|.", true);
                room->setPlayerFlag(player, "YawangLimitation");
            }
        } else if (triggerEvent == EventPhaseEnd) {
            if (player->hasFlag("YawangLimitation")) {
                room->setPlayerFlag(player, "-YawangLimitation");
                room->removePlayerCardLimitation(player, "use", ".|.|.|.$1");
            }
        } else {
            const Card *card = NULL;
            if (triggerEvent == CardUsed)
                card = data.value<CardUseStruct>().card;
            else {
                CardResponseStruct resp = data.value<CardResponseStruct>();
                if (resp.m_isUse)
                    card = resp.m_card;
            }
            if (card != NULL && card->getTypeId() != Card::TypeSkill) {
                room->removePlayerMark(player, "#yawang");
                if (player->getMark("#yawang") < 1) {
                    room->setPlayerCardLimitation(player, "use", ".|.|.|.", true);
                    room->setPlayerFlag(player, "YawangLimitation");
                }
            }
        }
        return false;
    }
};

class Xunzhi : public PhaseChangeSkill
{
public:
    Xunzhi() : PhaseChangeSkill("xunzhi")
    {
    }

    virtual bool onPhaseChange(ServerPlayer *cuiyan) const
    {
        Room *room = cuiyan->getRoom();

        if (cuiyan->getPhase() != Player::Start)
            return false;
        if (cuiyan->getNextAlive()->getHp() == cuiyan->getHp())
            return false;

        foreach(ServerPlayer *p, room->getOtherPlayers(cuiyan))
        {
            if (p->getNextAlive() == cuiyan && p->getHp() == cuiyan->getHp())
                return false;
        }

        if (cuiyan->askForSkillInvoke(objectName())) {
            cuiyan->broadcastSkillInvoke(objectName());
            room->loseHp(cuiyan);
            room->addPlayerMark(cuiyan, "#xunzhi", 2);
        }

        return false;
    }
};

class XunzhiKeep : public MaxCardsSkill
{
public:
    XunzhiKeep() : MaxCardsSkill("#xunzhi")
    {
    }

    virtual int getExtra(const Player *target) const
    {
        return target->getMark("#xunzhi");
    }
};

ThrowEquips::ThrowEquips(Card::Suit suit, int number)
    : SingleTargetTrick(suit, number)
{
    setObjectName("throw_equips");
}

bool ThrowEquips::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select->hasEquip() && to_select != Self;
}

void ThrowEquips::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.from->getRoom();
    DummyCard *dummy1 = new DummyCard;
    DummyCard *dummy2 = new DummyCard;
    foreach (const Card *card, effect.to->getCards("he")) {
        if (card->isKindOf("Weapon") || card->isKindOf("OffensiveHorse"))
            dummy1->addSubcard(card);
        if (card->isKindOf("Armor") || card->isKindOf("DefensiveHorse"))
            dummy2->addSubcard(card);
    }
    QStringList choices;
    if (dummy1->subcardsLength() > 0)
        choices << "OffensiveEquips";
    if (dummy2->subcardsLength() > 0)
        choices << "DefensiveEquips";
    if (!choices.isEmpty()) {
        QString choice = room->askForChoice(effect.to, objectName(), choices.join("+"), QVariant(), QString(), "OffensiveEquips+DefensiveEquips");
        if (choice == "OffensiveEquips")
            room->throwCard(dummy1, effect.to);
        else if (choice == "DefensiveEquips")
            room->throwCard(dummy2, effect.to);
    }
    delete dummy1;
    delete dummy2;
}

Escape::Escape(Suit suit, int number)
    : SingleTargetTrick(suit, number)
{
    target_fixed = true;
    setObjectName("escape");
}

void Escape::onEffect(const CardEffectStruct &effect) const
{
    //effect.to_card->setFlags("Global_Nullification_Effected");
}

bool Escape::isAvailable(const Player *) const
{
    return false;
}

Thunder::Thunder(Suit suit, int number) :Disaster(suit, number)
{
    setObjectName("thunder");

    judge.pattern = ".|spade|.";
    judge.good = false;
    judge.reason = objectName();
}

void Thunder::onEffect(const CardEffectStruct &effect) const
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

    if (judge_struct.isBad())
        takeEffect(effect.to);
    onNullified(effect.to);
}

void Thunder::takeEffect(ServerPlayer *target) const
{
    target->broadcastSkillInvoke("@lightning");
    int n = tag["thundernum"].toInt();
    target->getRoom()->damage(DamageStruct(this, NULL, target, ++n, DamageStruct::Thunder));
    setTag("thundernum", n);
}

class TreasuredSwordSkill : public WeaponSkill
{
public:
    TreasuredSwordSkill() : WeaponSkill("treasured_sword")
    {
        events << TargetSpecified << DamageCaused;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == TargetSpecified) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card->isKindOf("Slash")) {
                foreach (ServerPlayer *p, use.to.toSet()) {
                    if (p->getMark("Equips_of_Others_Nullified_to_You") == 0) {
                        room->sendCompulsoryTriggerLog(player, objectName(), false);
                        room->setEmotion(use.from, "weapon/treasured_sword");
                        room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, use.from->objectName(), p->objectName());
                        p->addQinggangTag(use.card);
                    }
                }
            }
        } else if (triggerEvent == DamageCaused) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.card && damage.card->isKindOf("Slash")
                && damage.to->getMark("Equips_of_Others_Nullified_to_You") == 0
                && damage.to->getLostHp() == 0 && damage.by_user && !damage.chain && !damage.transfer) {
                room->setEmotion(player, "weapon/treasured_sword");
                room->sendCompulsoryTriggerLog(player, objectName(), false);
                room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), damage.to->objectName());
                ++damage.damage;
                data = QVariant::fromValue(damage);
            }
        }
        return false;
    }
};

TreasuredSword::TreasuredSword(Suit suit, int number)
    : Weapon(suit, number, 2)
{
    setObjectName("treasured_sword");
}

class SteelSpearSkill : public WeaponSkill
{
public:
    SteelSpearSkill() : WeaponSkill("steel_spear")
    {
        events << TargetSpecified;
    }

    virtual bool trigger(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        foreach (ServerPlayer *to, use.to) {
            if (use.card->isKindOf("Slash")) {
                if ((player->canDiscard(to, "h") || to->canDiscard(player, "h")) && player->askForSkillInvoke(this)) {
                    room->setEmotion(player, "weapon/steel_spear");
                    if (to->canDiscard(player, "h")) {
                        int to_throw = room->askForCardChosen(to, player, "h", objectName(), false, Card::MethodDiscard);
                        room->throwCard(Sanguosha->getCard(to_throw), player, to);
                    }
                    if (player->canDiscard(to, "h")) {
                        int to_throw = room->askForCardChosen(player, to, "h", objectName(), false, Card::MethodDiscard);
                        room->throwCard(Sanguosha->getCard(to_throw), to, player);
                    }
                }
            }
        }
        return false;
    }
};

SteelSpear::SteelSpear(Suit suit, int number)
    : Weapon(suit, number, 3)
{
    setObjectName("steel_spear");
}

class SilverArmorViewAsSkill : public OneCardViewAsSkill
{
public:
    SilverArmorViewAsSkill() : OneCardViewAsSkill("silver_armor")
    {
        filter_pattern = ".|.|.|hand";
        response_pattern = "jink";
        response_or_use = true;
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        Jink *jink = new Jink(originalCard->getSuit(), originalCard->getNumber());
        jink->setSkillName(objectName());
        jink->addSubcard(originalCard->getId());
        return jink;
    }
};

class SilverArmorSkill : public ArmorSkill
{
public:
    SilverArmorSkill() : ArmorSkill("silver_armor")
    {
        events << Damaged;
        view_as_skill = new SilverArmorViewAsSkill;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (damage.card && damage.card->isKindOf("Slash") && player->getArmor()) {
            room->sendCompulsoryTriggerLog(player, objectName(), false);
            room->setEmotion(player, "armor/silver_armor");
            room->throwCard(player->getArmor(), player);
        }
        return false;
    }
};

SilverArmor::SilverArmor(Suit suit, int number)
    : Armor(suit, number)
{
    setObjectName("silver_armor");
}


FenyueCard::FenyueCard()
{
}

bool FenyueCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && Self->canPindian(to_select);
}

void FenyueCard::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *source = effect.from;
    Room *room = source->getRoom();
    ServerPlayer *target = effect.to;

    bool success = source->pindian(target, "fenyue", NULL);
    if (success) {
        QStringList choices;
        choices << "sealhandcards";
        if (source->canSlash(target, NULL, false)) choices << "useslash";
        QString choice = room->askForChoice(source, "fenyue", choices.join("+"), QVariant(), "@fenyue-choose::"+target->objectName());
        if (choice == "sealhandcards") {
            target->addMark("fenyue");
            room->setPlayerCardLimitation(target, "use,response", ".|.|.|hand", true);
            room->addPlayerTip(target, "#fenyue");

            foreach(ServerPlayer *p, room->getAllPlayers())
                room->filterCards(p, p->getCards("he"), true);
            JsonArray args;
            args << QSanProtocol::S_GAME_EVENT_UPDATE_SKILL;
            room->doBroadcastNotify(QSanProtocol::S_COMMAND_LOG_EVENT, args);
        } else if (choice == "useslash") {
            Slash *slash = new Slash(Card::NoSuit, 0);
            slash->setSkillName("_fenyue");
            room->useCard(CardUseStruct(slash, source, target));
        }
    } else
        room->setPlayerFlag(source, "Global_PlayPhaseTerminated");
}

class FenyueViewAsSkill : public ZeroCardViewAsSkill
{
public:
    FenyueViewAsSkill() : ZeroCardViewAsSkill("fenyue")
    {
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return player->getMark("#fenyue") > 0 && !player->isKongcheng();
    }

    virtual const Card *viewAs() const
    {
        return new FenyueCard;
    }
};

class Fenyue : public TriggerSkill
{
public:
    Fenyue() : TriggerSkill("fenyue")
    {
        events << PlayCard << EventPhaseChanging;
        view_as_skill = new FenyueViewAsSkill;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == PlayCard) {
            int loyalist_num = 0;
            foreach (ServerPlayer *p, room->getAlivePlayers()) {
                if (p->getRole() == "loyalist")
                    loyalist_num++;
            }
            room->setPlayerMark(player, "#fenyue", loyalist_num - player->usedTimes("FenyueCard"));
        } else {
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (change.from == Player::Play)
                room->setPlayerMark(player, "#fenyue", 0);
            if (change.to != Player::NotActive) return false;
            QList<ServerPlayer *> players = room->getAllPlayers();
            foreach (ServerPlayer *player, players) {
                if (player->getMark("fenyue") == 0) continue;
                player->removeMark("fenyue");
                room->removePlayerTip(player, "#fenyue");

                foreach(ServerPlayer *p, room->getAllPlayers())
                    room->filterCards(p, p->getCards("he"), false);

                JsonArray args;
                args << QSanProtocol::S_GAME_EVENT_UPDATE_SKILL;
                room->doBroadcastNotify(QSanProtocol::S_COMMAND_LOG_EVENT, args);

                room->removePlayerCardLimitation(player, "use,response", ".|.|.|hand$1");
            }
        }
        return false;
    }
};

class BLDongchaSee : public TriggerSkill
{
public:
    BLDongchaSee() : TriggerSkill("#bl_dongcha-see")
    {
        events << TurnStart;
        frequency = Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target, Room *room) const
    {
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (!room->getTag("FirstRound").toBool()) return false;
        QList<ServerPlayer *> rebels;
        foreach (ServerPlayer *p, room->getAllPlayers(true))
            if (p->getRole() == "rebel")
                rebels << p;
        qShuffle(rebels);
        if (!rebels.isEmpty())
        {
            ServerPlayer *showee = rebels.first();
            foreach (ServerPlayer *bl, room->getAllPlayers())
                if (bl->hasSkill("bl_dongcha"))
                {
                    room->sendCompulsoryTriggerLog(bl, "bl_dongcha");
                    room->notifySkillInvoked(bl, "bl_dongcha");
                    room->broadcastProperty(bl, showee, "role", showee->getRole());
                    room->askForChoice(bl, "bl_dongcha", showee->getGeneralName()+"+cancel", QVariant(), "@@bl_dongcha");
                }
        }
        return false;
    }
};

class BLDongcha : public TriggerSkill
{
public:
    BLDongcha() : TriggerSkill("bl_dongcha")
    {
        events << TurnStart;
        frequency = Frequent;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        QList<ServerPlayer *> targets;
        foreach (ServerPlayer *p, room->getAlivePlayers())
            if (!(p->getEquips().isEmpty() && p->getJudgingArea().isEmpty()) && player->canDiscard(p, "je"))
                targets << p;
        if (targets.isEmpty()) return false;
        if (room->askForSkillInvoke(player, objectName()))
        {
            room->sendCompulsoryTriggerLog(player, objectName());
            room->notifySkillInvoked(player, objectName());
            ServerPlayer *target = room->askForPlayerChosen(player, targets, objectName(), QString(), true);
            if (target)
            {
                int id = room->askForCardChosen(player, target, "ej", objectName(), false, Card::MethodDiscard);
                room->throwCard(id, target, player);
            }
        }
        return false;
    }
};

class BLSheshen : public TriggerSkill
{
public:
    BLSheshen() : TriggerSkill("bl_sheshen")
    {
        events << AskForPeachesDone;
        frequency = Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->getRole() == "lord";
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (!player->hasFlag("Global_Dying") || player->getHp() > 0) return false;
        foreach (ServerPlayer *loyalist, room->getAlivePlayers())
            if (loyalist->hasSkill(objectName()) && loyalist->getMark("shown_loyalist"))
            {
                room->sendCompulsoryTriggerLog(loyalist, objectName());
                room->notifySkillInvoked(loyalist, objectName());
                room->setPlayerFlag(player, "-Global_Dying");

                LogMessage log;
                log.type = "#GainMaxHp";
                log.from = player;
                log.arg = QString::number(1);
                int new_maxhp = player->getMaxHp() + 1;
                log.arg2 = QString::number(new_maxhp);
                room->sendLog(log);
                room->setPlayerProperty(loyalist, "maxhp", new_maxhp);

                int to_hp = loyalist->getHp();
                room->recover(player, RecoverStruct(player, NULL, to_hp - player->getHp()));

                DummyCard *dummy = new DummyCard(loyalist->handCards());
                QList <const Card *> equips = loyalist->getEquips();
                foreach(const Card *card, equips)
                    dummy->addSubcard(card);

                if (dummy->subcardsLength() > 0) {
                    CardMoveReason reason(CardMoveReason::S_REASON_RECYCLE, player->objectName());
                    room->obtainCard(player, dummy, reason, false);
                }
                delete dummy;

                room->killPlayer(loyalist);

                return false;
            }
        return false;
    }

    virtual int getPriority(TriggerEvent triggerEvent) const
    {
        return 0;
    }
};

BestLoyalistCardPackage::BestLoyalistCardPackage()
    : Package("BestLoyalistCard", Package::CardPack)
{
    QList<Card *> cards;

    cards << new AllArmy(Card::Club, 4)
        << new AllArmy(Card::Spade, 10)
        << new BeatAnother(Card::Spade, 3)
        << new BeatAnother(Card::Spade, 4)
        << new BeatAnother(Card::Spade, 11);

    cards << new MoreTroops(Card::Heart, 3)
        << new MoreTroops(Card::Heart, 4)
        << new MoreTroops(Card::Heart, 7)
        << new MoreTroops(Card::Heart, 8)
        << new MoreTroops(Card::Heart, 9)
        << new MoreTroops(Card::Heart, 11);

    cards << new BeatAnother(Card::Diamond, 3)
        << new BeatAnother(Card::Diamond, 4);

    cards << new ThrowEquips(Card::Club, 12)
          << new ThrowEquips(Card::Club, 13);

    cards << new Escape(Card::Spade, 11)
          << new Escape(Card::Club, 12)
          << new Escape(Card::Club, 13)
          << new Escape(Card::Diamond, 12)
          << new Escape(Card::Spade, 13)
          << new Escape(Card::Heart, 1)
          << new Escape(Card::Heart, 13);

    cards << new Thunder(Card::Spade, 1)
          << new Thunder(Card::Heart, 12);

    cards << new TreasuredSword

          << new SteelSpear

          << new SilverArmor(Card::Spade)
          << new SilverArmor(Card::Club);

    foreach(Card *card, cards)
        card->setParent(this);

    skills << new ShowLord << new ByStove << new BeatAnotherGive << new TreasuredSwordSkill
           << new SteelSpearSkill << new SilverArmorSkill;
}

ADD_PACKAGE(BestLoyalistCard)

BestLoyalistPackage::BestLoyalistPackage()
    :Package("BestLoyalist")
{
    General *cuiyan = new General(this, "cuiyan", "wei", 3);
    cuiyan->addSkill(new Yawang);
    cuiyan->addSkill(new YawangLimitation);
    cuiyan->addSkill(new Xunzhi);
    cuiyan->addSkill(new XunzhiKeep);
    related_skills.insertMulti("yawang", "#yawang");
    related_skills.insertMulti("xunzhi", "#xunzhi");

    General *huangfusong = new General(this, "huangfusong", "qun");
    huangfusong->addSkill(new Fenyue);

    addMetaObject<FenyueCard>();

    skills << new BLDongcha << new BLDongchaSee << new BLSheshen;
}

ADD_PACKAGE(BestLoyalist)
