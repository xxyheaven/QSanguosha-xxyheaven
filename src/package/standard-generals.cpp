#include "general.h"
#include "standard.h"
#include "skill.h"
#include "engine.h"
#include "client.h"
#include "serverplayer.h"
#include "room.h"
#include "standard-skillcards.h"
#include "ai.h"
#include "settings.h"
#include "sp.h"
#include "god.h"
#include "maneuvering.h"
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

class NoDistanceTargetMod : public TargetModSkill
{
public:
    NoDistanceTargetMod() : TargetModSkill("#nodistance-target")
    {
        pattern = "^SkillCard";
    }

    virtual int getDistanceLimit(const Player *, const Card *card, const Player *) const
    {
        if (card->hasFlag("Global_NoDistanceChecking"))
            return 1000;
        else
            return 0;
    }
};

class Jianxiong : public MasochismSkill
{
public:
    Jianxiong() : MasochismSkill("jianxiong")
    {
		view_as_skill = new dummyVS;
    }

    virtual void onDamaged(ServerPlayer *caocao, const DamageStruct &damage) const
    {
        Room *room = caocao->getRoom();
        QVariant data = QVariant::fromValue(damage);
        if (room->askForSkillInvoke(caocao, objectName(), data)) {
            caocao->broadcastSkillInvoke(objectName());
            if (damage.card && room->isAllOnPlace(damage.card, Player::PlaceTable))
                caocao->obtainCard(damage.card);
            caocao->drawCards(1, objectName());
        }
    }
};

class Hujia : public TriggerSkill
{
public:
    Hujia() : TriggerSkill("hujia$")
    {
        events << CardAsked;
    }

    virtual QStringList triggerable(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (player != NULL && player->isAlive() && player->hasLordSkill(objectName())) {
            QString pattern = data.toStringList().first();
            QString prompt = data.toStringList().at(1);
            if (pattern != "jink" || prompt.startsWith("@hujia-jink")) return QStringList();
            if (!room->getLieges("wei", player).isEmpty())  return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *caocao, QVariant &data) const
    {
        QList<ServerPlayer *> lieges = room->getLieges("wei", caocao);
        if (lieges.isEmpty()) return false;

        if (!room->askForSkillInvoke(caocao, objectName(), data)) return false;

        caocao->broadcastSkillInvoke(objectName());
        QVariant tohelp = QVariant::fromValue(caocao);
        foreach (ServerPlayer *liege, lieges) {
            const Card *jink = room->askForCard(liege, "jink", "@hujia-jink:" + caocao->objectName(),
                tohelp, Card::MethodResponse, caocao, false, QString(), true);
            if (jink) {
                room->provide(jink);
                return true;
            }
        }

        return false;
    }
};

class Tuxi : public DrawCardsSkill
{
public:
    Tuxi() : DrawCardsSkill("tuxi")
    {
        view_as_skill = new dummyVS;
    }

    virtual QStringList triggerable(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (DrawCardsSkill::triggerable(player)) {
            int n = data.toInt();
            if (n > 0) {
                QList<ServerPlayer *> targets = room->getOtherPlayers(player);
                foreach(ServerPlayer *p, targets)
                    if (player->canGetCard(p, "h"))
                        return QStringList(objectName());
            }
        }
        return QStringList();
    }

    virtual int getDrawNum(ServerPlayer *zhangliao, int n) const
    {
        Room *room = zhangliao->getRoom();

        QList<ServerPlayer *> to_choose;
        QList<ServerPlayer *> players = room->getOtherPlayers(zhangliao);
        foreach(ServerPlayer *p, players) {
            if (zhangliao->canGetCard(p, "h"))
                to_choose << p;
        }

        if (to_choose.isEmpty()) return n;
        QList<ServerPlayer *> choosees = room->askForPlayersChosen(zhangliao, to_choose, objectName(), 0, n, "@tuxi-card:::" + QString::number(n), true);

        if (choosees.length() > 0) {
            zhangliao->broadcastSkillInvoke(objectName());
            room->sortByActionOrder(choosees);
            foreach (ServerPlayer *target, choosees) {
                if (!zhangliao->canGetCard(target, "h")) continue;
                int card_id = room->askForCardChosen(zhangliao, target, "h", "tuxi", false, Card::MethodGet);
                CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, zhangliao->objectName());
                room->obtainCard(zhangliao, Sanguosha->getCard(card_id), reason, false);
            }
            return n - choosees.length();
        }
        return n;
    }
};

class Tiandu : public TriggerSkill
{
public:
    Tiandu() : TriggerSkill("tiandu")
    {
        frequency = Frequent;
        events << FinishJudge;
    }

    virtual QStringList triggerable(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        JudgeStruct *judge = data.value<JudgeStruct *>();
        if (room->getCardPlace(judge->card->getEffectiveId()) == Player::PlaceJudge && TriggerSkill::triggerable(player))
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *guojia, QVariant &data) const
    {
        JudgeStruct *judge = data.value<JudgeStruct *>();
        const Card *card = judge->card;

        QVariant data_card = QVariant::fromValue(card);
        if (room->getCardPlace(card->getEffectiveId()) == Player::PlaceJudge
            && guojia->askForSkillInvoke(this, data_card)) {
            guojia->broadcastSkillInvoke(objectName());
            guojia->obtainCard(judge->card);
            return false;
        }

        return false;
    }
};

class Yiji : public MasochismSkill
{
public:
    Yiji() : MasochismSkill("yiji")
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

    virtual void onDamaged(ServerPlayer *guojia, const DamageStruct &) const
    {
        if (guojia->askForSkillInvoke(objectName())) {
            guojia->broadcastSkillInvoke(objectName());
            guojia->drawCards(2, objectName());
            if (!guojia->isKongcheng()) {
                QList<int> handcards = guojia->handCards();
                guojia->getRoom()->askForRende(guojia, handcards, objectName(), false, false, true, 2, 0);
            }
        }
    }
};

class Ganglie : public MasochismSkill
{
public:
    Ganglie() : MasochismSkill("ganglie")
    {

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

    virtual void onDamaged(ServerPlayer *xiahou, const DamageStruct &damage) const
    {
        Room *room = xiahou->getRoom();
        ServerPlayer *from = damage.from;
        if (xiahou->askForSkillInvoke(objectName(), QVariant::fromValue(from))) {
            xiahou->broadcastSkillInvoke(objectName());
            if (from)
                room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, xiahou->objectName(), from->objectName());
            JudgeStruct judge;
            judge.pattern = ".";
            judge.patterns << ".|red" << ".|black";
            judge.play_animation = false;
            judge.reason = objectName();
            judge.who = xiahou;

            room->judge(judge);

            if (from == NULL || from->isDead()) return;
            if (judge.pattern == ".|red")
                room->damage(DamageStruct(objectName(), xiahou, from));
            else if (judge.pattern == ".|black" && xiahou->canDiscard(from, "he")) {
                int id = room->askForCardChosen(xiahou, from, "he", objectName(), false, Card::MethodDiscard);
                room->throwCard(id, from, xiahou);
            }
        }
    }
};

QingjianCard::QingjianCard()
{
    will_throw = false;
}

void QingjianCard::onEffect(const CardEffectStruct &effect) const
{
    effect.to->setFlags("QingjianTarget");
}

class QingjianViewAsSkill : public ViewAsSkill
{
public:
    QingjianViewAsSkill() : ViewAsSkill("qingjian")
    {
        response_pattern = "@@qingjian";
    }

    virtual bool viewFilter(const QList<const Card *> &, const Card *) const
    {
        return true;
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.isEmpty()) return NULL;
        QingjianCard *to_give = new QingjianCard;
        to_give->addSubcards(cards);
        return to_give;
    }
};

class Qingjian : public TriggerSkill
{
public:
    Qingjian() : TriggerSkill("qingjian")
    {
        events << CardsMoveOneTime << EventPhaseStart;
        view_as_skill = new QingjianViewAsSkill;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (triggerEvent == EventPhaseStart && player->getPhase() == Player::NotActive) {
            room->setPlayerMark(player, "#qingjian", 0);
        }
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (triggerEvent == CardsMoveOneTime && TriggerSkill::triggerable(player) && !player->hasFlag("qingjianUsed") &&
                player->getPhase() != Player::Draw && !room->getTag("FirstRound").toBool() && !player->isNude()) {
            QVariantList move_datas = data.toList();
            foreach(QVariant move_data, move_datas) {
                CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
                if (move.to == player && move.to_place == Player::PlaceHand)
                    return QStringList(objectName());

            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        const Card *to_give = room->askForUseCard(player, "@@qingjian", "@qingjian-give");
        ServerPlayer *target = NULL;
        foreach (ServerPlayer *p, room->getAlivePlayers()) {
            if (p->hasFlag("QingjianTarget")) {
                p->setFlags("-QingjianTarget");
                target = p;
                break;
            }
        }

        if (to_give && target) {
            room->setPlayerFlag(player, "qingjianUsed");
            QStringList types;
            foreach (int id, to_give->getSubcards()) {
                QString type = Sanguosha->getCard(id)->getType();
                if (!types.contains(type))
                    types << type;
            }

            int x = types.length();

            CardMoveReason reason(CardMoveReason::S_REASON_GIVE, player->objectName(), target->objectName(), objectName(), QString());
            room->obtainCard(target, to_give, reason);

            ServerPlayer *current = room->getCurrent();
            if (current && current->isAlive() && current->getPhase() != Player::NotActive) {
                room->addPlayerMark(current, "#qingjian", x);
                room->addPlayerMark(current, "Global_MaxcardsIncrease", x);
            }
        }
        return false;
    }
};

class Fankui : public MasochismSkill
{
public:
    Fankui() : MasochismSkill("fankui")
    {
    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (MasochismSkill::triggerable(player)) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.from && player->canGetCard(damage.from, "he")) {
                QStringList trigger_list;
                for (int i = 1; i <= damage.damage; i++) {
                    trigger_list << objectName();
                }
                return trigger_list;
            }
        }

        return QStringList();
    }

