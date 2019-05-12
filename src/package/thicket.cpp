#include "thicket.h"
#include "general.h"
#include "skill.h"
#include "room.h"
#include "maneuvering.h"
#include "clientplayer.h"
#include "client.h"
#include "engine.h"
#include "general.h"
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

class Xingshang : public TriggerSkill
{
public:
    Xingshang() : TriggerSkill("xingshang")
    {
        events << Death;
    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        DeathStruct death = data.value<DeathStruct>();
        ServerPlayer *dead = death.who;
        if (dead->isNude() || player == dead) return QStringList();
        return (TriggerSkill::triggerable(player) && player->isAlive()) ? QStringList(objectName()) : QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *caopi, QVariant &data, ServerPlayer *) const
    {
        DeathStruct death = data.value<DeathStruct>();
        ServerPlayer *player = death.who;
        if (room->askForSkillInvoke(caopi, objectName(), data)) {
            caopi->broadcastSkillInvoke(objectName());

            DummyCard *dummy = new DummyCard(player->handCards());
            QList <const Card *> equips = player->getEquips();
            foreach(const Card *card, equips)
                dummy->addSubcard(card);

            if (dummy->subcardsLength() > 0) {
                CardMoveReason reason(CardMoveReason::S_REASON_RECYCLE, caopi->objectName());
                room->obtainCard(caopi, dummy, reason, false);
            }
            delete dummy;
        }
        return false;
    }
};

class Fangzhu : public MasochismSkill
{
public:
    Fangzhu() : MasochismSkill("fangzhu")
    {
		view_as_skill = new dummyVS;
    }

    virtual void onDamaged(ServerPlayer *caopi, const DamageStruct &) const
    {
        Room *room = caopi->getRoom();
        ServerPlayer *to = room->askForPlayerChosen(caopi, room->getOtherPlayers(caopi), objectName(),
            "@fangzhu-invoke:::" + QString::number(caopi->getLostHp()), true, true);
        if (to) {
            caopi->broadcastSkillInvoke("fangzhu");
            to->turnOver();
            if (caopi->isAlive() && caopi->getLostHp() > 0)
                to->drawCards(caopi->getLostHp(), objectName());
        }
    }
};

class Songwei : public TriggerSkill
{
public:
    Songwei() : TriggerSkill("songwei$")
    {
        events << FinishJudge;
		view_as_skill = new dummyVS;
    }

    virtual TriggerList triggerable(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        TriggerList skill_list;
        JudgeStruct *judge = data.value<JudgeStruct *>();
        if (player->isAlive() && player->getKingdom() == "wei" && judge->card->isBlack()) {
            foreach (ServerPlayer *caopi, room->getOtherPlayers(player)) {
                if (caopi->hasLordSkill(objectName()))
                    skill_list.insert(caopi, QStringList("songwei!"));
            }
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *caopi) const
    {
        if (room->askForChoice(player, objectName(), "yes+no", data, "@songwei-to:" + caopi->objectName()) == "yes") {
            LogMessage log;
            log.type = "#InvokeOthersSkill";
            log.from = player;
            log.to << caopi;
            log.arg = objectName();
            room->sendLog(log);
            room->notifySkillInvoked(caopi, objectName());
            caopi->broadcastSkillInvoke(objectName());

            caopi->drawCards(1, objectName());
        }
        return false;
    }
};

class Duanliang : public OneCardViewAsSkill
{
public:
    Duanliang() : OneCardViewAsSkill("duanliang")
    {
        filter_pattern = "BasicCard,EquipCard|black";
        response_or_use = true;
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        SupplyShortage *shortage = new SupplyShortage(originalCard->getSuit(), originalCard->getNumber());
        shortage->setSkillName(objectName());
        shortage->addSubcard(originalCard);

        return shortage;
    }
};

class DuanliangTargetMod : public TargetModSkill
{
public:
    DuanliangTargetMod() : TargetModSkill("#duanliang-target")
    {
        frequency = NotFrequent;
        pattern = "SupplyShortage";
    }

