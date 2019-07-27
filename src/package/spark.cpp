#include "spark.h"
#include "client.h"
#include "general.h"
#include "skill.h"
#include "yjcm2013.h"
#include "standard-skillcards.h"
#include "engine.h"
#include "maneuvering.h"

#include "settings.h"
#include "json.h"






class Shenxian : public TriggerSkill
{
public:
    Shenxian() : TriggerSkill("shenxian")
    {
        events << CardsMoveOneTime;
        frequency = Frequent;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (TriggerSkill::triggerable(player) && player->getPhase() == Player::NotActive && !player->hasFlag("shenxianUsed")) {
            QVariantList move_datas = data.toList();
            foreach(QVariant move_data, move_datas) {
                CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
                if (move.from && move.from->objectName() != player->objectName()
                        && (move.reason.m_reason & CardMoveReason::S_MASK_BASIC_REASON) == CardMoveReason::S_REASON_DISCARD) {
                    int i = 0;
                    foreach (int id, move.card_ids) {
                        if (Sanguosha->getCard(id)->getTypeId() == Card::TypeBasic
                                && (move.from_places[i] == Player::PlaceHand || move.from_places[i] == Player::PlaceEquip)) {
                            return QStringList(objectName());
                        }
                        i++;
                    }
                }
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        if (room->askForSkillInvoke(player, objectName(), data)) {
            player->broadcastSkillInvoke(objectName());
            room->setPlayerFlag(player, "shenxianUsed");
            player->drawCards(1, objectName());
        }
        return false;
    }
};

QiangwuCard::QiangwuCard()
{
    target_fixed = true;
}

void QiangwuCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const
{
    JudgeStruct judge;
    judge.pattern = ".";
    judge.who = source;
    judge.reason = "qiangwu";
    judge.play_animation = false;

    for (int i = 1; i <= 13; i++)
        judge.patterns << QString(".|.|%1|.").arg(QString::number(i));

    room->judge(judge);

    QStringList factors = judge.pattern.split('|');
    if (factors.length()>2) {
        QString num_str = factors.at(2);
        bool ok = false;
        int num = num_str.toInt(&ok);
        if (ok)
            room->setPlayerMark(source, "QiangwuNumber", num);
    }
}

class QiangwuViewAsSkill : public ZeroCardViewAsSkill
{
public:
    QiangwuViewAsSkill() : ZeroCardViewAsSkill("qiangwu")
    {
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("QiangwuCard");
    }

    const Card *viewAs() const
    {
        return new QiangwuCard;
    }
};

class Qiangwu : public TriggerSkill
{
public:
    Qiangwu() : TriggerSkill("qiangwu")
    {
        events << EventPhaseChanging << PreCardUsed;
        view_as_skill = new QiangwuViewAsSkill;
    }

    bool triggerable(const ServerPlayer *) const
    {
        return false;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseChanging) {
            if (data.value<PhaseChangeStruct>().from == Player::Play) {
                room->setPlayerMark(player, "QiangwuNumber", 0);
            }
        } else if (triggerEvent == PreCardUsed) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card->isKindOf("Slash") && player->getMark("QiangwuNumber") > 0
                && use.card->getNumber() > player->getMark("QiangwuNumber")) {
                if (use.m_addHistory) {
                    room->addPlayerHistory(player, use.card->getClassName(), -1);
                    use.m_addHistory = false;
                    data = QVariant::fromValue(use);
                }
            }
        }
    }

};

class QiangwuTargetMod : public TargetModSkill
{
public:
    QiangwuTargetMod() : TargetModSkill("#qiangwu-target")
    {
    }

    int getDistanceLimit(const Player *from, const Card *card, const Player *) const
    {
        if (card->getNumber() > 0 && card->getNumber() < from->getMark("QiangwuNumber"))
            return 1000;
        else
            return 0;
    }
};

class Fumian : public TriggerSkill
{
public:
    Fumian() : TriggerSkill("fumian")
    {
        events << EventPhaseStart << DrawNCards << TargetChosed;
    }

    virtual void record(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &) const
    {
        if (triggerEvent == EventPhaseStart && player->getPhase() == Player::NotActive) {
            player->tag["FumianLastChoice"] = player->tag["FumianChoice"];
            player->tag.remove("FumianChoice");
        }
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (triggerEvent == EventPhaseStart && TriggerSkill::triggerable(player) && player->getPhase() == Player::Start)
            return QStringList(objectName());
        else if (triggerEvent == DrawNCards && player->tag["FumianChoice"].toString() == "draw")
            return QStringList("fumian!");
        else if (triggerEvent == TargetChosed && player->tag["FumianChoice"].toString() == "target" && !player->hasFlag("FumianTargetUsed")) {
            CardUseStruct use = data.value<CardUseStruct>();
            if ((use.card->getTypeId() != Card::TypeBasic && !use.card->isNDTrick()) || !use.card->isRed()) return QStringList();
            if (use.card->isKindOf("Collateral") || use.card->isKindOf("BeatAnother")) return QStringList();
            if (use.card->isKindOf("Slash") && use.card->hasFlag("slashDisableExtraTarget")) return QStringList();
            if (!player->getUseExtraTargets(use).isEmpty()) return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        if (triggerEvent == EventPhaseStart) {
            QString lastchoice = player->tag["FumianLastChoice"].toString();
            int i = 1,j = 1;
            if (lastchoice == "draw")
                i++;
            else if (lastchoice == "target")
                j++;
            if (room->askForSkillInvoke(player, "skill_ask", "prompt:::"+objectName())) {
                QString choice = room->askForChoice(player, objectName(), "draw+target+cancel", QVariant(),
                        "@fumian-choose:::" + QString::number(j) + ":" + QString::number(i));
                if (choice != "cancel") {
                    LogMessage log;
                    log.type = "#InvokeSkill";
                    log.from = player;
                    log.arg = objectName();
                    room->sendLog(log);

                    room->notifySkillInvoked(player, objectName());
                    player->broadcastSkillInvoke(objectName());

                    player->tag["FumianChoice"] = choice;
                }
            }
        } else if (triggerEvent == DrawNCards) {
            QString lastchoice = player->tag["FumianLastChoice"].toString();
            LogMessage log;
            log.type = "#SkillForce";
            log.from = player;
            log.arg = objectName();
            room->sendLog(log);
            player->drawCards((lastchoice == "target" ? 2 : 1), "fumian");
        } else if (triggerEvent == TargetChosed) {
            QString lastchoice = player->tag["FumianLastChoice"].toString();
            int x = (lastchoice == "draw" ? 2 : 1);
            CardUseStruct use = data.value<CardUseStruct>();
            QList<ServerPlayer *> to_choosees = player->getUseExtraTargets(use, true);

            QList<ServerPlayer *> choosees = room->askForPlayersChosen(player, to_choosees, "fumian_target", 0, x,
                    "@fumian-add:::" + use.card->objectName() + ":" + QString::number(x));
            if (choosees.length() > 0) {
                LogMessage log;
                log.type = "#AddCardTarget";
                log.from = player;
                log.to = choosees;
                log.card_str = use.card->toString();
                log.arg = objectName();
                room->sendLog(log);
                foreach (ServerPlayer *target, choosees) {
                    room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), target->objectName());
                    use.to.append(target);
                }
                room->sortByActionOrder(use.to);
                data = QVariant::fromValue(use);
            }
        }
        return false;
    }
};

class Daiyan : public PhaseChangeSkill
{
public:
    Daiyan() : PhaseChangeSkill("daiyan")
    {

    }

    virtual void record(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &) const
    {
        if (triggerEvent == EventPhaseStart && player->getPhase() == Player::NotActive) {
            player->tag["DaiyanLastTarget"] = player->tag["DaiyanTarget"];
            player->tag.remove("DaiyanTarget");
        }
    }

    bool triggerable(const ServerPlayer *player) const
    {
        return player->getPhase() == Player::Finish && PhaseChangeSkill::triggerable(player);
    }

    virtual bool onPhaseChange(ServerPlayer *player) const
    {
        Room *room = player->getRoom();
        ServerPlayer *last = player->tag["DaiyanLastTarget"].value<ServerPlayer *>();
        if (last)
            room->addPlayerTip(last, "#daiyan");
        ServerPlayer *target = room->askForPlayerChosen(player, room->getOtherPlayers(player), objectName(), "@daiyan-target", true, true);
        if (last)
            room->removePlayerTip(last, "#daiyan");
        if (target) {
            player->broadcastSkillInvoke(objectName());
            player->tag["DaiyanTarget"] = QVariant::fromValue(target);
            QList<int> peachs;
            foreach (int card_id, room->getDrawPile())
                if (Sanguosha->getCard(card_id)->getTypeId() == Card::TypeBasic && Sanguosha->getCard(card_id)->getSuit() == Card::Heart)
                    peachs << card_id;
            if (peachs.isEmpty()){
                LogMessage log;
                log.type = "$SearchFailed";
                log.from = player;
                log.arg = "heartbasic";
                room->sendLog(log);
            } else {
                int index = qrand() % peachs.length();
                int id = peachs.at(index);
                target->obtainCard(Sanguosha->getCard(id), true);
            }
            if (last && last == target)
                room->loseHp(target);
        }
        return false;
    }
};

KuangbiCard::KuangbiCard()
{
}

bool KuangbiCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && !to_select->isNude() && to_select != Self;
}

void KuangbiCard::onEffect(const CardEffectStruct &effect) const
{
    if (effect.to->isNude()) return;
    const Card *card = effect.from->getRoom()->askForExchange(effect.to, "kuangbi", 3, 1, true, "@kuangbi-put:" + effect.from->objectName(), false);
    effect.from->addToPile("wrong", card->getSubcards(), false);
    effect.from->tag["KuangbiTarget"] = QVariant::fromValue(effect.to);
}

class KuangbiViewAsSkill : public ZeroCardViewAsSkill
{
public:
    KuangbiViewAsSkill() : ZeroCardViewAsSkill("kuangbi")
    {
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("KuangbiCard") && player->getPile("wrong").isEmpty();
    }

    virtual const Card *viewAs() const
    {
        return new KuangbiCard;
    }
};

class Kuangbi : public PhaseChangeSkill
{
public:
    Kuangbi() : PhaseChangeSkill("kuangbi")
    {
        view_as_skill = new KuangbiViewAsSkill;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &, ServerPlayer * &) const
    {
        if (player && player->isAlive() && !player->getPile("wrong").isEmpty() && player->getPhase() == Player::Start)
            return QStringList("kuangbi!");
        return QStringList();
    }

    virtual bool onPhaseChange(ServerPlayer *sundeng) const
    {
        Room *room = sundeng->getRoom();
        LogMessage log;
        log.type = "#SkillEffected";
        log.from = sundeng;
        log.arg = objectName();
        room->sendLog(log);
        int n = sundeng->getPile("wrong").length();
        DummyCard *dummy = new DummyCard(sundeng->getPile("wrong"));
        CardMoveReason reason(CardMoveReason::S_REASON_EXCHANGE_FROM_PILE, sundeng->objectName(), "wrong", QString());
        room->obtainCard(sundeng, dummy, reason, false);
        delete dummy;
        ServerPlayer *target = sundeng->tag["KuangbiTarget"].value<ServerPlayer *>();
        sundeng->tag.remove("KuangbiTarget");
        if (target)
            target->drawCards(n, objectName());

        return false;
    }
};









class Hongde : public TriggerSkill
{
public:
    Hongde() : TriggerSkill("hongde")
    {
        events << CardsMoveOneTime;
    }

    virtual QStringList triggerable(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(player) || room->getTag("FirstRound").toBool()) return QStringList();
        int get_num = 0, lose_num = 0;
        QVariantList move_datas = data.toList();
        foreach(QVariant move_data, move_datas) {
            CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
            if (move.to == player && move.to_place == Player::PlaceHand) {
                get_num = get_num+move.card_ids.size();
            } else if (move.from == player && !(move.to == player && (move.to_place == Player::PlaceHand || move.to_place == Player::PlaceEquip))) {
                for (int i = 0; i < move.card_ids.length(); i++) {
                    if (move.from_places[i] == Player::PlaceHand || move.from_places[i] == Player::PlaceEquip) {
                        lose_num ++;
                    }
                }
            }
        }
        if (get_num > 1 || lose_num > 1)
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        ServerPlayer *target = room->askForPlayerChosen(player, room->getOtherPlayers(player), objectName(), "@hongde-invoke", true, true);
        if (target) {
            player->broadcastSkillInvoke(objectName());
            target->drawCards(1, objectName());
        }
        return false;
    }
};

DingpanCard::DingpanCard()
{
}

bool DingpanCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *) const
{
    return targets.isEmpty() && to_select->hasEquip();
}

void DingpanCard::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *source = effect.from;
    ServerPlayer *target = effect.to;
    Room *room = source->getRoom();
    target->drawCards(1, "dingpan");
    if (!target->hasEquip()) return;
    QStringList choices;
    foreach (const Card *card, target->getEquips()) {
        if (source->canDiscard(target, card->getEffectiveId())) {
            choices << "disequip";
            break;
        }
    }
    choices << "takeback";
    if (room->askForChoice(target, "dingpan", choices.join("+"), QVariant::fromValue(source), "@dingpan-choose:"+source->objectName(), "disequip+takeback") == "disequip")
        room->throwCard(room->askForCardChosen(source, target, "e", "dingpan", false, Card::MethodDiscard), target, source);
    else {
        QList<const Card *> equips = target->getEquips();
        if (equips.isEmpty()) return;
        DummyCard *card = new DummyCard;
        foreach (const Card *equip, equips) {
            card->addSubcard(equip);
        }
        if (card->subcardsLength() > 0)
            target->obtainCard(card);
        room->damage(DamageStruct("dingpan", source, target));
    }
}

class DingpanViewAsSkill : public ZeroCardViewAsSkill
{
public:
    DingpanViewAsSkill() : ZeroCardViewAsSkill("dingpan")
    {
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return player->getMark("dingpan") > player->usedTimes("DingpanCard");
    }

    virtual const Card *viewAs() const
    {
        return new DingpanCard;
    }
};

class Dingpan : public TriggerSkill
{
public:
    Dingpan() : TriggerSkill("dingpan")
    {
        events << EventPhaseChanging << PlayCard;
        view_as_skill = new DingpanViewAsSkill;
    }

    bool triggerable(const ServerPlayer *) const
    {
        return false;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseChanging) {
            if (data.value<PhaseChangeStruct>().from == Player::Play)
                room->setPlayerMark(player, "dingpan", 0);
        } else if (triggerEvent == PlayCard && TriggerSkill::triggerable(player)) {
            int x = 0;
            foreach (ServerPlayer *p, room->getAlivePlayers()) {
                if (p->getRole() == "rebel")
                    x++;
            }
            room->setPlayerMark(player, "dingpan", x);
        };
    }
};

class Kangkai : public TriggerSkill
{
public:
    Kangkai() : TriggerSkill("kangkai")
    {
        events << TargetConfirmed;
    }

    virtual TriggerList triggerable(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data) const
    {
        TriggerList skill_list;
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card->isKindOf("Slash")) {
            QList<ServerPlayer *> caoangs = room->findPlayersBySkillName(objectName());
            foreach (ServerPlayer *caoang, caoangs) {
                if (caoang->distanceTo(player) < 2)
                skill_list.insert(caoang, QStringList(objectName()));
            }
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *to, QVariant &data, ServerPlayer *player) const
    {
        player->tag["KangkaiSlash"] = data;
        bool will_use = room->askForSkillInvoke(player, objectName(), QVariant::fromValue(to));
        player->tag.remove("KangkaiSlash");
        if (!will_use) return false;

        player->broadcastSkillInvoke(objectName());
        room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), to->objectName());

        player->drawCards(1, "kangkai");
        if (!player->isNude() && player != to) {
            const Card *card = NULL;
            if (player->getCardCount() > 1) {
                card = room->askForCard(player, "..!", "@kangkai-give:" + to->objectName(), data, Card::MethodNone);
                if (!card)
                    card = player->getCards("he").first();
            } else {
                Q_ASSERT(player->getCardCount() == 1);
                card = player->getCards("he").first();
            }
            CardMoveReason r(CardMoveReason::S_REASON_GIVE, player->objectName(), objectName(), QString());
            room->obtainCard(to, card, r);
            if (card->getTypeId() == Card::TypeEquip && to->handCards().contains(card->getEffectiveId()) && card->isAvailable(to)) {
                to->tag["KangkaiSlash"] = data;
                to->tag["KangkaiGivenCard"] = QVariant::fromValue(card);
                bool use_equip = room->askForChoice(to, "kangkai_use", "yes+no", data, "@kangkai-use:::" + card->objectName()) == "yes";
                to->tag.remove("KangkaiSlash");
                to->tag.remove("KangkaiGivenCard");
                if (use_equip)
                    room->useCard(CardUseStruct(card, to, to));
            }
        }
        return false;
    }
};

class Gongao : public TriggerSkill
{
public:
    Gongao() : TriggerSkill("gongao")
    {
        events << DeathAfter;
        frequency = Compulsory;
    }

    virtual TriggerList triggerable(TriggerEvent , Room *room, ServerPlayer *, QVariant &) const
    {
        TriggerList skill_list;
        QList<ServerPlayer *> zhugedans = room->findPlayersBySkillName(objectName());
        foreach (ServerPlayer *zhugedan, zhugedans)
            skill_list.insert(zhugedan, QStringList(objectName()));
        return skill_list;
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *, QVariant &, ServerPlayer *zhugedan) const
    {
        zhugedan->broadcastSkillInvoke(objectName());
        room->sendCompulsoryTriggerLog(zhugedan, objectName());

        LogMessage log;
        log.type = "#GainMaxHp";
        log.from = zhugedan;
        log.arg = "1";
        log.arg2 = QString::number(zhugedan->getMaxHp() + 1);
        room->sendLog(log);

        room->setPlayerProperty(zhugedan, "maxhp", zhugedan->getMaxHp() + 1);

        if (zhugedan->isWounded())
            room->recover(zhugedan, RecoverStruct(zhugedan));

        return false;
    }
};

class Juyi : public PhaseChangeSkill
{
public:
    Juyi() : PhaseChangeSkill("juyi")
    {
        frequency = Wake;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && PhaseChangeSkill::triggerable(target)
            && target->getPhase() == Player::Start
            && target->getMark("juyi") == 0
            && target->isWounded()
            && target->getMaxHp() > target->aliveCount();
    }

    bool onPhaseChange(ServerPlayer *zhugedan) const
    {
        Room *room = zhugedan->getRoom();

        zhugedan->broadcastSkillInvoke(objectName());
        room->sendCompulsoryTriggerLog(zhugedan, objectName());

        room->setPlayerMark(zhugedan, "juyi", 1);
        room->addPlayerMark(zhugedan, "@waked");
        int diff = zhugedan->getHandcardNum() - zhugedan->getMaxHp();
        if (diff < 0)
            room->drawCards(zhugedan, -diff, objectName());
        if (zhugedan->getMark("juyi") == 1)
            room->handleAcquireDetachSkills(zhugedan, "benghuai|weizhong");

        return false;
    }
};

class Weizhong : public TriggerSkill
{
public:
    Weizhong() : TriggerSkill("weizhong")
    {
        events << MaxHpChanged;
        frequency = Compulsory;
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        player->broadcastSkillInvoke(objectName());
        room->sendCompulsoryTriggerLog(player, objectName());
        player->drawCards(1, objectName());
        return false;
    }
};




class TianmingViewAsSkill : public ViewAsSkill
{
public:
    TianmingViewAsSkill() : ViewAsSkill("tianming")
    {
        response_pattern = "@@tianming";
    }

    bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
        return !Self->isJilei(to_select) && selected.length() < Self->getMark("tianming_count");
    }

    const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.length() != Self->getMark("tianming_count"))
            return NULL;

        DummyCard *xt = new DummyCard;
        xt->addSubcards(cards);
        return xt;
    }
};

class Tianming : public TriggerSkill
{
public:
    Tianming() : TriggerSkill("tianming")
    {
        events << TargetConfirmed;
        view_as_skill = new TianmingViewAsSkill;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card->isKindOf("Slash")) {
            return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        int i = qMin(2, player->forceToDiscard(2, true).length());
        bool invoke = false;
        if (i == 0) {
            if (room->askForSkillInvoke(player, objectName(), "prompt")) {
                invoke = true;
                player->broadcastSkillInvoke(objectName());
            }
        } else {
            room->setPlayerMark(player, "tianming_count", i);
            invoke = room->askForCard(player, "@@tianming", "@tianming-discard:::" + QString::number(i), data, objectName());
            room->setPlayerMark(player, "tianming_count", 0);
        }
        if (invoke){
            player->drawCards(2, objectName());
            int max = -1000;
            foreach (ServerPlayer *p, room->getAllPlayers()) {
                if (p->getHp() > max)
                    max = p->getHp();
            }
            if (player->getHp() == max)
                return false;

            QList<ServerPlayer *> maxs;
            foreach (ServerPlayer *p, room->getAllPlayers()) {
                if (p->getHp() == max)
                    maxs << p;
                if (maxs.size() > 1)
                    return false;
            }
            ServerPlayer *mosthp = maxs.first();
            int j = qMin(2, mosthp->forceToDiscard(2, true).length());
            if ((j == 0 && room->askForSkillInvoke(mosthp, "tianming_draw", "prompt")) || (j > 0 &&
                room->askForDiscard(mosthp, objectName(), j, j, true, true, "@tianming-discard:::" + QString::number(j))))
                mosthp->drawCards(2, objectName());
        }
        return false;
    }
};

MizhaoCard::MizhaoCard()
{
}

bool MizhaoCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select != Self;
}

void MizhaoCard::onEffect(const CardEffectStruct &effect) const
{
    DummyCard *handcards = effect.from->wholeHandCards();
    handcards->deleteLater();
    CardMoveReason r(CardMoveReason::S_REASON_GIVE, effect.from->objectName());
    Room *room = effect.from->getRoom();
    room->obtainCard(effect.to, handcards, r, false);
    if (effect.to->isKongcheng()) return;

    QList<ServerPlayer *> targets;
    foreach (ServerPlayer *p, room->getOtherPlayers(effect.to)) {
        if (effect.to->canPindian(p) && p != effect.from)
            targets << p;
    }

    if (!targets.isEmpty()) {
        ServerPlayer *target = room->askForPlayerChosen(effect.from, targets, "mizhao", "@mizhao-pindian:" + effect.to->objectName());
        PindianStruct *pd = effect.to->pindianStruct(target, "mizhao", NULL);
        int x1 = pd->from_number,x2 = pd->to_number;

        ServerPlayer *winner = NULL;
        ServerPlayer *loser = NULL;
        if (x1 > x2) {
            winner = effect.to;
            loser = target;
        } else if (x1 > x2) {
            winner = target;
            loser = effect.to;
        }
        if (winner && loser && winner->canSlash(loser, NULL, false)) {
            Slash *slash = new Slash(Card::NoSuit, 0);
            slash->setSkillName("_mizhao");
            room->useCard(CardUseStruct(slash, winner, loser));
        }

    }
}

class Mizhao : public ZeroCardViewAsSkill
{
public:
    Mizhao() : ZeroCardViewAsSkill("mizhao")
    {
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->isKongcheng() && !player->hasUsed("MizhaoCard");
    }

    const Card *viewAs() const
    {
        return new MizhaoCard;
    }
};

class ShouxiViewAsSkill : public OneCardViewAsSkill
{
public:
    ShouxiViewAsSkill() : OneCardViewAsSkill("shouxi")
    {
        response_pattern = "@@shouxi";
        guhuo_type = "sbtd";
    }

    bool viewFilter(const Card *to_select) const
    {
        QString classname = to_select->getClassName();
        if (to_select->isKindOf("Slash"))
            classname = "Slash";
        return Self->getMark("Shouxi_" + classname) == 0 && to_select->isVirtualCard();
    }

    const Card *viewAs(const Card *originalCard) const
    {
        return Sanguosha->cloneCard(originalCard->objectName());
    }
};

class Shouxi : public TriggerSkill
{
public:
    Shouxi() : TriggerSkill("shouxi")
    {
        events << TargetConfirmed;
        view_as_skill = new ShouxiViewAsSkill;
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

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *caojie, QVariant &data, ServerPlayer *) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        if (!room->askForSkillInvoke(caojie, "skill_ask", "prompt:::"+objectName())) return false;
        const Card *declare = room->askForCard(caojie, "@@shouxi", "@shouxi:" + use.from->objectName(), QVariant(), Card::MethodNone);
        if (!declare) return false;
        LogMessage log;
        log.type = "#InvokeSkill";
        log.from = caojie;
        log.arg = objectName();
        room->sendLog(log);