    virtual void onDamaged(ServerPlayer *simayi, const DamageStruct &damage) const
    {
        ServerPlayer *from = damage.from;
        Room *room = simayi->getRoom();
        if (from && simayi->canGetCard(from, "he") && room->askForSkillInvoke(simayi, objectName(), QVariant::fromValue(from))) {
            simayi->broadcastSkillInvoke(objectName());
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, simayi->objectName(), from->objectName());
            int card_id = room->askForCardChosen(simayi, from, "he", objectName(), false, Card::MethodGet);
            CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, simayi->objectName());
            room->obtainCard(simayi, Sanguosha->getCard(card_id), reason, false);
        }
    }
};

class Guicai : public TriggerSkill
{
public:
    Guicai() : TriggerSkill("guicai")
    {
        events << AskForRetrial;
		view_as_skill = new dummyVS;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return TriggerSkill::triggerable(target) && !(target->isNude() && target->getHandPile().isEmpty());
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        JudgeStruct *judge = data.value<JudgeStruct *>();

        QStringList prompt_list;
        prompt_list << "@guicai-card" << judge->who->objectName()
            << objectName() << judge->reason << QString::number(judge->card->getEffectiveId());
        QString prompt = prompt_list.join(":");
        const Card *card = room->askForCard(player, "..", prompt, data, Card::MethodResponse, judge->who, true, objectName());
        if (card != NULL)
            room->retrial(card, player, judge, objectName());

        return false;
    }
};

class Luoyi : public TriggerSkill
{
public:
    Luoyi() : TriggerSkill("luoyi")
    {
        events << EventPhaseStart << DamageCaused;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (triggerEvent == EventPhaseStart && player->getPhase() == Player::RoundStart && player->getMark("#luoyi") > 0)
            room->removePlayerTip(player, "#luoyi");
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (triggerEvent == EventPhaseStart && TriggerSkill::triggerable(player) && player->getPhase() == Player::Draw)
            return QStringList("luoyi!");
        else if (triggerEvent == DamageCaused && player && player->isAlive() && player->getMark("#luoyi") > 0) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.chain || damage.transfer) return QStringList();
            const Card *reason = damage.card;
            if (reason && (reason->isKindOf("Slash") || reason->isKindOf("Duel")))
                return QStringList("luoyi!");
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        if (triggerEvent == EventPhaseStart) {
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            QList<int> ids = room->getNCards(3, false);
            CardsMoveStruct move(ids, player, Player::PlaceTable,
                CardMoveReason(CardMoveReason::S_REASON_TURNOVER, player->objectName(), objectName(), QString()));
            room->moveCardsAtomic(move, true);

            if (!room->askForSkillInvoke(player, "_luoyi", "prompt")) {
                DummyCard *dummy = new DummyCard(ids);
                CardMoveReason reason(CardMoveReason::S_REASON_NATURAL_ENTER, player->objectName(), objectName(), QString());
                room->throwCard(dummy, reason, NULL);
                delete dummy;
                return false;
            }

            QList<int> card_to_throw;
            QList<int> card_to_gotback;
            for (int i = 0; i < 3; i++) {
                const Card *card = Sanguosha->getCard(ids[i]);
                if (card->getTypeId() == Card::TypeBasic || card->isKindOf("Weapon") || card->isKindOf("Duel"))
                    card_to_gotback << ids[i];
                else
                    card_to_throw << ids[i];
            }
            if (!card_to_gotback.isEmpty()) {
                DummyCard *dummy = new DummyCard(card_to_gotback);
                room->obtainCard(player, dummy);
                delete dummy;
            }
            if (!card_to_throw.isEmpty()) {
                DummyCard *dummy = new DummyCard(card_to_throw);
                CardMoveReason reason(CardMoveReason::S_REASON_NATURAL_ENTER, player->objectName(), "luoyi", QString());
                room->throwCard(dummy, reason, NULL);
                delete dummy;
            }
            room->addPlayerTip(player, "#luoyi");
            return true;
        } else {
            LogMessage log;
            log.type = "#SkillForce";
            log.from = player;
            log.arg = objectName();
            room->sendLog(log);
            DamageStruct damage = data.value<DamageStruct>();
            damage.damage++;
            data = QVariant::fromValue(damage);
        }
        return false;
    }
};

class Luoshen : public TriggerSkill
{
public:
    Luoshen() : TriggerSkill("luoshen")
    {
        events << EventPhaseStart << FinishJudge << PreCardsMoveOneTime;
        frequency = Frequent;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == PreCardsMoveOneTime) {
            QVariantList move_datas = data.toList();
            foreach(QVariant move_data, move_datas) {
                CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
                if (move.from == player && player->property("luoshen_getcards").toString() != "") {
                    QStringList card_list = player->property("luoshen_getcards").toString().split("+");
                    QStringList copy_list = card_list;
                    foreach (QString id_str, card_list) {
                        int id = id_str.toInt();
                        if (move.card_ids.contains(id))
                            copy_list.removeOne(id_str);
                    }
                    room->setPlayerProperty(player, "luoshen_getcards", copy_list.join("+"));
                }
            }
        } else if (triggerEvent == EventPhaseStart && player->getPhase() == Player::NotActive) {
            room->setPlayerProperty(player, "luoshen_getcards", QVariant());
        }
    }
    int getPriority(TriggerEvent triggerEvent) const
    {
        if (triggerEvent == FinishJudge)
            return 5;
        return TriggerSkill::getPriority(triggerEvent);
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (triggerEvent == EventPhaseStart && player->getPhase() == Player::Start && TriggerSkill::triggerable(player))
            return QStringList(objectName());
        else if (triggerEvent == FinishJudge) {
            JudgeStruct *judge = data.value<JudgeStruct *>();
            if (judge->reason == objectName() && judge->isGood() && room->getCardPlace(judge->card->getEffectiveId()) == Player::PlaceJudge)
                return QStringList("luoshen!");
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *zhenji, QVariant &data, ServerPlayer *) const
    {
        if (triggerEvent == EventPhaseStart) {
            bool first = true;
            while (zhenji->isAlive() && zhenji->askForSkillInvoke(objectName())) {
                if (first) {
                    LogMessage log;
                    log.type = "#InvokeSkill";
                    log.from = zhenji;
                    log.arg = objectName();
                    room->sendLog(log);
                    room->notifySkillInvoked(zhenji, objectName());
                    zhenji->broadcastSkillInvoke(objectName());
                    first = false;
                }

                JudgeStruct judge;
                judge.pattern = ".|black";
                judge.good = true;
                judge.reason = objectName();
                judge.who = zhenji;
                judge.time_consuming = true;
                room->judge(judge);

                if (judge.isBad()) break;
            }
        } else if (triggerEvent == FinishJudge) {
            JudgeStruct *judge = data.value<JudgeStruct *>();
            const Card *card = judge->card;
            zhenji->obtainCard(card);
            if (zhenji->handCards().contains(card->getEffectiveId())) {
                QStringList card_list;
                if (zhenji->property("luoshen_getcards").toString() != "")
                    card_list = zhenji->property("luoshen_getcards").toString().split("+");
                card_list << QString::number(card->getEffectiveId());
                room->setPlayerProperty(zhenji, "luoshen_getcards", card_list.join("+"));

            }
        }
        return false;
    }
};

class LuoshenHideCard : public HideCardSkill
{
public:
    LuoshenHideCard() : HideCardSkill("#luoshen-hidecard")
    {
    }
    virtual bool isCardHided(const Player *player, const Card *card) const
    {
        QStringList card_list = player->property("luoshen_getcards").toString().split("+");
        foreach (QString id, card_list) {
            bool ok;
            if (id.toInt(&ok) == card->getEffectiveId() && ok)
                return true;
        }
        return false;
    }
};

class Qingguo : public OneCardViewAsSkill
{
public:
    Qingguo() : OneCardViewAsSkill("qingguo")
    {
        filter_pattern = ".|black|.|hand";
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

class RendeBasic : public OneCardViewAsSkill
{
public:
    RendeBasic() : OneCardViewAsSkill("rende_basic")
    {
        response_pattern = "@@rende_basic";
        guhuo_type = "b";
    }

    bool viewFilter(const Card *to_select) const
    {
        return to_select->isVirtualCard() && to_select->isAvailable(Self);
    }

    const Card *viewAs(const Card *originalCard) const
    {
        Card *basic = Sanguosha->cloneCard(originalCard->objectName());
        basic->setSkillName("_rende");
        return basic;
    }
};

class RendeViewAsSkill : public ViewAsSkill
{
public:
    RendeViewAsSkill() : ViewAsSkill("rende")
    {
    }

    virtual bool viewFilter(const QList<const Card *> &, const Card *to_select) const
    {
        return !to_select->isEquipped();
    }

    virtual bool isEnabledAtPlay(const Player *) const
    {
        return true;
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.isEmpty())
            return NULL;

        RendeCard *rende_card = new RendeCard;
        rende_card->addSubcards(cards);
        return rende_card;
    }
};

class Rende : public TriggerSkill
{
public:
    Rende() : TriggerSkill("rende")
    {
        events << EventPhaseChanging;
        view_as_skill = new RendeViewAsSkill;
    }