    virtual int getDistanceLimit(const Player *from, const Card *, const Player *to) const
    {
        if (from->hasSkill("duanliang") && to && from->getHandcardNum() <= to->getHandcardNum())
            return 10000;
        else
            return 0;
    }
};

class Jiezi : public TriggerSkill
{
public:
    Jiezi() : TriggerSkill("jiezi")
    {
        events << EventPhaseChanging;
        frequency = Compulsory;
    }

    virtual TriggerList triggerable(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        TriggerList skill_list;
        PhaseChangeStruct change = data.value<PhaseChangeStruct>();
        if (player->hasSkipped(Player::Draw) && (change.to == Player::Play || change.to == Player::Discard || change.to == Player::Finish)) {
            foreach (ServerPlayer *xuhuang, room->findPlayersBySkillName(objectName())) {
                if (xuhuang != player && !xuhuang->hasFlag("JieziTriggered"))
                    skill_list.insert(xuhuang, QStringList(objectName()));
            }
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *, QVariant &, ServerPlayer *xuhuang) const
    {
        room->sendCompulsoryTriggerLog(xuhuang, objectName());
        xuhuang->broadcastSkillInvoke(objectName());
        room->setPlayerFlag(xuhuang, "JieziTriggered");
        xuhuang->drawCards(1, objectName());
        return false;
    }
};

class SavageAssaultAvoid : public TriggerSkill
{
public:
    SavageAssaultAvoid(const QString &avoid_skill)
        : TriggerSkill("#sa_avoid_" + avoid_skill), avoid_skill(avoid_skill)
    {
        events << CardEffected;
    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        CardEffectStruct effect = data.value<CardEffectStruct>();
        if (player== NULL || player->isDead() || !player->hasSkill(avoid_skill)) return QStringList();
        return (effect.card->isKindOf("SavageAssault")) ? QStringList(objectName()) : QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        player->broadcastSkillInvoke(avoid_skill, 2);
        room->notifySkillInvoked(player, avoid_skill);

        LogMessage log;
        log.type = "#SkillNullify";
        log.from = player;
        log.arg = avoid_skill;
        log.arg2 = "savage_assault";
        room->sendLog(log);

        return true;
    }

private:
    QString avoid_skill;
};

class Huoshou : public TriggerSkill
{
public:
    Huoshou() : TriggerSkill("huoshou")
    {
        events << TargetSpecified;
        frequency = Compulsory;
    }

    virtual TriggerList triggerable(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        TriggerList skill_list;
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card->isKindOf("SavageAssault")) {
            QList<ServerPlayer *> menghuos = room->findPlayersBySkillName(objectName());
            foreach (ServerPlayer *menghuo, menghuos) {
                if (menghuo != player)
                    skill_list.insert(menghuo, QStringList(objectName()));
            }
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *, QVariant &data, ServerPlayer *ask_who) const
    {
        room->sendCompulsoryTriggerLog(ask_who, objectName());
        ask_who->broadcastSkillInvoke(objectName(), 1);
        CardUseStruct use = data.value<CardUseStruct>();
        use.card->setTag("GlobalDamageSource", QVariant::fromValue(ask_who));
        return false;
    }
};

class Zaiqi : public PhaseChangeSkill
{
public:
    Zaiqi() : PhaseChangeSkill("zaiqi")
    {
    }
    
    virtual bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Draw && target->isWounded();
    }