        room->notifySkillInvoked(caojie, objectName());
        caojie->broadcastSkillInvoke(objectName());
        room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, caojie->objectName(), use.from->objectName());

        QString classname;
        if (declare->isKindOf("Slash"))
            classname = "Slash";
        else
            classname = declare->getClassName();

        room->setPlayerMark(caojie, "Shouxi_" + classname, 1);

        QVariant dataforai = QVariant::fromValue(caojie);
        if (room->askForCard(use.from, declare->getClassName(), "@shouxi-discard:"+caojie->objectName()+"::"+declare->objectName(), dataforai)) {
            if (!caojie->isNude() && use.from->isAlive() && caojie->isAlive()) {
                int card_id = room->askForCardChosen(use.from, caojie, "he", objectName());
                CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, use.from->objectName());
                room->obtainCard(use.from, Sanguosha->getCard(card_id), reason, false);
            }
        } else {
            int index = use.index;
            QVariantList nullified_list = use.card->tag["Nullified_List"].toList();
            nullified_list[index] = true;
            use.card->setTag("Nullified_List", nullified_list);
        }
        return false;
    }
};

HuiminGraceCard::HuiminGraceCard()
{
    will_throw = false;
    mute = true;
    handling_method = Card::MethodNone;
}

bool HuiminGraceCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && Self->property("huimin_targets").toString().split("+").contains(to_select->objectName());
}

void HuiminGraceCard::use(Room *room, ServerPlayer *caojie, QList<ServerPlayer *> &targets) const
{
    ServerPlayer *first = targets.first();
    QStringList target_names = caojie->property("huimin_targets").toString().split("+");

    CardMoveReason reason(CardMoveReason::S_REASON_SHOW, caojie->objectName(), "huimin", QString());
    room->moveCardTo(this, NULL, Player::PlaceTable, reason, true);

    QList<int> card_ids = getSubcards();
    room->fillAG(card_ids);

    bool start = false;
    QList<ServerPlayer *> huimin_targets;
    foreach (ServerPlayer *p, room->getAlivePlayers()) {
        if (target_names.contains(p->objectName())) {
            if (p == first || start) {
                start = true;
                huimin_targets << p;
            }
        }
    }
    foreach (ServerPlayer *p, room->getAlivePlayers()) {
        if (target_names.contains(p->objectName())) {
            if (p == first) break;
            huimin_targets << p;
        }
    }

    foreach (ServerPlayer *p, huimin_targets) {
        if (card_ids.isEmpty()) break;
        int card_id = room->askForAG(p, card_ids, false, "huimin");
        card_ids.removeOne(card_id);
        room->takeAG(p, card_id);
    }
    room->clearAG();
    if (!card_ids.isEmpty()) {
        DummyCard *dummy = new DummyCard(card_ids);
        CardMoveReason reason(CardMoveReason::S_REASON_NATURAL_ENTER, QString(), "huimin", QString());
        room->throwCard(dummy, reason, NULL);
        delete dummy;
    }
}

class HuiminGrace : public ViewAsSkill
{
public:
    HuiminGrace() : ViewAsSkill("huimingrace")
    {
        response_pattern = "@@huimingrace!";
    }

    virtual bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
        if (to_select->isEquipped())
            return false;

        QStringList targets = Self->property("huimin_targets").toString().split("+");
        return selected.length() < qMin(targets.length(), Self->getHandcardNum());
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const
    {
        QStringList targets = Self->property("huimin_targets").toString().split("+");
        if (cards.length() != qMin(targets.length(), Self->getHandcardNum()))
            return NULL;

        HuiminGraceCard *card = new HuiminGraceCard;
        card->addSubcards(cards);
        return card;
    }
};

class Huimin : public PhaseChangeSkill
{
public:
    Huimin() : PhaseChangeSkill("huimin")
    {

    }

    bool triggerable(const ServerPlayer *caojie) const
    {
        Room *room = caojie->getRoom();
        if (caojie->getPhase() == Player::Finish && PhaseChangeSkill::triggerable(caojie)) {
            foreach (ServerPlayer *p, room->getAlivePlayers()) {
                if (p->getHandcardNum() < p->getHp()) {
                    return true;
                }
            }
        }
        return false;
    }

    virtual bool onPhaseChange(ServerPlayer *caojie) const
    {
        if (caojie->getPhase() == Player::Finish) {
            Room *room = caojie->getRoom();
            QList<ServerPlayer *> targets;
            QStringList target_names;
            foreach (ServerPlayer *p, room->getAlivePlayers()) {
                if (p->getHandcardNum() < p->getHp()) {
                    targets << p;
                    target_names << p->objectName();
                }
            }
            if (targets.isEmpty()) return false;
            if (room->askForSkillInvoke(caojie, objectName(), "prompt:::"+QString::number(targets.length()))) {
                caojie->broadcastSkillInvoke(objectName());
                foreach (ServerPlayer *p, targets)
                    room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, caojie->objectName(), p->objectName());
                caojie->drawCards(targets.length(), objectName());
                int n = qMin(targets.length(), caojie->getHandcardNum());
                if (n < 1) return false;
                room->setPlayerProperty(caojie, "huimin_targets", target_names.join("+"));
                if (!room->askForUseCard(caojie, "@@huimingrace!", "@huimin:::"+QString::number(n), QVariant(), Card::MethodNone)) {
                    QList<int> to_give = caojie->handCards().mid(0, n);
                    HuiminGraceCard *huimin_card = new HuiminGraceCard;
                    huimin_card->addSubcards(to_give);
                    QList<ServerPlayer *> s_targets;
                    s_targets << targets.first();
                    huimin_card->use(room, caojie, s_targets);
                    delete huimin_card;
                }
                room->setPlayerProperty(caojie, "huimin_targets", QVariant());
            }
        }
        return false;
    }
};

class Bingzheng : public TriggerSkill
{
public:
    Bingzheng() : TriggerSkill("bingzheng")
    {
        events << EventPhaseEnd;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        if (TriggerSkill::triggerable(target) && target->getPhase() == Player::Play) {
            QList<ServerPlayer *> targets = target->getRoom()->getAlivePlayers();
            foreach (ServerPlayer *p, targets) {
                if (p->getHandcardNum() != p->getHp())
                    return true;
            }
        }
        return false;
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        QList<ServerPlayer *> targets;
        foreach (ServerPlayer *p, room->getAlivePlayers()) {
            if (p->getHandcardNum() != p->getHp())
                targets << p;
        }

        ServerPlayer *target = room->askForPlayerChosen(player, targets, objectName(), "@bingzheng-invoke", true, true);
        if (target) {
            player->broadcastSkillInvoke(objectName());
            QStringList choices;
            choices << "draw";
            if (!target->isKongcheng())
                choices << "discard";
            QString choice = room->askForChoice(player, objectName(), choices.join("+"), QVariant(), "@bingzheng-choice:"+target->objectName(), "draw+discard");
            if (choice == "draw")
                target->drawCards(1, objectName());
            else if (target->canDiscard(target, "h"))
                room->askForDiscard(target, objectName(), 1, 1);
            if (target->getHandcardNum() == target->getHp()) {
                player->drawCards(1, objectName());
                if (player == target) return false;
                const Card *card = room->askForExchange(player, objectName(), 1, 1, true, "@bingzheng-give:"+target->objectName(), true);
                if (card) {
                    CardMoveReason r(CardMoveReason::S_REASON_GIVE, player->objectName(), target->objectName(), objectName(), QString());
                    room->obtainCard(target, card, r, false);
                }
            }
        }
        return false;
    }
};

class Sheyan : public TriggerSkill
{
public:
    Sheyan() : TriggerSkill("sheyan")
    {
        events << TargetConfirming;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        if (!TriggerSkill::triggerable(player)) return QStringList();
        if (use.card->isNDTrick() && !use.card->isKindOf("Collateral") && !use.card->isKindOf("BeatAnother")) {
            if (!use.from->getUseExtraTargets(use).isEmpty()) return QStringList(objectName());
            foreach (ServerPlayer *p, use.to) {
                if (p->isAlive() && p != player)
                    return QStringList(objectName());
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        QList<ServerPlayer *> targets = use.from->getUseExtraTargets(use);
        foreach (ServerPlayer *p, use.to) {
            if (p->isAlive() && p != player) {
                targets << use.to;
                break;
            }
        }
        ServerPlayer *target = room->askForPlayerChosen(player, targets, objectName(), "@sheyan-invoke:::" + use.card->objectName(), true, true);
        if (target) {
            player->broadcastSkillInvoke(objectName());
            if (use.to.contains(target))
                use.nullified_list << target->objectName();
            else {
                use.to.append(target);
                room->sortByActionOrder(use.to);
            }
            data = QVariant::fromValue(use);
        }
        return false;
    }
};

class Hongyuan : public TriggerSkill
{
public:
    Hongyuan() : TriggerSkill("hongyuan")
    {
        events << DrawNCards << EventPhaseEnd << EventPhaseChanging;
    }

    virtual void record(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseChanging && data.value<PhaseChangeStruct>().from == Player::Draw) {
            player->tag.remove("hongyuan_invoke");
        }
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &, ServerPlayer * &) const
    {
        if (triggerEvent == DrawNCards && TriggerSkill::triggerable(player)) return QStringList(objectName());
        else if (triggerEvent == EventPhaseEnd && player->isAlive() && player->getPhase() == Player::Draw) {
            QList<ServerPlayer *> targets = player->tag["hongyuan_invoke"].value<QList<ServerPlayer *> >();
            if (!targets.isEmpty())
                return QStringList("hongyuan!");
        }

        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        if (triggerEvent == DrawNCards) {
            QList<ServerPlayer *> choosees = room->askForPlayersChosen(player, room->getOtherPlayers(player), objectName(), 0, 2, "@hongyuan", true);
            if (choosees.length() > 0) {
                player->broadcastSkillInvoke(objectName());
                player->tag["hongyuan_invoke"] = QVariant::fromValue(choosees);
                data = data.toInt()-1;
            }
        } else if (triggerEvent == EventPhaseEnd) {
            QList<ServerPlayer *> targets = player->tag["hongyuan_invoke"].value<QList<ServerPlayer *> >();
            room->sortByActionOrder(targets);
            foreach (ServerPlayer *p, targets) {
                if (p->isAlive())
                    p->drawCards(1, objectName());
            }
        }
        return false;
    }
};

class Huanshi : public TriggerSkill
{
public:
    Huanshi() : TriggerSkill("huanshi")
    {
        events << AskForRetrial;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (TriggerSkill::triggerable(player) && !player->isKongcheng()) {
            JudgeStruct *judge = data.value<JudgeStruct *>();
            if (judge->who->isAlive())
                return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        JudgeStruct *judge = data.value<JudgeStruct *>();
        if (player->askForSkillInvoke(this, data)) {
            player->broadcastSkillInvoke(objectName());
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), judge->who->objectName());
            if (judge->who != player) {
                LogMessage log;
                log.type = "$ViewAllCards";
                log.from = judge->who;
                log.to << player;
                log.card_str = IntList2StringList(player->handCards()).join("+");
                room->sendLog(log, judge->who);
            }
            judge->who->tag["HuanshiJudge"] = data;
            int card_id = room->askForCardChosen(judge->who, player, "he", objectName(), true, Card::MethodNone);
            judge->who->tag.remove("HuanshiJudge");
            const Card *card = Sanguosha->getCard(card_id);
            if (!player->isCardLimited(card, Card::MethodResponse))
                room->retrial(card, player, judge, objectName());
        }
        return false;
    }
};

class Mingzhe : public TriggerSkill
{
public:
    Mingzhe() : TriggerSkill("mingzhe")
    {
        events << CardsMoveOneTime << CardUsed << CardResponded;
        frequency = Frequent;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(player) || player->getPhase() != Player::NotActive) return QStringList();
        if (triggerEvent == CardUsed || triggerEvent == CardResponded) {
            const Card *cardstar = NULL;
            if (triggerEvent == CardUsed) {
                CardUseStruct use = data.value<CardUseStruct>();
                cardstar = use.card;
            } else {
                CardResponseStruct resp = data.value<CardResponseStruct>();
                cardstar = resp.m_card;
            }
            if (cardstar && cardstar->getTypeId() != Card::TypeSkill && cardstar->isRed())
                return QStringList(objectName());
        } else if (triggerEvent == CardsMoveOneTime) {
            QStringList trigger_list;
            QVariantList move_datas = data.toList();
            foreach(QVariant move_data, move_datas) {
                CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
                if (move.from == player && (move.reason.m_reason & CardMoveReason::S_MASK_BASIC_REASON) == CardMoveReason::S_REASON_DISCARD) {
                    int i = 0;

                    foreach (QString card_str, move.cards) {

                        const Card *card = Card::Parse(card_str);
                        if (card && card->isRed() && (move.from_places[i] == Player::PlaceHand || move.from_places[i] == Player::PlaceEquip)) {
                            trigger_list << objectName();
                        }
                        i++;
                    }

                }
            }
            return trigger_list;
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

class Guanchao : public TriggerSkill
{
public:
    Guanchao() : TriggerSkill("guanchao")
    {
        events << EventPhaseStart << CardUsed << CardResponded << EventPhaseChanging;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseChanging && data.value<PhaseChangeStruct>().from == Player::Play) {
            room->setPlayerMark(player, "guanchao_increase", 0);
            room->setPlayerMark(player, "guanchao_decrease", 0);
            room->setPlayerMark(player, "#guanchao_increase", 0);
            room->setPlayerMark(player, "#guanchao_decrease", 0);
            room->setPlayerMark(player, "#guanchao", 0);
        }
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (triggerEvent == EventPhaseStart) {
            if (TriggerSkill::triggerable(player) && player->getPhase() == Player::Play)
                return QStringList(objectName());
        } else {
            const Card *card = NULL;
            if (triggerEvent == CardUsed)
                card = data.value<CardUseStruct>().card;
            else if (triggerEvent == CardResponded) {
                CardResponseStruct response = data.value<CardResponseStruct>();
                if (response.m_isUse)
                    card = response.m_card;
            }
            if (card && card->getTypeId() != Card::TypeSkill && card->getHandlingMethod() == Card::MethodUse
                    &&(player->getMark("guanchao_increase") > 0 ||  player->getMark("guanchao_decrease") > 0)) {
                QVariantList card_list = player->tag["PhaseUsedCards"].toList();

                int x = 0, num = 0;
                bool is_increase = false;

                foreach (QVariant card_data, card_list) {
                    const Card *card = card_data.value<const Card *>();
                    if (card) {
                        int n = card->getNumber();
                        if (n == 0 || (x > 0 && n == num)) return QStringList();
                        if (x == 1) is_increase = (n > num);
                        if (x > 1 && is_increase != (n > num)) return QStringList();
                        num = n;
                        x++;
                    }
                }
                if (player->getMark(is_increase?"guanchao_increase":"guanchao_decrease") > 0 && x > 1)
                    return QStringList("guanchao!");
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        if (triggerEvent == EventPhaseStart) {
            QString choice = room->askForChoice(player, objectName(), "increase+decrease+cancel", QVariant(), "@guanchao-choose");
            if (choice != "cancel") {
                LogMessage log;
                log.type = "#InvokeSkill";
                log.arg = objectName();
                log.from = player;
                room->sendLog(log);
                room->notifySkillInvoked(player, objectName());
                player->broadcastSkillInvoke(objectName());

                room->addPlayerTip(player, "#guanchao_"+choice);
                room->addPlayerMark(player, "guanchao_"+choice);
            }
        } else
            player->drawCards(1, objectName());
        return false;
    }
};

class Xunxian : public TriggerSkill
{
public:
    Xunxian() : TriggerSkill("xunxian")
    {
        events << CardFinished << CardsMoveOneTime;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (TriggerSkill::triggerable(player) && player->getPhase() == Player::NotActive && !player->hasFlag("XunxianUsed")) {
            if (triggerEvent == CardFinished) {
                CardUseStruct use = data.value<CardUseStruct>();
                if (use.card && room->isAllOnPlace(use.card, Player::PlaceTable)) {
                    QList<ServerPlayer *> all_players = room->getAlivePlayers();
                    foreach (ServerPlayer *p, all_players) {
                        if (p->getHandcardNum() > player->getHandcardNum())
                            return QStringList(objectName());
                    }
                }
            } else if (triggerEvent == CardsMoveOneTime) {
                QVariantList move_datas = data.toList();
                if (move_datas.size() != 1) return QStringList();
                CardsMoveOneTimeStruct move = move_datas.first().value<CardsMoveOneTimeStruct>();
                if (move.to_place == Player::DiscardPile && move.reason.m_playerId == player->objectName() &&
                        (move.reason.m_reason & CardMoveReason::S_MASK_BASIC_REASON) == CardMoveReason::S_REASON_RESPONSE) {
                    QVariant r_data = move.reason.m_extraData;
                    if (r_data.canConvert<CardResponseStruct>()) {
                        CardResponseStruct resp = r_data.value<CardResponseStruct>();
                        QList<int> cards = resp.m_card->getSubcards();
                        foreach (int id, cards) {
                            if (!move.card_ids.contains(id))
                                return QStringList();
                        }
                        if (room->isAllOnPlace(resp.m_card, Player::DiscardPile)) {
                            QList<ServerPlayer *> all_players = room->getAlivePlayers();
                            foreach (ServerPlayer *p, all_players) {
                                if (p->getHandcardNum() > player->getHandcardNum())
                                    return QStringList(objectName());
                            }
                        }
                    }
                }
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        QList<ServerPlayer *> targets, all_players = room->getAlivePlayers();
        foreach (ServerPlayer *p, all_players) {
            if (p->getHandcardNum() > player->getHandcardNum())
                targets << p;
        }
        if (triggerEvent == CardFinished) {
            CardUseStruct use = data.value<CardUseStruct>();
            ServerPlayer *target = room->askForPlayerChosen(player, targets, objectName(), "@xunxian:::"+use.card->objectName(), true, true);
            if (target) {
                player->broadcastSkillInvoke(objectName());
                target->obtainCard(use.card);
            }
        } else if (triggerEvent == CardsMoveOneTime) {
            QVariant move_data = data.toList().first();
            CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
            QVariant r_data = move.reason.m_extraData;
            CardResponseStruct resp = r_data.value<CardResponseStruct>();
            const Card *to_obtain = resp.m_card;
            ServerPlayer *target = room->askForPlayerChosen(player, targets, objectName(), "@xunxian:::"+to_obtain->objectName(), true, true);
            if (target) {
                player->broadcastSkillInvoke(objectName());
                target->obtainCard(to_obtain);
            }
        }
        return false;
    }
};



class Xunxun : public PhaseChangeSkill
{
public:
    Xunxun() : PhaseChangeSkill("xunxun")
    {
        frequency = Frequent;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Draw;
    }

    virtual bool onPhaseChange(ServerPlayer *lidian) const
    {
        Room *room = lidian->getRoom();
        if (!room->askForSkillInvoke(lidian, objectName())) return false;
        lidian->broadcastSkillInvoke(objectName());
        QList<ServerPlayer *> p_list;
        p_list << lidian;
        QList<int> card_ids = room->getNCards(4);
        LogMessage log;
        log.type = "$ViewDrawPile";
        log.from = lidian;
        log.card_str = IntList2StringList(card_ids).join("+");
        room->sendLog(log, lidian);
        AskForMoveCardsStruct result = room->askForMoveCards(lidian, card_ids, QList<int>(), true, objectName(), "", 2, 2, false, false, QList<int>() << -1);
        for (int i = result.bottom.length() - 1; i >= 0; i--)
            room->getDrawPile().prepend(result.bottom.at(i));
        foreach(int id, result.top)
            room->getDrawPile().append(id);
        room->doBroadcastNotify(QSanProtocol::S_COMMAND_UPDATE_PILE, QVariant(room->getDrawPile().length()));
        LogMessage a;
        a.type = "#GuanxingResult";
        a.from = lidian;
        a.arg = QString::number(2);
        a.arg2 = QString::number(2);
        room->sendLog(a);
        LogMessage b;
        b.type = "$GuanxingTop";
        b.from = lidian;
        b.card_str = IntList2StringList(result.bottom).join("+");
        room->doNotify(lidian, QSanProtocol::S_COMMAND_LOG_SKILL, b.toVariant());
        LogMessage c;
        c.type = "$GuanxingBottom";
        c.from = lidian;
        c.card_str = IntList2StringList(result.top).join("+");
        room->doNotify(lidian, QSanProtocol::S_COMMAND_LOG_SKILL, c.toVariant());
        return false;
    }
};

class Wangxi : public TriggerSkill
{
public:
    Wangxi() : TriggerSkill("wangxi")
    {
        events << Damage << Damaged;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        DamageStruct damage = data.value<DamageStruct>();
        ServerPlayer *target = NULL;
        if (triggerEvent == Damage)
            target = damage.to;
        else
            target = damage.from;
        if (!target || !target->isAlive() || target == player || target->hasFlag("Global_DFDebut")) return QStringList();

        QStringList trigger_list;

        for (int i = 1; i <= damage.damage; i++)
            trigger_list << objectName();

        return trigger_list;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        ServerPlayer *target = NULL;
        if (triggerEvent == Damage)
            target = damage.to;
        else
            target = damage.from;
        if (player->askForSkillInvoke(this, QVariant::fromValue(target))) {
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), target->objectName());
            player->broadcastSkillInvoke(objectName());
            QList<ServerPlayer *> players;
            players << player << target;
            room->sortByActionOrder(players);

            foreach (ServerPlayer *p, players)
                p->drawCards(1, objectName());
        }
        return false;
    }
};

class Junbing : public TriggerSkill
{
public:
    Junbing() : TriggerSkill("junbing")
    {
        events << EventPhaseStart;
    }

    virtual TriggerList triggerable(TriggerEvent , Room *room, ServerPlayer *player, QVariant &) const
    {
        TriggerList skill_list;
        if (player && player->isAlive() && player->getPhase() == Player::Finish && player->getHandcardNum() < 2) {
            QList<ServerPlayer *> simalangs = room->findPlayersBySkillName(objectName());
            foreach (ServerPlayer *simalang, simalangs)
                skill_list.insert(simalang, QStringList("junbing!"));

        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *simalang) const
    {
        if (room->askForChoice(player, objectName(), "yes+no", data, "@junbing-choose:" + simalang->objectName()) == "yes") {
            LogMessage log;
            log.type = "#InvokeOthersSkill";
            log.from = player;
            log.to << simalang;
            log.arg = objectName();
            room->sendLog(log);
            simalang->broadcastSkillInvoke(objectName());
            room->notifySkillInvoked(simalang, objectName());

            player->drawCards(1);
            if (player->objectName() != simalang->objectName() && !player->isKongcheng()) {
                DummyCard *cards = player->wholeHandCards();
                cards->deleteLater();
                CardMoveReason reason = CardMoveReason(CardMoveReason::S_REASON_GIVE, player->objectName());
                room->moveCardTo(cards, simalang, Player::PlaceHand, reason);

                int x = qMin(cards->subcardsLength(), simalang->getCardCount(true));

                if (x > 0) {
                    const Card *return_cards = room->askForExchange(simalang, objectName(), x, x, true, QString("@junbing-return:%1::%2").arg(player->objectName()).arg(cards->subcardsLength()));
                    CardMoveReason return_reason = CardMoveReason(CardMoveReason::S_REASON_GIVE, simalang->objectName());
                    room->moveCardTo(return_cards, player, Player::PlaceHand, return_reason);
                }
            }
        }
        return false;
    }
};

QujiCard::QujiCard()
{
}

bool QujiCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *) const
{
    return subcardsLength() > targets.length() && to_select->isWounded();
}

void QujiCard::extraCost(Room *room, const CardUseStruct &card_use) const
{
    foreach (int id, getSubcards()) {
        if (Sanguosha->getCard(id)->isBlack()) {
            setFlags("QujiBlack");
            break;
        }
    }
    SkillCard::extraCost(room, card_use);
}

void QujiCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    foreach(ServerPlayer *p, targets)
        room->cardEffect(this, source, p);
    if (hasFlag("QujiBlack"))
        room->loseHp(source);
}

void QujiCard::onEffect(const CardEffectStruct &effect) const
{
    RecoverStruct recover;
    recover.who = effect.from;
    recover.recover = 1;
    effect.to->getRoom()->recover(effect.to, recover);
}

class Quji : public ViewAsSkill
{
public:
    Quji() : ViewAsSkill("quji")
    {
    }

    bool viewFilter(const QList<const Card *> &selected, const Card *) const
    {
        return selected.length() < Self->getLostHp();
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return player->isWounded() && !player->hasUsed("QujiCard");
    }

    const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.length() == Self->getLostHp()) {
            QujiCard *quji = new QujiCard;
            quji->addSubcards(cards);
            return quji;
        }
        return NULL;
    }
};