    bool triggerable(const ServerPlayer *) const
    {
        return false;
    }

    virtual void record(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (data.value<PhaseChangeStruct>().from == Player::Play) {
            room->setPlayerMark(player, "rende", 0);
            room->setPlayerProperty(player, "rende", QVariant());
        }
    }
};

Jijiang::Jijiang() : ZeroCardViewAsSkill("jijiang$")
{
}

bool Jijiang::isEnabledAtPlay(const Player *player) const
{
    return hasShuGenerals(player) && !player->hasFlag("Global_JijiangFailed") && Slash::IsAvailable(player);
}

bool Jijiang::isEnabledAtResponse(const Player *player, const QString &pattern) const
{
    return hasShuGenerals(player) && pattern == "slash" && !player->hasFlag("Global_JijiangFailed");
}

const Card *Jijiang::viewAs() const
{
    return new JijiangCard;
}

bool Jijiang::hasShuGenerals(const Player *player)
{
    foreach(const Player *p, player->getAliveSiblings())
        if (p->getKingdom() == "shu")
            return true;
    return false;
}

class Wusheng : public OneCardViewAsSkill
{
public:
    Wusheng() : OneCardViewAsSkill("wusheng")
    {
        response_or_use = true;
        filter_pattern = ".|red";
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return Slash::IsAvailable(player);
    }

    virtual bool isEnabledAtResponse(const Player *, const QString &pattern) const
    {
        return pattern == "slash";
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        Card *slash = new Slash(originalCard->getSuit(), originalCard->getNumber());
        slash->addSubcard(originalCard->getId());
        slash->setSkillName(objectName());
        return slash;
    }
};

class WushengTargetMod : public TargetModSkill
{
public:
    WushengTargetMod() : TargetModSkill("#wusheng-target")
    {
    }

    virtual int getDistanceLimit(const Player *from, const Card *card, const Player *) const
    {
        if (from->hasSkill("wusheng") && card->getSuit() == Card::Diamond)
            return 1000;
        else
            return 0;
    }
};

class YijueViewAsSkill : public OneCardViewAsSkill
{
public:
    YijueViewAsSkill() : OneCardViewAsSkill("yijue")
    {
        filter_pattern = ".!";
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("YijueCard");
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        YijueCard *yijueCard = new YijueCard;
        yijueCard->addSubcard(originalCard);
        return yijueCard;
    }
};

class Yijue : public TriggerSkill
{
public:
    Yijue() : TriggerSkill("yijue")
    {
        events << EventPhaseStart << DamageCaused;
        view_as_skill = new YijueViewAsSkill;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *target, QVariant &) const
    {
        if (triggerEvent != EventPhaseStart || target->getPhase() != Player::NotActive) return;

        room->setPlayerProperty(target, "yijue_targets", QVariant());

        QList<ServerPlayer *> players = room->getAllPlayers();
        foreach (ServerPlayer *player, players) {
            if (player->getMark("yijue") == 0) continue;
            player->removeMark("yijue");
            room->removePlayerMark(player, "skill_invalidity");
            room->removePlayerTip(player, "#yijue");

            foreach(ServerPlayer *p, room->getAllPlayers())
                room->filterCards(p, p->getCards("he"), false);

            JsonArray args;
            args << QSanProtocol::S_GAME_EVENT_UPDATE_SKILL;
            room->doBroadcastNotify(QSanProtocol::S_COMMAND_LOG_EVENT, args);

            room->removePlayerCardLimitation(player, "use,response", ".|.|.|hand$1");
        }
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (triggerEvent == DamageCaused && player->isAlive()) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.card && damage.card->isKindOf("Slash") && damage.card->getSuit() == Card::Heart && !damage.chain && !damage.transfer) {
                QStringList assignee_list = player->property("yijue_targets").toString().split("+");
                if (assignee_list.contains(damage.to->objectName()))
                    return QStringList("yijue!");
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *, ServerPlayer *, QVariant &data, ServerPlayer *) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        damage.damage++;
        data = QVariant::fromValue(damage);
        return false;
    }
};

class NonCompulsoryInvalidity : public InvaliditySkill
{
public:
    NonCompulsoryInvalidity() : InvaliditySkill("#non-compulsory-invalidity")
    {
    }

    virtual bool isSkillValid(const Player *player, const Skill *skill) const
    {
        const Skill *mainskill = Sanguosha->getMainSkill(skill->objectName());
        if (mainskill->getFrequency(player) != Skill::Compulsory && mainskill->getFrequency(player) != Skill::Wake)
            return player->getMark("skill_invalidity") == 0;
        return true;
    }
};

class Paoxiao : public TargetModSkill
{
public:
    Paoxiao() : TargetModSkill("paoxiao")
    {
    }

    virtual int getResidueNum(const Player *from, const Card *, const Player *) const
    {
        if (from->hasSkill(this))
            return 1000;
        else
            return 0;
    }

    virtual int getDistanceLimit(const Player *from, const Card *, const Player *) const
    {
        if (from->hasSkill(this) && from->hasFlag("paoxiaoHadUseSlash"))
            return 1000;
        else
            return 0;
    }
};

class Tishen : public TriggerSkill
{
public:
    Tishen() : TriggerSkill("tishen")
    {
        events << EventPhaseEnd << EventPhaseStart << CardFinished;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (triggerEvent == EventPhaseStart && player->getPhase() == Player::RoundStart && player->getMark("#tishen") > 0)
            room->removePlayerTip(player, "#tishen");
    }

    virtual TriggerList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        TriggerList skill_list;
        if (triggerEvent == EventPhaseEnd && TriggerSkill::triggerable(player) && player->getPhase() == Player::Play)
            skill_list.insert(player, QStringList(objectName()));
        else if (triggerEvent == CardFinished) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card && use.card->isKindOf("Slash") && room->isAllOnPlace(use.card, Player::PlaceTable)) {
                QStringList damaged_tag = use.card->tag["GlobalCardDamagedTag"].toStringList();
                foreach (ServerPlayer *to, use.to) {
                    if (to->getMark("#tishen") > 0 && !damaged_tag.contains(to->objectName()))
                        skill_list.insert(to, QStringList("tishen!"));
                }
            }
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *zhangfei) const
    {
        if (triggerEvent == EventPhaseEnd && player->askForSkillInvoke(objectName())) {
            player->broadcastSkillInvoke(objectName());
            DummyCard *dummy = new DummyCard;
            foreach (const Card *card, player->getCards("he")) {
                if (card->getTypeId() == Card::TypeTrick || card->isKindOf("Horse"))
                    dummy->addSubcard(card);
            }
            if (dummy->subcardsLength() > 0)
                room->throwCard(dummy, player);
            delete dummy;
            room->addPlayerTip(player, "#tishen");
        } else if (triggerEvent == CardFinished) {
            CardUseStruct use = data.value<CardUseStruct>();
            zhangfei->obtainCard(use.card);
        }
        return false;
    }
};

class Longdan : public OneCardViewAsSkill
{
public:
    Longdan() : OneCardViewAsSkill("longdan")
    {
        response_or_use = true;
    }

    virtual bool viewFilter(const Card *to_select) const
    {
        const Card *card = to_select;

        switch (Sanguosha->currentRoomState()->getCurrentCardUseReason()) {
        case CardUseStruct::CARD_USE_REASON_PLAY: {
            return card->isKindOf("Jink");
        }
        case CardUseStruct::CARD_USE_REASON_RESPONSE:
        case CardUseStruct::CARD_USE_REASON_RESPONSE_USE: {
            QString pattern = Sanguosha->currentRoomState()->getCurrentCardUsePattern();
            if (pattern == "slash")
                return card->isKindOf("Jink");
            else if (pattern == "jink")
                return card->isKindOf("Slash");
        }
        default:
            return false;
        }
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return Slash::IsAvailable(player);
    }

    virtual bool isEnabledAtResponse(const Player *, const QString &pattern) const
    {
        return pattern == "jink" || pattern == "slash";
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        if (originalCard->isKindOf("Slash")) {
            Jink *jink = new Jink(originalCard->getSuit(), originalCard->getNumber());
            jink->addSubcard(originalCard);
            jink->setSkillName(objectName());
            return jink;
        } else if (originalCard->isKindOf("Jink")) {
            Slash *slash = new Slash(originalCard->getSuit(), originalCard->getNumber());
            slash->addSubcard(originalCard);
            slash->setSkillName(objectName());
            return slash;
        } else
            return NULL;
    }
};

class Yajiao : public TriggerSkill
{
public:
    Yajiao() : TriggerSkill("yajiao")
    {
        events << CardUsed << CardResponded;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(player) || player->getPhase() != Player::NotActive) return QStringList();
        const Card *cardstar = NULL;
        bool isHandcard = false;
        if (triggerEvent == CardUsed) {
            CardUseStruct use = data.value<CardUseStruct>();
            cardstar = use.card;
            isHandcard = use.m_isHandcard;
        } else {
            CardResponseStruct resp = data.value<CardResponseStruct>();
            cardstar = resp.m_card;
            isHandcard = resp.m_isHandcard;
        }
        if (cardstar && cardstar->getTypeId() != Card::TypeSkill && isHandcard)
            return QStringList(objectName());

        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        const Card *cardstar = NULL;
        if (triggerEvent == CardUsed) {
            CardUseStruct use = data.value<CardUseStruct>();
            cardstar = use.card;
        } else {
            CardResponseStruct resp = data.value<CardResponseStruct>();
            cardstar = resp.m_card;
        }
        if (room->askForSkillInvoke(player, objectName(), data)) {
            player->broadcastSkillInvoke(objectName());
            QList<int> ids = room->getNCards(1, false);
            CardsMoveStruct move(ids, NULL, Player::PlaceTable,
                CardMoveReason(CardMoveReason::S_REASON_TURNOVER, player->objectName(), "yajiao", QString()));
            room->moveCardsAtomic(move, true);

            int id = ids.first();
            const Card *card = Sanguosha->getCard(id);
            bool discard = (cardstar == NULL || card->getTypeId() != cardstar->getTypeId());

            player->setMark("yajiao", id); // For AI
            ServerPlayer *target = room->askForPlayerChosen(player, room->getAlivePlayers(), objectName(),
                QString("@yajiao-give:::%1:%2\\%3").arg(card->objectName())
                .arg(card->getSuitString() + "_char")
                .arg(card->getNumberString()));
            player->setMark("yajiao", 0); // For AI
            room->obtainCard(target, card);
            if (discard)
                room->askForDiscard(player, objectName(), 1, 1, false, true);
        }
        return false;
    }
};