    virtual bool onPhaseChange(ServerPlayer *menghuo) const
    {
        Room *room = menghuo->getRoom();
        if (room->askForSkillInvoke(menghuo, objectName())) {
            menghuo->broadcastSkillInvoke(objectName());

            int x = menghuo->getLostHp();
            QList<int> ids = room->getNCards(x, false);
            CardsMoveStruct move(ids, menghuo, Player::PlaceTable,
                CardMoveReason(CardMoveReason::S_REASON_TURNOVER, menghuo->objectName(), "zaiqi", QString()));
            room->moveCardsAtomic(move, true);

            QList<int> card_to_throw;
            QList<int> card_to_gotback;
            for (int i = 0; i < x; i++) {
                if (Sanguosha->getCard(ids[i])->getSuit() == Card::Heart)
                    card_to_throw << ids[i];
                else
                    card_to_gotback << ids[i];
            }
            if (!card_to_throw.isEmpty()) {
                room->recover(menghuo, RecoverStruct(menghuo, NULL, card_to_throw.length()));

                DummyCard *dummy = new DummyCard(card_to_throw);
                CardMoveReason reason(CardMoveReason::S_REASON_NATURAL_ENTER, menghuo->objectName(), "zaiqi", QString());
                room->throwCard(dummy, reason, NULL);
                delete dummy;
            }
            if (!card_to_gotback.isEmpty()) {
                DummyCard *dummy2 = new DummyCard(card_to_gotback);
                room->obtainCard(menghuo, dummy2);
                delete dummy2;
            }

            
            return true;
        }
        return false;
    }
};

class Juxiang : public TriggerSkill
{
public:
    Juxiang() : TriggerSkill("juxiang")
    {
        events << CardFinished;
        frequency = Compulsory;
    }

    virtual TriggerList triggerable(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card->isKindOf("SavageAssault") && room->isAllOnPlace(use.card, Player::PlaceTable)) {
            QList<ServerPlayer *> zhurongs = room->findPlayersBySkillName(objectName());
            TriggerList skill_list;
            foreach (ServerPlayer *zhurong, zhurongs)
                if (zhurong != player)
                    skill_list.insert(zhurong, QStringList(objectName()));
            return skill_list;
        }
        return TriggerList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *, QVariant &data, ServerPlayer *zhurong) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        room->sendCompulsoryTriggerLog(zhurong, objectName());
        zhurong->broadcastSkillInvoke(objectName(), 1);
        zhurong->obtainCard(use.card);
        return false;
    }
};

class Lieren : public TriggerSkill
{
public:
    Lieren() : TriggerSkill("lieren")
    {
        events << Damage;
    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *zhurong, QVariant &data, ServerPlayer* &) const
    {
        if (!TriggerSkill::triggerable(zhurong)) return QStringList();
        DamageStruct damage = data.value<DamageStruct>();
        if (damage.card && damage.card->isKindOf("Slash") && zhurong->canPindian(damage.to)
            && !damage.chain && !damage.transfer && !damage.to->hasFlag("Global_DebutFlag"))
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *zhurong, QVariant &data, ServerPlayer *) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        ServerPlayer *target = damage.to;
        if (room->askForSkillInvoke(zhurong, objectName(), QVariant::fromValue(target))) {
            zhurong->broadcastSkillInvoke(objectName());
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, zhurong->objectName(), target->objectName());

            bool success = zhurong->pindian(target, "lieren", NULL);
            if (!success) return false;

            if (zhurong->canGetCard(target, "he")) {
                int card_id = room->askForCardChosen(zhurong, target, "he", objectName(), false, Card::MethodGet);
                CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, zhurong->objectName());
                room->obtainCard(zhurong, Sanguosha->getCard(card_id), reason, false);
            }
        }
        return false;
    }
};