class Andong : public TriggerSkill
{
public:
    Andong() : TriggerSkill("andong")
    {
        events << DamageInflicted;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (TriggerSkill::triggerable(player) && damage.from && damage.from->isAlive() && damage.from != player)
            return QStringList(objectName());
        return QStringList();

    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        ServerPlayer *from = damage.from;
        if (room->askForSkillInvoke(player, objectName(), QVariant::fromValue(from))) {
            player->broadcastSkillInvoke(objectName());
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), from->objectName());

            QStringList choices;
            choices << "prevent";
            if (!from->isKongcheng()) choices << "view";
            QString choice = room->askForChoice(from, objectName(), choices.join("+"), data, "@andong-choice:"+player->objectName(), "prevent+view");
            if (choice == "prevent") {
                room->preventDamage(damage);
                room->setPlayerFlag(from, "AndongEffect");
                return true;
            } else if (choice == "view") {
                room->doGongxin(player, from, QList<int>(), objectName());

                DummyCard *dummy = new DummyCard;
                foreach (const Card *card, from->getHandcards()) {
                    if (card->getSuit() == Card::Heart)
                        dummy->addSubcard(card);
                }
                if (dummy->subcardsLength() > 0) {
                    CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, player->objectName());
                    room->obtainCard(player, dummy, reason);
                }
            }
        }
        return false;
    }
};

class AndongHideCard : public HideCardSkill
{
public:
    AndongHideCard() : HideCardSkill("#andong-hidecard")
    {
    }
    virtual bool isCardHided(const Player *player, const Card *card) const
    {
        return (player->hasFlag("AndongEffect") && card->getSuit() == Card::Heart);
    }
};

class Yingshi : public TriggerSkill
{
public:
    Yingshi() : TriggerSkill("yingshi")
    {
        events << EventPhaseStart << Damage << Death;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (triggerEvent == EventPhaseStart && TriggerSkill::triggerable(player) && player->getPhase() == Player::Play) {
            QList<ServerPlayer *> all_players = room->getAlivePlayers();
            foreach (ServerPlayer *p, all_players) {
                if (!p->getPile("remuneration").isEmpty())
                    return QStringList();
            }
            return QStringList(objectName());
        } else if (triggerEvent == Damage && player->isAlive()) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.card && damage.card->isKindOf("Slash") && damage.by_user && !damage.chain && !damage.transfer
                    && damage.to->isAlive() && !damage.to->getPile("remuneration").isEmpty())
                return QStringList("yingshi");
        } else if (triggerEvent == Death && player->isAlive() && room->getTag("YingshiSource").value<ServerPlayer *>() == player) {
            DeathStruct death = data.value<DeathStruct>();
            if (!death.who->getPile("remuneration").isEmpty())
                return QStringList("yingshi!");
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        if (triggerEvent == EventPhaseStart) {
            DummyCard *dummy = new DummyCard;
            foreach (const Card *card, player->getHandcards()) {
                if (card->getSuit() == Card::Heart)
                    dummy->addSubcard(card);
            }
            QList<ServerPlayer *> targets;
            if (dummy->subcardsLength() > 0) targets = room->getOtherPlayers(player);
            ServerPlayer *target = room->askForPlayerChosen(player, targets, objectName(), "@yingshi-target", true, true);
            if (target){
                player->broadcastSkillInvoke(objectName());
                target->addToPile("remuneration", dummy, true);
                room->setTag("YingshiSource", QVariant::fromValue(player));
            }
            delete dummy;
        } else if (triggerEvent == Damage) {
            DamageStruct damage = data.value<DamageStruct>();
            QList<int> ids = damage.to->getPile("remuneration");
            room->fillAG(ids, player);
            int id = room->askForAG(player, ids, false, objectName());
            room->clearAG(player);
            CardMoveReason reason(CardMoveReason::S_REASON_EXCHANGE_FROM_PILE, player->objectName(), objectName(), QString());
            room->obtainCard(player, Sanguosha->getCard(id), reason, true);
        } else if (triggerEvent == Death) {
            DeathStruct death = data.value<DeathStruct>();
            DummyCard *dummy = new DummyCard(death.who->getPile("remuneration"));
            CardMoveReason reason(CardMoveReason::S_REASON_EXCHANGE_FROM_PILE, player->objectName(), objectName(), QString());
            room->obtainCard(player, dummy, reason, true);
            delete dummy;
        }
        return false;
    }
};

class Tushe : public TriggerSkill
{
public:
    Tushe() : TriggerSkill("tushe")
    {
        events << TargetSpecified;
    }

    virtual QStringList triggerable(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card->getTypeId() != Card::TypeBasic && use.card->getTypeId() != Card::TypeTrick) return QStringList();
        if (use.index > 0) return QStringList();
        foreach (const Card *card, player->getHandcards()) {
            if (card->getTypeId() == Card::TypeBasic)
                return QStringList();
        }
        QList<ServerPlayer *> all_players = room->getAlivePlayers();
        foreach (ServerPlayer *p, all_players) {
            if (use.to.contains(p))
                return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        int n = 0;
        QList<ServerPlayer *> all_players = room->getAlivePlayers();
        foreach (ServerPlayer *p, all_players) {
            if (use.to.contains(p))
                n++;
        }
        if (player->askForSkillInvoke(this, "prompt:::" + QString::number(n))) {
            player->broadcastSkillInvoke(objectName());
            player->drawCards(n, objectName());
        }
        return false;
    }
};

LimuCard::LimuCard()
{
    target_fixed = true;
    will_throw = false;
    handling_method = Card::MethodNone;
    mute = true;
}

void LimuCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const
{
    Indulgence *indulgence = new Indulgence(getSuit(), getNumber());
    indulgence->addSubcard(getEffectiveId());
    indulgence->setSkillName("limu");
    room->useCard(CardUseStruct(indulgence, source, source));

    if (source->isAlive() && source->isWounded())
        room->recover(source, RecoverStruct(source));
}

class Limu : public OneCardViewAsSkill
{
public:
    Limu() : OneCardViewAsSkill("limu")
    {
        filter_pattern = ".|diamond";
        response_or_use = true;
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        Indulgence *indulgence = new Indulgence(Card::NoSuit, 0);
        indulgence->deleteLater();
        return !player->isLocked(indulgence) && !player->isProhibited(player, indulgence);
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        Indulgence *indulgence = new Indulgence(originalCard->getSuit(), originalCard->getNumber());
        indulgence->addSubcard(originalCard);
        indulgence->setSkillName(objectName());
        indulgence->deleteLater();
        if (!Self->isLocked(indulgence) && !Self->isProhibited(Self, indulgence)) {
            LimuCard *card = new LimuCard;
            card->addSubcard(originalCard);
            card->setSkillName(objectName());
            return card;
        }
        return NULL;
    }
};

class LimuTargetMod : public TargetModSkill
{
public:
    LimuTargetMod() : TargetModSkill("#limu-target")
    {
        pattern = "^SkillCard";
    }

    int getResidueNum(const Player *from, const Card *, const Player *to) const
    {
        if (from->hasSkill("limu") && !from->getJudgingArea().isEmpty()) {
            if (from->inMyAttackRange(to)) {
                return 1000;
            }
        }
        return 0;
    }

    int getDistanceLimit(const Player *from, const Card *, const Player *to) const
    {
        if (from->hasSkill("limu") && !from->getJudgingArea().isEmpty()) {
            if (from->inMyAttackRange(to)) {
                return 1000;
            }
        }
        return 0;
    }
};

class Yishe : public TriggerSkill
{
public:
    Yishe() : TriggerSkill("yishe")
    {
        events << EventPhaseStart << CardsMoveOneTime;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (TriggerSkill::triggerable(player) && player->getPile("rice").isEmpty()) {
            if (triggerEvent == EventPhaseStart && player->getPhase() == Player::Finish)
                return QStringList(objectName());
            else if (triggerEvent == CardsMoveOneTime && player->isWounded()) {
                QVariantList move_datas = data.toList();
                foreach(QVariant move_data, move_datas) {
                    CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
                    if (move.from == player && move.from_places.contains(Player::PlaceSpecial) && move.from_pile_names.contains("rice"))
                        return QStringList("yishe!");
                }
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        if (triggerEvent == EventPhaseStart && player->askForSkillInvoke(this)) {
            player->broadcastSkillInvoke(objectName());
            player->drawCards(2, objectName());
            const Card *card = room->askForExchange(player, objectName(), 2, 2, true, "YishePush");
            player->addToPile("rice", card);
            delete card;
        } else if (triggerEvent == CardsMoveOneTime) {
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            room->recover(player, RecoverStruct(player));
        }
        return false;
    }
};

class Bushi : public TriggerSkill
{
public:
    Bushi() : TriggerSkill("bushi")
    {
        events << Damage << Damaged;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (TriggerSkill::triggerable(player) && !player->getPile("rice").isEmpty() && damage.to->isAlive()) {
            QString skill_name = objectName();
            if (triggerEvent == Damage) {
                if (player == damage.to) return QStringList();
                skill_name = "bushi!";
            }
            QStringList trigger_list;
            for (int i = 1; i <= damage.damage; i++) {
                trigger_list << skill_name;
            }
            return trigger_list;
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        ServerPlayer *target = damage.to;
        if (room->askForChoice(target, objectName(), "yes+no", data, "@bushi-invoke:" + player->objectName()) == "yes") {
            LogMessage log;
            log.type = (target == player) ? "#InvokeSkill" : "#InvokeOthersSkill";
            log.from = target;
            log.to << player;
            log.arg = objectName();
            room->sendLog(log);
            room->notifySkillInvoked(player, objectName());
            player->broadcastSkillInvoke(objectName());
            QList<int> ids = player->getPile("rice");
            room->fillAG(ids, target);
            int id = room->askForAG(target, ids, false, objectName());
            room->clearAG(target);
            CardMoveReason reason(CardMoveReason::S_REASON_EXCHANGE_FROM_PILE, target->objectName(), objectName(), QString());
            room->obtainCard(target, Sanguosha->getCard(id), reason, true);
        }
        return false;
    }
};

class MidaoVS : public OneCardViewAsSkill
{
public:
    MidaoVS() : OneCardViewAsSkill("midao")
    {
        expand_pile = "rice";
        filter_pattern = ".|.|.|rice";
        response_pattern = "@@midao";
    }

    const Card *viewAs(const Card *originalCard) const
    {
        return originalCard;
    }
};

class Midao : public TriggerSkill
{
public:
    Midao() : TriggerSkill("midao")
    {
        events << AskForRetrial;
        view_as_skill = new MidaoVS;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return TriggerSkill::triggerable(target) && !target->getPile("rice").isEmpty();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        JudgeStruct *judge = data.value<JudgeStruct *>();

        QStringList prompt_list;
        prompt_list << "@midao-card" << judge->who->objectName()
            << objectName() << judge->reason << QString::number(judge->card->getEffectiveId());
        QString prompt = prompt_list.join(":");
        const Card *card = room->askForCard(player, "@@midao", prompt, data, Card::MethodResponse, judge->who, true);
        if (card != NULL)
            room->retrial(card, player, judge, objectName());
        return false;
    }
};

class Qianya : public TriggerSkill
{
public:
    Qianya() : TriggerSkill("qianya")
    {
        events << TargetConfirmed;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(player) || player->isKongcheng()) return QStringList();
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card->isKindOf("TrickCard")) {
            return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        QList<int> handcards = player->handCards();
        room->askForYiji(player, handcards, objectName(), false, false, true, -1, room->getOtherPlayers(player),
                         CardMoveReason(), "@qianya-give", QString(), true);
        return false;
    }
};

class Shuimeng : public TriggerSkill
{
public:
    Shuimeng() : TriggerSkill("shuimeng")
    {
        events << EventPhaseEnd;
    }

    virtual QStringList triggerable(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(player) || player->getPhase() != Player::Play || player->isKongcheng()) return QStringList();
        QList<ServerPlayer *> players = room->getOtherPlayers(player);
        foreach (ServerPlayer *p, players) {
            if (player->canPindian(p))
                return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        QList<ServerPlayer *> targets;
        foreach (ServerPlayer *p, room->getOtherPlayers(player)) {
            if (player->canPindian(p))
                targets << p;
        }
        if (targets.isEmpty()) return false;
        ServerPlayer *target = room->askForPlayerChosen(player, targets, objectName(), "@shuimeng-invoke", true, true);
        if (target) {
            player->broadcastSkillInvoke(objectName());
            if (player->pindian(target, objectName())) {
                Card *to_use = Sanguosha->cloneCard("ex_nihilo", Card::NoSuit, 0);
                to_use->setSkillName("_shuimeng");
                if (!room->isProhibited(player, player, to_use) && to_use->targetFilter(QList<const Player *>(), player, player))
                    room->useCard(CardUseStruct(to_use, player, player));
            } else {
                Card *to_use = Sanguosha->cloneCard("dismantlement", Card::NoSuit, 0);
                to_use->setSkillName("_shuimeng");
                if (!room->isProhibited(target, player, to_use) && to_use->targetFilter(QList<const Player *>(), player, target))
                    room->useCard(CardUseStruct(to_use, target, player));
            }
        }
        return false;
    }
};

class Jianzheng : public TriggerSkill
{
public:
    Jianzheng() : TriggerSkill("jianzheng")
    {
        events << TargetSpecifying;
    }

    virtual TriggerList triggerable(TriggerEvent , Room *room, ServerPlayer *, QVariant &data) const
    {
        TriggerList skill_list;
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card->isKindOf("Slash") && use.from && use.from->isAlive()) {
            QList<ServerPlayer *> qinmis = room->findPlayersBySkillName(objectName());
            foreach (ServerPlayer *qinmi, qinmis) {
                if (use.from->inMyAttackRange(qinmi) && !use.to.contains(qinmi) && !qinmi->isKongcheng())
                    skill_list.insert(qinmi, QStringList(objectName()));
            }
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *, QVariant &data, ServerPlayer *qinmi) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        const Card *c = room->askForCard(qinmi, ".", "@jianzheng-put:"+use.from->objectName(), data, Card::MethodNone);
        if (c == NULL) return false;
        LogMessage log;
        log.from = qinmi;
        log.type = "#InvokeSkill";
        log.arg = objectName();
        room->sendLog(log);
        room->notifySkillInvoked(qinmi, objectName());
        qinmi->broadcastSkillInvoke(objectName());
        CardMoveReason reason(CardMoveReason::S_REASON_PUT, qinmi->objectName(), QString(), objectName(), QString());
        room->moveCardTo(c, NULL, Player::DrawPile, reason, false);
        use.to.clear();
        if (!use.card->isBlack())
            use.to.append(qinmi);
        data = QVariant::fromValue(use);

        return false;
    }
};

class Zhuandui : public TriggerSkill
{
public:
    Zhuandui() : TriggerSkill("zhuandui")
    {
        events << TargetSpecified << TargetConfirmed;
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
        if (player->canPindian(to))
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        int index = use.index;
        if (triggerEvent == TargetSpecified) {
            ServerPlayer *to = use.to.at(index);
            if (player->askForSkillInvoke(this, QVariant::fromValue(to))) {
                player->broadcastSkillInvoke(objectName());
                room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), to->objectName());
                if (player->pindian(to, objectName())) {
                    LogMessage log;
                    log.type = "#NoJink";
                    log.from = to;
                    room->sendLog(log);
                    QVariantList jink_list = use.card->tag["Jink_List"].toList();
                    jink_list[index] = 0;
                    use.card->setTag("Jink_List", jink_list);
                }
            }
        } else if (triggerEvent == TargetSpecified) {
            ServerPlayer *to = use.from;
            if (player->askForSkillInvoke(this, QVariant::fromValue(to))) {
                player->broadcastSkillInvoke(objectName());
                room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), to->objectName());
                if (player->pindian(to, objectName())) {
                    QVariantList nullified_list = use.card->tag["Nullified_List"].toList();
                    nullified_list[index] = true;
                    use.card->setTag("Nullified_List", nullified_list);
                }
            }
        }
        return false;
    }
};

TianbianCard::TianbianCard()
{
    target_fixed = true;
}

const Card *TianbianCard::validateInResponse(ServerPlayer *user) const
{
    Room *room = user->getRoom();
    LogMessage log;
    log.from = user;
    log.type = "#UseCard";
    log.card_str = toString();
    room->sendLog(log);
    room->notifySkillInvoked(user, "tianbian");
    user->broadcastSkillInvoke("tianbian");

    int id = room->drawCard();
    return Sanguosha->getCard(id);
}

class TianbianViewAsSkill : public ZeroCardViewAsSkill
{
public:
    TianbianViewAsSkill() : ZeroCardViewAsSkill("tianbian")
    {
        response_pattern = "pindian";
    }

    virtual const Card *viewAs() const
    {
        return new TianbianCard;
    }
};

class Tianbian : public TriggerSkill
{
public:
    Tianbian() : TriggerSkill("tianbian")
    {
        events << PindianVerifying;
        view_as_skill = new TianbianViewAsSkill;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        PindianStruct *pindian = data.value<PindianStruct *>();
        if (TriggerSkill::triggerable(player) && ((pindian->from == player && pindian->from_card->getSuit() == Card::Heart)
                || (pindian->to == player && pindian->to_card->getSuit() == Card::Heart)))
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        PindianStruct *pindian = data.value<PindianStruct *>();
        bool isFrom = (pindian->from == player);

        LogMessage log;
        log.type = "$TianbianNumber";
        log.from = player;
        log.arg = objectName();
        log.arg2 = QString::number(13);
        room->sendLog(log);
        room->notifySkillInvoked(player, objectName());
        player->broadcastSkillInvoke(objectName());
        if (isFrom)
            pindian->from_number = 13;
        else
            pindian->to_number = 13;

        return false;
    }
};

class Funan : public TriggerSkill
{
public:
    Funan() : TriggerSkill("funan")
    {
        events << CardUsed << CardResponded << EventPhaseStart << PreCardsMoveOneTime;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (triggerEvent == PreCardsMoveOneTime) {

        } else if (triggerEvent == EventPhaseStart && player->getPhase() == Player::NotActive) {
            foreach (ServerPlayer *p, room->getAllPlayers()) {
                room->setPlayerProperty(p, "funan_record", QVariant());
            }
        }
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &ask_who) const
    {
        if (triggerEvent == CardUsed || triggerEvent == CardResponded) {
            const Card *card = NULL;
            const Card *e_card = NULL;
            ServerPlayer *xuezong = NULL;
            if (triggerEvent == CardUsed) {
                CardUseStruct use = data.value<CardUseStruct>();
                QVariant m_data = use.m_data;
                if (m_data.canConvert<CardEffectStruct>()) {
                    CardEffectStruct effect = m_data.value<CardEffectStruct>();
                    xuezong = effect.from;
                    e_card = effect.card;
                    card = use.card;
                }
            } else {
                CardResponseStruct response = data.value<CardResponseStruct>();
                QVariant m_data = response.m_data;
                if (m_data.canConvert<CardEffectStruct>()) {
                    CardEffectStruct effect = m_data.value<CardEffectStruct>();
                    xuezong = effect.from;
                    e_card = effect.card;
                    card = response.m_card;
                } else if (m_data.canConvert<SlashEffectStruct>()) {
                    SlashEffectStruct effect = m_data.value<SlashEffectStruct>();
                    xuezong = effect.from;
                    e_card = effect.slash;
                    card = response.m_card;
                }
            }
            if (card && card->getTypeId() != Card::TypeSkill && e_card && e_card->getTypeId() != Card::TypeSkill
                    && TriggerSkill::triggerable(xuezong) && xuezong != player) {
                bool initial = (xuezong->getMark("funan_upgrade") == 0);
                if (room->isAllOnPlace(initial ? e_card:card, Player::PlaceTable)) {
                    ask_who = xuezong;
                    return QStringList(objectName());
                }
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *xuezong) const
    {
        if (triggerEvent == CardUsed || triggerEvent == CardResponded) {
            const Card *card = NULL;
            const Card *e_card = NULL;
            if (triggerEvent == CardUsed) {
                CardUseStruct use = data.value<CardUseStruct>();
                QVariant m_data = use.m_data;
                if (m_data.canConvert<CardEffectStruct>()) {
                    CardEffectStruct effect = m_data.value<CardEffectStruct>();
                    e_card = effect.card;
                    card = use.card;
                }
            } else {
                CardResponseStruct response = data.value<CardResponseStruct>();
                QVariant m_data = response.m_data;
                if (m_data.canConvert<CardEffectStruct>()) {
                    CardEffectStruct effect = m_data.value<CardEffectStruct>();
                    e_card = effect.card;
                    card = response.m_card;
                } else if (m_data.canConvert<SlashEffectStruct>()) {
                    SlashEffectStruct effect = m_data.value<SlashEffectStruct>();
                    e_card = effect.slash;
                    card = response.m_card;
                }
            }
            if (card && card->getTypeId() != Card::TypeSkill && e_card && e_card->getTypeId() != Card::TypeSkill) {
                bool initial = (xuezong->getMark("funan_upgrade") == 0);
                if (xuezong->askForSkillInvoke(objectName())) {
                    xuezong->broadcastSkillInvoke(objectName());
                    if (initial && room->isAllOnPlace(e_card, Player::PlaceTable) && player->isAlive()) {
                        room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, xuezong->objectName(), player->objectName());
                        player->obtainCard(e_card);
                        QVariantList funan_ids = player->property("funan_record").toList();
                        foreach (int id, e_card->getSubcards()) {
                            if (room->getCardOwner(id) == player && room->getCardPlace(id) == Player::PlaceHand) {
                                funan_ids << QVariant::fromValue(id);
                            }
                        }
                        room->setPlayerProperty(player, "funan_record", QVariant::fromValue(funan_ids));
                    }
                    if (room->isAllOnPlace(card, Player::PlaceTable) && xuezong->isAlive())
                        xuezong->obtainCard(card);
                }
            }
        }

        return false;
    }
};

class FunanCardLimited : public CardLimitedSkill
{
public:
    FunanCardLimited() : CardLimitedSkill("#funan-limited")
    {
    }
    virtual bool isCardLimited(const Player *player, const Card *card, Card::HandlingMethod method) const
    {
        if (method == Card::MethodUse || method == Card::MethodResponse) {
            QVariantList funan_ids = player->property("funan_record").toList();
            foreach (QVariant card_data, funan_ids) {
                int id = card_data.toInt();
                if (!card->isVirtualCard()) {
                    if (card->getEffectiveId() == id)
                        return true;
                } else if (card->getSubcards().contains(id))
                    return true;
            }
        }
        return false;
    }
};

class Jiexun : public PhaseChangeSkill
{
public:
    Jiexun() : PhaseChangeSkill("jiexun")
    {

    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Finish;
    }

    virtual bool onPhaseChange(ServerPlayer *player) const
    {
        Room *room = player->getRoom();
        int x = 0;
        foreach (ServerPlayer *p, room->getAlivePlayers()) {
            foreach(const Card *card, p->getCards("ej")){
                if (card->getSuit() == Card::Diamond)
                    x++;
            }
        }
        int y = player->getMark("JiexueTimes");
        ServerPlayer *target = room->askForPlayerChosen(player, room->getOtherPlayers(player), objectName(),
                "@jiexun-target:::"+QString::number(x)+":"+QString::number(y), true, true);
        if (target) {
            player->broadcastSkillInvoke(objectName());
            room->addPlayerMark(player, "JiexueTimes");
            if (x>0)
                target->drawCards(x, objectName());
            if (y < 1) return false;
            if (target->forceToDiscard(y,true).length() >= target->getCardCount()) {
                room->detachSkillFromPlayer(player, objectName());
                QString translation = Sanguosha->translate(":funan-upgrade");
                room->setPlayerMark(player, "funan_upgrade", 1);
                Sanguosha->addTranslationEntry(":funan", translation.toStdString().c_str());
                JsonArray args;
                args << QSanProtocol::S_GAME_EVENT_UPDATE_SKILL;
                room->doBroadcastNotify(QSanProtocol::S_COMMAND_LOG_EVENT, args);
            }
            room->askForDiscard(target, objectName(), y, y, false, true, "@jiexun-discard:::"+QString::number(y));
        }
        return false;
    }
};















class Guanwei : public TriggerSkill
{
public:
    Guanwei() : TriggerSkill("guanwei")
    {
        events << EventPhaseEnd;
    }