class Tieji : public TriggerSkill
{
public:
    Tieji() : TriggerSkill("tieji")
    {
        events << TargetSpecified << EventPhaseStart;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *target, QVariant &) const
    {
        if (triggerEvent != EventPhaseStart || target->getPhase() != Player::NotActive) return;

        QList<ServerPlayer *> players = room->getAllPlayers();
        foreach (ServerPlayer *player, players) {
            if (player->getMark("tieji") == 0) continue;
            room->removePlayerTip(player, "#tieji");
            room->removePlayerMark(player, "skill_invalidity");
            player->setMark("tieji", 0);

            foreach(ServerPlayer *p, room->getAllPlayers())
                room->filterCards(p, p->getCards("he"), false);
            JsonArray args;
            args << QSanProtocol::S_GAME_EVENT_UPDATE_SKILL;
            room->doBroadcastNotify(QSanProtocol::S_COMMAND_LOG_EVENT, args);
        }
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (triggerEvent == TargetSpecified && TriggerSkill::triggerable(player)) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card != NULL && use.card->isKindOf("Slash")) {
                ServerPlayer *to = use.to.at(use.index);
                if (to && to->isAlive())
                    return QStringList(objectName());
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *, QVariant &data, ServerPlayer *player) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        int index = use.index;
        ServerPlayer *to = use.to.at(index);
        if (player->askForSkillInvoke(this, QVariant::fromValue(to))) {
            player->broadcastSkillInvoke(objectName());
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), to->objectName());

            if (to->getMark("tieji") == 0) {
                to->addMark("tieji");
                room->addPlayerTip(to, "#tieji");
                room->addPlayerMark(to, "skill_invalidity");
                foreach(ServerPlayer *p, room->getAllPlayers())
                    room->filterCards(p, p->getCards("he"), true);
                JsonArray args;
                args << QSanProtocol::S_GAME_EVENT_UPDATE_SKILL;
                room->doBroadcastNotify(QSanProtocol::S_COMMAND_LOG_EVENT, args);
            }

            JudgeStruct judge;
            judge.pattern = ".";
            judge.good = true;
            judge.reason = objectName();
            judge.who = player;
            judge.play_animation = false;
            judge.patterns << ".|heart" << ".|diamond" << ".|club" << ".|spade";

            room->judge(judge);

            if (to->isDead()) return false;

            if (!room->askForCard(to, judge.pattern, "@tieji-discard:::" + judge.pattern.mid(2), data, Card::MethodDiscard)) {
                LogMessage log;
                log.type = "#NoJink";
                log.from = to;
                room->sendLog(log);

                QVariantList jink_list = use.card->tag["Jink_List"].toList();
                jink_list[index] = 0;
                use.card->setTag("Jink_List", jink_list);

            }
        }

        return false;
    }
};

class Guanxing : public PhaseChangeSkill
{
public:
    Guanxing() : PhaseChangeSkill("guanxing")
    {
        frequency = Frequent;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return ((PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Start)
                || (target != NULL &&target->isAlive() && target->getPhase() == Player::Finish && target->hasFlag("GuanxingAllButtom")));
    }

    virtual bool onPhaseChange(ServerPlayer *zhuge) const
    {
        if (zhuge->askForSkillInvoke(this)) {
            zhuge->broadcastSkillInvoke(objectName());
            Room *room = zhuge->getRoom();
            QList<int> guanxing = room->getNCards(getGuanxingNum(room));

            LogMessage log;
            log.type = "$ViewDrawPile";
            log.from = zhuge;
            log.card_str = IntList2StringList(guanxing).join("+");
            room->sendLog(log, zhuge);

            AskForMoveCardsStruct result = room->askForArrangeCards(zhuge, guanxing);
            QList<int> top_cards = result.top, bottom_cards = result.bottom;
            if (top_cards.isEmpty()) room->setPlayerFlag(zhuge, "GuanxingAllButtom");
            room->guanxingFinish(zhuge, top_cards, bottom_cards);
        }

        return false;
    }

    virtual int getGuanxingNum(Room *room) const
    {
        return room->alivePlayerCount() > 3 ? 5:3;
    }
};

class Kongcheng : public ProhibitSkill
{
public:
    Kongcheng() : ProhibitSkill("kongcheng")
    {
    }

    virtual bool isProhibited(const Player *, const Player *to, const Card *card, const QList<const Player *> &) const
    {
        return to->hasSkill(this) && (card->isKindOf("Slash") || card->isKindOf("Duel")) && to->isKongcheng();
    }
};

class KongchengEffect : public TriggerSkill
{
public:
    KongchengEffect() :TriggerSkill("#kongcheng-effect")
    {
        events << CardsMoveOneTime;
    }

    virtual void record(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (player == NULL || player->isDead() || !player->hasSkill("kongcheng")) return;
        QVariantList move_datas = data.toList();
        foreach(QVariant move_data, move_datas) {
            CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
            if (move.from == player && move.from_places.contains(Player::PlaceHand) && player->isKongcheng()) {
                room->sendCompulsoryTriggerLog(player, "kongcheng");
                player->broadcastSkillInvoke("kongcheng");
                break;
            }
        }
    }

    virtual bool triggerable(const ServerPlayer *) const
    {
        return false;
    }
};

class Jizhi : public TriggerSkill
{
public:
    Jizhi() : TriggerSkill("jizhi")
    {
        frequency = Frequent;
        events << CardUsed << EventPhaseStart;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (triggerEvent == EventPhaseStart && player->getPhase() == Player::NotActive) {
            QList<ServerPlayer *> all_players = room->getAllPlayers();
            foreach (ServerPlayer *p, all_players) {
                room->setPlayerMark(p, "#jizhi", 0);
            }
        }
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (triggerEvent != CardUsed || !TriggerSkill::triggerable(player)) return QStringList();
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card && use.card->isNDTrick())
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *yueying, QVariant &, ServerPlayer *) const
    {
        if (room->askForSkillInvoke(yueying, objectName())) {
            yueying->broadcastSkillInvoke(objectName());

            bool from_up = true;
            if (yueying->hasSkill("cunmu")) {
                room->sendCompulsoryTriggerLog(yueying, "cunmu");
                yueying->broadcastSkillInvoke("cunmu");
                from_up = false;
            }
            int id = room->drawCard(from_up);
            const Card *card = Sanguosha->getCard(id);
            CardMoveReason reason(CardMoveReason::S_REASON_DRAW, yueying->objectName(), objectName(), QString());
            room->obtainCard(yueying, card, reason, false);
            if (card->getTypeId() == Card::TypeBasic && yueying->handCards().contains(id)
                    && room->askForChoice(yueying, objectName(),"yes+no", QVariant::fromValue(card), "@jizhi-discard:::"+card->objectName()) == "yes") {
                room->throwCard(card, yueying);
                room->addPlayerMark(yueying, "#jizhi");
                room->addPlayerMark(yueying, "Global_MaxcardsIncrease");
            }
        }
        return false;
    }
};

class Qicai : public TargetModSkill
{
public:
    Qicai() : TargetModSkill("qicai")
    {
        pattern = "TrickCard";
    }

    virtual int getDistanceLimit(const Player *from, const Card *, const Player *) const
    {
        if (from->hasSkill(this))
            return 1000;
        else
            return 0;
    }
};

class Zhiheng : public ViewAsSkill
{
public:
    Zhiheng() : ViewAsSkill("zhiheng")
    {
    }

    virtual bool viewFilter(const QList<const Card *> &, const Card *to_select) const
    {
        return !Self->isJilei(to_select);
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const
    {
		if (cards.isEmpty())
            return NULL;

        ZhihengCard *zhiheng_card = new ZhihengCard;
        zhiheng_card->addSubcards(cards);
        zhiheng_card->setSkillName(objectName());
        return zhiheng_card;
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("ZhihengCard");
    }
};

class Jiuyuan : public TriggerSkill
{
public:
    Jiuyuan() : TriggerSkill("jiuyuan$")
    {
        events << TargetSpecifying << CardFinished;
    }