class Yinghun : public PhaseChangeSkill
{
public:
    Yinghun() : PhaseChangeSkill("yinghun")
    {

    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target)
            && target->getPhase() == Player::Start && target->isWounded();
    }

    virtual bool onPhaseChange(ServerPlayer *sunjian) const
    {
        Room *room = sunjian->getRoom();
        ServerPlayer *to = room->askForPlayerChosen(sunjian, room->getOtherPlayers(sunjian), objectName(), objectName() + "-invoke", true, true);
        if (to) {
            sunjian->broadcastSkillInvoke(objectName());
            int x = sunjian->getLostHp();

            if (x == 1) {
                to->drawCards(1, objectName());
                room->askForDiscard(to, objectName(), 1, 1, false, true);
            } else {
                to->setFlags("YinghunTarget");
                QString choice = room->askForChoice(sunjian, objectName(), "d1tx+dxt1");
                to->setFlags("-YinghunTarget");
                if (choice == "d1tx") {
                    to->drawCards(1, objectName());
                    room->askForDiscard(to, objectName(), x, x, false, true);
                } else {
                    to->drawCards(x, objectName());
                    room->askForDiscard(to, objectName(), 1, 1, false, true);
                }
            }
        }
        return false;
    }
};

HaoshiCard::HaoshiCard()
{
    will_throw = false;
    mute = true;
    handling_method = Card::MethodNone;
    m_skillName = "_haoshi";
}

bool HaoshiCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    if (!targets.isEmpty() || to_select == Self)
        return false;

    return to_select->getHandcardNum() == Self->getMark("haoshi");
}

void HaoshiCard::onUse(Room *room, const CardUseStruct &card_use) const
{
    ServerPlayer *lusu = card_use.from;

    QVariant data = QVariant::fromValue(card_use);
    RoomThread *thread = room->getThread();

    thread->trigger(PreCardUsed, room, lusu, data);
    thread->trigger(CardUsed, room, lusu, data);
    thread->trigger(CardFinished, room, lusu, data);
}

void HaoshiCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    CardMoveReason reason(CardMoveReason::S_REASON_GIVE, source->objectName(),
        targets.at(0)->objectName(), "haoshi", QString());
    room->moveCardTo(this, targets.at(0), Player::PlaceHand, reason);
}

class HaoshiGive : public ViewAsSkill
{
public:
    HaoshiGive() : ViewAsSkill("haoshigive")
    {
        response_pattern = "@@haoshigive!";
    }

    virtual bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
        if (to_select->isEquipped())
            return false;

        int length = Self->getHandcardNum() / 2;
        return selected.length() < length;
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.length() != Self->getHandcardNum() / 2)
            return NULL;

        HaoshiCard *card = new HaoshiCard;
        card->addSubcards(cards);
        return card;
    }
};

class Haoshi : public TriggerSkill
{
public:
    Haoshi() : TriggerSkill("haoshi")
    {
        events << DrawNCards << EventPhaseEnd << EventPhaseChanging;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseChanging && data.value<PhaseChangeStruct>().from == Player::Draw) {
            room->setPlayerFlag(player, "-haoshiInvoked");
        }
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *lusu, QVariant &, ServerPlayer * &) const
    {
        if (triggerEvent == DrawNCards && TriggerSkill::triggerable(lusu)) return QStringList(objectName());
        else if (triggerEvent == EventPhaseEnd && lusu->isAlive() && lusu->getPhase() == Player::Draw) {
            if (lusu->hasFlag("haoshiInvoked") && lusu->getHandcardNum() > 5)
                return QStringList("haoshi!");
        }

        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *lusu, QVariant &data, ServerPlayer *) const
    {
        if (triggerEvent == DrawNCards) {
            if (lusu->askForSkillInvoke(objectName(), data)) {
                lusu->broadcastSkillInvoke(objectName());
                room->setPlayerFlag(lusu, "haoshiInvoked");
                data = data.toInt()+2;
            }
        } else {
            QList<ServerPlayer *> other_players = room->getOtherPlayers(lusu);
            int least = 1000;
            foreach(ServerPlayer *player, other_players)
                least = qMin(player->getHandcardNum(), least);
            room->setPlayerMark(lusu, "haoshi", least);
            bool used = room->askForUseCard(lusu, "@@haoshigive!", "@haoshi", QVariant(), Card::MethodNone);

            if (!used) {
                // force lusu to give his half cards
                ServerPlayer *beggar = NULL;
                foreach (ServerPlayer *player, other_players) {
                    if (player->getHandcardNum() == least) {
                        beggar = player;
                        break;
                    }
                }

                int n = lusu->getHandcardNum() / 2;
                QList<int> to_give = lusu->handCards().mid(0, n);
                HaoshiCard *haoshi_card = new HaoshiCard;
                haoshi_card->addSubcards(to_give);
                QList<ServerPlayer *> targets;
                targets << beggar;
                haoshi_card->use(room, lusu, targets);
                delete haoshi_card;
            }
        }
        return false;
    }
};