    virtual TriggerList triggerable(TriggerEvent , Room *room, ServerPlayer *player, QVariant &) const
    {
        TriggerList skill_list;
        if (player->isAlive() && player->getPhase() == Player::Play) {
            int x = 0;
            Card::Suit suit = Card::NoSuit;

            QVariantList card_list = player->tag["RoundUsedCards"].toList();
            foreach (QVariant card_data, card_list) {
                const Card *card = card_data.value<const Card *>();
                if (card) {
                    if (x == 0)
                        suit = card->getSuit();
                    else if (suit != card->getSuit()) {
                        suit = Card::NoSuit;
                        break;
                    }
                    x++;
                }
            }

            if (suit <= Card::Diamond && x > 1) {
                QList<ServerPlayer *> panjuns = room->findPlayersBySkillName(objectName());
                foreach (ServerPlayer *panjun, panjuns) {
                    if (!panjun->isNude() && !panjun->hasFlag("GuanweiUsed"))
                        skill_list.insert(panjun, QStringList(objectName()));
                }
            }
        }

        return skill_list;
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *panjun) const
    {
        if (room->askForCard(panjun, "..", "@guanwei:" + player->objectName(), data, Card::MethodDiscard, player, false, objectName())) {
            player->drawCards(2, objectName());
            player->insertPhase(Player::Play);
        }
        return false;
    }
};

class Gongqing : public TriggerSkill
{
public:
    Gongqing() : TriggerSkill("gongqing")
    {
        events << DamageInflicted;
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (TriggerSkill::triggerable(player)) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.from && damage.from->isAlive() && damage.from->getAttackRange() < 3 && damage.damage > 1)
                return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (damage.from && damage.from->isAlive()) {
            int x = damage.from->getAttackRange();
            if (x > 3) {
                room->sendCompulsoryTriggerLog(player, objectName());
                player->broadcastSkillInvoke(objectName());
                damage.damage++;
            } else if (x < 3 && damage.damage > 1) {
                room->sendCompulsoryTriggerLog(player, objectName());
                player->broadcastSkillInvoke(objectName());
                damage.damage = 1;
            }
        }
        data = QVariant::fromValue(damage);
        return false;
    }
};

class GongqingNegative : public TriggerSkill
{
public:
    GongqingNegative() : TriggerSkill("#gongqing")
    {
        events << DamageInflicted;
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (TriggerSkill::triggerable(player)) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.from && damage.from->isAlive() && damage.from->getAttackRange() > 3)
                return QStringList("gongqing");
        }
        return QStringList();
    }
};



class Xianfu : public TriggerSkill
{
public:
    Xianfu() : TriggerSkill("xianfu")
    {
        events << TurnStart << Damaged << HpRecover;
        frequency = Compulsory;
    }

    virtual TriggerList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        TriggerList skill_list;
        if (triggerEvent == TurnStart) {
            if (!room->getTag("FirstRound").toBool()) return skill_list;
            QList<ServerPlayer *> xizhicais = room->findPlayersBySkillName(objectName());
            foreach (ServerPlayer *xizhicai, xizhicais)
                skill_list.insert(xizhicai, QStringList(objectName()));

        } else if (player && player->isAlive()) {
            foreach (ServerPlayer *xizhicai, room->getAllPlayers()) {
                ServerPlayer *AssistTarget = xizhicai->tag["XianfuTarget"].value<ServerPlayer *>();
                if (AssistTarget == player && (triggerEvent == Damaged || xizhicai->isWounded()))
                    skill_list.insert(xizhicai, QStringList(objectName()));
            }
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *xizhicai) const
    {
        if (triggerEvent == TurnStart) {
            room->sendCompulsoryTriggerLog(xizhicai, objectName());
            room->notifySkillInvoked(xizhicai, objectName());
            QList<int> types;
            types << 1;
            types << 2;
            room->broadcastSkillInvoke(objectName(), xizhicai, types);
            ServerPlayer *target = room->askForPlayerChosen(xizhicai, room->getOtherPlayers(xizhicai), objectName(), "@xianfu");
            xizhicai->tag["XianfuTarget"] = QVariant::fromValue(target);

        } else if (triggerEvent == Damaged) {
            DamageStruct damage = data.value<DamageStruct>();
            LogMessage log;
            log.type = "#SkillForce";
            log.from = xizhicai;
            log.arg = objectName();
            room->sendLog(log);
            room->notifySkillInvoked(xizhicai, objectName());
            QList<int> types;
            types << 5;
            types << 6;
            room->broadcastSkillInvoke(objectName(), xizhicai, types);
            room->addPlayerTip(player, "#xianfu");
            room->damage(DamageStruct(objectName(), NULL, xizhicai, damage.damage));
        } else if (triggerEvent == HpRecover) {
            LogMessage log;
            log.type = "#SkillForce";
            log.from = xizhicai;
            log.arg = objectName();
            room->sendLog(log);
            room->notifySkillInvoked(xizhicai, objectName());
            QList<int> types;
            types << 3;
            types << 4;
            room->broadcastSkillInvoke(objectName(), xizhicai, types);
            room->addPlayerTip(player, "#xianfu");
            room->recover(xizhicai, RecoverStruct(xizhicai, NULL, data.value<RecoverStruct>().recover));
        }
        return false;
    }
};

class Chouce : public MasochismSkill
{
public:
    Chouce() : MasochismSkill("chouce")
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

    virtual void onDamaged(ServerPlayer *xizhicai, const DamageStruct &) const
    {
        Room *room = xizhicai->getRoom();
        if (xizhicai->askForSkillInvoke(objectName())) {
            xizhicai->broadcastSkillInvoke(objectName());
            JudgeStruct judge;
            judge.pattern = ".";
            judge.patterns << ".|red" << ".|black";
            judge.play_animation = false;
            judge.reason = objectName();
            judge.who = xizhicai;

            room->judge(judge);

            if (judge.pattern == ".|red") {
                ServerPlayer *target = room->askForPlayerChosen(xizhicai, room->getAlivePlayers(), objectName(), "@chouce-draw");
                ServerPlayer *AssistTarget = xizhicai->tag["XianfuTarget"].value<ServerPlayer *>();
                if (AssistTarget && AssistTarget == target) {
                    room->addPlayerTip(target, "#xianfu");
                    target->drawCards(2, objectName());
                } else
                    target->drawCards(1, objectName());
            } else if (judge.pattern == ".|black") {
                QList<ServerPlayer *> targets;
                foreach (ServerPlayer *p, room->getAlivePlayers()) {
                    if (xizhicai->canDiscard(p, "hej"))
                        targets << p;
                }
                if (!targets.isEmpty()) {
                    xizhicai->setFlags("ChouceAIDiscard");
                    ServerPlayer *target = room->askForPlayerChosen(xizhicai, targets, objectName(), "@chouce-dis");
                    xizhicai->setFlags("-ChouceAIDiscard");
                    int card_id = room->askForCardChosen(xizhicai, target, "hej", objectName(), false, Card::MethodDiscard);
                    room->throwCard(card_id, room->getCardPlace(card_id) == Player::PlaceDelayedTrick ? NULL : target, xizhicai);
                }
            }
        }
    }
};






JiaozhaoCard::JiaozhaoCard()
{
    will_throw = false;
    handling_method = Card::MethodNone;
}

bool JiaozhaoCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    if (Self->getMark("danxin_modify") > 1 || !targets.isEmpty()) return false;
    int nearest = 1000;
    foreach (const Player *p, Self->getAliveSiblings()) {
        nearest = qMin(nearest, Self->distanceTo(p));
    }
    return Self->distanceTo(to_select) == nearest;
}

bool JiaozhaoCard::targetsFeasible(const QList<const Player *> &targets, const Player *Self) const
{
    if (Self->getMark("danxin_modify") > 1)
        return true;
    else
        return !targets.isEmpty();
}

void JiaozhaoCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    room->showCard(source, getEffectiveId());

    ServerPlayer *target = NULL;
    if (targets.isEmpty())
        target = source;
    else
        target = targets.first();

    QString pattern = "@@jiaozhao_first!";
    if (source->getMark("danxin_modify") > 0)
        pattern = "@@jiaozhao_second!";
    const Card *card = room->askForCard(target, pattern, "@jiaozhao-declare:" + source->objectName(), QVariant(), Card::MethodNone);

    QString card_name = "slash";
    if (card != NULL)
        card_name = card->objectName();

    LogMessage log;
    log.type = "$JiaozhaoDeclare";
    log.arg = card_name;
    log.from = target;
    room->sendLog(log);
    room->setPlayerProperty(source, "jiaozhao_record_id", QString::number(getEffectiveId()));
    room->setPlayerProperty(source, "jiaozhao_record_name", card_name);
    room->setPlayerMark(source, "ViewAsSkill_jiaozhaoEffect", 1);
}

class JiaozhaoViewAsSkill : public OneCardViewAsSkill
{
public:
    JiaozhaoViewAsSkill() : OneCardViewAsSkill("jiaozhao")
    {
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        if (player->hasUsed("JiaozhaoCard")) {
            QString record_id = Self->property("jiaozhao_record_id").toString();
            QString record_name = Self->property("jiaozhao_record_name").toString();
            if (record_id == "" || record_name == "") return false;
            Card *use_card = Sanguosha->cloneCard(record_name);
            if (!use_card) return false;
            use_card->setCanRecast(false);
            use_card->addSubcard(record_id.toInt());
            use_card->setSkillName("jiaozhao");
            return use_card->isAvailable(player);
        }
        return true;
    }

    virtual bool viewFilter(const Card *card) const
    {
        if (card->isEquipped()) return false;
        if (Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_PLAY && !Self->hasUsed("JiaozhaoCard"))
            return true;
        QString record_id = Self->property("jiaozhao_record_id").toString();
        return record_id != "" && record_id.toInt() == card->getEffectiveId();
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        if (Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_PLAY && !Self->hasUsed("JiaozhaoCard")) {
            JiaozhaoCard *jiaozhao = new JiaozhaoCard;
            jiaozhao->addSubcard(originalCard);
            if (Self->getMark("danxin_modify") > 1)
                jiaozhao->setTargetFixed(true);
            return jiaozhao;
        }
        QString record_name = Self->property("jiaozhao_record_name").toString();
        Card *use_card = Sanguosha->cloneCard(record_name);
        if (!use_card) return NULL;
        use_card->setCanRecast(false);
        use_card->addSubcard(originalCard);
        use_card->setSkillName("jiaozhao");
        return use_card;
    }

    bool isEnabledAtResponse(const Player *player, const QString &pattern) const
    {
        if (Sanguosha->currentRoomState()->getCurrentCardUseReason() != CardUseStruct::CARD_USE_REASON_RESPONSE_USE)
            return false;

        if (pattern.startsWith(".") || pattern.startsWith("@"))
            return false;

        QString record_name = player->property("jiaozhao_record_name").toString();
        if (record_name == "") return false;
        QString pattern_names = pattern;
        if (pattern == "slash")
            pattern_names = "slash+fire_slash+thunder_slash";
        else if (pattern == "peach+analeptic")
            return false;

        return pattern_names.split("+").contains(record_name);
    }

    virtual bool isEnabledAtNullification(const ServerPlayer *player) const
    {
        return player->property("jiaozhao_record_name").toString() == "nullification";
    }
};

class Jiaozhao : public TriggerSkill
{
public:
    Jiaozhao() : TriggerSkill("jiaozhao")
    {
        events << PreCardsMoveOneTime << EventPhaseChanging;
        view_as_skill = new JiaozhaoViewAsSkill;
    }

    bool triggerable(const ServerPlayer *) const
    {
        return false;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == PreCardsMoveOneTime) {
            QVariantList move_datas = data.toList();
            foreach(QVariant move_data, move_datas) {
                CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
                if (player != move.from) return;
                QString record_id = player->property("jiaozhao_record_id").toString();
                if (record_id == "") return;
                int id = record_id.toInt();
                if (move.card_ids.contains(id) && move.from_places[move.card_ids.indexOf(id)] == Player::PlaceHand) {
                    room->setPlayerProperty(player, "jiaozhao_record_id", QString());
                    room->setPlayerProperty(player, "jiaozhao_record_name", QString());
                    room->setPlayerMark(player, "ViewAsSkill_jiaozhaoEffect", 0);
                    return;
                }
            }
        } else if (triggerEvent == EventPhaseChanging) {
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (change.from != Player::Play) return;
            room->setPlayerProperty(player, "jiaozhao_record_id", QString());
            room->setPlayerProperty(player, "jiaozhao_record_name", QString());
            room->setPlayerMark(player, "ViewAsSkill_jiaozhaoEffect", 0);
        }
    }
};

class JiaozhaoProhibit : public ProhibitSkill
{
public:
    JiaozhaoProhibit() : ProhibitSkill("#jiaozhao")
    {
    }

    bool isProhibited(const Player *from, const Player *to, const Card *card, const QList<const Player *> & /* = QList<const Player *>() */) const
    {
        return (!card->isKindOf("SkillCard") && card->getSkillName() == "jiaozhao" && from && from == to);
    }
};

class JiaozhaoFirst : public OneCardViewAsSkill
{
public:
    JiaozhaoFirst() : OneCardViewAsSkill("jiaozhao_first")
    {
        response_pattern = "@@jiaozhao_first!";
        guhuo_type = "b";
    }

    bool viewFilter(const Card *to_select) const
    {
        return to_select->isVirtualCard();
    }

    const Card *viewAs(const Card *originalCard) const
    {
        return Sanguosha->cloneCard(originalCard->objectName());
    }
};

class JiaozhaoSecond : public OneCardViewAsSkill
{
public:
    JiaozhaoSecond() : OneCardViewAsSkill("jiaozhao_second")
    {
        response_pattern = "@@jiaozhao_second!";
        guhuo_type = "bt";
    }

    bool viewFilter(const Card *to_select) const
    {
        return to_select->isVirtualCard();
    }

    const Card *viewAs(const Card *originalCard) const
    {
        return Sanguosha->cloneCard(originalCard->objectName());
    }
};

class Danxin : public MasochismSkill
{
public:
    Danxin() : MasochismSkill("danxin")
    {
    }

    virtual void onDamaged(ServerPlayer *target, const DamageStruct &) const
    {
        Room *room = target->getRoom();
        if (target->askForSkillInvoke(this)){
            target->broadcastSkillInvoke(objectName());
            if (!target->hasSkill("jiaozhao", true) || target->getMark("danxin_modify") > 1 || room->askForChoice(target, objectName(), "modify+draw") == "draw")
                target->drawCards(1, objectName());
            else {
                room->addPlayerMark(target, "danxin_modify");
                QString translate = Sanguosha->translate(":jiaozhao");
                int i = target->getMark("danxin_modify");
                translate.replace(Sanguosha->translate("jiaozhao:modify"+QString::number(i)), Sanguosha->translate("jiaozhao:modified"+QString::number(i)));
                Sanguosha->addTranslationEntry(":jiaozhao", translate.toStdString().c_str());
                JsonArray args;
                args << QSanProtocol::S_GAME_EVENT_UPDATE_SKILL;
                room->doBroadcastNotify(QSanProtocol::S_COMMAND_LOG_EVENT, args);
            }
        }
    }
};

class Sanwen : public TriggerSkill
{
public:
    Sanwen() : TriggerSkill("sanwen")
    {
        events << CardsMoveOneTime;
    }

    static bool sameCardName(const Card *card1, const Card *card2)
    {
        return ((card1->isKindOf("Slash") && card2->isKindOf("Slash")) || card1->getClassName() == card2->getClassName());
    }

    static QList<int> getSanwenCards(ServerPlayer *wangcan, QVariant &data, bool all)
    {
        QVariantList move_datas = data.toList();
        QList<int> cards, move_cards, handcards = wangcan->handCards();
        foreach(QVariant move_data, move_datas) {
            CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
            if (move.to && move.to == wangcan && move.to_place == Player::PlaceHand) {
                foreach (int id, move.card_ids) {
                    if (handcards.contains(id))
                        move_cards.append(id);
                }
            }
        }
        foreach (int id1, move_cards) {
            foreach (int id2, handcards) {
                if (!move_cards.contains(id2) && sameCardName(Sanguosha->getCard(id1), Sanguosha->getCard(id2))) {
                    cards.append(id1);
                    break;
                }
            }
        }
        if (all && !cards.isEmpty()) {
            QList<int> show_cards;
            foreach (int id1, handcards) {
                foreach (int id2, cards) {
                    if (sameCardName(Sanguosha->getCard(id1), Sanguosha->getCard(id2))) {
                        show_cards.append(id1);
                        break;
                    }
                }
            }
            return show_cards;
        }
        return cards;
    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *wangcan, QVariant &data, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(wangcan) || wangcan->hasFlag("sanwenUsed")) return QStringList();
        if (!getSanwenCards(wangcan,data,false).isEmpty()) return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *wangcan, QVariant &data, ServerPlayer *) const
    {
        if (wangcan->askForSkillInvoke(this)) {
            wangcan->broadcastSkillInvoke(objectName());
            room->setPlayerFlag(wangcan, "sanwenUsed");

            room->showCard(wangcan, getSanwenCards(wangcan,data,true));

            QList<int> cards = getSanwenCards(wangcan, data, false);

            room->getThread()->delay(3000);

            DummyCard *dummy = new DummyCard;
            foreach (int id, cards) {
                if (!wangcan->isJilei(Sanguosha->getCard(id)))
                    dummy->addSubcard(id);
            }
            int x = dummy->subcardsLength();
            if (x > 0) {
                room->throwCard(dummy, wangcan);
                wangcan->drawCards(x*2, objectName());
            }
            delete dummy;

        }
        return false;
    }
};

class Qiai : public TriggerSkill
{
public:
    Qiai() : TriggerSkill("qiai")
    {
        events << AskForPeaches;
        frequency = Limited;
        limit_mark = "@lament";
    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *target, QVariant &data, ServerPlayer * &) const
    {
        if (TriggerSkill::triggerable(target) && target->getMark(limit_mark) > 0) {
            DyingStruct dying_data = data.value<DyingStruct>();

            if (target->getHp() > 0 || dying_data.who != target) return QStringList();

            return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *wangcan, QVariant &, ServerPlayer *) const
    {
        if (wangcan->askForSkillInvoke(this)) {
            wangcan->broadcastSkillInvoke(objectName());

            room->removePlayerMark(wangcan, limit_mark);

            QList<ServerPlayer *> players = room->getOtherPlayers(wangcan);

            foreach (ServerPlayer *player, players)
                room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, wangcan->objectName(), player->objectName());

            foreach (ServerPlayer *player, players) {
                if (wangcan->isDead()) break;
                if (player->isDead() || player->isNude()) continue;
                const Card *card = room->askForExchange(player, objectName(), 1, 1, true, "@qiai-give::" + wangcan->objectName());
                if (card) {
                    CardMoveReason reason(CardMoveReason::S_REASON_GIVE, player->objectName(), wangcan->objectName(), objectName(), QString());
                    reason.m_playerId = player->objectName();
                    room->moveCardTo(card, player, wangcan, Player::PlaceHand, reason);
                    delete card;
                }
            }
        }
        return false;
    }
};

DenglouUseCard::DenglouUseCard()
{
    will_throw = false;
    handling_method = Card::MethodNone;
}

bool DenglouUseCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    const Card *card = Sanguosha->getCard(getEffectiveId());
    return card && card->targetFilter(targets, to_select, Self) && !Self->isProhibited(to_select, card, targets);
}

bool DenglouUseCard::targetFixed() const
{
    const Card *card = Sanguosha->getCard(getEffectiveId());
    return card && card->targetFixed();
}

bool DenglouUseCard::targetsFeasible(const QList<const Player *> &targets, const Player *Self) const
{
    const Card *card = Sanguosha->getCard(getEffectiveId());
    return card && card->targetsFeasible(targets, Self);
}

const Card *DenglouUseCard::validate(CardUseStruct &card_use) const
{
    ServerPlayer *wangcan = card_use.from;
    Room *room = wangcan->getRoom();
    QList<int> basics = StringList2IntList(wangcan->tag["denglou_forAI"].toString().split("+"));
    room->notifyMoveToPile(wangcan, basics, "denglou", Player::PlaceTable, false, false);
    const Card *use_card = Sanguosha->getCard(getEffectiveId());
    return use_card;
}

class DenglouUse : public OneCardViewAsSkill
{
public:
    DenglouUse() : OneCardViewAsSkill("denglou_use")
    {
        response_pattern = "@@denglou_use!";
        expand_pile = "#denglou";
    }

    bool viewFilter(const Card *to_select) const
    {
        return Self->getPile("#denglou").contains(to_select->getEffectiveId()) && to_select->isAvailable(Self);
    }

    const Card *viewAs(const Card *originalCard) const
    {
        DenglouUseCard *card = new DenglouUseCard;
        card->addSubcard(originalCard);
        return card;
    }
};

class Denglou : public PhaseChangeSkill
{
public:
    Denglou() : PhaseChangeSkill("denglou")
    {
        frequency = Limited;
        limit_mark = "@ascend";
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Finish
                && target->isKongcheng() && target->getMark(limit_mark) > 0;
    }

    virtual bool onPhaseChange(ServerPlayer *wangcan) const
    {
        Room *room = wangcan->getRoom();
        if (room->askForSkillInvoke(wangcan, objectName())) {
            wangcan->broadcastSkillInvoke(objectName());
            room->removePlayerMark(wangcan, limit_mark);

            QList<int> ids = room->getNCards(4, false);
            CardsMoveStruct move(ids, wangcan, Player::PlaceTable,
                CardMoveReason(CardMoveReason::S_REASON_TURNOVER, wangcan->objectName(), objectName(), QString()));
            room->moveCardsAtomic(move, true);

            QList<int> basics, card_to_gotback;
            foreach (int id, ids) {
                if (Sanguosha->getCard(id)->getTypeId() == Card::TypeBasic)
                    basics << id;
                else
                    card_to_gotback << id;
            }
            if (!card_to_gotback.isEmpty()) {
                DummyCard *dummy = new DummyCard(card_to_gotback);
                room->obtainCard(wangcan, dummy);
                delete dummy;
            }

            while (true)  {
                const Card *use_card = NULL;
                QList<ServerPlayer *> to;
                foreach (int id, basics) {
                    const Card *card = Sanguosha->getCard(id);
                    if (card->isAvailable(wangcan)) {
                        to = wangcan->getCardDefautTargets(card, true);
                        if (card->targetFixed() || !to.isEmpty()) {
                            use_card = card;
                            break;
                        }
                    }
                }
                if (use_card != NULL) {
                    room->notifyMoveToPile(wangcan, basics, "denglou", Player::PlaceTable, true, true);
                    const Card *use = room->askForUseCard(wangcan, "@@denglou_use!", "@denglou-use", QVariant(), Card::MethodNone, false);
                    if (!use) {
                        room->notifyMoveToPile(wangcan, basics, "denglou", Player::PlaceTable, false, false);
                        room->useCard(CardUseStruct(use_card, wangcan, to));
                    }


                } else break;
                basics = room->getCardIdsOnTable(basics);
            }

            basics = room->getCardIdsOnTable(basics);

            if (!basics.isEmpty()) {
                DummyCard *dummy = new DummyCard(basics);
                CardMoveReason reason(CardMoveReason::S_REASON_NATURAL_ENTER, wangcan->objectName(), objectName(), QString());
                room->throwCard(dummy, reason, NULL);
                delete dummy;
            }
        }
        return false;
    }
};

class Pizhuan : public TriggerSkill
{
public:
    Pizhuan() : TriggerSkill("pizhuan")
    {
        events << CardUsed << CardResponded << TargetConfirmed;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(player) || player->getPile("books").length() > 3) return QStringList();
        const Card *card = NULL;
        if (triggerEvent == TargetConfirmed) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.from == player) return QStringList();
            card = use.card;
        } else {
            if (triggerEvent == CardUsed)
                card = data.value<CardUseStruct>().card;
            else {
                CardResponseStruct resp = data.value<CardResponseStruct>();
                if (resp.m_isUse)
                    card = resp.m_card;
            }

        }
        if (card && card->getTypeId() != Card::TypeSkill && card->getSuit() == Card::Spade)
            return QStringList(objectName());

        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        if (player->askForSkillInvoke(objectName())) {
            player->broadcastSkillInvoke(objectName());
            QList<ServerPlayer *> players;
            players << player;
            player->addToPile("books", room->drawCard(), false, players);
        }
        return false;
    }
};