    virtual TriggerList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        TriggerList skill_list;
        if (triggerEvent == TargetSpecifying) {
            if (use.card->isKindOf("Peach") && !use.to.isEmpty() && player->isAlive() && player->getKingdom() == "wu") {
                if (use.to.first() != player) return skill_list;
                QList<ServerPlayer *> all_players = room->getAllPlayers();
                foreach (ServerPlayer *sunquan, all_players) {
                    if (sunquan->hasLordSkill(this) && !use.to.contains(sunquan) && sunquan->isWounded() && sunquan->getHp() < player->getHp())
                        skill_list.insert(sunquan, QStringList("jiuyuan!"));
                }
            }
        } else if (triggerEvent == CardFinished) {
            if (use.card->isKindOf("Peach") && use.card->hasFlag("JiuyuanEffect") && player->isAlive())
                skill_list.insert(player, QStringList("jiuyuan!"));

        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *sunquan) const
    {
        if (triggerEvent == TargetSpecifying) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (room->askForChoice(player, objectName(), "yes+no", data, "@jiuyuan:"+sunquan->objectName()) == "yes") {
                LogMessage log;
                log.type = "#InvokeOthersSkill";
                log.from = player;
                log.to << sunquan;
                log.arg = objectName();
                room->sendLog(log);
                sunquan->broadcastSkillInvoke(objectName());
                room->notifySkillInvoked(sunquan, objectName());

                use.to.removeOne(player);
                use.to.append(sunquan);
                room->sortByActionOrder(use.to);
                data = QVariant::fromValue(use);
                room->setCardFlag(use.card, "JiuyuanEffect");
            }
        } else if (triggerEvent == CardFinished)
            player->drawCards(1, objectName());

        return false;
    }
};

class Yingzi : public DrawCardsSkill
{
public:
    Yingzi() : DrawCardsSkill("yingzi")
    {
        frequency = Compulsory;
    }

    virtual int getDrawNum(ServerPlayer *zhouyu, int n) const
    {
        Room *room = zhouyu->getRoom();

        zhouyu->broadcastSkillInvoke(objectName());
        room->sendCompulsoryTriggerLog(zhouyu, objectName());
        return n + 1;
    }
};

class YingziMaxCards : public MaxCardsSkill
{
public:
    YingziMaxCards() : MaxCardsSkill("#yingzi")
    {
    }

    virtual int getFixed(const Player *target) const
    {
        if (target->hasSkill("yingzi"))
            return target->getMaxHp();
        else
            return -1;
    }
};

class Fanjian : public OneCardViewAsSkill
{
public:
    Fanjian() : OneCardViewAsSkill("fanjian")
    {
        filter_pattern = ".|.|.|hand";
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("FanjianCard");
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        FanjianCard *card = new FanjianCard;
        card->addSubcard(originalCard);
        card->setSkillName(objectName());
        return card;
    }
};

class Keji : public TriggerSkill
{
public:
    Keji() : TriggerSkill("keji")
    {
        events << EventPhaseChanging;
        frequency = Frequent;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (TriggerSkill::triggerable(player)) {
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (change.to == Player::Discard && player->getCardUsedTimes("Slash|play") == 0 && player->getCardRespondedTimes("Slash|play") == 0)
                return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *, ServerPlayer *lvmeng, QVariant &, ServerPlayer *) const
    {
        if (lvmeng->askForSkillInvoke(this)) {
            lvmeng->broadcastSkillInvoke(objectName());
            lvmeng->skip(Player::Discard);
        }
        return false;
    }
};

class Qinxue : public PhaseChangeSkill
{
public:
    Qinxue() : PhaseChangeSkill("qinxue")
    {
        frequency = Wake;
    }

    virtual QStringList triggerable(TriggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer* &) const
    {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        if (player->getPhase() == Player::Start && player->getMark("qinxue") == 0) {
            int n = player->getHandcardNum() - player->getHp();
            int wake_lim = (Sanguosha->getPlayerCount(room->getMode()) >= 7) ? 2 : 3;
            if (n >= wake_lim) return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool onPhaseChange(ServerPlayer *lvmeng) const
    {
        Room *room = lvmeng->getRoom();
        room->sendCompulsoryTriggerLog(lvmeng, objectName());
        lvmeng->broadcastSkillInvoke(objectName());

        room->setPlayerMark(lvmeng, "qinxue", 1);
        if (room->changeMaxHpForAwakenSkill(lvmeng) && lvmeng->getMark("qinxue") == 1)
            room->acquireSkill(lvmeng, "gongxin");

        return false;
    }
};

class Qixi : public OneCardViewAsSkill
{
public:
    Qixi() : OneCardViewAsSkill("qixi")
    {
        filter_pattern = ".|black";
        response_or_use = true;
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        Dismantlement *dismantlement = new Dismantlement(originalCard->getSuit(), originalCard->getNumber());
        dismantlement->addSubcard(originalCard->getId());
        dismantlement->setSkillName(objectName());
        return dismantlement;
    }
};

class Fenwei : public TriggerSkill
{
public:
    Fenwei() : TriggerSkill("fenwei")
    {
        events << TargetSpecified;
        frequency = Limited;
        limit_mark = "@fenwei";
    }

    virtual TriggerList triggerable(TriggerEvent , Room *room, ServerPlayer *, QVariant &data) const
    {
        TriggerList skill_list;
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card->getTypeId() != Card::TypeTrick || use.index > 0) return skill_list;
        ServerPlayer *first = use.to.first();
        foreach (ServerPlayer *to, use.to) {
            if (to->isAlive() && to != first) {
                QList<ServerPlayer *> gannings = room->findPlayersBySkillName(objectName());
                foreach (ServerPlayer *ganning, gannings) {
                    if (ganning->getMark(limit_mark) > 0)
                        skill_list.insert(ganning, QStringList(objectName()));
                }
                return skill_list;
            }
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *, QVariant &data, ServerPlayer *ganning) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        QList<ServerPlayer *> choosees = room->askForPlayersChosen(ganning, use.to, objectName(), 0, use.to.length(),
                "@fenwei-card:::"+use.card->objectName(), true);
        if (choosees.length() > 0) {
            ganning->broadcastSkillInvoke(objectName());
            room->removePlayerMark(ganning, limit_mark);

            foreach (ServerPlayer *target, choosees) {
                use.nullified_list << target->objectName();
            }
        }
        data = QVariant::fromValue(use);
        return false;
    }
};

class Kurou : public OneCardViewAsSkill
{
public:
    Kurou() : OneCardViewAsSkill("kurou")
    {
        filter_pattern = ".!";
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("KurouCard");
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        KurouCard *card = new KurouCard;
        card->addSubcard(originalCard);
        card->setSkillName(objectName());
        return card;
    }
};

class Zhaxiang : public TriggerSkill
{
public:
    Zhaxiang() : TriggerSkill("zhaxiang")
    {
        events << HpLost << EventPhaseStart << CardUsed;
        frequency = Compulsory;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == CardUsed && player->getMark(objectName()) > 0) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card->isKindOf("Slash") && use.card->isRed()) {
                QStringList fuji_tag = use.card->tag["Fuji_tag"].toStringList();
                fuji_tag << "_ALL_PLAYERS";
                use.card->setTag("Fuji_tag", fuji_tag);
            }
        } else if (triggerEvent == EventPhaseStart && player->getPhase() == Player::NotActive)
            room->setPlayerMark(player, objectName(), 0);
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (triggerEvent == HpLost && TriggerSkill::triggerable(player)) {
            int lose = data.toInt();
            QStringList trigger_list;
            for (int i = 1; i <= lose; i++) {
                trigger_list << objectName();
            }
            return trigger_list;
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());
        player->drawCards(3, objectName());
        if (player->getPhase() == Player::Play)
            room->addPlayerMark(player, objectName());

        return false;
    }
};

class ZhaxiangTargetMod : public TargetModSkill
{
public:
    ZhaxiangTargetMod() : TargetModSkill("#zhaxiang-target")
    {
    }

    virtual int getResidueNum(const Player *from, const Card *, const Player *) const
    {
        return from->getMark("zhaxiang");
    }

    virtual int getDistanceLimit(const Player *from, const Card *card, const Player *) const
    {
        if (card->isRed() && from->getMark("zhaxiang") > 0)
            return 1000;
        else
            return 0;
    }
};

class Guose : public OneCardViewAsSkill
{
public:
    Guose() : OneCardViewAsSkill("guose")
    {

    }

    virtual bool isResponseOrUse() const
    {
        return Self->tag["guose"].toString() == "use_indulgence";
    }

    virtual bool viewFilter(const Card *card) const
    {
        if (card->getSuit() != Card::Diamond) return false;
        QString choice = Self->tag["guose"].toString();
        if (choice == "use_indulgence") {
            Indulgence *indulgence = new Indulgence(card->getSuit(), card->getNumber());
            indulgence->addSubcard(card);
            indulgence->setSkillName("guose");
            indulgence->deleteLater();
            return indulgence->isAvailable(Self);
        } else if (choice == "dis_indulgence")
            return !Self->isJilei(card);
        return false;
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("GuoseCard");
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        QString user_string = Self->tag["guose"].toString();
        if (user_string.isEmpty()) return NULL;
        GuoseCard *card = new GuoseCard;
        card->addSubcard(originalCard);
        card->setSkillName(objectName());
        card->setUserString(user_string);
        return card;
    }

    QString getSelectBox() const
    {
        return "use_indulgence+dis_indulgence";
    }

    virtual int getEffectIndex(const ServerPlayer *, const Card *card) const
    {
        int index = -1;
        if (card->isKindOf("GuoseCard")) {
            index = 2;
        } else if (card->isKindOf("Indulgence")) {
            index = 1;
        }
        return index;
    }
};

class LiuliViewAsSkill : public OneCardViewAsSkill
{
public:
    LiuliViewAsSkill() : OneCardViewAsSkill("liuli")
    {
        filter_pattern = ".!";
        response_pattern = "@@liuli";
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        LiuliCard *liuli_card = new LiuliCard;
        liuli_card->addSubcard(originalCard);
        return liuli_card;
    }
};