DimengCard::DimengCard()
{
}

bool DimengCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    if (to_select == Self)
        return false;

    if (targets.isEmpty())
        return true;

    if (targets.length() == 1) {
        return qAbs(to_select->getHandcardNum() - targets.first()->getHandcardNum()) == subcardsLength();
    }

    return false;
}

bool DimengCard::targetsFeasible(const QList<const Player *> &targets, const Player *) const
{
    return targets.length() == 2;
}

void DimengCard::use(Room *room, ServerPlayer *, QList<ServerPlayer *> &targets) const
{
	LogMessage log;
    log.type = "#Dimeng";
    log.to = targets;
    room->sendLog(log);
	
    room->swapCards(targets.at(0), targets.at(1), "h", "dimeng");
}

class Dimeng : public ViewAsSkill
{
public:
    Dimeng() : ViewAsSkill("dimeng")
    {
    }

    virtual bool viewFilter(const QList<const Card *> &, const Card *to_select) const
    {
        return !Self->isJilei(to_select);
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const
    {
        DimengCard *card = new DimengCard;
        foreach(const Card *c, cards)
            card->addSubcard(c);
        return card;
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("DimengCard");
    }
};

class Wansha : public TriggerSkill
{
public:
    Wansha() : TriggerSkill("wansha")
    {
        events << AskForPeaches;
        frequency = Compulsory;
    }

    int getPriority(TriggerEvent) const
    {
        return 1;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        DyingStruct dying = data.value<DyingStruct>();
        ServerPlayer *jiaxu = room->getCurrent();
        if (!jiaxu || !TriggerSkill::triggerable(jiaxu) || jiaxu->getPhase() == Player::NotActive)
            return false;
        if (player == jiaxu) {
            jiaxu->broadcastSkillInvoke(objectName());
            room->sendCompulsoryTriggerLog(jiaxu, objectName());
        } else if (player != dying.who)
            room->setTag("SkipGameRule", true);
        return false;
    }
};

class Luanwu : public ZeroCardViewAsSkill
{
public:
    Luanwu() : ZeroCardViewAsSkill("luanwu")
    {
        frequency = Limited;
        limit_mark = "@chaos";
    }

    virtual const Card *viewAs() const
    {
        return new LuanwuCard;
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return player->getMark("@chaos") >= 1;
    }
};

LuanwuCard::LuanwuCard()
{
    target_fixed = true;
}

void LuanwuCard::onUse(Room *room, const CardUseStruct &card_use) const
{
	CardUseStruct use = card_use;
    ServerPlayer *source = card_use.from;
    QVariant data = QVariant::fromValue(use);
    RoomThread *thread = room->getThread();

    thread->trigger(PreCardUsed, room, card_use.from, data);
    use = data.value<CardUseStruct>();
	
    LogMessage log;
    log.from = source;
    log.type = "#UseCard";
    log.card_str = toString();
    room->sendLog(log);
    foreach (ServerPlayer *p, room->getOtherPlayers(source))
        room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, source->objectName(), p->objectName());
    room->removePlayerMark(source, "@chaos");

    thread->trigger(CardUsed, room, source, data);
    use = data.value<CardUseStruct>();
    thread->trigger(CardFinished, room, source, data);
}

void LuanwuCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const
{
    QList<ServerPlayer *> players = room->getOtherPlayers(source);
    foreach (ServerPlayer *player, players) {
        if (player->isAlive())
            room->cardEffect(this, source, player);
    }
}

void LuanwuCard::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.to->getRoom();

    QList<ServerPlayer *> players = room->getOtherPlayers(effect.to);
    QList<int> distance_list;
    int nearest = 1000;
    foreach (ServerPlayer *player, players) {
        int distance = effect.to->distanceTo(player);
        distance_list << distance;
        nearest = qMin(nearest, distance);
    }

    QList<ServerPlayer *> luanwu_targets;
    for (int i = 0; i < distance_list.length(); i++) {
        if (distance_list[i] == nearest && effect.to->canSlash(players[i], NULL, false))
            luanwu_targets << players[i];
    }

    if (luanwu_targets.isEmpty() || !room->askForUseSlashTo(effect.to, luanwu_targets, "@luanwu-slash"))
        room->loseHp(effect.to);
}

class Weimu : public ProhibitSkill
{
public:
    Weimu() : ProhibitSkill("weimu")
    {
    }

    virtual bool isProhibited(const Player *, const Player *to, const Card *card, const QList<const Player *> &) const
    {
        return to->hasSkill(this) && card->isKindOf("TrickCard") && card->isBlack();
    }
};

class Jiuchi : public OneCardViewAsSkill
{
public:
    Jiuchi() : OneCardViewAsSkill("jiuchi")
    {
        filter_pattern = ".|spade|.|hand";
        response_or_use = true;
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return Analeptic::IsAvailable(player);
    }

    virtual bool isEnabledAtResponse(const Player *, const QString &pattern) const
    {
        return  pattern.contains("analeptic");
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        Analeptic *analeptic = new Analeptic(originalCard->getSuit(), originalCard->getNumber());
        analeptic->setSkillName(objectName());
        analeptic->addSubcard(originalCard->getId());
        return analeptic;
    }
};

class Roulin : public TriggerSkill
{
public:
    Roulin() : TriggerSkill("roulin")
    {
        events << TargetConfirmed << TargetSpecified;
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        if (!TriggerSkill::triggerable(player) || !use.card->isKindOf("Slash")) return QStringList();
        ServerPlayer *to = NULL;
        if (triggerEvent == TargetSpecified)
            to = use.to.at(use.index);
        else if (triggerEvent == TargetConfirmed)
            to = use.from;
        if (to->isAlive() && to->isFemale())
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());
        CardUseStruct use = data.value<CardUseStruct>();
        int index = use.index;
        if (triggerEvent == TargetSpecified) {
            ServerPlayer *to = use.to.at(index);
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), to->objectName());
        }
        QVariantList jink_list = use.card->tag["Jink_List"].toList();
        if (jink_list.at(index).toInt() == 1)
            jink_list[index] = 2;
        use.card->setTag("Jink_List", jink_list);
        return false;
    }
};

class Benghuai : public PhaseChangeSkill
{
public:
    Benghuai() : PhaseChangeSkill("benghuai")
    {
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer* &) const
    {
        if (!PhaseChangeSkill::triggerable(player)) return QStringList();
        if (player->getPhase() == Player::Finish) {
            QList<ServerPlayer *> players = room->getOtherPlayers(player);
            foreach(ServerPlayer *p, players)
                if (player->getHp() > p->getHp())
                    return QStringList(objectName());
        }

        return QStringList();
    }

    virtual bool onPhaseChange(ServerPlayer *dongzhuo) const
    {
        Room *room = dongzhuo->getRoom();
        room->sendCompulsoryTriggerLog(dongzhuo, objectName());
        dongzhuo->broadcastSkillInvoke(objectName());

        QString result = room->askForChoice(dongzhuo, "benghuai", "hp+maxhp");

        if (result == "hp")
            room->loseHp(dongzhuo);
        else
            room->loseMaxHp(dongzhuo);
        return false;
    }
};