class PizhuanKeep : public MaxCardsSkill
{
public:
    PizhuanKeep() : MaxCardsSkill("#pizhuan-keep")
    {
        frequency = Frequent;
    }

    virtual int getExtra(const Player *target) const
    {
        if (target->hasSkill("pizhuan"))
            return target->getPile("books").length();
        else
            return 0;
    }
};

TongboCard::TongboCard()
{
    will_throw = false;
    handling_method = Card::MethodNone;
    target_fixed = true;
}

void TongboCard::onUse(Room *room, const CardUseStruct &card_use) const
{
    ServerPlayer *caiyong = card_use.from;
    QList<int> pile = caiyong->getPile("books");
    QList<int> subCards = card_use.card->getSubcards();
    QList<int> to_handcard;
    QList<int> to_pile;
    foreach (int id, (subCards + pile).toSet()) {
        if (!subCards.contains(id))
            to_handcard << id;
        else if (!pile.contains(id))
            to_pile << id;
    }

    Q_ASSERT(to_handcard.length() == to_pile.length());

    if (to_pile.length() == 0 || to_handcard.length() != to_pile.length())
        return;

    LogMessage log;
    log.type = "#InvokeSkill";
    log.from = caiyong;
    log.arg = "tongbo";
    room->sendLog(log);
    room->notifySkillInvoked(caiyong, "tongbo");
    caiyong->broadcastSkillInvoke("tongbo");

    caiyong->addToPile("books", to_pile, false);

    DummyCard *to_handcard_x = new DummyCard(to_handcard);
    CardMoveReason reason(CardMoveReason::S_REASON_EXCHANGE_FROM_PILE, caiyong->objectName());
    room->obtainCard(caiyong, to_handcard_x, reason, false);
    to_handcard_x->deleteLater();

    pile = caiyong->getPile("books");
    if (pile.length() != 4) return;
    QStringList suitlist;
    foreach(int card_id, pile){
        const Card *card = Sanguosha->getCard(card_id);
        QString suit = card->getSuitString();
        if (!suitlist.contains(suit))
            suitlist << suit;
        else{
            return;
        }
    }
    room->askForRende(caiyong, pile, "tongbo", false, false, false, 4, 4, room->getOtherPlayers(caiyong), CardMoveReason(), "@tongbo-give", "books");
}

class TongboVS : public ViewAsSkill
{
public:
    TongboVS() : ViewAsSkill("tongbo")
    {
        response_pattern = "@@tongbo";
        expand_pile = "books";
    }

    virtual bool viewFilter(const QList<const Card *> &selected, const Card *) const
    {
        return selected.length() < Self->getPile("books").length();
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.length() == Self->getPile("books").length()) {
            TongboCard *c = new TongboCard;
            c->addSubcards(cards);
            return c;
        }
        return NULL;
    }
};


class Tongbo : public TriggerSkill
{
public:
    Tongbo() : TriggerSkill("tongbo")
    {
        view_as_skill = new TongboVS;
        events << EventPhaseEnd;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return TriggerSkill::triggerable(target) && target->getPile("books").length() > 0
            && target->getPhase() == Player::Draw;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *caiyong, QVariant &) const
    {
        room->askForUseCard(caiyong, "@@tongbo", "@tongbo-exchange:::"+QString::number(caiyong->getPile("books").length()), QVariant(), Card::MethodNone);
        return false;
    }
};


class Liangzhu : public TriggerSkill
{
public:
    Liangzhu() : TriggerSkill("liangzhu")
    {
        events << HpRecover;
    }

    virtual TriggerList triggerable(TriggerEvent , Room *room, ServerPlayer *player, QVariant &) const
    {
        TriggerList skill_list;
        if (player == NULL || player->isDead() || player->getPhase() != Player::Play) return skill_list;
        QList<ServerPlayer *> sunshangxiangs = room->findPlayersBySkillName(objectName());
        foreach (ServerPlayer *sunshangxiang, sunshangxiangs)
            skill_list.insert(sunshangxiang, QStringList(objectName()));
        return skill_list;
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *sun) const
    {
        QString choice = room->askForChoice(sun, objectName(), "draw+letdraw+cancel", QVariant::fromValue(player),
            "@liangzhu-choose::"+player->objectName());
        if (choice != "cancel") {
            LogMessage log;
            log.type = "#InvokeSkill";
            log.from = sun;
            log.arg = objectName();
            room->sendLog(log);
            sun->broadcastSkillInvoke(objectName());
            room->notifySkillInvoked(sun, objectName());
            if (choice == "draw") {
                sun->drawCards(1, objectName());
            } else if (choice == "letdraw") {
                player->drawCards(2, objectName());
                room->setPlayerMark(player, "yuan", 1);
            }
        }
        return false;
    }
};

class Fanxiang : public TriggerSkill
{
public:
    Fanxiang() : TriggerSkill("fanxiang")
    {
        events << EventPhaseStart;
        frequency = Skill::Wake;
    }

    virtual QStringList triggerable(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(player) || player->getPhase() != Player::Start || player->getMark("fanxiang") > 0) return QStringList();
        foreach (ServerPlayer *p, room->getAlivePlayers()) {
            if (p->getMark("yuan") > 0 && p->isWounded()) {
                return QStringList(objectName());
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());
        room->setPlayerMark(player, "fanxiang", 1);
        if (room->changeMaxHpForAwakenSkill(player, 1) && player->getMark("fanxiang") == 1) {
            room->recover(player, RecoverStruct(player));
            room->handleAcquireDetachSkills(player, "-liangzhu|xiaoji");
        }
        return false;
    }
};

class Wuyan : public TriggerSkill
{
public:
    Wuyan() : TriggerSkill("wuyan")
    {
        events << DamageCaused << DamageInflicted;
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        DamageStruct damage = data.value<DamageStruct>();
        if (damage.card && damage.card->getTypeId() == Card::TypeTrick)
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());
        return true;
    }
};

JujianCard::JujianCard()
{
}

bool JujianCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select != Self;
}

void JujianCard::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.from->getRoom();
    QStringList choicelist;
    choicelist << "draw";
    if (effect.to->isWounded())
        choicelist << "recover";
    if (!effect.to->faceUp() || effect.to->isChained())
        choicelist << "reset";
    QString choice = room->askForChoice(effect.to, "jujian", choicelist.join("+"), QVariant(), QString(), "draw+recover+reset");

    if (choice == "draw")
        effect.to->drawCards(2, "jujian");
    else if (choice == "recover")
        room->recover(effect.to, RecoverStruct(effect.from));
    else if (choice == "reset") {
        if (effect.to->isChained())
            room->setPlayerProperty(effect.to, "chained", false);
        if (!effect.to->faceUp())
            effect.to->turnOver();
    }
}

class JujianViewAsSkill : public OneCardViewAsSkill
{
public:
    JujianViewAsSkill() : OneCardViewAsSkill("jujian")
    {
        filter_pattern = "^BasicCard!";
        response_pattern = "@@jujian";
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        JujianCard *jujianCard = new JujianCard;
        jujianCard->addSubcard(originalCard);
        return jujianCard;
    }
};

class Jujian : public PhaseChangeSkill
{
public:
    Jujian() : PhaseChangeSkill("jujian")
    {
        view_as_skill = new JujianViewAsSkill;
    }

    bool triggerable(const ServerPlayer *xushu) const
    {
        return TriggerSkill::triggerable(xushu) && xushu->getPhase() == Player::Finish && !xushu->isNude();
    }

    virtual bool onPhaseChange(ServerPlayer *xushu) const
    {
        xushu->getRoom()->askForUseCard(xushu, "@@jujian", "@jujian-card", QVariant(), Card::MethodDiscard);
        return false;
    }
};











GuolunCard::GuolunCard()
{
}

bool GuolunCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select != Self && !to_select->isKongcheng();
}

void GuolunCard::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *player = effect.from;
    ServerPlayer *target = effect.to;
    Room *room = player->getRoom();
    int id1 = room->askForCardChosen(player, target, "h", "guolun");
    room->showCard(target, id1);
    const Card *card2 = room->askForExchange(player, "guolun", 1, 1, true, "@guolun-show", true);
    if (card2) {
        int id2 = card2->getEffectiveId();
        room->showCard(player, id2);
        const Card *card1 = Sanguosha->getCard(id1);
        int number1 = card1->getNumber(), number2 = card2->getNumber();


        CardMoveReason reason1(CardMoveReason::S_REASON_SWAP, player->objectName(), target->objectName(), "guolun", QString());
        CardMoveReason reason2(CardMoveReason::S_REASON_SWAP, target->objectName(), player->objectName(), "guolun", QString());

        QList<CardsMoveStruct> move_to_table;
        move_to_table.push_back(CardsMoveStruct(id1, NULL, Player::PlaceTable, reason2));
        move_to_table.push_back(CardsMoveStruct(id2, NULL, Player::PlaceTable, reason1));
        room->moveCardsAtomic(move_to_table, false);

        QList<CardsMoveStruct> back_move;

        if (room->getCardPlace(id2) == Player::PlaceTable)
            back_move.push_back(CardsMoveStruct(id2, target, Player::PlaceHand, reason1));

        if (room->getCardPlace(id1) == Player::PlaceTable)
            back_move.push_back(CardsMoveStruct(id1, player, Player::PlaceHand, reason2));

        if (!back_move.isEmpty())
            room->moveCardsAtomic(back_move, false);


        if (number1 < number2) target->drawCards(1, "guolun");
        if (number1 > number2) player->drawCards(1, "guolun");
    }
}

class Guolun : public ZeroCardViewAsSkill
{
public:
    Guolun() : ZeroCardViewAsSkill("guolun")
    {

    }

    virtual const Card *viewAs() const
    {
        return new GuolunCard;
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("GuolunCard");
    }
};

class Songsang : public TriggerSkill
{
public:
    Songsang() : TriggerSkill("songsang")
    {
        events << Death;
        frequency = Limited;
        limit_mark = "@funeral";
    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *player, QVariant &, ServerPlayer * &) const
    {
        return (TriggerSkill::triggerable(player) && player->getMark(limit_mark) > 0) ? QStringList(objectName()) : QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *pangtong, QVariant &, ServerPlayer *) const
    {
        if (pangtong->askForSkillInvoke(this)) {
            pangtong->broadcastSkillInvoke(objectName());
            room->removePlayerMark(pangtong, limit_mark);

            if (pangtong->isWounded())
                room->recover(pangtong, RecoverStruct(pangtong));
            else {
                LogMessage log;
                log.type = "#GainMaxHp";
                log.from = pangtong;
                log.arg = "1";
                log.arg2 = QString::number(pangtong->getMaxHp() + 1);
                room->sendLog(log);

                room->setPlayerProperty(pangtong, "maxhp", pangtong->getMaxHp() + 1);
            }
            room->acquireSkill(pangtong, "zhanji");
        }
        return false;
    }
};

class Zhanji : public TriggerSkill
{
public:
    Zhanji() : TriggerSkill("zhanji")
    {
        events << CardsMoveOneTime;
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (TriggerSkill::triggerable(player) && player->getPhase() == Player::Play && !room->getTag("FirstRound").toBool()) {
            QVariantList move_datas = data.toList();
            foreach(QVariant move_data, move_datas) {
                CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
                if (move.to == player && move.to_place == Player::PlaceHand && move.reason.m_reason == CardMoveReason::S_REASON_DRAW
                        && move.reason.m_skillName != objectName()) {
                    return QStringList(objectName());
                }
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());
        player->drawCards(1, objectName());
        return false;
    }
};

class JuesiDiscard : public OneCardViewAsSkill
{
public:
    JuesiDiscard() : OneCardViewAsSkill("juesi_discard")
    {
        filter_pattern = ".!";
        response_pattern = "@@juesi_discard!";
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        return originalCard;
    }
};

JuesiCard::JuesiCard()
{
    will_throw = true;
}

bool JuesiCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && Self->inMyAttackRange(to_select) && !to_select->isNude();
}

void JuesiCard::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *source = effect.from;
    Room *room = source->getRoom();
    ServerPlayer *target = effect.to;
    QList<int> cards = target->forceToDiscard(1, true);
    if (cards.isEmpty()) return;

    const Card *card = room->askForCard(target, "@@juesi_discard!", "@juesi-discard:" + source->objectName(), QVariant(), Card::MethodNone);
    if (card == NULL)
        card = Sanguosha->getCard(cards.first());

    bool isslash = card->isKindOf("Slash");
    CardMoveReason mreason(CardMoveReason::S_REASON_THROW, target->objectName(), QString(), QString(), QString());
    room->throwCard(card, mreason, target);
    if (!isslash && source->getHp() <= target->getHp()) {
        Duel *duel = new Duel(Card::NoSuit, 0);
        duel->setSkillName("_juesi");
        if (!source->isCardLimited(duel, Card::MethodUse) && !source->isProhibited(target, duel))
            room->useCard(CardUseStruct(duel, source, target));
        else
            delete duel;
    }
}

class Juesi : public OneCardViewAsSkill
{
public:
    Juesi() : OneCardViewAsSkill("juesi")
    {
        filter_pattern = "Slash!";
    }

    virtual bool isEnabledAtPlay(const Player *) const
    {
        return true;
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        JuesiCard *skillcard = new JuesiCard;
        skillcard->addSubcard(originalCard);
        return skillcard;
    }
};

JixuCard::JixuCard()
{

}

bool JixuCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    foreach (const Player *p, targets) {
        if (to_select->getHp() != p->getHp())
            return false;
    }
    return to_select != Self;
}

void JixuCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    QStringList reult = room->askForJixu(targets, "jixu", "jixu_haveslash+jixu_noslash",
                                         QVariant::fromValue(source), "@jixu-guess:"+source->objectName());

    bool have_slash = false;
    foreach (const Card *card, source->getHandcards()) {
        if (card->isKindOf("Slash")) {
            have_slash = true;
            break;
        }
    }
    room->addPlayerTip(source, have_slash?"#jixu_haveslash":"#jixu_noslash");

    QList<ServerPlayer *> wrong_targets;

    for (int i = 0; i < targets.length(); i++) {
        ServerPlayer *p = targets.at(i);
        if (reult.length() <= i || (have_slash == (reult.at(i) == "jixu_noslash"))) {
            room->addPlayerTip(p, "#jixu_wrong");
            wrong_targets << p;
        } else
            room->addPlayerTip(p, "#jixu_right");
    }

    if (wrong_targets.isEmpty()) {
        room->setPlayerFlag(source, "Global_PlayPhaseTerminated");
        return;
    }

    if (!have_slash) {
        foreach (ServerPlayer *p, wrong_targets) {
            if (source->canDiscard(p, "he")) {
                int to_throw = room->askForCardChosen(source, p, "he", "jixu", false, Card::MethodDiscard);
                room->throwCard(to_throw, p, source);
            }
        }
    }

    source->drawCards(wrong_targets.length(), "jixu");

}

class JixuViewAsSkill : public ZeroCardViewAsSkill
{
public:
    JixuViewAsSkill() : ZeroCardViewAsSkill("jixu")
    {

    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("JixuCard");
    }

    virtual const Card *viewAs() const
    {
        return new JixuCard;
    }
};

class Jixu : public TriggerSkill
{
public:
    Jixu() : TriggerSkill("jixu")
    {
        events << TargetChosed << EventPhaseChanging;
        view_as_skill = new JixuViewAsSkill;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseChanging && data.value<PhaseChangeStruct>().from == Player::Play) {
            room->removePlayerTip(player, "#jixu_haveslash");
            room->removePlayerTip(player, "#jixu_noslash");
            QList<ServerPlayer *> allplayers = room->getAllPlayers(true);
            foreach (ServerPlayer *p, allplayers) {
                room->removePlayerTip(p, "#jixu_right");
                room->removePlayerTip(p, "#jixu_wrong");
            }
        }
    }
    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (triggerEvent == TargetChosed && player->getMark("#jixu_haveslash") > 0) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card->isKindOf("Slash")) {
                QList<ServerPlayer *> available_targets = player->getUseExtraTargets(use);
                foreach (ServerPlayer *p, available_targets) {
                    if (p->getMark("#jixu_wrong") > 0)
                        return QStringList("jixu!");
                }
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        QList<ServerPlayer *> available_targets = player->getUseExtraTargets(use);
        foreach (ServerPlayer *p, available_targets) {
            if (p->getMark("#jixu_wrong") == 0)
                available_targets.removeOne(p);
        }

        foreach (ServerPlayer *p, available_targets) {
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), p->objectName());
            use.to.append(p);
        }
        room->sortByActionOrder(use.to);
        data = QVariant::fromValue(use);
        return false;
    }
};










class ChenqingViewAsSkill : public ViewAsSkill
{
public:
    ChenqingViewAsSkill() : ViewAsSkill("chenqing")
    {
        response_pattern = "@@chenqing!";
    }


    bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
        return selected.length() < 4 && !Self->isJilei(to_select);
    }

    const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.length() == 4) {
            DummyCard *discard = new DummyCard;
            discard->addSubcards(cards);
            return discard;
        }

        return NULL;
    }
};

class Chenqing : public TriggerSkill
{
public:
    Chenqing() : TriggerSkill("chenqing")
    {
        events << Dying << RoundStart;
        view_as_skill = new ChenqingViewAsSkill;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (triggerEvent == RoundStart && player->getMark("chenqing_times") > 0)
            room->setPlayerMark(player, "chenqing_times", 0);
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (triggerEvent != Dying) return QStringList();
        if (!TriggerSkill::triggerable(player) || player->getMark("chenqing_times") > 0) return QStringList();
        DyingStruct dying_data = data.value<DyingStruct>();
        QList<ServerPlayer *> targets = room->getOtherPlayers(player);
        targets.removeOne(dying_data.who);
        if (!targets.isEmpty())
            return QStringList(objectName());

        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *caiwenji, QVariant &data, ServerPlayer *) const
    {
        DyingStruct dying_data = data.value<DyingStruct>();
        QList<ServerPlayer *> targets = room->getOtherPlayers(caiwenji);
        targets.removeOne(dying_data.who);
        if (targets.isEmpty()) return false;
        ServerPlayer *target = room->askForPlayerChosen(caiwenji, targets, objectName(), "@chenqing-save:" + dying_data.who->objectName(), true, true);
        if (target) {
            caiwenji->broadcastSkillInvoke(objectName());
            room->addPlayerMark(caiwenji, "chenqing_times");
            target->drawCards(4, objectName());
            QList<int> all_cards = target->forceToDiscard(10086, true);
            QList<int> to_discard = target->forceToDiscard(4, true);
            if (all_cards.length() > 4){
                const Card *card = room->askForCard(target, "@@chenqing!", "@chenqing-discard:" + caiwenji->objectName() + ":" + dying_data.who->objectName(), data, Card::MethodNone);
                if (card != NULL && card->subcardsLength() == 4) {
                    to_discard = card->getSubcards();
                }
            }
            bool usepeach = false;
            if (to_discard.length() == 4) {
                usepeach = true;
                QStringList suitlist;
                foreach(int card_id, to_discard){
                    const Card *card = Sanguosha->getCard(card_id);
                    QString suit = card->getSuitString();
                    if (!suitlist.contains(suit))
                        suitlist << suit;
                    else{
                        usepeach = false;
                        break;
                    }
                }
            }

            if (!to_discard.isEmpty()) {
                DummyCard *dummy_card = new DummyCard(to_discard);
                dummy_card->deleteLater();
                CardMoveReason mreason(CardMoveReason::S_REASON_THROW, target->objectName(), QString(), objectName(), QString());
                room->throwCard(dummy_card, mreason, target);
            }

            if (usepeach) {
                Peach *peach = new Peach(Card::NoSuit, 0);
                peach->setSkillName(QString("_%1").arg(objectName()));
                if (!target->isCardLimited(peach, Card::MethodUse) && !target->isProhibited(dying_data.who, peach))
                    room->useCard(CardUseStruct(peach, target, dying_data.who));
                else
                    delete peach;
            }
        }
        return false;
    }

};

class MozhiViewAsSkill : public OneCardViewAsSkill
{
public:
    MozhiViewAsSkill() : OneCardViewAsSkill("mozhi")
    {
        response_pattern = "@@mozhi";
        response_or_use = true;
    }

    bool viewFilter(const Card *to_select) const
    {
        if (to_select->isEquipped())
            return false;
        QString mozhi_card = Self->property("mozhi").toString();
        if (mozhi_card.isEmpty()) return false;
        Card *use_card = Sanguosha->cloneCard(mozhi_card);
        use_card->addSubcard(to_select);
        return use_card->isAvailable(Self);
    }

    const Card *viewAs(const Card *originalCard) const
    {
        QString mozhi_card = Self->property("mozhi").toString();
        if (mozhi_card.isEmpty()) return NULL;
        Card *use_card = Sanguosha->cloneCard(mozhi_card);
        use_card->setCanRecast(false);
        use_card->addSubcard(originalCard);
        use_card->setSkillName("mozhi");
        return use_card;
     }
};

class Mozhi : public PhaseChangeSkill
{
public:
    Mozhi() : PhaseChangeSkill("mozhi")
    {
        view_as_skill = new MozhiViewAsSkill;
    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *player, QVariant &, ServerPlayer* &) const
    {
        if (!PhaseChangeSkill::triggerable(player) || player->getPhase() != Player::Finish) return QStringList();
        if (player->isKongcheng() && player->getHandPile().isEmpty()) return QStringList();
        QVariantList card_list = player->tag["PhaseUsedCards"].toList();
        foreach (QVariant card_data, card_list) {
            const Card *card = card_data.value<const Card *>();
            if (card && (card->getTypeId() == Card::TypeBasic || card->isNDTrick())) {
                Card *use_card = Sanguosha->cloneCard(card->objectName());
                use_card->setSkillName("mozhi");
                if (use_card->isAvailable(player))
                    return QStringList(objectName());
                else
                    return QStringList();
            }
        }
        return QStringList();
    }

    virtual bool onPhaseChange(ServerPlayer *player) const
    {
        Room *room = player->getRoom();
        int times = 0;
        QVariantList card_list = player->tag["PhaseUsedCards"].toList();
        foreach (QVariant card_data, card_list) {
            if (times >= 2 || (player->isKongcheng() && player->getHandPile().isEmpty())) break;
            const Card *card = card_data.value<const Card *>();
            if (card && (card->getTypeId() == Card::TypeBasic || card->isNDTrick())) {
                Card *use_card = Sanguosha->cloneCard(card->objectName());
                use_card->setSkillName("mozhi");
                if (use_card->isAvailable(player)) {
                    room->setPlayerProperty(player, "mozhi", card->objectName());
                    if (!room->askForUseCard(player, "@@mozhi", "@mozhi-use:::" + card->objectName()))
                        break;
                } else
                    break;
                ++times;
            }
        }
        return false;
    }
};

class Kunfen : public PhaseChangeSkill
{
public:
    Kunfen() : PhaseChangeSkill("kunfen")
    {

    }

    virtual Frequency getFrequency(const Player *target) const
    {
        if (target != NULL) {
            return target->getMark("fengliang") > 0 ? NotFrequent : Compulsory;
        }

        return Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        if (PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Finish) {
            if (getFrequency(target) != Compulsory && target->getHp() < 1) return false;
            return true;
        }
        return false;
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
        if (invoke(target))
            effect(target);

        return false;
    }

private:
    bool invoke(ServerPlayer *target) const
    {
        if (getFrequency(target) == Compulsory){
            Room *room = target->getRoom();
            room->sendCompulsoryTriggerLog(target, objectName());
            return true;
        }else
            return target->askForSkillInvoke(this);
    }

    void effect(ServerPlayer *target) const
    {
        Room *room = target->getRoom();

        target->broadcastSkillInvoke(objectName());

        room->loseHp(target);
        if (target->isAlive())
            target->drawCards(2, objectName());
    }
};

class Fengliang : public TriggerSkill
{
public:
    Fengliang() : TriggerSkill("fengliang")
    {
        frequency = Wake;
        events << Dying;
    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (!TriggerSkill::triggerable(player) || player->getMark(objectName()) > 0) return QStringList();
        DyingStruct dying = data.value<DyingStruct>();
        if (dying.who == player)
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());