class Liuli : public TriggerSkill
{
public:
    Liuli() : TriggerSkill("liuli")
    {
        events << TargetConfirming;
        view_as_skill = new LiuliViewAsSkill;
    }

    virtual QStringList triggerable(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(player) || player->isNude()) return QStringList();
        CardUseStruct use = data.value<CardUseStruct>();
        if (!use.card->isKindOf("Slash")) return QStringList();
        QList<ServerPlayer *> players = room->getOtherPlayers(player);
        players.removeOne(use.from);
        if (players.isEmpty()) return QStringList();
        return QStringList(objectName());
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *daqiao, QVariant &data, ServerPlayer *) const
    {
        CardUseStruct use = data.value<CardUseStruct>();

        QString prompt = "@liuli:" + use.from->objectName();
        // a temp nasty trick
        daqiao->tag["liuli-data"] = QVariant::fromValue(use); // for the server (AI)

        QStringList available_targets;
        QList<ServerPlayer *> to_choosees = use.from->getUseExtraTargets(use, true);

        foreach (ServerPlayer *p, to_choosees)
            available_targets << p->objectName();


        room->setPlayerProperty(daqiao, "liuli_available_targets", available_targets.join("+"));
        daqiao->tag["liuli-use"] = data;
        const Card *card = room->askForUseCard(daqiao, "@@liuli", prompt, data, Card::MethodDiscard);
        room->setPlayerProperty(daqiao, "liuli_available_targets", QVariant());
        daqiao->tag.remove("liuli-use");
        room->setPlayerProperty(daqiao, "liuli", QString());
        if (card) {
            QList<ServerPlayer *> players = room->getAlivePlayers();
            foreach (ServerPlayer *p, players) {
                if (p->hasFlag("LiuliTarget")) {
                    p->setFlags("-LiuliTarget");
                    use.to.removeOne(daqiao);
                    use.to.append(p);
                    room->sortByActionOrder(use.to);
                    data = QVariant::fromValue(use);
                    return true;
                }
            }
        }
        return false;
    }
};

class Qianxun : public TriggerSkill
{
public:
    Qianxun() : TriggerSkill("qianxun")
    {
        events << TrickEffect << EventPhaseChanging;
    }

    virtual TriggerList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        TriggerList skill_list;
        if ((triggerEvent == EventPhaseChanging)) {
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (change.to == Player::NotActive) {
                QList<ServerPlayer *> allplayers = room->getAlivePlayers();
                foreach (ServerPlayer *p, allplayers) {
                    if (!p->getPile("modesty").isEmpty())
                        skill_list.insert(p, QStringList("qianxun!"));
                }
            }
        } else if (triggerEvent == TrickEffect && TriggerSkill::triggerable(player)) {
            CardEffectStruct effect = data.value<CardEffectStruct>();
            foreach (ServerPlayer *p, effect.targets) {
                if (p->isAlive() && p != player)
                    return skill_list;
            }
            if (effect.card && effect.card->getTypeId() == Card::TypeTrick
                && (effect.card->isKindOf("DelayedTrick") || effect.from != player) && !player->isKongcheng())
                    skill_list.insert(player, QStringList(objectName()));
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *, QVariant &data, ServerPlayer *player) const
    {
        if (triggerEvent == TrickEffect) {
            if (room->askForSkillInvoke(player, objectName(), data)) {
                player->broadcastSkillInvoke(objectName());
                player->tag["QianxunEffectData"] = data;
                QList<ServerPlayer *> open;
                open << player;
                player->addToPile("modesty", player->handCards(), false, open);
            }
        } else if (triggerEvent == EventPhaseChanging) {
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            DummyCard *dummy = new DummyCard(player->getPile("modesty"));
            CardMoveReason reason(CardMoveReason::S_REASON_EXCHANGE_FROM_PILE, player->objectName(), objectName(), QString());
            room->obtainCard(player, dummy, reason, false);
            delete dummy;
        }
        return false;
    }
};

class LianyingViewAsSkill : public ZeroCardViewAsSkill
{
public:
    LianyingViewAsSkill() : ZeroCardViewAsSkill("lianying")
    {
        response_pattern = "@@lianying";
    }

    virtual const Card *viewAs() const
    {
        return new LianyingCard;
    }
};

class Lianying : public TriggerSkill
{
public:
    Lianying() : TriggerSkill("lianying")
    {
        events << CardsMoveOneTime;
        view_as_skill = new LianyingViewAsSkill;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (!TriggerSkill::triggerable(player) || !player->isKongcheng()) return QStringList();
        QVariantList move_datas = data.toList();
        foreach(QVariant move_data, move_datas) {
            CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
            if (move.from == player && move.from_places.contains(Player::PlaceHand))
                return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *luxun, QVariant &data, ServerPlayer *) const
    {
        QVariantList move_datas = data.toList();
        int count = 0;
        foreach(QVariant move_data, move_datas) {
            CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
            if (move.from != luxun) continue;
            for (int i = 0; i < move.from_places.length(); i++) {
                if (move.from_places[i] == Player::PlaceHand) count++;
            }
        }

        luxun->tag["LianyingMoveData"] = data;

        room->setPlayerMark(luxun, "lianying", count);
        room->askForUseCard(luxun, "@@lianying", "@lianying-card:::" + QString::number(count));

        return false;
    }
};

class Jieyin : public OneCardViewAsSkill
{
public:
    Jieyin() : OneCardViewAsSkill("jieyin")
    {
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("JieyinCard");
    }

    virtual bool viewFilter(const Card *to_select) const
    {
        QString choice = Self->tag["jieyin"].toString();
        if (choice == "discard")
            return !Self->isJilei(to_select) && !to_select->isEquipped();
        else if (choice == "putequip")
            return to_select->getTypeId() == Card::TypeEquip;
        return false;
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        JieyinCard *jieyin_card = new JieyinCard();
        jieyin_card->addSubcard(originalCard->getId());
        jieyin_card->setSkillName(objectName());
        return jieyin_card;
    }

    QString getSelectBox() const
    {
        return "discard+putequip";
    }
};

class Xiaoji : public TriggerSkill
{
public:
    Xiaoji() : TriggerSkill("xiaoji")
    {
        events << CardsMoveOneTime;
        frequency = Frequent;
    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *sunshangxiang, QVariant &data, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(sunshangxiang)) return QStringList();
        QStringList trigger_list;
        QVariantList move_datas = data.toList();
        foreach(QVariant move_data, move_datas) {
            CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
            if (move.from == sunshangxiang && move.from_places.contains(Player::PlaceEquip)) {
                for (int i = 0; i < move.card_ids.size(); i++) {
                    if (move.from_places[i] == Player::PlaceEquip)
                        trigger_list << objectName();
                }
            }
        }
        return trigger_list;
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *sunshangxiang, QVariant &, ServerPlayer *) const
    {
        if (room->askForSkillInvoke(sunshangxiang, objectName())) {
            sunshangxiang->broadcastSkillInvoke(objectName());
            sunshangxiang->drawCards(2, objectName());
        }
        return false;
    }
};

class Wushuang : public TriggerSkill
{
public:
    Wushuang() : TriggerSkill("wushuang")
    {
        events << TargetSpecified << TargetConfirmed;
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        if (triggerEvent == TargetSpecified) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card != NULL && (use.card->isKindOf("Slash") || use.card->isKindOf("Duel"))) {
                ServerPlayer *to = use.to.at(use.index);
                if (to && to->isAlive())
                    return QStringList(objectName());
            }
        } else if (triggerEvent == TargetConfirmed) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card != NULL && (use.card->isKindOf("Duel"))) {
                return QStringList(objectName());
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *, QVariant &data, ServerPlayer *player) const
    {
        if (triggerEvent == TargetSpecified) {
            CardUseStruct use = data.value<CardUseStruct>();
            int index = use.index;
            ServerPlayer *to = use.to.at(index);
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), to->objectName());

            if (use.card->isKindOf("Slash")) {
                QVariantList jink_list = use.card->tag["Jink_List"].toList();
                if (jink_list.at(index).toInt() == 1)
                    jink_list[index] = 2;
                use.card->setTag("Jink_List", jink_list);
            } else if (use.card->isKindOf("Duel")) {
                QVariantList wushuang_list = use.card->tag["Wushuang1_List"].toList();
                wushuang_list[index] = true;
                use.card->setTag("Wushuang1_List", wushuang_list);
            }

        } else if (triggerEvent == TargetConfirmed) {
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            CardUseStruct use = data.value<CardUseStruct>();
            int index = use.index;
            QVariantList wushuang_list = use.card->tag["Wushuang2_List"].toList();
            wushuang_list[index] = true;
            use.card->setTag("Wushuang2_List", wushuang_list);
        }
        return false;
    }
};