class Baonue : public TriggerSkill
{
public:
    Baonue() : TriggerSkill("baonue$")
    {
        events << Damage;
		view_as_skill = new dummyVS;
    }

    virtual TriggerList triggerable(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        TriggerList skill_list;
        DamageStruct damage = data.value<DamageStruct>();
        if (player->isAlive() && player->getKingdom() == "qun") {
            foreach (ServerPlayer *dongzhuo, room->getOtherPlayers(player)) {
                if (dongzhuo->hasLordSkill(objectName()))
                    skill_list.insert(dongzhuo, QStringList("baonue!"));
            }
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *dongzhuo) const
    {
        if (room->askForChoice(player, objectName(), "yes+no", data, "@baonue-to:" + dongzhuo->objectName()) == "yes") {
            LogMessage log;
            log.type = "#InvokeOthersSkill";
            log.from = player;
            log.to << dongzhuo;
            log.arg = objectName();
            room->sendLog(log);
            room->notifySkillInvoked(dongzhuo, objectName());
            dongzhuo->broadcastSkillInvoke(objectName());

            JudgeStruct judge;
            judge.pattern = ".|spade";
            judge.good = true;
            judge.reason = objectName();
            judge.who = player;

            room->judge(judge);

            if (judge.isGood())
                room->recover(dongzhuo, RecoverStruct(player));
        }
        return false;
    }
};

ThicketPackage::ThicketPackage()
    : Package("thicket")
{
    General *xuhuang = new General(this, "xuhuang", "wei"); // WEI 010
    xuhuang->addSkill(new Duanliang);
    xuhuang->addSkill(new DuanliangTargetMod);
    related_skills.insertMulti("duanliang", "#duanliang-target");
    xuhuang->addSkill(new Jiezi);

    General *caopi = new General(this, "caopi$", "wei", 3); // WEI 014
    caopi->addSkill(new Xingshang);
    caopi->addSkill(new Fangzhu);
    caopi->addSkill(new Songwei);

    General *menghuo = new General(this, "menghuo", "shu"); // SHU 014
    menghuo->addSkill(new SavageAssaultAvoid("huoshou"));
    menghuo->addSkill(new Huoshou);
    menghuo->addSkill(new Zaiqi);
    related_skills.insertMulti("huoshou", "#sa_avoid_huoshou");

    General *zhurong = new General(this, "zhurong", "shu", 4, false); // SHU 015
    zhurong->addSkill(new SavageAssaultAvoid("juxiang"));
    zhurong->addSkill(new Juxiang);
    zhurong->addSkill(new Lieren);
    related_skills.insertMulti("juxiang", "#sa_avoid_juxiang");

    General *sunjian = new General(this, "sunjian", "wu"); // WU 009
    sunjian->addSkill(new Yinghun);

    General *lusu = new General(this, "lusu", "wu", 3); // WU 014
    lusu->addSkill(new Haoshi);
    lusu->addSkill(new Dimeng);

    General *dongzhuo = new General(this, "dongzhuo$", "qun", 8); // QUN 006
    dongzhuo->addSkill(new Jiuchi);
    dongzhuo->addSkill(new Roulin);
    dongzhuo->addSkill(new Benghuai);
    dongzhuo->addSkill(new Baonue);

    General *jiaxu = new General(this, "jiaxu", "qun", 3); // QUN 007
    jiaxu->addSkill(new Wansha);
    jiaxu->addSkill(new Luanwu);
    jiaxu->addSkill(new Weimu);

    addMetaObject<DimengCard>();
    addMetaObject<LuanwuCard>();
    addMetaObject<HaoshiCard>();

    skills << new HaoshiGive;
}

ADD_PACKAGE(Thicket)