        room->addPlayerMark(player, objectName(), 1);
        if (room->changeMaxHpForAwakenSkill(player) && player->getMark(objectName()) > 0) {
            int recover = 2 - player->getHp();
            room->recover(player, RecoverStruct(NULL, NULL, recover));
            room->handleAcquireDetachSkills(player, "tiaoxin");

            if (player->hasSkill("kunfen", true)) {
                QString translation = Sanguosha->translate(":kunfen-frequent");
                Sanguosha->addTranslationEntry(":kunfen", translation.toStdString().c_str());
                room->doNotify(player, QSanProtocol::S_COMMAND_UPDATE_SKILL, QVariant("kunfen"));
            }
        }

        return false;
    }
};

class JiqiaoViewAsSkill : public ViewAsSkill
{
public:
    JiqiaoViewAsSkill() : ViewAsSkill("jiqiao")
    {
        response_pattern = "@@jiqiao";
    }

    bool viewFilter(const QList<const Card *> &, const Card *to_select) const
    {
        return to_select->isKindOf("EquipCard") && !Self->isJilei(to_select);
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.isEmpty()) return NULL;

        DummyCard *discard = new DummyCard;
        discard->addSubcards(cards);
        return discard;
    }
};

class Jiqiao : public PhaseChangeSkill
{
public:
    Jiqiao() : PhaseChangeSkill("jiqiao")
    {
        view_as_skill = new JiqiaoViewAsSkill;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Play && !target->isNude();
    }

    virtual bool onPhaseChange(ServerPlayer *yueying) const
    {
        Room *room = yueying->getRoom();
        const Card *card = room->askForCard(yueying, "@jiqiao", "@jiqiao-card", QVariant(), objectName());
        if (card && card->subcardsLength() > 0) {
            QList<int> ids = room->getNCards(card->subcardsLength() * 2, false);
            CardsMoveStruct move(ids, yueying, Player::PlaceTable,
                CardMoveReason(CardMoveReason::S_REASON_TURNOVER, yueying->objectName(), objectName(), QString()));
            room->moveCardsAtomic(move, true);
            QList<int> card_to_throw;
            QList<int> card_to_gotback;
            foreach (int id, ids) {
                if (Sanguosha->getCard(id)->isKindOf("TrickCard"))
                    card_to_gotback << id;
                else
                    card_to_throw << id;
            }
            if (!card_to_gotback.isEmpty()) {
                DummyCard *dummy = new DummyCard(card_to_gotback);
                dummy->deleteLater();
                room->obtainCard(yueying, dummy);
            }
            if (!card_to_throw.isEmpty()) {
                DummyCard *dummy2 = new DummyCard(card_to_throw);
                dummy2->deleteLater();
                CardMoveReason reason(CardMoveReason::S_REASON_NATURAL_ENTER, yueying->objectName(), objectName(), QString());
                room->throwCard(dummy2, reason, NULL);
            }
        }
        return false;
    }
};

class Linglong : public MaxCardsSkill
{
public:
    Linglong() : MaxCardsSkill("linglong")
    {
    }

    virtual int getExtra(const Player *target) const
    {
        if (target->hasSkill("linglong") && !target->getOffensiveHorse() && !target->getDefensiveHorse())
            return 1;
        else
            return 0;
    }

    int getEffectIndex(const ServerPlayer *, const QString &prompt) const
    {
        if (prompt == "MaxCardsSkill")
            return 2;
        return -1;
    }
};

class LinglongTrigger : public TriggerSkill
{
public:
    LinglongTrigger() : TriggerSkill("#linglong")
    {
        frequency = Compulsory;
        events << CardAsked << CardsMoveOneTime << EventAcquireSkill << EventLoseSkill << GameStart;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *yueying, QVariant &data) const
    {
        if (triggerEvent == CardAsked && yueying->hasSkill("linglong") && !yueying->getArmor() && yueying->hasArmorEffect("eight_diagram")){
            QString pattern = data.toStringList().first();

            if (pattern != "jink")
                return false;
            room->sendCompulsoryTriggerLog(yueying, "linglong");
            yueying->broadcastSkillInvoke("linglong", 1);
            if (!yueying->askForSkillInvoke("eight_diagram"))
                return false;
            room->setEmotion(yueying, "armor/eight_diagram");
            JudgeStruct judge;
            judge.pattern = ".|red";
            judge.good = true;
            judge.reason = "eight_diagram";
            judge.who = yueying;

            room->judge(judge);

            if (judge.isGood()) {
                Jink *jink = new Jink(Card::NoSuit, 0);
                jink->setSkillName("eight_diagram");
                room->provide(jink);
                return true;
            }
        }else if (triggerEvent == EventLoseSkill && data.toString() == "linglong") {
            room->handleAcquireDetachSkills(yueying, "-nosqicai", true);
            yueying->setMark("linglong_qicai", 0);
        } else if ((triggerEvent == EventAcquireSkill && data.toString() == "linglong") || (triggerEvent == GameStart && yueying->hasSkill("linglong"))) {
            if (yueying->getTreasure() == NULL) {
                room->handleAcquireDetachSkills(yueying, "nosqicai");
                yueying->setMark("linglong_qicai", 1);
            }
        } else if (triggerEvent == CardsMoveOneTime && yueying->isAlive() && yueying->hasSkill(this, true)) {
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            if (move.to == yueying && move.to_place == Player::PlaceEquip) {
                if (yueying->getTreasure() != NULL && yueying->getMark("linglong_qicai") > 0) {
                    room->handleAcquireDetachSkills(yueying, "-nosqicai", true);
                    yueying->setMark("linglong_qicai", 0);
                }
            } else if (move.from == yueying && move.from_places.contains(Player::PlaceEquip)) {
                if (yueying->getTreasure() == NULL && yueying->getMark("linglong_qicai") == 0) {
                    room->handleAcquireDetachSkills(yueying, "nosqicai");
                    yueying->setMark("linglong_qicai", 1);
                }
            }
        }

        return false;
    }
};

DuliangCard::DuliangCard()
{

}

bool DuliangCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && !to_select->isKongcheng() && to_select != Self;
}

void DuliangCard::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.from->getRoom();
    if (effect.to->isKongcheng()) return;
    int card_id = room->askForCardChosen(effect.from, effect.to, "h", "duliang");
    CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, effect.from->objectName());
    room->obtainCard(effect.from, Sanguosha->getCard(card_id), reason, room->getCardPlace(card_id) != Player::PlaceHand);
    if (room->askForChoice(effect.from, "duliang", "send+delay", QVariant(), "@duliang-choose::" + effect.to->objectName()) == "delay") {
        room->addPlayerMark(effect.to, "#duliang");
    }
    else {
        QList<int> ids = room->getNCards(2, false);
        LogMessage log;
        log.type = "$ViewDrawPile";
        log.from = effect.to;
        log.card_str = IntList2StringList(ids).join("+");
        room->sendLog(log, effect.to);
        room->fillAG(ids, effect.to);
        room->getThread()->delay(2000);
        room->clearAG(effect.to);
        room->returnToTopDrawPile(ids);
        QList<int> to_obtain;
        foreach (int card_id, ids)
            if (Sanguosha->getCard(card_id)->getTypeId() == Card::TypeBasic)
                to_obtain << card_id;
        if (!to_obtain.isEmpty()) {
            DummyCard *dummy = new DummyCard(to_obtain);
            effect.to->obtainCard(dummy, false);
            delete dummy;
        }
    }
}

class DuliangViewAsSkill : public ZeroCardViewAsSkill
{
public:
    DuliangViewAsSkill() : ZeroCardViewAsSkill("duliang")
    {
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("DuliangCard");
    }

    virtual const Card *viewAs() const
    {
        return new DuliangCard;
    }
};

class Duliang : public TriggerSkill
{
public:
    Duliang() : TriggerSkill("duliang")
    {
        events << DrawNCards << EventPhaseEnd;
        view_as_skill = new DuliangViewAsSkill;
    }

    int getPriority(TriggerEvent triggerEvent) const
    {
        if (triggerEvent == DrawNCards)
            return 6;
        return TriggerSkill::getPriority(triggerEvent);
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target && target->getMark("#duliang") > 0;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *target, QVariant &data) const
    {
        if (triggerEvent == DrawNCards)
            data = data.toInt() + target->getMark("#duliang");
        else if (target->getPhase() == Player::Draw)
            room->setPlayerMark(target, "#duliang", 0);
        return false;
    }
};

class Fulin : public HideCardSkill
{
public:
    Fulin() : HideCardSkill("fulin")
    {

    }
    virtual bool isCardHided(const Player *player, const Card *card) const
    {
        if (!player->hasSkill(objectName())) return false;
        QStringList card_list = player->property("GlobalGetCards").toString().split("+");
        foreach (QString id, card_list) {
            bool ok;
            if (id.toInt(&ok) == card->getEffectiveId() && ok)
                return true;
        }
        return false;
    }
};

ZiyuanCard::ZiyuanCard()
{
    will_throw = false;
    handling_method = Card::MethodNone;
}

void ZiyuanCard::extraCost(Room *room, const CardUseStruct &card_use) const
{
    CardMoveReason reason(CardMoveReason::S_REASON_GIVE, card_use.from->objectName(), card_use.to.first()->objectName(), "ziyuan", QString());
    room->obtainCard(card_use.to.first(), this, reason, false);
}

void ZiyuanCard::onEffect(const CardEffectStruct &effect) const
{
    effect.from->getRoom()->recover(effect.to, RecoverStruct(effect.from));
}

class Ziyuan : public ViewAsSkill
{
public:
    Ziyuan() : ViewAsSkill("ziyuan")
    {
    }

    virtual bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
        int sum = 0;
        foreach(const Card *card, selected)
            sum += card->getNumber();

        sum += to_select->getNumber();
        return !to_select->isEquipped() && sum <= 13;
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("ZiyuanCard");
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const
    {
        int sum = 0;
        foreach(const Card *card, cards)
            sum += card->getNumber();

        if (sum != 13)
            return NULL;

        ZiyuanCard *rende_card = new ZiyuanCard;
        rende_card->addSubcards(cards);
        return rende_card;
    }
};

class Jugu : public TriggerSkill
{
public:
    Jugu() : TriggerSkill("jugu")
    {
        events << TurnStart;
        frequency = Compulsory;
    }

    virtual TriggerList triggerable(TriggerEvent , Room *room, ServerPlayer *, QVariant &) const
    {
        TriggerList skill_list;
        if (!room->getTag("FirstRound").toBool()) return skill_list;
        QList<ServerPlayer *> mizhus = room->findPlayersBySkillName(objectName());
        foreach (ServerPlayer *mizhu, mizhus)
            skill_list.insert(mizhu, QStringList(objectName()));
        return skill_list;
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *, QVariant &, ServerPlayer *mizhu) const
    {
        room->sendCompulsoryTriggerLog(mizhu, objectName());
        room->broadcastSkillInvoke(objectName(), mizhu);
        mizhu->drawCards(mizhu->getMaxHp(), objectName());
        return false;
    }
};

class JuguKeep : public MaxCardsSkill
{
public:
    JuguKeep() : MaxCardsSkill("#jugu-keep")
    {
    }

    virtual int getExtra(const Player *target) const
    {
        if (target->hasSkill("jugu"))
            return target->getMaxHp();
        else
            return 0;
    }
};


class QinguoViewAsSkill : public ZeroCardViewAsSkill
{
public:
    QinguoViewAsSkill() : ZeroCardViewAsSkill("qinguo")
    {
        response_pattern = "@@qinguo";
    }

    const Card *viewAs() const
    {
        Slash *slash = new Slash(Card::NoSuit, 0);
        slash->setSkillName("qinguo");
        return slash;
    }
};

FumanCard::FumanCard()
{
    handling_method = Card::MethodNone;
}

bool FumanCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    QStringList fuman_prop = Self->property("fuman").toString().split("+");
    return !fuman_prop.contains(to_select->objectName()) && targets.isEmpty() && to_select != Self;
}

void FumanCard::extraCost(Room *room, const CardUseStruct &card_use) const
{
    CardMoveReason reason(CardMoveReason::S_REASON_GIVE, card_use.from->objectName(), card_use.to.first()->objectName(), "fuman", QString());
    room->obtainCard(card_use.to.first(), this, reason);
}

void FumanCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    ServerPlayer *target = targets.first();
    QSet<QString> fuman_prop = source->property("fuman").toString().split("+").toSet();
    fuman_prop.insert(target->objectName());
    room->setPlayerProperty(source, "fuman", QStringList(fuman_prop.toList()).join("+"));
}

class FumanViewAsSkill : public OneCardViewAsSkill
{
public:
    FumanViewAsSkill() : OneCardViewAsSkill("fuman")
    {
        filter_pattern = "Slash";
    }

    bool isEnabledAtPlay(const Player *) const
    {
        return true;
    }

    const Card *viewAs(const Card *originalCard) const
    {
        FumanCard *card = new FumanCard;
        card->addSubcard(originalCard);
        return card;
    }
};

class Fuman : public TriggerSkill
{
public:
    Fuman() : TriggerSkill("fuman")
    {
        events << CardUsed << CardResponded << CardsMoveOneTime << EventPhaseChanging;
        view_as_skill = new FumanViewAsSkill;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == CardsMoveOneTime) {
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            if (!move.from || move.from != player) return false;
            if (move.to && move.to_place == Player::PlaceHand && move.reason.m_reason == CardMoveReason::S_REASON_GIVE && move.reason.m_skillName == objectName()) {
                QVariantList fuman_ids = player->tag["fuman_ids"].toList();
                ServerPlayer *target = (ServerPlayer *)move.to;
                foreach (int id, move.card_ids) {
                    if (room->getCardOwner(id) == target && room->getCardPlace(id) == Player::PlaceHand)
                        fuman_ids << id;
                }
                player->tag["fuman_ids"] = fuman_ids;
            }else if ((move.reason.m_reason & CardMoveReason::S_MASK_BASIC_REASON) != CardMoveReason::S_REASON_USE) {
                foreach (ServerPlayer *p, room->getAlivePlayers()) {
                    QVariantList fuman_ids = p->tag["fuman_ids"].toList();
                    QVariantList new_ids;
                    foreach (QVariant card_data, fuman_ids) {
                        int card_id = card_data.toInt();
                        if (!move.card_ids.contains(card_id))
                            new_ids << card_id;
                    }
                    p->tag["fuman_ids"] = new_ids;
                }
            }
        } else if (triggerEvent == EventPhaseChanging) {
            if (data.value<PhaseChangeStruct>().to != Player::NotActive) return false;
            foreach (ServerPlayer *p, room->getAlivePlayers()) {
                QVariantList fuman_ids = p->tag["fuman_ids"].toList();
                QVariantList new_ids;
                foreach (QVariant card_data, fuman_ids) {
                    int card_id = card_data.toInt();
                    if (!player->handCards().contains(card_id))
                        new_ids << card_id;
                }
                p->tag["fuman_ids"] = new_ids;
            }
        } else {
            const Card *usecard = NULL;
            if (triggerEvent == CardUsed)
                usecard = data.value<CardUseStruct>().card;
            else if (triggerEvent == CardResponded) {
                CardResponseStruct resp = data.value<CardResponseStruct>();
                if (resp.m_isUse)
                    usecard = resp.m_card;
            }
            if (usecard && usecard->getTypeId() != Card::TypeSkill) {
                foreach (ServerPlayer *p, room->getAlivePlayers()) {
                    bool can_invoke = false;
                    QVariantList fuman_ids = p->tag["fuman_ids"].toList();
                    QVariantList new_ids;
                    foreach (QVariant card_data, fuman_ids) {
                        int card_id = card_data.toInt();
                        if (usecard->getSubcards().contains(card_id))
                            can_invoke = true;
                        else
                            new_ids << card_id;
                    }
                    p->tag["fuman_ids"] = new_ids;
                    if (can_invoke) {
                        LogMessage log;
                        log.type = "#SkillForce";
                        log.from = p;
                        log.arg = objectName();
                        room->sendLog(log);
                        p->drawCards(1, "fuman");
                    }
                }
            }
        }
        return false;
    }
};

DuanfaCard::DuanfaCard()
{
    target_fixed = true;
}

void DuanfaCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const
{
    if (source->isAlive()) {
        int n = subcards.length();
        room->addPlayerMark(source, "DuanfaDiscardCount", n);
        source->drawCards(n, "duanfa");
    }
}

class DuanfaViewAsSkill : public ViewAsSkill
{
public:
    DuanfaViewAsSkill() : ViewAsSkill("duanfa")
    {
    }

    virtual bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
        return selected.length()+Self->getMark("DuanfaDiscardCount") < 3 && to_select->isBlack() && !Self->isJilei(to_select);
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.isEmpty()) return NULL;

        DuanfaCard *zhiheng_card = new DuanfaCard;
        zhiheng_card->addSubcards(cards);
        zhiheng_card->setSkillName(objectName());
        return zhiheng_card;
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return player->getMark("DuanfaDiscardCount") < 3;
    }
};

class Duanfa : public TriggerSkill
{
public:
    Duanfa() : TriggerSkill("duanfa")
    {
        events << EventPhaseChanging;
        view_as_skill = new DuanfaViewAsSkill;
    }

    bool triggerable(const ServerPlayer *) const
    {
        return false;
    }

    virtual void record(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (data.value<PhaseChangeStruct>().from == Player::Play)
            room->setPlayerMark(player, "DuanfaDiscardCount", 0);
    }
};

class Youdi : public PhaseChangeSkill
{
public:
    Youdi() : PhaseChangeSkill("youdi")
    {

    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Finish && !target->isKongcheng();
    }

    virtual bool onPhaseChange(ServerPlayer *player) const
    {
        Room *room = player->getRoom();
        QList<ServerPlayer *> targets, oplayers = room->getOtherPlayers(player);
        foreach (ServerPlayer *p, oplayers) {
            if (p->canDiscard(player, "h"))
                targets << p;
        }
        ServerPlayer *target = room->askForPlayerChosen(player, targets, objectName(), "@youdi-target", true, true);
        if (target){
            player->broadcastSkillInvoke(objectName());
            int to_throw = room->askForCardChosen(target, player, "h", objectName(), false, Card::MethodDiscard);
            bool get = !Sanguosha->getCard(to_throw)->isKindOf("Slash");
            bool draw = !Sanguosha->getCard(to_throw)->isBlack();
            room->throwCard(to_throw, player, target);

            if (get && player->canGetCard(target, "he")) {
                int card_id = room->askForCardChosen(player, target, "he", objectName(), false, Card::MethodGet);
                CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, player->objectName());
                room->obtainCard(player, Sanguosha->getCard(card_id), reason, false);
            }
            if (draw && player->isAlive())
                player->drawCards(1, objectName());
        }
        return false;
    }
};

class Qizhou : public TriggerSkill
{
public:
    Qizhou() : TriggerSkill("qizhou")
    {
        events << PreCardsMoveOneTime << EventAcquireSkill << EventLoseSkill;
        frequency = Compulsory;
    }

    bool triggerable(const ServerPlayer *) const
    {
        return false;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventLoseSkill) {
            if (data.toString() == objectName()) {
                QStringList qizhou_skills = player->tag["QizhouSkills"].toStringList();
                QStringList detachList;
                foreach(QString skill_name, qizhou_skills)
                    detachList.append("-" + skill_name);
                room->handleAcquireDetachSkills(player, detachList);
                player->tag["QizhouSkills"] = QVariant();
            }
            return;
        } else if (triggerEvent == EventAcquireSkill) {
            if (data.toString() != objectName()) return;
        }

        if (!player->isAlive() || !player->hasSkill(this, true)) return;

        acquired_skills.clear();
        detached_skills.clear();
        QizhouChange(room, player, 1, "mashu");
        QizhouChange(room, player, 2, "yingzi");
        QizhouChange(room, player, 3, "duanbing");
        QizhouChange(room, player, 4, "fenwei");
        if (!acquired_skills.isEmpty() || !detached_skills.isEmpty())
            room->handleAcquireDetachSkills(player, acquired_skills + detached_skills);
    }

private:
    void QizhouChange(Room *room, ServerPlayer *player, int x, const QString &skill_name) const
    {
        QStringList qizhou_skills = player->tag["QizhouSkills"].toStringList();
        QStringList suitlist;
        foreach (const Card *card, player->getEquips()) {
            QString suit = card->getSuitString();
            if (!suitlist.contains(suit))
                suitlist << suit;
        }
        if (suitlist.length() >= x) {
            if (!qizhou_skills.contains(skill_name)) {
                room->notifySkillInvoked(player, "qizhou");
                acquired_skills.append(skill_name);
                qizhou_skills << skill_name;
            }
        } else {
            if (qizhou_skills.contains(skill_name)) {
                detached_skills.append("-" + skill_name);
                qizhou_skills.removeOne(skill_name);
            }
        }
        player->tag["QizhouSkills"] = QVariant::fromValue(qizhou_skills);
    }

    mutable QStringList acquired_skills, detached_skills;
};

ShanxiCard::ShanxiCard()
{
}

bool ShanxiCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && Self->inMyAttackRange(to_select) && Self->canDiscard(to_select, "he");
}

void ShanxiCard::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.from->getRoom();
    if (effect.from->canDiscard(effect.to, "he")) {
        const Card *card = Sanguosha->getCard(room->askForCardChosen(effect.from, effect.to, "he", "shanxi", false, Card::MethodDiscard));
        bool isjink = card->isKindOf("Jink");
        room->throwCard(card, effect.to, effect.from);
        if (isjink && !effect.to->isKongcheng())
            room->doGongxin(effect.from, effect.to, QList<int>(), "shanxi");
        else if (!isjink && !effect.from->isKongcheng())
            room->doGongxin(effect.to, effect.from, QList<int>(), "shanxi");
    }
}

class Shanxi : public OneCardViewAsSkill
{
public:
    Shanxi() : OneCardViewAsSkill("shanxi")
    {
        filter_pattern = "BasicCard|red!";
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("ShanxiCard");
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        ShanxiCard *sx = new ShanxiCard;
        sx->addSubcard(originalCard);
        return sx;
    }
};

class Zhenwei : public TriggerSkill
{
public:
    Zhenwei() : TriggerSkill("zhenwei")
    {
        events << TargetConfirming << EventPhaseChanging;
    }

    virtual TriggerList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        TriggerList skill_list;
        if (triggerEvent == TargetConfirming) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card && (use.card->isKindOf("Slash") || (use.card->getTypeId() == Card::TypeTrick && use.card->isBlack()))) {
                foreach (ServerPlayer *to, use.to) {
                    if (to->isAlive() && to != player) return skill_list;
                }
                QList<ServerPlayer *> wenpins = room->findPlayersBySkillName(objectName());
                foreach (ServerPlayer *wenpin, wenpins) {
                    if (!wenpin->isNude() && wenpin->getHp() > player->getHp())
                        skill_list.insert(wenpin, QStringList(objectName()));
                }
            }
        } else if (triggerEvent == EventPhaseChanging) {
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (change.to != Player::NotActive) return skill_list;
            QList<ServerPlayer *> all = room->getAlivePlayers();
            foreach (ServerPlayer *p, all) {
                if (!p->getPile("zhenwei_zheng").isEmpty()) {
                    skill_list.insert(p, QStringList("zhenwei!"));
                }
            }
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *ask_who) const
    {
        if (triggerEvent == EventPhaseChanging) {
            DummyCard *dummy = new DummyCard(ask_who->getPile("army"));
            CardMoveReason reason(CardMoveReason::S_REASON_EXCHANGE_FROM_PILE, ask_who->objectName(), objectName(), QString());
            room->obtainCard(ask_who, dummy, reason, false);
            delete dummy;
        } else {
            CardUseStruct use = data.value<CardUseStruct>();
            ServerPlayer *wp = ask_who;
            if (!room->askForCard(ask_who, "..", QString("@zhenwei:%1").arg(player->objectName()), data, objectName()))
                return false;
            QString choice = room->askForChoice(wp, objectName(), "draw+null", data);
            if (choice == "draw") {
                room->drawCards(wp, 1, objectName());
                if (use.card->isKindOf("Slash")) {
                    if (!use.from->canSlash(wp, use.card, false))
                        return false;
                }
                if (use.from->isProhibited(wp, use.card))
                    return false;
                if (use.card->isKindOf("Collateral")) {
                    ServerPlayer *victim = player->tag["collateralVictim"].value<ServerPlayer *>();
                    player->tag.remove("collateralVictim");
                    wp->tag["collateralVictim"] = QVariant::fromValue(victim);
                }

                use.to = QList<ServerPlayer *>();
                use.to << wp;
                data = QVariant::fromValue(use);
                return true;
            } else {
                use.from->addToPile("zhenwei_zheng", use.card);
                use.to.clear();
                data = QVariant::fromValue(use);
                return true;
            }
        }
        return false;
    }
};