class Liyu : public TriggerSkill
{
public:
    Liyu() : TriggerSkill("liyu")
    {
        events << Damage;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (TriggerSkill::triggerable(player)) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.card && damage.card->isKindOf("Slash") && damage.to && damage.to->isAlive()
                    && player->canGetCard(damage.to, "hej")  && !damage.to->hasFlag("Global_DebutFlag"))
                return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        ServerPlayer *target = damage.to;
        if (target && target->isAlive() && player->canGetCard(target, "hej")
                && player->askForSkillInvoke(objectName(), QVariant::fromValue(target))) {
            player->broadcastSkillInvoke(objectName());
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), target->objectName());

            int id = room->askForCardChosen(player, target, "hej", objectName(), false, Card::MethodGet);
            bool is_equip = Sanguosha->getCard(id)->getTypeId() == Card::TypeEquip;
            CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, player->objectName());
            room->obtainCard(player, Sanguosha->getCard(id), reason);
            if (is_equip) {
                Duel *duel = new Duel(Card::NoSuit, 0);
                duel->setSkillName("_liyu");

                if (player->isLocked(duel)) {
                    delete duel;
                    return false;
                }
                QList<ServerPlayer *> targets;
                foreach (ServerPlayer *p, room->getOtherPlayers(player)) {
                    if (p != target && !player->isProhibited(p, duel))
                        targets << p;
                }
                if (targets.isEmpty())
                    delete duel;
                else {
                    ServerPlayer *victim = room->askForPlayerChosen(target, targets, objectName(), "@liyu:" + player->objectName());
                    if (victim) {
                        if (player->isAlive() && victim->isAlive() && !player->isLocked(duel))
                            room->useCard(CardUseStruct(duel, player, victim));
                        else
                            delete duel;
                    }
                }
            } else
                target->drawCards(1, objectName());
        }
        return false;
    }
	
	virtual int getEffectIndex(const ServerPlayer *, const Card *card) const
    {
        if (card->isKindOf("Duel"))
            return 0;
        else
            return -1;
    }
};

class Lijian : public OneCardViewAsSkill
{
public:
    Lijian() : OneCardViewAsSkill("lijian")
    {
        filter_pattern = ".!";
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("LijianCard");
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        LijianCard *lijian_card = new LijianCard;
        lijian_card->addSubcard(originalCard->getId());
        return lijian_card;
    }

    virtual int getEffectIndex(const ServerPlayer *, const Card *card) const
    {
        return card->isKindOf("Duel") ? 0 : -1;
    }
};

class Biyue : public PhaseChangeSkill
{
public:
    Biyue() : PhaseChangeSkill("biyue")
    {
        frequency = Frequent;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Finish;
    }

    virtual bool onPhaseChange(ServerPlayer *diaochan) const
    {
        Room *room = diaochan->getRoom();
        if (room->askForSkillInvoke(diaochan, objectName())) {
            diaochan->broadcastSkillInvoke(objectName());
            diaochan->drawCards(diaochan->isKongcheng()?2:1, objectName());
        }
        return false;
    }
};

class Qingnang : public ZeroCardViewAsSkill
{
public:
    Qingnang() : ZeroCardViewAsSkill("qingnang")
    {
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return player->usedTimes("QingnangCard") <= player->getMark("qingnangTimesIncrease");
    }

    virtual const Card *viewAs() const
    {
        return new QingnangCard;
    }
};

class Jijiu : public OneCardViewAsSkill
{
public:
    Jijiu() : OneCardViewAsSkill("jijiu")
    {
        filter_pattern = ".|red";
        response_or_use = true;
    }

    virtual bool isEnabledAtPlay(const Player *) const
    {
        return false;
    }

    virtual bool isEnabledAtResponse(const Player *player, const QString &pattern) const
    {
        return pattern.contains("peach") && player->getPhase() == Player::NotActive;
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        Peach *peach = new Peach(originalCard->getSuit(), originalCard->getNumber());
        peach->addSubcard(originalCard->getId());
        peach->setSkillName(objectName());
        return peach;
    }
};

class Mashu : public DistanceSkill
{
public:
    Mashu() : DistanceSkill("mashu")
    {
    }

    virtual int getCorrect(const Player *from, const Player *) const
    {
        if (from->hasSkill(this))
            return -1;
        else
            return 0;
    }
};

class Wangzun : public PhaseChangeSkill
{
public:
    Wangzun() : PhaseChangeSkill("wangzun")
    {
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
        Room *room = target->getRoom();
        if (!isNormalGameMode(room->getMode()))
            return false;
        if (target->isLord() && target->getPhase() == Player::Start) {
            ServerPlayer *yuanshu = room->findPlayerBySkillName(objectName());
            if (yuanshu && room->askForSkillInvoke(yuanshu, objectName(), QVariant::fromValue(target))) {
                yuanshu->broadcastSkillInvoke(objectName());
				room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, yuanshu->objectName(), target->objectName());
                yuanshu->drawCards(1, objectName());
                if (target->getMaxCards() > 0)
                    room->setPlayerFlag(target, "WangzunDecMaxCards");
            }
        }
        return false;
    }
};

class WangzunMaxCards : public MaxCardsSkill
{
public:
    WangzunMaxCards() : MaxCardsSkill("#wangzun-maxcard")
    {
    }

    virtual int getExtra(const Player *target) const
    {
        if (target->hasFlag("WangzunDecMaxCards"))
            return -1;
        else
            return 0;
    }
};

class Tongji : public ProhibitSkill
{
public:
    Tongji() : ProhibitSkill("tongji")
    {
    }

    virtual bool isProhibited(const Player *from, const Player *to, const Card *card, const QList<const Player *> &) const
    {
        if (from && card->isKindOf("Slash")) {
            // get rangefix
            int rangefix = 0;
            if (card->isVirtualCard()) {
                if (from->getWeapon() && card->usecontains(from->getWeapon())) {
                    const Weapon *weapon = qobject_cast<const Weapon *>(from->getWeapon()->getRealCard());
                    rangefix += weapon->getRange() - from->getAttackRange(false);
                }

                if (from->getOffensiveHorse() && card->usecontains(from->getOffensiveHorse()))
                    rangefix += 1;
            }
            // find yuanshu
            foreach (const Player *p, from->getAliveSiblings()) {
                if (p->hasSkill(this) && p != to && p->getHandcardNum() > p->getHp()
                    && from->inMyAttackRange(p, rangefix)) {
                    return true;
                }
            }
        }
        return false;
    }
};