class Qinguo : public TriggerSkill
{
public:
    Qinguo() : TriggerSkill("qinguo")
    {
        events << CardFinished << CardsMoveOneTime;
        view_as_skill = new QinguoViewAsSkill;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        if (triggerEvent == CardFinished && player->getPhase() != Player::NotActive) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card->getTypeId() == Card::TypeEquip)
                return QStringList(objectName());
        } else if (triggerEvent == CardsMoveOneTime && player->getEquips().length() == player->getHp() && player->isWounded()) {
            int x=0, y=0;
            QVariantList move_datas = data.toList();
            foreach(QVariant move_data, move_datas) {
                CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
                for (int i = 0; i < move.card_ids.size(); i++) {
                    if (move.from == player && move.from_places[i] == Player::PlaceEquip)
                        x++;
                    if (move.to == player && move.to_place == Player::PlaceEquip)
                        y++;
                }
            }
            if (x != y)
                return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        if (triggerEvent == CardFinished) {
            room->askForUseCard(player, "@@qinguo", "@qinguo-slash");
        } else if (triggerEvent == CardsMoveOneTime) {
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            room->recover(player, RecoverStruct(player));
        }

        return false;
    }
};

KannanCard::KannanCard()
{
}

bool KannanCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    QStringList names = Self->property("kannantargets").toString().split("+");
    return targets.isEmpty() && Self->canPindian(to_select) && !names.contains(to_select->objectName());
}

void KannanCard::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *source = effect.from;
    Room *room = source->getRoom();
    ServerPlayer *target = effect.to;
    QSet<QString> names = source->property("kannantargets").toString().split("+").toSet();
    names.insert(target->objectName());
    room->setPlayerProperty(source, "kannantargets", QStringList(names.toList()).join("+"));

    PindianStruct *pd = source->pindianStruct(target, "kannan", NULL);
    int x1 = pd->from_number, x2 = pd->to_number;
    if (x1>x2) {
        room->addPlayerMark(source, "#kannan");
        room->setPlayerFlag(source, "KannanCannot");
    } else if (x1<x2)
        room->addPlayerMark(target, "#kannan");
}

class KannanViewAsSkill : public ZeroCardViewAsSkill
{
public:
    KannanViewAsSkill() : ZeroCardViewAsSkill("kannan")
    {
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->isKongcheng() && !player->hasFlag("KannanCannot");
    }

    virtual const Card *viewAs() const
    {
        return new KannanCard;
    }
};

class Kannan : public TriggerSkill
{
public:
    Kannan() : TriggerSkill("kannan")
    {
        events << EventPhaseChanging;
        view_as_skill = new KannanViewAsSkill;
    }

    bool triggerable(const ServerPlayer *) const
    {
        return false;
    }

    virtual void record(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (data.value<PhaseChangeStruct>().from == Player::Play) {
            room->setPlayerFlag(player, "-KannanCannot");
            room->setPlayerProperty(player, "kannantargets", QVariant());
        }
    }
};

class Fengpo : public TriggerSkill
{
public:
    Fengpo() : TriggerSkill("fengpo")
    {
        events << TargetSpecified;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (TriggerSkill::triggerable(player) &&player->getPhase() == Player::Play) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card != NULL && (use.card->isKindOf("Slash") || use.card->isKindOf("Duel"))
                    && player->getCardUsedTimes("Slash+Duel|play") == 1) {
                ServerPlayer *to = use.to.at(use.index);
                foreach (ServerPlayer *p, use.to) {
                    if (p->isAlive() && p != to)
                        return QStringList();
                }
                if (to && to->isAlive()) {
                    return QStringList(objectName());
                }
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

            int n = 0;
            foreach (const Card *card, to->getHandcards())
                if (card->getSuit() == Card::Diamond)
                    ++n;
            QString choice = room->askForChoice(player, objectName(), "drawCards+addDamage");
            if (choice == "drawCards" && n > 0){
                player->drawCards(n, objectName());
            }else if (choice == "addDamage") {
                QVariantList damage_list = use.card->tag["Damage_List"].toList();
                damage_list[index] = damage_list[index].toInt()+n;
                use.card->setTag("Damage_List", damage_list);
            }
        }

        return false;
    }
};

XuejiCard::XuejiCard()
{
}

bool XuejiCard::targetFilter(const QList<const Player *> &targets, const Player *, const Player *Self) const
{
    return targets.length() < qMax(Self->getLostHp(), 1);
}

void XuejiCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    foreach (ServerPlayer *p, targets) {
        if (p->isAlive() && !p->isChained())
            room->setPlayerProperty(p, "chained", true);
    }
    QList<ServerPlayer *> can_select;
    foreach (ServerPlayer *p, targets) {
        if (p->isAlive())
            can_select << p;
    }
    if (!can_select.isEmpty()) {
        ServerPlayer *to_damage = room->askForPlayerChosen(source, can_select, "xueji", "@xueji-target");
        room->damage(DamageStruct("xueji", source, to_damage, 1, DamageStruct::Fire));
    }
}

class Xueji : public OneCardViewAsSkill
{
public:
    Xueji() : OneCardViewAsSkill("xueji")
    {
        filter_pattern = ".|red!";
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("XuejiCard");
    }

    const Card *viewAs(const Card *originalcard) const
    {
        XuejiCard *first = new XuejiCard;
        first->addSubcard(originalcard->getId());
        first->setSkillName(objectName());
        return first;
    }
};

class Huxiao : public TriggerSkill
{
public:
    Huxiao() : TriggerSkill("huxiao")
    {
        events << Damage << EventPhaseStart;
        frequency = Compulsory;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (triggerEvent == EventPhaseStart && player->getPhase()== Player::NotActive) {
            foreach (ServerPlayer *p, room->getAlivePlayers())
                room->setPlayerProperty(p, "huxiao_targets", QVariant());
        }
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (triggerEvent == Damage && TriggerSkill::triggerable(player)) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.nature == DamageStruct::Fire && damage.to && damage.to->isAlive())
                return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());
        if (damage.to && damage.to->isAlive()) {
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), damage.to->objectName());
            damage.to->drawCards(1, objectName());
            QStringList assignee_list = player->property("huxiao_targets").toString().split("+");
            assignee_list << damage.to->objectName();
            room->setPlayerProperty(player, "huxiao_targets", assignee_list.join("+"));
        }
        return false;
    }
};

class HuxiaoTargetMod : public TargetModSkill
{
public:
    HuxiaoTargetMod() : TargetModSkill("#huxiao-target")
    {
        pattern = "^SkillCard";
    }

    int getResidueNum(const Player *from, const Card *, const Player *to) const
    {
        QStringList assignee_list = from->property("huxiao_targets").toString().split("+");
        if (to && assignee_list.contains(to->objectName()))
            return 10000;
        return 0;
    }
};

class Wuji : public PhaseChangeSkill
{
public:
    Wuji() : PhaseChangeSkill("wuji")
    {
        frequency = Wake;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target)
            && target->getPhase() == Player::Finish
            && target->getMark("wuji") == 0
            && target->getMark("damage_point_round") > 2;
    }

    bool onPhaseChange(ServerPlayer *player) const
    {
        Room *room = player->getRoom();
        room->sendCompulsoryTriggerLog(player, objectName());

        player->broadcastSkillInvoke(objectName());

        room->setPlayerMark(player, "wuji", 1);
        if (room->changeMaxHpForAwakenSkill(player, 1)) {
            room->recover(player, RecoverStruct(player));
            room->detachSkillFromPlayer(player, "huxiao");

            int blade_id = 57;
            const Card *blade = Sanguosha->getCard(blade_id);
            Player::Place place = room->getCardPlace(blade_id);
            if (place == Player::PlaceDelayedTrick || place == Player::PlaceEquip || place == Player::DrawPile || place == Player::DiscardPile)
                player->obtainCard(blade);

        }

        return false;
    }
};

class Yinbing : public TriggerSkill
{
public:
    Yinbing() : TriggerSkill("yinbing")
    {
        events << EventPhaseStart << Damaged;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        if (triggerEvent == EventPhaseStart && player->getPhase() == Player::Finish && !player->isNude())
            return QStringList(objectName());
        else if (triggerEvent == Damaged && !player->getPile("kerchief").isEmpty()) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.card && (damage.card->isKindOf("Slash") || damage.card->isKindOf("Duel")))
                return QStringList("yinbing!");
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        if (triggerEvent == EventPhaseStart) {
            const Card *card = room->askForExchange(player, objectName(), 998, 1, true, "@yinbing-put", true, "^BasicCard");
            if (card) {
                player->broadcastSkillInvoke(objectName());
                room->notifySkillInvoked(player, objectName());
                LogMessage log;
                log.from = player;
                log.type = "#InvokeSkill";
                log.arg = objectName();
                room->sendLog(log);
                player->addToPile("kerchief", card->getSubcards(), true);
            }
        } else if (triggerEvent == Damaged && !player->getPile("kerchief").isEmpty()) {
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());

            QList<int> ids = player->getPile("kerchief");
            room->fillAG(ids, player);
            int id = room->askForAG(player, ids, false, objectName());
            room->clearAG(player);
            CardMoveReason reason(CardMoveReason::S_REASON_REMOVE_FROM_PILE, QString(), player->objectName(), "yinbing", QString());
            room->throwCard(Sanguosha->getCard(id), reason, NULL);
        }
        return false;
    }
};

class Juedi : public PhaseChangeSkill
{
public:
    Juedi() : PhaseChangeSkill("juedi")
    {
        frequency = Compulsory;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Start && !target->getPile("kerchief").isEmpty();
    }

    bool onPhaseChange(ServerPlayer *target) const
    {
        Room *room = target->getRoom();
        room->sendCompulsoryTriggerLog(target, objectName());
        target->broadcastSkillInvoke(objectName());
        QStringList choices;
        choices << "self";
        QList<ServerPlayer *> playerlist;
        foreach (ServerPlayer *p, room->getOtherPlayers(target)) {
            if (p->getHp() <= target->getHp())
                playerlist << p;
        }
        if (!playerlist.isEmpty())
            choices << "give";
        if (room->askForChoice(target, objectName(), choices.join("+"), QVariant(), QString(), "self+give") == "give") {
            ServerPlayer *to_give = room->askForPlayerChosen(target, playerlist, objectName(), "@juedi");
            int len = target->getPile("kerchief").length();
            DummyCard *dummy = new DummyCard(target->getPile("kerchief"));
            dummy->deleteLater();
            CardMoveReason reason(CardMoveReason::S_REASON_GIVE, target->objectName(), to_give->objectName(), "juedi", QString());
            room->obtainCard(to_give, dummy, reason);
            room->recover(to_give, RecoverStruct(target));
            room->drawCards(to_give, len, objectName());
        } else {
            target->clearOnePrivatePile("kerchief");
            int x = target->getMaxHp() - target->getHandcardNum();
            if (x > 0)
                target->drawCards(x, objectName());
        }
        return false;
    }
};

WenguaCard::WenguaCard()
{
    target_fixed = true;
    m_skillName = "wengua";
    will_throw = false;
    handling_method = Card::MethodNone;
}

void WenguaCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const
{
    CardMoveReason reason(CardMoveReason::S_REASON_PUT, source->objectName(), QString(), "wengua", QString());
    room->moveCardTo(this, NULL, user_string == "top" ? Player::DrawPile : Player::DrawPileBottom, reason, false);
    source->drawCards(1, "wengua", user_string != "top");
}

class WenguaViewAsSkill : public OneCardViewAsSkill
{
public:
    WenguaViewAsSkill() : OneCardViewAsSkill("wengua")
    {
        filter_pattern = ".|.|.|.";
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("WenguaCard");
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        WenguaCard *wengua_card = new WenguaCard;
        wengua_card->addSubcard(originalCard->getId());
        return wengua_card;
    }
};

WenguaAttachCard::WenguaAttachCard()
{
    will_throw = false;
    handling_method = Card::MethodNone;
    m_skillName = "wengua_attach";
    mute = true;
}

void WenguaAttachCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    ServerPlayer *xushi = targets.first();
    room->setPlayerFlag(xushi, "WenguaInvoked");
    LogMessage log;
    log.type = "#InvokeOthersSkill";
    log.from = source;
    log.to << xushi;
    log.arg = "wengua";
    room->sendLog(log);
    room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, source->objectName(), xushi->objectName());
    xushi->broadcastSkillInvoke("wengua");

    room->notifySkillInvoked(xushi, "wengua");
    CardMoveReason reason(CardMoveReason::S_REASON_GIVE, source->objectName(), xushi->objectName(), "wengua", QString());
    room->obtainCard(xushi, this, reason, false);
    const Card *card = Sanguosha->getCard(getEffectiveId());
    xushi->setMark("wenguacard_id", getEffectiveId()); // For AI
    QString choice = room->askForChoice(xushi, "wengua", "top+bottom+cancel", QVariant(), "@wengua-choose:::"+card->objectName());
    xushi->setMark("wenguacard_id", getEffectiveId()); // For AI
    if (choice == "cancel") return;
    CardMoveReason reason2(CardMoveReason::S_REASON_PUT, xushi->objectName(), QString(), "wengua", QString());
    room->moveCardTo(this, NULL, choice == "top" ? Player::DrawPile : Player::DrawPileBottom, reason2, false);

    source->drawCards(1, "wengua", choice != "top");
    xushi->drawCards(1, "wengua", choice != "top");
}

bool WenguaAttachCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select->hasSkill("wengua") && to_select != Self && !to_select->hasFlag("WenguaInvoked");
}

class WenguaAttach : public OneCardViewAsSkill
{
public:
    WenguaAttach() : OneCardViewAsSkill("wengua_attach")
    {
        filter_pattern = ".|.|.|.";
        attached_lord_skill = true;
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        foreach(const Player *sib, player->getAliveSiblings())
            if (sib->hasSkill("wengua") && !sib->hasFlag("WenguaInvoked"))
                return true;
        return false;
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        WenguaAttachCard *wengua_card = new WenguaAttachCard;
        wengua_card->addSubcard(originalCard->getId());
        return wengua_card;
    }
};

class Wengua : public TriggerSkill
{
public:
    Wengua() : TriggerSkill("wengua")
    {
        events << GameStart << EventAcquireSkill << EventLoseSkill << Debut;
        view_as_skill = new WenguaViewAsSkill;
    }

    virtual bool triggerable(const ServerPlayer *) const
    {
        return false;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if ((triggerEvent == GameStart && player->hasSkill("wengua", true))
            || (triggerEvent == EventAcquireSkill && data.toString() == "wengua")) {
            foreach (ServerPlayer *p, room->getOtherPlayers(player)) {
                if (!p->hasSkill("wengua_attach"))
                    room->attachSkillToPlayer(p, "wengua_attach");
            }
        } else if (triggerEvent == EventLoseSkill && data.toString() == "wengua") {
            foreach (ServerPlayer *p, room->getOtherPlayers(player)) {
                if (p->hasSkill("wengua_attach"))
                    room->detachSkillFromPlayer(p, "wengua_attach", true);
            }
        } else if (triggerEvent == Debut) {
            QList<ServerPlayer *> liufengs = room->findPlayersBySkillName("wengua");
            foreach (ServerPlayer *liufeng, liufengs) {
                if (player != liufeng && !player->hasSkill("wengua_attach")) {
                    room->attachSkillToPlayer(player, "wengua_attach");
                    break;
                }
            }
        }
    }

    QString getSelectBox() const
    {
        return "top+bottom";
    }

    bool buttonEnabled(const QString &button_name, const QList<const Card *> &selected, const QList<const Player *> &) const
    {
        return button_name.isEmpty() || !selected.isEmpty();
    }

};

class Fuzhu : public TriggerSkill
{
public:
    Fuzhu() : TriggerSkill("fuzhu")
    {
        events << EventPhaseStart;
    }

    virtual TriggerList triggerable(TriggerEvent , Room *room, ServerPlayer *player, QVariant &) const
    {
        TriggerList skill_list;
        if (player->getPhase() != Player::Finish || !player->isMale() || player->isDead()) return skill_list;

        QList<ServerPlayer *> xushis = room->findPlayersBySkillName(objectName());
        foreach (ServerPlayer *xushi, xushis) {
            if (room->getDrawPile().length() > xushi->getHp()*10 || xushi->canSlash(player, false)) continue;
            skill_list.insert(xushi, QStringList(objectName()));
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *xushi) const
    {
        if (!room->askForSkillInvoke(xushi, objectName(), QVariant::fromValue(player))) return false;
        xushi->broadcastSkillInvoke(objectName());
        int times = room->getTag("SwapPile").toInt();
        int max_times = Sanguosha->getPlayerCount(room->getMode());
        for (int i = 0; i < max_times; i++) {
            if (xushi->isDead() || player->isDead() || room->getDrawPile().isEmpty() || room->getTag("SwapPile").toInt() > times) break;
            const Card *slash = NULL;
            for (int i = room->getDrawPile().length()-1; i >= 0; i--) {
                const Card *card = Sanguosha->getCard(room->getDrawPile().at(i));
                if (card->isKindOf("Slash") && xushi->canSlash(player, card, false)) {
                    slash = card;
                    break;
                }
            }
            if (slash) {
                room->useCard(CardUseStruct(slash, xushi, player));
            } else
                break;
        }
        room->swapPile();
        return false;
    }
};

class Weilu : public TriggerSkill
{
public:
    Weilu() : TriggerSkill("weilu")
    {
        events << Damaged << EventPhaseStart << EventPhaseEnd << EventPhaseChanging;
        frequency = Compulsory;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *lvqian, QVariant &data) const
    {
        if (triggerEvent == EventPhaseStart) {
            if (lvqian->getPhase() == Player::RoundStart) {
                lvqian->tag["WeiluToRevenge"] = lvqian->tag["WeiluRecord"];
                lvqian->tag.remove("WeiluRecord");
            }
        }else if (triggerEvent == EventPhaseChanging) {
            if (data.value<PhaseChangeStruct>().from == Player::Play) {
                QList<ServerPlayer *> players = room->getAlivePlayers();
                foreach(ServerPlayer *p, players) {
                    room->setPlayerMark(p, "WeiluRecover", 0);
                }
            }
            if (data.value<PhaseChangeStruct>().to == Player::NotActive) {
                lvqian->tag.remove("WeiluToRevenge");

            }
        }
    }

    virtual TriggerList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        TriggerList skill_list;
        if (triggerEvent == Damaged && TriggerSkill::triggerable(player)) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.from && damage.from->isAlive() && damage.from != player) {
                skill_list.insert(player, QStringList(objectName()));
            }
        } else if (triggerEvent == EventPhaseStart && player->getPhase() == Player::Play) {
            QStringList weilu_list = player->tag["WeiluToRevenge"].toStringList();
            QList<ServerPlayer *> players = room->getAlivePlayers();
            foreach(ServerPlayer *p, players) {
                if (weilu_list.contains(p->objectName()) && p->getHp() > 1) {
                    skill_list.insert(p, QStringList(objectName()));
                }
            }
        } else if (triggerEvent == EventPhaseEnd && player->getPhase() == Player::Play) {
            QList<ServerPlayer *> players = room->getAlivePlayers();
            foreach(ServerPlayer *p, players) {
                if (p->getMark("WeiluRecover") > 0 && p->isWounded()) {
                    skill_list.insert(p, QStringList(objectName()));
                }
            }
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *lvqian, QVariant &data, ServerPlayer *target) const
    {
        if (triggerEvent == Damaged) {
            DamageStruct damage = data.value<DamageStruct>();
            ServerPlayer *from = damage.from;
            if (from && from->isAlive()) {
                room->sendCompulsoryTriggerLog(lvqian, objectName());
                lvqian->broadcastSkillInvoke(objectName());
                room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, lvqian->objectName(), from->objectName());
                QStringList weilu_list = lvqian->tag["WeiluRecord"].toStringList();
                weilu_list.append(from->objectName());
                lvqian->tag["WeiluRecord"] = QVariant::fromValue(weilu_list);
            }
        } else if (triggerEvent == EventPhaseStart) {
            int x = target->getHp()-1;
            room->loseHp(target, x);
            room->setPlayerMark(target, "WeiluRecover", x);
        } else if (triggerEvent == EventPhaseEnd) {
            int x = target->getMark("WeiluRecover");
            room->recover(target, RecoverStruct(target, NULL, x));
        }
        return false;
    }
};

ZengdaoCard::ZengdaoCard()
{
    will_throw = false;
    handling_method = Card::MethodNone;
}

void ZengdaoCard::extraCost(Room *room, const CardUseStruct &card_use) const
{
    room->removePlayerMark(card_use.from, "@blade");
    card_use.to.first()->addToPile("lvblade", this);
}

class ZengdaoViewAsSkill : public ViewAsSkill
{
public:
    ZengdaoViewAsSkill() : ViewAsSkill("zengdao")
    {
        response_pattern = "@@zengdao";
    }

    bool viewFilter(const QList<const Card *> &, const Card *to_select) const
    {
        return to_select->isEquipped();
    }

    const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.isEmpty()) return NULL;

        ZengdaoCard *skillcard = new ZengdaoCard;
        skillcard->addSubcards(cards);
        return skillcard;
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return player->getMark("@blade") > 0 && player->hasEquip();
    }

};

class Zengdao : public TriggerSkill
{
public:
    Zengdao() : TriggerSkill("zengdao")
    {
        events << DamageCaused;
        frequency = Limited;
        view_as_skill = new ZengdaoViewAsSkill;
        limit_mark = "@blade";
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &, ServerPlayer * &) const
    {
        if (!player->getPile("lvblade").isEmpty()) {
            return QStringList("zengdao!");
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        QList<int> ids = player->getPile("lvblade");
        room->fillAG(ids, player);
        int id = room->askForAG(player, ids, false, objectName());
        room->clearAG(player);
        CardMoveReason reason(CardMoveReason::S_REASON_REMOVE_FROM_PILE, QString(), player->objectName(), objectName(), QString());
        room->throwCard(Sanguosha->getCard(id), reason, NULL);

        DamageStruct damage = data.value<DamageStruct>();
        damage.damage++;
        data = QVariant::fromValue(damage);
        return false;
    }
};














class Jijun : public TriggerSkill
{
public:
    Jijun() : TriggerSkill("jijun")
    {
        events << TargetSpecified << CardsMoveOneTime;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (triggerEvent == TargetSpecified && TriggerSkill::triggerable(player)) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card->isKindOf("Weapon") || (use.card->getTypeId() != Card::TypeEquip && use.card->getTypeId() != Card::TypeSkill)) {
                if (use.to.contains(player) && player->getPhase() == Player::Play)
                    return QStringList(objectName());
            }
        } else if (triggerEvent == CardsMoveOneTime && player->isAlive()) {
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            if (move.to_place == Player::DiscardPile && move.reason.m_reason == CardMoveReason::S_REASON_JUDGEDONE) {
                JudgeStruct *judge = move.reason.m_extraData.value<JudgeStruct *>();
                if (judge->reason == objectName() && judge->who == player) {
                    foreach (int card_id, move.card_ids) {
                        if (room->getCardPlace(card_id) == Player::DiscardPile)
                            return QStringList("jijun!");
                    }
                }
            }
        }

        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        if (triggerEvent == TargetSpecified && TriggerSkill::triggerable(player)) {
            if (player->askForSkillInvoke(objectName())) {
                JudgeStruct judge;
                judge.pattern = ".";
                judge.play_animation = false;
                judge.reason = objectName();
                judge.who = player;
                room->judge(judge);
            }
        } else if (triggerEvent == CardsMoveOneTime) {
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();

            QList<int> card_ids;
            foreach (int card_id, move.card_ids) {
                if (room->getCardPlace(card_id) == Player::DiscardPile)
                    card_ids << card_id;
            }

            if (card_ids.isEmpty()) return false;
            if (room->askForChoice(player, objectName(), "yes+no", data, "@jijun-choose") == "yes") {
                data = QVariant::fromValue(move);
                player->addToPile("phalanx", card_ids);
            }
        }
        return false;
    }
};