class Yaowu : public TriggerSkill
{
public:
    Yaowu() : TriggerSkill("yaowu")
    {
        events << DamageInflicted;
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (TriggerSkill::triggerable(player)) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.card && damage.card->isKindOf("Slash") && (!damage.card->isRed()
                || (damage.from && damage.from->isAlive()))) {
                return QStringList(objectName());
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {

        DamageStruct damage = data.value<DamageStruct>();
        if (damage.card && damage.card->isKindOf("Slash")) {
            if (damage.card->isRed()) {
                if (damage.from && damage.from->isAlive()) {
                    room->sendCompulsoryTriggerLog(player, objectName());
                    player->broadcastSkillInvoke(objectName());
                    if (damage.from->isWounded() && room->askForChoice(damage.from, objectName(), "recover+draw", data) == "recover")
                        room->recover(damage.from, RecoverStruct(damage.to));
                    else
                        damage.from->drawCards(1, objectName());
                }
            } else {
                room->sendCompulsoryTriggerLog(player, objectName());
                player->broadcastSkillInvoke(objectName());
                player->drawCards(1, objectName());
            }
        }
        return false;
    }
};

class Xiaoxi : public TriggerSkill
{
public:
    Xiaoxi() : TriggerSkill("xiaoxi")
    {
        events << Debut;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        ServerPlayer *opponent = player->getNext();
        if (!opponent->isAlive())
            return false;
        Slash *slash = new Slash(Card::NoSuit, 0);
        slash->setSkillName("xiaoxi");
        if (player->isLocked(slash) || !player->canSlash(opponent, slash, false)) {
            delete slash;
            return false;
        }
        if (room->askForSkillInvoke(player, objectName()))
            room->useCard(CardUseStruct(slash, player, opponent));
        return false;
    }
};

void StandardPackage::addGenerals()
{
    // Wei
    General *caocao = new General(this, "caocao$", "wei"); // WEI 001
    caocao->addSkill(new Jianxiong);
    caocao->addSkill(new Hujia);

    General *simayi = new General(this, "simayi", "wei", 3); // WEI 002
    simayi->addSkill(new Fankui);
    simayi->addSkill(new Guicai);

    General *xiahoudun = new General(this, "xiahoudun", "wei"); // WEI 003
    xiahoudun->addSkill(new Ganglie);
    xiahoudun->addSkill(new Qingjian);

    General *zhangliao = new General(this, "zhangliao", "wei"); // WEI 004
    zhangliao->addSkill(new Tuxi);

    General *xuchu = new General(this, "xuchu", "wei"); // WEI 005
    xuchu->addSkill(new Luoyi);

    General *guojia = new General(this, "guojia", "wei", 3); // WEI 006
    guojia->addSkill(new Tiandu);
    guojia->addSkill(new Yiji);

    General *zhenji = new General(this, "zhenji", "wei", 3, false); // WEI 007
    zhenji->addSkill(new Qingguo);
    zhenji->addSkill(new Luoshen);
    zhenji->addSkill(new LuoshenHideCard);
    related_skills.insertMulti("luoshen", "#luoshen-hidecard");

    // Shu
    General *liubei = new General(this, "liubei$", "shu"); // SHU 001
    liubei->addSkill(new Rende);
    liubei->addSkill(new Jijiang);

    General *guanyu = new General(this, "guanyu", "shu"); // SHU 002
    guanyu->addSkill(new Wusheng);
    guanyu->addSkill(new WushengTargetMod);
    guanyu->addSkill(new Yijue);
    related_skills.insertMulti("wusheng", "#wusheng-target");

    General *zhangfei = new General(this, "zhangfei", "shu"); // SHU 003
    zhangfei->addSkill(new Paoxiao);
    zhangfei->addSkill(new Tishen);

    General *zhugeliang = new General(this, "zhugeliang", "shu", 3); // SHU 004
    zhugeliang->addSkill(new Guanxing);
    zhugeliang->addSkill(new Kongcheng);
    zhugeliang->addSkill(new KongchengEffect);
    related_skills.insertMulti("kongcheng", "#kongcheng-effect");

    General *zhaoyun = new General(this, "zhaoyun", "shu"); // SHU 005
    zhaoyun->addSkill(new Longdan);
    zhaoyun->addSkill(new Yajiao);

    General *machao = new General(this, "machao", "shu"); // SHU 006
    machao->addSkill(new Mashu);
    machao->addSkill(new Tieji);

    General *huangyueying = new General(this, "huangyueying", "shu", 3, false); // SHU 007
    huangyueying->addSkill(new Jizhi);
    huangyueying->addSkill(new Qicai);

    // Wu
    General *sunquan = new General(this, "sunquan$", "wu"); // WU 001
    sunquan->addSkill(new Zhiheng);
    sunquan->addSkill(new Jiuyuan);

    General *ganning = new General(this, "ganning", "wu"); // WU 002
    ganning->addSkill(new Qixi);
    ganning->addSkill(new Fenwei);

    General *lvmeng = new General(this, "lvmeng", "wu"); // WU 003
    lvmeng->addSkill(new Keji);
    lvmeng->addSkill(new Qinxue);
    lvmeng->addRelateSkill("gongxin");

    General *huanggai = new General(this, "huanggai", "wu"); // WU 004
    huanggai->addSkill(new Kurou);
    huanggai->addSkill(new Zhaxiang);
    huanggai->addSkill(new ZhaxiangTargetMod);
    related_skills.insertMulti("zhaxiang", "#zhaxiang-target");

    General *zhouyu = new General(this, "zhouyu", "wu", 3); // WU 005
    zhouyu->addSkill(new Yingzi);
    zhouyu->addSkill(new YingziMaxCards);
    zhouyu->addSkill(new Fanjian);
    related_skills.insertMulti("yingzi", "#yingzi");

    General *daqiao = new General(this, "daqiao", "wu", 3, false); // WU 006
    daqiao->addSkill(new Guose);
    daqiao->addSkill(new Liuli);

    General *luxun = new General(this, "luxun", "wu", 3); // WU 007
    luxun->addSkill(new Qianxun);
	luxun->addSkill(new DetachEffectSkill("qianxun", "qianxun"));
	related_skills.insertMulti("qianxun", "#qianxun-clear");
    luxun->addSkill(new Lianying);

    General *sunshangxiang = new General(this, "sunshangxiang", "wu", 3, false); // WU 008
    sunshangxiang->addSkill(new Jieyin);
    sunshangxiang->addSkill(new Xiaoji);

    // Qun
    General *huatuo = new General(this, "huatuo", "qun", 3); // QUN 001
    huatuo->addSkill(new Qingnang);
    huatuo->addSkill(new Jijiu);

    General *lvbu = new General(this, "lvbu", "qun", 5); // QUN 002
    lvbu->addSkill(new Wushuang);
    lvbu->addSkill(new Liyu);

    General *diaochan = new General(this, "diaochan", "qun", 3, false); // QUN 003
    diaochan->addSkill(new Lijian);
    diaochan->addSkill(new Biyue);

    General *huaxiong = new General(this, "huaxiong", "qun", 6); // QUN 019
    huaxiong->addSkill(new Yaowu);

    // for skill cards
    addMetaObject<QingjianCard>();
    addMetaObject<ZhihengCard>();
    addMetaObject<RendeCard>();
    addMetaObject<YijueCard>();
    addMetaObject<JieyinCard>();
    addMetaObject<KurouCard>();
    addMetaObject<LijianCard>();
    addMetaObject<FanjianCard>();
    addMetaObject<QingnangCard>();
    addMetaObject<LiuliCard>();
    addMetaObject<LianyingCard>();
    addMetaObject<JijiangCard>();
    addMetaObject<GuoseCard>();

    skills << new NoDistanceTargetMod << new Xiaoxi << new NonCompulsoryInvalidity << new RendeBasic;
}

class GdJuejing : public TriggerSkill
{
public:
    GdJuejing() : TriggerSkill("gdjuejing")
    {
        events << CardsMoveOneTime;
        frequency = Compulsory;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *gaodayihao, QVariant &data) const
    {
        if (triggerEvent == CardsMoveOneTime) {
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            if (move.from != gaodayihao && move.to != gaodayihao)
                return false;
            if (move.to_place != Player::PlaceHand && !move.from_places.contains(Player::PlaceHand))
                return false;
        }
        if (gaodayihao->getHandcardNum() == 4)
            return false;
        int diff = abs(gaodayihao->getHandcardNum() - 4);
        if (gaodayihao->getHandcardNum() < 4) {
            room->sendCompulsoryTriggerLog(gaodayihao, objectName());
            gaodayihao->drawCards(diff, objectName());
        } else if (gaodayihao->getHandcardNum() > 4) {
            room->sendCompulsoryTriggerLog(gaodayihao, objectName());
            room->askForDiscard(gaodayihao, objectName(), diff, diff);
        }

        return false;
    }
};

class GdJuejingSkipDraw : public DrawCardsSkill
{
public:
    GdJuejingSkipDraw() : DrawCardsSkill("#gdjuejing")
    {
    }

    virtual int getPriority(TriggerEvent) const
    {
        return 1;
    }

    virtual int getDrawNum(ServerPlayer *gaodayihao, int) const
    {
        LogMessage log;
        log.type = "#GdJuejing";
        log.from = gaodayihao;
        log.arg = "gdjuejing";
        gaodayihao->getRoom()->sendLog(log);
        gaodayihao->getRoom()->notifySkillInvoked(gaodayihao, "gdjuejing");

        return 0;
    }
};

class GdLonghun : public OneCardViewAsSkill
{
public:
    GdLonghun() : OneCardViewAsSkill("gdlonghun")
    {
        response_or_use = true;
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        Peach *peach = new Peach(Card::NoSuit, 0);
        return Slash::IsAvailable(player) || peach->isAvailable(player);
    }

    virtual bool isEnabledAtResponse(const Player *, const QString &pattern) const
    {
        return pattern == "slash" || pattern == "jink" || pattern.contains("peach") || pattern == "nullification";
    }

    virtual bool isEnabledAtNullification(const ServerPlayer *player) const
    {
        return !player->isNude() || !player->getHandPile().isEmpty();
    }

    virtual bool viewFilter(const Card *card) const
    {
        if (card->hasFlag("using")) return false;

        switch (Sanguosha->currentRoomState()->getCurrentCardUseReason()) {
        case CardUseStruct::CARD_USE_REASON_PLAY: {
            if (Self->isWounded() && card->getSuit() == Card::Heart)
                return true;
            else if (card->getSuit() == Card::Diamond) {
                FireSlash *slash = new FireSlash(Card::SuitToBeDecided, -1);
                slash->addSubcard(card->getEffectiveId());
                slash->deleteLater();
                return slash->isAvailable(Self);
            } else
                return false;
        }
        case CardUseStruct::CARD_USE_REASON_RESPONSE:
        case CardUseStruct::CARD_USE_REASON_RESPONSE_USE: {
            QString pattern = Sanguosha->currentRoomState()->getCurrentCardUsePattern();
            if (pattern == "jink")
                return card->getSuit() == Card::Club;
            else if (pattern == "nullification")
                return card->getSuit() == Card::Spade;
            else if (pattern == "peach" || pattern == "peach+analeptic")
                return card->getSuit() == Card::Heart;
            else if (pattern == "slash")
                return card->getSuit() == Card::Diamond;
        }
        default:
            break;
        }

        return false;
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        Card *new_card = NULL;

        switch (originalCard->getSuit()) {
        case Card::Spade: {
            new_card = new Nullification(Card::SuitToBeDecided, 0);
            break;
        }
        case Card::Heart: {
            new_card = new Peach(Card::SuitToBeDecided, 0);
            break;
        }
        case Card::Club: {
            new_card = new Jink(Card::SuitToBeDecided, 0);
            break;
        }
        case Card::Diamond: {
            new_card = new FireSlash(Card::SuitToBeDecided, 0);
            break;
        }
        default:
            break;
        }

        if (new_card) {
            new_card->setSkillName(objectName());
            new_card->addSubcard(originalCard);
        }

        return new_card;
    }
};

class GdLonghunDuojian : public TriggerSkill
{
public:
    GdLonghunDuojian() : TriggerSkill("#gdlonghun-duojian")
    {
        events << EventPhaseStart;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *gaodayihao, QVariant &) const
    {
        if (gaodayihao->getPhase() == Player::Start) {
            foreach (ServerPlayer *p, room->getOtherPlayers(gaodayihao)) {
                if (p->getWeapon() && p->getWeapon()->isKindOf("QinggangSword")) {
                    if (room->askForSkillInvoke(gaodayihao, "gdlonghun")) {
                        gaodayihao->broadcastSkillInvoke("gdlonghun", 5);
                        gaodayihao->obtainCard(p->getWeapon());
                    }
                    break;
                }
            }
        }

        return false;
    }
};

TestPackage::TestPackage()
    : Package("test")
{
    // for test only
    General *gaodayihao = new General(this, "gaodayihao", "god", 1, true, true);
    gaodayihao->addSkill(new GdJuejing);
    gaodayihao->addSkill(new GdJuejingSkipDraw);
    gaodayihao->addSkill(new GdLonghun);
    gaodayihao->addSkill(new GdLonghunDuojian);
    related_skills.insertMulti("gdjuejing", "#gdjuejing");
    related_skills.insertMulti("gdlonghun", "#gdlonghun-duojian");

    new General(this, "sujiang", "god", 5, true, true);
    new General(this, "sujiangf", "god", 5, false, true);

    new General(this, "anjiang", "god", 4, true, true, true);
}

ADD_PACKAGE(Test)