class FangtongRemove : public ViewAsSkill
{
public:
    FangtongRemove() : ViewAsSkill("fangtong_remove")
    {
        response_pattern = "@@fangtong_remove!";
        expand_pile = "phalanx";
    }

    bool viewFilter(const QList<const Card *> &, const Card *to_select) const
    {
        return Self->getPile("phalanx").contains(to_select->getEffectiveId());
    }

    const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.isEmpty()) return NULL;
        DummyCard *remove = new DummyCard;
        remove->addSubcards(cards);
        return remove;
    }
};

class Fangtong : public PhaseChangeSkill
{
public:
    Fangtong() : PhaseChangeSkill("fangtong")
    {

    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &, ServerPlayer * &) const
    {
        if (PhaseChangeSkill::triggerable(player) && player->getPhase() == Player::Finish
                && !player->isNude() && !player->getPile("phalanx").isEmpty())
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool onPhaseChange(ServerPlayer *player) const
    {
        Room *room = player->getRoom();
        const Card *card = room->askForCard(player, "..", "@fangtong", QVariant(), objectName());
        if (card && !player->getPile("phalanx").isEmpty()) {
            const Card *remove = room->askForCard(player, "@@fangtong_remove!", "@fangtong-remove", QVariant(), Card::MethodNone);
            if (!remove && !player->getPile("phalanx").isEmpty())
                remove = Sanguosha->getCard(player->getPile("phalanx").first());
            if (remove) {
                CardMoveReason reason(CardMoveReason::S_REASON_REMOVE_FROM_PILE, QString(), player->objectName(), objectName(), QString());
                room->throwCard(remove, reason, NULL);
            }
            int point = 0;
            point += card->getNumber();
            foreach (int id, remove->getSubcards()) {
                point += Sanguosha->getCard(id)->getNumber();
            }
            if (point == 36) {
                ServerPlayer *target = room->askForPlayerChosen(player, room->getOtherPlayers(player), objectName(), "@fangtong-target");
                room->damage(DamageStruct(objectName(), player, target, 3, DamageStruct::Thunder));
            }
        }

        return false;
    }
};

LuanzhanCard::LuanzhanCard()
{
}

bool LuanzhanCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    QStringList available_targets = Self->property("luanzhan_available_targets").toString().split("+");

    return targets.length() < Self->getMark("#luanzhan") && available_targets.contains(to_select->objectName());
}

void LuanzhanCard::use(Room *, ServerPlayer *, QList<ServerPlayer *> &targets) const
{
    foreach (ServerPlayer *p, targets)
        p->setFlags("LuanzhanExtraTarget");
}

class LuanzhanViewAsSkill : public ZeroCardViewAsSkill
{
public:
    LuanzhanViewAsSkill() : ZeroCardViewAsSkill("luanzhan")
    {
        response_pattern = "@@luanzhan";
    }

    virtual const Card *viewAs() const
    {
        return new LuanzhanCard;
    }
};

class LuanzhanColl : public ZeroCardViewAsSkill
{
public:
    LuanzhanColl() : ZeroCardViewAsSkill("luanzhan_coll")
    {
        response_pattern = "@@luanzhan_coll";
    }

    virtual const Card *viewAs() const
    {
        return new ExtraCollateralCard;
    }
};

class Luanzhan : public TriggerSkill
{
public:
    Luanzhan() : TriggerSkill("luanzhan")
    {
        events << HpChanged << TargetChosed << TargetSpecified;
        view_as_skill = new LuanzhanViewAsSkill;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &ask_who) const
    {
        if (triggerEvent == HpChanged && !data.isNull() && data.canConvert<DamageStruct>()) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.from && TriggerSkill::triggerable(damage.from)){
                ask_who = damage.from;
                return QStringList("luanzhan!");
            }
        } else if (triggerEvent == TargetSpecified && player->getMark("#luanzhan") > 0 && TriggerSkill::triggerable(player)) {
            CardUseStruct use = data.value<CardUseStruct>();
            if ((use.card->isKindOf("Slash") || (use.card->isNDTrick() && use.card->isBlack())) && use.to.length() < player->getMark("#luanzhan")) {
                return QStringList("luanzhan!");
            }
        } else if (triggerEvent == TargetChosed && player->getMark("#luanzhan") > 0 && TriggerSkill::triggerable(player)) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card->isKindOf("Slash") || (use.card->isNDTrick() && use.card->isBlack())) {
                if (!player->getUseExtraTargets(use, true).isEmpty())
                    return QStringList(objectName());
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *ask_who) const
    {
        if (triggerEvent == HpChanged) {
            if (ask_who){
                room->sendCompulsoryTriggerLog(ask_who, objectName());
                ask_who->broadcastSkillInvoke(objectName());
                room->addPlayerMark(ask_who, "#luanzhan");
            }
        } else if (triggerEvent == TargetSpecified) {
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            room->setPlayerMark(player, "#luanzhan", 0);
        } else if (triggerEvent == TargetChosed) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (!use.card->isKindOf("Slash") && !(use.card->isNDTrick() && use.card->isBlack())) return false;
            bool no_distance_limit = false;
            if (use.card->isKindOf("Slash")) {
                if (use.card->hasFlag("slashDisableExtraTarget")) return false;
                if (use.card->hasFlag("slashNoDistanceLimit")){
                    no_distance_limit = true;
                    room->setPlayerFlag(player, "slashNoDistanceLimit");
                }
            }
            QStringList available_targets;
            QList<ServerPlayer *> extra_targets;
            if (!use.card->isKindOf("AOE") && !use.card->isKindOf("GlobalEffect")) {
                foreach (ServerPlayer *p, room->getAlivePlayers()) {
                    if (use.to.contains(p) || room->isProhibited(player, p, use.card)) continue;
                    if (use.card->targetFixed()) {
                        if (!use.card->isKindOf("Peach") || p->isWounded())
                            available_targets.append(p->objectName());
                    } else {
                        if (use.card->targetFilter(QList<const Player *>(), p, player))
                            available_targets.append(p->objectName());
                    }
                }
            }
            if (no_distance_limit)
                room->setPlayerFlag(player, "-slashNoDistanceLimit");
            if (!available_targets.isEmpty()) {
                if (use.card->isKindOf("Collateral")) {
                    QStringList tos;
                    foreach(ServerPlayer *t, use.to)
                        tos.append(t->objectName());
                    for (int i = player->getMark("#luanzhan"); i > 0; i--) {
                        if (available_targets.isEmpty()) break;
                        room->setPlayerProperty(player, "extra_collateral", use.card->toString());
                        room->setPlayerProperty(player, "extra_collateral_current_targets", tos.join("+"));
                        player->tag["luanzhan-use"] = data;
                        room->askForUseCard(player, "@@luanzhan_coll", "@luanzhan-add:::collateral:" + QString::number(i), QVariant(), Card::MethodUse, false);
                        player->tag.remove("luanzhan-use");
                        room->setPlayerProperty(player, "extra_collateral", QString());
                        room->setPlayerProperty(player, "extra_collateral_current_targets", QString());
                        ServerPlayer *extra = NULL;
                        foreach (ServerPlayer *p, room->getOtherPlayers(player)) {
                            if (p->hasFlag("ExtraCollateralTarget")) {
                                p->setFlags("-ExtraCollateralTarget");
                                extra = p;
                                break;
                            }
                        }
                        if (extra == NULL)
                            break;
                        extra->setFlags("LuanzhanExtraTarget");
                        tos.append(extra->objectName());
                        available_targets.removeOne(extra->objectName());
                    }
                } else {
                    room->setPlayerProperty(player, "luanzhan_available_targets", available_targets.join("+"));
                    player->tag["luanzhan-use"] = data;
                    room->askForUseCard(player, "@@luanzhan", "@luanzhan-add:::" + use.card->objectName() + ":" + QString::number(player->getMark("#luanzhan")));
                    player->tag.remove("luanzhan-use");
                    room->setPlayerProperty(player, "luanzhan_available_targets", QString());
                }
                foreach (ServerPlayer *p, room->getAllPlayers()) {
                    if (p->hasFlag("LuanzhanExtraTarget")) {
                        p->setFlags("-LuanzhanExtraTarget");
                        extra_targets << p;
                    }
                }
            }

            if (!extra_targets.isEmpty()) {
                if (use.card->isKindOf("Collateral")) {
                    LogMessage log;
                    log.type = "#UseCard";
                    log.from = player;
                    log.to = extra_targets;
                    log.card_str = "@LuanzhanCard[no_suit:-]=.";
                    room->sendLog(log);
                    player->broadcastSkillInvoke("luanzhan");
                    foreach (ServerPlayer *p, extra_targets) {
                        ServerPlayer *victim = p->tag["collateralVictim"].value<ServerPlayer *>();
                        if (victim) {
                            LogMessage log;
                            log.type = "#LuanzhanCollateralSlash";
                            log.from = p;
                            log.to << victim;
                            room->sendLog(log);
                            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, p->objectName(), victim->objectName());
                        }
                    }
                }

                foreach (ServerPlayer *extra, extra_targets){
                    use.to.append(extra);
                }
                room->sortByActionOrder(use.to);
                data = QVariant::fromValue(use);
            }
        }
        return false;
    }
};

class Qiaomeng : public TriggerSkill
{
public:
    Qiaomeng() : TriggerSkill("qiaomeng")
    {
        events << Damage << CardsMoveOneTime;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (triggerEvent == Damage && TriggerSkill::triggerable(player)) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.to->isAlive() && !damage.to->hasFlag("Global_DebutFlag")
                    && damage.card && damage.card->isKindOf("Slash") && damage.card->isBlack()
                    && player->canDiscard(damage.to, "e")) {
                return QStringList(objectName());
            }
        } else if (triggerEvent == CardsMoveOneTime) {
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            if (move.reason.m_skillName == objectName() && move.reason.m_playerId == player->objectName()
                && move.card_ids.length() == 1) {
                const Card *card = Sanguosha->getCard(move.card_ids.first());
                if (card->isKindOf("Horse") && room->getCardPlace(card->getEffectiveId()) == Player::DiscardPile) {
                    return QStringList("qiaomeng!");
                }
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        if (triggerEvent == Damage) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.to->isAlive() && player->canDiscard(damage.to, "e")
                    && room->askForSkillInvoke(player, objectName(), QVariant::fromValue(damage.to))) {
                player->broadcastSkillInvoke(objectName());
                room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), damage.to->objectName());
                int id = room->askForCardChosen(player, damage.to, "e", objectName(), false, Card::MethodDiscard);
                CardMoveReason reason(CardMoveReason::S_REASON_DISMANTLE, player->objectName(), damage.to->objectName(),
                    objectName(), QString());
                room->throwCard(Sanguosha->getCard(id), reason, damage.to, player);
            }
        } else if (triggerEvent == CardsMoveOneTime) {
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            if (move.card_ids.length() == 1) {
                const Card *card = Sanguosha->getCard(move.card_ids.first());
                if (card->isKindOf("Horse") && room->getCardPlace(card->getEffectiveId()) == Player::DiscardPile) {
                    room->obtainCard(player, card);
                    data = QVariant::fromValue(move);
                }
            }
        }
        return false;
    }
};

class Yicong : public DistanceSkill
{
public:
    Yicong() : DistanceSkill("yicong")
    {
    }

    virtual int getCorrect(const Player *from, const Player *to) const
    {
        int correct = 0;
        if (from->hasSkill(this) && from->getHp() > 2)
            correct--;
        if (to->hasSkill(this) && to->getHp() <= 2)
            correct++;

        return correct;
    }
};

class YicongEffect : public TriggerSkill
{
public:
    YicongEffect() : TriggerSkill("#yicong-effect")
    {
        events << HpChanged;
    }

    virtual void record(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        int hp = player->getHp();
        int index = 0;
        int reduce = 0;
        if (data.canConvert<RecoverStruct>()) {
            int rec = data.value<RecoverStruct>().recover;
            if (hp > 2 && hp - rec <= 2)
                index = 1;
        } else {
            if (data.canConvert<DamageStruct>()) {
                DamageStruct damage = data.value<DamageStruct>();
                reduce = damage.damage;
            } else if (!data.isNull()) {
                reduce = data.toInt();
            }
            if (hp <= 2 && hp + reduce > 2)
                index = 2;
        }

        if (index > 0 && player->hasSkill("yicong")) {
            room->sendCompulsoryTriggerLog(player, "yicong");
            player->broadcastSkillInvoke("yicong", index);
        }
    }

    virtual bool triggerable(const ServerPlayer *) const
    {
        return false;
    }

};


Spark1Package::Spark1Package()
: Package("spark1")
{
    General *xingcai = new General(this, "zhangxingcai", "shu", 3, false);
    xingcai->addSkill(new Shenxian);
    xingcai->addSkill(new Qiangwu);
    xingcai->addSkill(new QiangwuTargetMod);
    related_skills.insertMulti("qiangwu", "#qiangwu-target");

    General *wuxian = new General(this, "wuxian", "shu", 3, false);
    wuxian->addSkill(new Fumian);
    wuxian->addSkill(new Daiyan);

    General *sundeng = new General(this, "sundeng", "wu");
    sundeng->addSkill(new Kuangbi);
    sundeng->addSkill(new DetachEffectSkill("kuangbi", "wrong"));
    related_skills.insertMulti("kuangbi", "#kuangbi-clear");

    General *buzhi = new General(this, "buzhi", "wu", 3);
    buzhi->addSkill(new Hongde);
    buzhi->addSkill(new Dingpan);

    General *caoang = new General(this, "caoang", "wei");
    caoang->addSkill(new Kangkai);

    General *zhugedan = new General(this, "zhugedan", "wei", 4);
    zhugedan->addSkill(new Gongao);
    zhugedan->addSkill(new Juyi);
    zhugedan->addRelateSkill("weizhong");
    zhugedan->addRelateSkill("benghuai");

    General *liuxie = new General(this, "liuxie", "qun", 3);
    liuxie->addSkill(new Tianming);
    liuxie->addSkill(new Mizhao);

    General *caojie = new General(this, "caojie", "qun", 3, false);
    caojie->addSkill(new Shouxi);
    caojie->addSkill(new Huimin);


    addMetaObject<QiangwuCard>();
    addMetaObject<KuangbiCard>();
    addMetaObject<DingpanCard>();
    addMetaObject<MizhaoCard>();
    addMetaObject<HuiminGraceCard>();


    skills << new Weizhong << new HuiminGrace;
}

Spark2Package::Spark2Package()
: Package("spark2")
{
    General *dongyun = new General(this, "dongyun", "shu", 3);
    dongyun->addSkill(new Bingzheng);
    dongyun->addSkill(new Sheyan);

    General *zhugejin = new General(this, "zhugejin", "wu", 3);
    zhugejin->addSkill(new Hongyuan);
    zhugejin->addSkill(new Huanshi);
    zhugejin->addSkill(new Mingzhe);

    General *yanjun = new General(this, "yanjun", "wu", 3);
    yanjun->addSkill(new Guanchao);
    yanjun->addSkill(new Xunxian);

    General *lidian = new General(this, "lidian", "wei", 3);
    lidian->addSkill(new Xunxun);
    lidian->addSkill(new Wangxi);

    General *simalang = new General(this, "simalang", "wei", 3);
    simalang->addSkill(new Junbing);
    simalang->addSkill(new Quji);

    General *duji = new General(this, "duji", "wei", 3);
    duji->addSkill(new Andong);
    duji->addSkill(new AndongHideCard);
    duji->addSkill(new Yingshi);
    related_skills.insertMulti("andong", "#andong-hidecard");

    General *liuyan = new General(this, "liuyan", "qun", 3);
    liuyan->addSkill(new Tushe);
    liuyan->addSkill(new Limu);
    liuyan->addSkill(new LimuTargetMod);
    related_skills.insertMulti("limu", "#limu-target");

    General *zhanglu = new General(this, "zhanglu", "qun", 3);
    zhanglu->addSkill(new Yishe);
    zhanglu->addSkill(new DetachEffectSkill("yishe", "rice"));
    related_skills.insertMulti("yishe", "#yishe-clear");
    zhanglu->addSkill(new Bushi);
    zhanglu->addSkill(new Midao);

    addMetaObject<QujiCard>();
    addMetaObject<LimuCard>();
}

Spark3Package::Spark3Package()
: Package("spark3")
{

    General *sunqian = new General(this, "sunqian", "shu", 3);
    sunqian->addSkill(new Qianya);
    sunqian->addSkill(new Shuimeng);

    General *qinmi = new General(this, "qinmi", "shu", 3);
    qinmi->addSkill(new Jianzheng);
    qinmi->addSkill(new Zhuandui);
    qinmi->addSkill(new Tianbian);

    General *xuezong = new General(this, "xuezong", "wu", 3);
    xuezong->addSkill(new Funan);
    xuezong->addSkill(new FunanCardLimited);
    xuezong->addSkill(new Jiexun);
    related_skills.insertMulti("funan", "#funan-limited");

    General *panjun = new General(this, "panjun", "wu", 3);
    panjun->addSkill(new Guanwei);
    panjun->addSkill(new Gongqing);
    panjun->addSkill(new GongqingNegative);
    related_skills.insertMulti("gongqing", "#gongqing");

    General *xizhicai = new General(this, "xizhicai", "wei", 3);
    xizhicai->addSkill("tiandu");
    xizhicai->addSkill(new Xianfu);
    xizhicai->addSkill(new Chouce);

    General *guohuanghou = new General(this, "guohuanghou", "wei", 3, false);
    guohuanghou->addSkill(new Jiaozhao);
    guohuanghou->addSkill(new JiaozhaoProhibit);
    related_skills.insertMulti("jiaozhao", "#jiaozhao");
    guohuanghou->addSkill(new Danxin);

    General *wangcan = new General(this, "wangcan", "qun", 3);
    wangcan->addSkill(new Sanwen);
    wangcan->addSkill(new Qiai);
    wangcan->addSkill(new Denglou);

    General *caiyong = new General(this, "caiyong", "qun", 3);
    caiyong->addSkill(new Pizhuan);
    caiyong->addSkill(new PizhuanKeep);
    caiyong->addSkill(new DetachEffectSkill("pizhuan", "book"));
    related_skills.insertMulti("pizhuan", "#pizhuan-keep");
    related_skills.insertMulti("pizhuan", "#pizhuan-clear");
    caiyong->addSkill(new Tongbo);

    addMetaObject<TianbianCard>();
    addMetaObject<JiaozhaoCard>();
    addMetaObject<DenglouUseCard>();
    addMetaObject<TongboCard>();

    skills << new DenglouUse << new JiaozhaoFirst << new JiaozhaoSecond;
}

Spark4Package::Spark4Package()
: Package("spark4")
{
    General *sp_sunshangxiang = new General(this, "sp_sunshangxiang", "shu", 3, false);
    sp_sunshangxiang->addSkill(new Liangzhu);
    sp_sunshangxiang->addSkill(new Fanxiang);
    sp_sunshangxiang->addRelateSkill("xiaoji");

    General *xushu = new General(this, "sp_xushu", "shu", 3); // YJ 009
    xushu->addSkill(new Wuyan);
    xushu->addSkill(new Jujian);

    General *sp_pangtong = new General(this, "sp_pangtong", "wu", 3);
    sp_pangtong->addSkill(new Guolun);
    sp_pangtong->addSkill(new Songsang);
    sp_pangtong->addRelateSkill("zhanji");


    General *sp_pangde = new General(this, "sp_pangde", "wei");
    sp_pangde->addSkill("mashu");
    sp_pangde->addSkill(new Juesi);

    General *sp_caiwenji = new General(this, "sp_caiwenji", "wei", 3, false);
    sp_caiwenji->addSkill(new Chenqing);
    sp_caiwenji->addSkill(new Mozhi);

    General *sp_jiangwei = new General(this, "sp_jiangwei", "wei");
    sp_jiangwei->addSkill(new Kunfen);
    sp_jiangwei->addSkill(new Fengliang);
    sp_jiangwei->addRelateSkill("tiaoxin");

    General *sp_huangyueying = new General(this, "sp_huangyueying", "qun", 3, false);
    sp_huangyueying->addSkill(new Jiqiao);
    sp_huangyueying->addSkill(new Linglong);
    sp_huangyueying->addSkill(new LinglongTrigger);
    related_skills.insertMulti("linglong", "#linglong");

    General *sp_taishici = new General(this, "sp_taishici", "qun");
    sp_taishici->addSkill(new Jixu);

    addMetaObject<JujianCard>();
    addMetaObject<GuolunCard>();
    addMetaObject<JuesiCard>();
    addMetaObject<JixuCard>();


    skills << new Zhanji << new JuesiDiscard;
}

Spark5Package::Spark5Package()
: Package("spark5")
{

    General *liyan = new General(this, "liyan", "shu", 3);
    liyan->addSkill(new Duliang);
    liyan->addSkill(new Fulin);

    General *mizhu = new General(this, "mizhu", "shu", 3);
    mizhu->addSkill(new Ziyuan);
    mizhu->addSkill(new Jugu);
    mizhu->addSkill(new JuguKeep);
    related_skills.insertMulti("jugu", "#jugu-keep");

    General *mazhong = new General(this, "mazhong", "shu");
    mazhong->addSkill(new Fuman);

    General *zhoufang = new General(this, "zhoufang", "wu", 3);
    zhoufang->addSkill(new Duanfa);
    zhoufang->addSkill(new Youdi);

    General *heqi = new General(this, "heqi", "wu");
    heqi->addSkill(new Qizhou);
    heqi->addRelateSkill("mashu");
    heqi->addRelateSkill("yingzi");
    heqi->addRelateSkill("duanbing");
    heqi->addRelateSkill("fenwei");
    heqi->addSkill(new Shanxi);

    General *wenpin = new General(this, "wenpin", "wei");
    wenpin->addSkill(new Zhenwei);

    General *lvdai = new General(this, "lvdai", "wu");
    lvdai->addSkill(new Qinguo);

    General *liuyao = new General(this, "liuyao", "qun");
    liuyao->addSkill(new Kannan);

    addMetaObject<DuliangCard>();
    addMetaObject<ZiyuanCard>();
    addMetaObject<FumanCard>();
    addMetaObject<DuanfaCard>();
    addMetaObject<ShanxiCard>();
    addMetaObject<KannanCard>();
}

Spark6Package::Spark6Package()
: Package("spark6")
{

    General *mayunlu = new General(this, "mayunlu", "shu", 4, false);
    mayunlu->addSkill(new Fengpo);
    mayunlu->addSkill("mashu");

    General *guanyinping = new General(this, "guanyinping", "shu", 3, false);
    guanyinping->addSkill(new Xueji);
    guanyinping->addSkill(new Huxiao);
    guanyinping->addSkill(new HuxiaoTargetMod);
    related_skills.insertMulti("huxiao", "#huxiao-target");
    guanyinping->addSkill(new Wuji);

    General *zumao = new General(this, "zumao", "wu"); // SP 030
    zumao->addSkill(new Yinbing);
    zumao->addSkill(new Juedi);

    General *xushi = new General(this, "xushi", "wu", 3, false);
    xushi->addSkill(new Wengua);
    xushi->addSkill(new Fuzhu);

    General *lvqian = new General(this, "lvqian", "wei");
    lvqian->addSkill(new Weilu);
    lvqian->addSkill(new Zengdao);

    General *zhangliang = new General(this, "zhangliang", "qun"); // SP 036
    zhangliang->addSkill(new Jijun);
    zhangliang->addSkill(new Fangtong);

    General *tadun = new General(this, "tadun", "qun");
    tadun->addSkill(new Luanzhan);

    General *gongsunzan = new General(this, "gongsunzan", "qun"); // QUN 026
    gongsunzan->addSkill(new Qiaomeng);
    gongsunzan->addSkill(new Yicong);
    gongsunzan->addSkill(new YicongEffect);
    related_skills.insertMulti("yicong", "#yicong-effect");

    addMetaObject<XuejiCard>();
    addMetaObject<WenguaCard>();
    addMetaObject<WenguaAttachCard>();
    addMetaObject<ZengdaoCard>();
    addMetaObject<LuanzhanCard>();

    skills << new FangtongRemove << new WenguaAttach << new LuanzhanColl;
}

ADD_PACKAGE(Spark1)
ADD_PACKAGE(Spark2)
ADD_PACKAGE(Spark3)
ADD_PACKAGE(Spark4)
ADD_PACKAGE(Spark5)
ADD_PACKAGE(Spark6)
