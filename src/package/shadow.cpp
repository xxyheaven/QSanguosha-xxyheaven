#include "shadow.h"
#include "settings.h"
#include "skill.h"
#include "standard.h"
#include "client.h"
#include "clientplayer.h"
#include "engine.h"
#include "maneuvering.h"

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

class Juzhan : public TriggerSkill
{
public:
    Juzhan() : TriggerSkill("juzhan")
    {
        events << TargetSpecified << TargetConfirmed << EventPhaseStart;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (triggerEvent == EventPhaseStart && player->getPhase() == Player::NotActive) {
            QList<ServerPlayer *> players = room->getAlivePlayers();
            foreach (ServerPlayer *p, players) {
                room->setPlayerProperty(p, "JuzhanProhibit", QVariant());
                room->setPlayerMark(p, "#juzhan", 0);
            }
        }
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        if (triggerEvent == TargetSpecified && player->getMark("juzhanTransformed")%2) {
            if (use.index > 0) return QStringList();
            ServerPlayer *target = use.to.first();
            if (TriggerSkill::triggerable(player) && target->isAlive() && use.card->isKindOf("Slash") && player->canGetCard(target, "he")) {
                return QStringList(objectName());
            }
        } else if (triggerEvent == TargetConfirmed && player->getMark("juzhanTransformed")%2==0) {
            ServerPlayer *target = use.from;
            if (TriggerSkill::triggerable(player) && target->isAlive() && use.card->isKindOf("Slash")) {
                return QStringList(objectName());
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        if (triggerEvent == TargetSpecified) {
            ServerPlayer *target = use.to.first();
            if (player->askForSkillInvoke(this, QVariant::fromValue(target))) {
                room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), target->objectName());
                player->broadcastSkillInvoke(objectName());
                room->addPlayerMark(player, "juzhanTransformed");

                if (player->canGetCard(target, "he")) {
                    int card_id = room->askForCardChosen(player, target, "he", objectName(), false, Card::MethodGet);
                    CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, player->objectName());
                    room->obtainCard(player, Sanguosha->getCard(card_id), reason, false);
                }

                QStringList assignee_list = player->property("JuzhanProhibit").toString().split("+");
                assignee_list << target->objectName();
                room->setPlayerProperty(player, "JuzhanProhibit", assignee_list.join("+"));
                room->addPlayerTip(player, "#juzhan");
                room->addPlayerTip(target, "#juzhan");
            }
        } else if (triggerEvent == TargetConfirmed) {
            ServerPlayer *target = use.from;
            if (player->askForSkillInvoke(this, QVariant::fromValue(target))) {
                room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), target->objectName());
                player->broadcastSkillInvoke(objectName());
                room->addPlayerMark(player, "juzhanTransformed");

                QList<ServerPlayer *> players;
                players << player << target;
                room->sortByActionOrder(players);

                foreach (ServerPlayer *p, players)
                    p->drawCards(1, objectName());

                QStringList assignee_list = target->property("JuzhanProhibit").toString().split("+");
                assignee_list << player->objectName();
                room->setPlayerProperty(target, "JuzhanProhibit", assignee_list.join("+"));
                room->addPlayerTip(player, "#juzhan");
                room->addPlayerTip(target, "#juzhan");
            }
        }
        return false;
    }
};

class JuzhanProhibit : public ProhibitSkill
{
public:
    JuzhanProhibit() : ProhibitSkill("#juzhan-prohibit")
    {
    }

    virtual bool isProhibited(const Player *from, const Player *to, const Card *card, const QList<const Player *> &) const
    {
        if (from != NULL && card->getTypeId() != Card::TypeSkill) {
            QStringList assignee_list = from->property("JuzhanProhibit").toString().split("+");
            return assignee_list.contains(to->objectName());
        }
        return false;
    }
};

FeijunCard::FeijunCard()
{
    target_fixed = true;
}

void FeijunCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const
{
    QList<ServerPlayer *> targets1, targets2;
    foreach (ServerPlayer *p, room->getAllPlayers()) {
        if (p->getHandcardNum() > source->getHandcardNum())
            targets1 << p;
        if (p->getEquips().length() > source->getEquips().length())
            targets2 << p;
    }
    QStringList choices;
    if (!targets1.isEmpty())
        choices << "hand";
    if (!targets2.isEmpty())
        choices << "equip";

    if (!choices.isEmpty()){

        QString choice = room->askForChoice(source, "feijun", choices.join("+"), QVariant(), QString(), "hand+equip");

        QList<ServerPlayer *> targets = choice == "hand" ? targets1 : targets2;
        QString prompt = "@feijun-" + choice;

        source->tag["FeijunChoice"] = choice;//for AI
        ServerPlayer *target = room->askForPlayerChosen(source, targets, "feijun", prompt);
        source->tag.remove("FeijunChoice");//for AI

        if (choice == "hand"){
            const Card *card = room->askForExchange(target, "feijun", 1, 1, true, "@feijun-give::" + source->objectName());
            if (card) {
                CardMoveReason reason(CardMoveReason::S_REASON_GIVE, target->objectName(), source->objectName(), "feijun", QString());
                room->obtainCard(source, card, reason, false);
                delete card;
            }
        } else {
           room->askForDiscard(target, "feijun", 1, 1, false, true, "@feijun-discard", "EquipCard");
        }

        QStringList feijun_targets = source->tag["feijunUsedTargets"].toStringList();
        if (!feijun_targets.contains(target->objectName())) {
            feijun_targets << target->objectName();
            source->tag["feijunUsedTargets"] = feijun_targets;
            if (source->hasSkill("binglve")) {
                room->sendCompulsoryTriggerLog(source, "binglve");
                source->broadcastSkillInvoke("binglve");
                source->drawCards(2, "binglve");
            }
        }
    }
}

class Feijun : public OneCardViewAsSkill
{
public:
    Feijun() : OneCardViewAsSkill("feijun")
    {
        filter_pattern = ".!";
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("FeijunCard");
    }

    const Card *viewAs(const Card *originalCard) const
    {
        FeijunCard *skillcard = new FeijunCard;
        skillcard->addSubcard(originalCard);
        return skillcard;
    }
};

class Huaiju : public TriggerSkill
{
public:
    Huaiju() : TriggerSkill("huaiju")
    {
        events << TurnStart << DrawNCards << DamageInflicted;
        frequency = Compulsory;
    }

    virtual TriggerList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        TriggerList skill_list;
        if ((triggerEvent == TurnStart && room->getTag("FirstRound").toBool())
                || (triggerEvent == DrawNCards && player->isAlive() && player->getMark("#orange") > 0)
                || (triggerEvent == DamageInflicted && player->isAlive() && player->getMark("#orange") > 0)) {
            QList<ServerPlayer *> lujis = room->findPlayersBySkillName(objectName());
            foreach (ServerPlayer *luji, lujis)
                skill_list.insert(luji, QStringList(objectName()));

        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *ask_who) const
    {
        room->sendCompulsoryTriggerLog(ask_who, objectName());
        ask_who->broadcastSkillInvoke(objectName());
        if (triggerEvent == TurnStart)
            ask_who->gainMark("#orange", 3);
        else if (triggerEvent == DrawNCards)
            data = data.toInt()+1;
        else if (triggerEvent == DamageInflicted) {
            player->loseMark("#orange");
            return true;
        }
        return false;
    }
};

class Weili : public PhaseChangeSkill
{
public:
    Weili() : PhaseChangeSkill("weili")
    {
        view_as_skill = new dummyVS;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Play;
    }

    virtual bool onPhaseChange(ServerPlayer *player) const
    {
        Room *room = player->getRoom();
        ServerPlayer *target = room->askForPlayerChosen(player, room->getOtherPlayers(player), objectName(), "@weili-invoke", true, true);
        if (target) {
            player->broadcastSkillInvoke(objectName());
            if (player->getMark("#orange") >= 1 && room->askForChoice(player, objectName(), "dismark+losehp") == "dismark") {
                player->loseMark("#orange");
            } else
                room->loseHp(player);
            target->gainMark("#orange");
        }
        return false;
    }
};

class Zhenglun : public TriggerSkill
{
public:
    Zhenglun() : TriggerSkill("zhenglun")
    {
        events << EventPhaseChanging;
    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        PhaseChangeStruct change = data.value<PhaseChangeStruct>();
        if (change.to == Player::Draw && !player->isSkipped(Player::Draw) && player->getMark("#orange") == 0)
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        if (room->askForSkillInvoke(player, objectName())) {
            player->broadcastSkillInvoke(objectName());
            player->skip(Player::Draw, true);
            player->gainMark("#orange");
        }
        return false;
    }
};

KuizhuCard::KuizhuCard()
{
}

bool KuizhuCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    int x = 0;
    foreach (const Player *p, targets) {
        x+=p->getHp();
    }
    return x+to_select->getHp() <= Self->getMark("GlobalRuleDiscardCount");
}

bool KuizhuCard::targetsFeasible(const QList<const Player *> &targets, const Player *Self) const
{
    int x = 0;
    foreach (const Player *p, targets) {
        x+=p->getHp();
    }
    return x == Self->getMark("GlobalRuleDiscardCount");
}

void KuizhuCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    foreach(ServerPlayer *p, targets)
        room->damage(DamageStruct("kuizhu", source, p));

    if (targets.length() > 1)
        room->damage(DamageStruct("kuizhu", NULL, source));
}

class KuizhuViewAsSkill : public ZeroCardViewAsSkill
{
public:
    KuizhuViewAsSkill() : ZeroCardViewAsSkill("kuizhu")
    {
        response_pattern = "@@kuizhu";
    }

    virtual const Card *viewAs() const
    {
        return new KuizhuCard;
    }

};

class Kuizhu : public TriggerSkill
{
public:
    Kuizhu() : TriggerSkill("kuizhu")
    {
        events << EventPhaseEnd;
        view_as_skill = new KuizhuViewAsSkill;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return TriggerSkill::triggerable(target) && target->getPhase() == Player::Discard && target->getMark("GlobalRuleDiscardCount") > 0;
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        int x = player->getMark("GlobalRuleDiscardCount");
        if (x > 0 && room->askForSkillInvoke(player, "skill_ask", "prompt:::"+objectName()))  {
            QString choice = room->askForChoice(player, objectName(), "draw+damage", data, "@kuizhu-choose");
            if (choice == "draw") {
                QList<ServerPlayer *> choosees = room->askForPlayersChosen(player, room->getAlivePlayers(), objectName(), 0, x,
                        "@kuizhu1-target:::"+QString::number(x), true);
                if (!choosees.isEmpty()) {
                    room->broadcastSkillInvoke(objectName());
                    foreach(ServerPlayer *p, choosees)
                        p->drawCards(1, objectName());
                }
            } else if (choice == "damage") {
                room->askForUseCard(player, "@@kuizhu", "@kuizhu2-target:::"+QString::number(x));
            }
        }
        return false;
    }

};

class Chezheng : public TriggerSkill
{
public:
    Chezheng() : TriggerSkill("chezheng")
    {
        events << EventPhaseEnd;
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent, Room *room, ServerPlayer *target, QVariant &, ServerPlayer* &) const
    {
        if (TriggerSkill::triggerable(target) && target->getPhase() == Player::Play) {
            int x = 0;
            foreach (ServerPlayer *p, room->getOtherPlayers(target)) {
                if (!p->inMyAttackRange(target)) {
                    x++;
                }
            }
            if (target->getCardUsedTimes(".|play") < x) return QStringList(objectName());

        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        QList<ServerPlayer *> targets;
        foreach (ServerPlayer *p, room->getOtherPlayers(player)) {
            if (!p->inMyAttackRange(player) && player->canDiscard(p, "he"))
                targets << p;
        }
        if (targets.isEmpty()) return false;

        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());
        ServerPlayer *target = room->askForPlayerChosen(player, targets, objectName(), "@chezheng-target");
        int to_throw = room->askForCardChosen(player, target, "he", objectName(), false, Card::MethodDiscard);
        room->throwCard(to_throw, target, player);
        return false;
    }
};

class ChezhengProhibit : public ProhibitSkill
{
public:
    ChezhengProhibit() : ProhibitSkill("#chezheng-prohibit")
    {
    }

    virtual bool isProhibited(const Player *from, const Player *to, const Card *card, const QList<const Player *> &) const
    {
        return from && from->hasSkill(this) && from != to && !to->inMyAttackRange(from) && card->getTypeId() != Card::TypeSkill;
    }
};


class Lijun : public TriggerSkill
{
public:
    Lijun() : TriggerSkill("lijun$")
    {
        events << CardFinished;
        view_as_skill = new dummyVS;
    }

    virtual TriggerList triggerable(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        TriggerList skill_list;
        if (player->getPhase() == Player::Play && player->getKingdom() == "wu") {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card && use.card->isKindOf("Slash") && room->isAllOnPlace(use.card, Player::PlaceTable)) {
                foreach (ServerPlayer *sunliang, room->getOtherPlayers(player)) {
                    if (sunliang->hasLordSkill(objectName()))
                        skill_list.insert(sunliang, QStringList("lijun!"));
                }
            }
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *sunliang) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card && use.card->isKindOf("Slash") && room->isAllOnPlace(use.card, Player::PlaceTable)) {
            if (room->askForChoice(player, objectName(), "yes+no", data, "@lijun-invoke:" + sunliang->objectName()) == "yes") {
                LogMessage log;
                log.type = "#InvokeOthersSkill";
                log.from = player;
                log.to << sunliang;
                log.arg = objectName();
                room->sendLog(log);
                room->notifySkillInvoked(sunliang, objectName());
                sunliang->broadcastSkillInvoke(objectName());
                sunliang->obtainCard(use.card);
                if (room->askForChoice(sunliang, objectName(), "yes+no", data, "@lijun-draw:" + player->objectName()) == "yes")
                    player->drawCards(1, objectName());
            }
        }
        return false;
    }
};

class Qizhi : public TriggerSkill
{
public:
    Qizhi() : TriggerSkill("qizhi")
    {
        events << TargetSpecified << EventPhaseStart;
        view_as_skill = new dummyVS;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (triggerEvent == EventPhaseStart && player->getPhase() == Player::NotActive)
            room->setPlayerMark(player, "#qizhi", 0);
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (triggerEvent != TargetSpecified || !TriggerSkill::triggerable(player) || player->getPhase() == Player::NotActive) return QStringList();
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card->getTypeId() != Card::TypeBasic && use.card->getTypeId() != Card::TypeTrick) return QStringList();
        if (use.index > 0) return QStringList();
        QList<ServerPlayer *> all_players = room->getAlivePlayers();
        foreach (ServerPlayer *p, all_players) {
            if (!use.to.contains(p) && !p->isNude())
                return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        QList<ServerPlayer *> targets, all_players = room->getAlivePlayers();
        foreach (ServerPlayer *p, all_players) {
            if (!use.to.contains(p) && player->canDiscard(p, "he"))
                targets << p;
        }
        ServerPlayer *target = room->askForPlayerChosen(player, targets, objectName(), "@qizhi-target", true, true);
        if (target){
            room->addPlayerMark(player, "#qizhi");
            player->broadcastSkillInvoke(objectName());
            int id = room->askForCardChosen(player, target, "he", objectName(), false, Card::MethodDiscard);
            CardMoveReason reason(CardMoveReason::S_REASON_DISMANTLE, player->objectName(), target->objectName(), objectName(), QString());
            room->throwCard(Sanguosha->getCard(id), reason, target, player);
            target->drawCards(1, objectName());
        }
        return false;
    }
};

class Jinqu : public PhaseChangeSkill
{
public:
    Jinqu() : PhaseChangeSkill("jinqu")
    {
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Finish;
    }

    virtual bool onPhaseChange(ServerPlayer *player) const
    {
        Room *room = player->getRoom();
        if (room->askForSkillInvoke(player, objectName())) {
            player->broadcastSkillInvoke(objectName());
            player->drawCards(2, objectName());
            int n = player->getHandcardNum() - player->getMark("#qizhi");
            if (n > 0)
                room->askForDiscard(player, objectName(), n, n, false, false, "@jinqu-discard");
        }
        return false;
    }
};

class Jianxiang : public TriggerSkill
{
public:
    Jianxiang() : TriggerSkill("jianxiang")
    {
        events << TargetConfirmed;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        if (TriggerSkill::triggerable(player) && use.from != player && use.card->getTypeId() != Card::TypeSkill)
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        QList<ServerPlayer *> targets, players = room->getOtherPlayers(player);
        targets << player;
        int x = player->getHandcardNum();

        foreach (ServerPlayer *p, players) {
            if (p->getHandcardNum() > x) continue;
            if (p->getHandcardNum() < x) targets.clear();
            targets << p;
        }
        if (targets.isEmpty()) return false;
        ServerPlayer *target = room->askForPlayerChosen(player, targets, objectName(), "@jianxiang-target", true, true);
        if (target){
            player->broadcastSkillInvoke(objectName());
            target->drawCards(1, objectName());
        }
        return false;
    }
};


ShenshiCard::ShenshiCard()
{
    will_throw = false;
}

bool ShenshiCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    if (!targets.isEmpty()) return false;
    QList<const Player *> players = Self->getAliveSiblings();
    int max = 0;
    foreach (const Player *p, players) {
        if (max < p->getHandcardNum()) max = p->getHandcardNum();
    }
    return to_select->getHandcardNum() == max;
}

void ShenshiCard::extraCost(Room *room, const CardUseStruct &card_use) const
{
    room->addPlayerMark(card_use.from, "shenshiTransformed");
    CardMoveReason reason(CardMoveReason::S_REASON_GIVE, card_use.from->objectName(), card_use.to.first()->objectName(), "shenshi", QString());
    room->obtainCard(card_use.to.first(), this, reason, false);
}

void ShenshiCard::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *source = effect.from;
    ServerPlayer *target = effect.to;
    Room *room = source->getRoom();
    room->damage(DamageStruct("shenshi", source, target));
    if (source->isAlive() && target->isDead()) {
        QList<ServerPlayer *> targets;
        foreach (ServerPlayer *p, room->getAlivePlayers()) {
            if (p->getHandcardNum() < 4)
                targets << p;
        }
        if (targets.isEmpty()) return;

        ServerPlayer *to_select = room->askForPlayerChosen(source, targets, objectName(), "@shenshi-choose", true);
        if (to_select) {
            int x = 4 - to_select->getHandcardNum();
            if (x > 0)
                to_select->drawCards(x, "shenshi");
        }
    }
}

class ShenshiViewAsSkill : public OneCardViewAsSkill
{
public:
    ShenshiViewAsSkill() : OneCardViewAsSkill("shenshi")
    {
        filter_pattern = ".";
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("ShenshiCard") && player->getMark("shenshiTransformed")%2 == 0;
    }

    const Card *viewAs(const Card *originalcard) const
    {
        ShenshiCard *first = new ShenshiCard;
        first->addSubcard(originalcard->getId());
        first->setSkillName(objectName());
        return first;
    }
};

class Shenshi : public TriggerSkill
{
public:
    Shenshi() : TriggerSkill("shenshi")
    {
        events << Damaged << EventPhaseStart;
        view_as_skill = new ShenshiViewAsSkill;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (triggerEvent == EventPhaseStart && player->getPhase() == Player::NotActive) {
            QList<ServerPlayer *> players = room->getAlivePlayers();
            foreach(ServerPlayer *p, players) {
                p->tag.remove("ShenshiRecord");
            }
        }
    }

    virtual TriggerList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        TriggerList skill_list;
        if (triggerEvent == EventPhaseStart && player->getPhase() == Player::Finish) {
            QList<ServerPlayer *> players = room->getAlivePlayers();
            foreach(ServerPlayer *p, players) {
                QStringList record_list = p->tag["ShenshiRecord"].toStringList();
                foreach(QString record_str, record_list) {
                    bool can_invoke = false;
                    QString name = record_str.split(":").first();
                    int id = record_str.split(":").last().toInt();
                    ServerPlayer *target = room->findPlayer(name);
                    if (target) {
                        QList<const Card *> allcards = target->getCards("he");
                        foreach (const Card *card, allcards) {
                            if (card->getEffectiveId() == id) {
                                can_invoke = true;
                                break;
                            }
                        }
                    }
                    if (can_invoke) {
                        skill_list.insert(p, QStringList("shenshi!"));
                        break;
                    }
                }
            }
        } else if (triggerEvent == Damaged && TriggerSkill::triggerable(player) && player->getMark("shenshiTransformed")%2) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.from && damage.from->isAlive() && damage.from != player && !damage.from->isKongcheng())
                skill_list.insert(player, QStringList(objectName()));
        }

        return skill_list;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *, QVariant &data, ServerPlayer *player) const
    {
        if (triggerEvent == Damaged) {
            DamageStruct damage = data.value<DamageStruct>();
            ServerPlayer *from = damage.from;
            if (room->askForSkillInvoke(player, objectName(), QVariant::fromValue(from))) {
                player->broadcastSkillInvoke(objectName());
                room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), from->objectName());
                room->addPlayerMark(player, "shenshiTransformed");

                if (player->isKongcheng()) {
                    room->doGongxin(player, from, QList<int>(), objectName());
                } else {
                    room->fillAG(from->handCards(), player);
                    const Card *card = room->askForExchange(player, objectName(), 1, 1, true, "@shenshi-give::" + from->objectName());
                    room->clearAG(player);

                    CardMoveReason reason(CardMoveReason::S_REASON_GIVE, player->objectName(), from->objectName(), objectName(), QString());
                    room->moveCardTo(card, player, from, Player::PlaceHand, reason, true);

                    int id = card->getEffectiveId();

                    QStringList record_list = player->tag["ShenshiRecord"].toStringList();
                    record_list << QString("%1:%2").arg(from->objectName()).arg(id);

                    player->tag["ShenshiRecord"] = record_list;
                    delete card;
                }
            }
        } else if (triggerEvent == EventPhaseStart) {
            int x = 4 - player->getHandcardNum();
            if (x > 0)
                player->drawCards(x, objectName());
        }
        return false;
    }
};

class ChenglveDiscard : public ViewAsSkill
{
public:
    ChenglveDiscard() : ViewAsSkill("chenglve_discard")
    {
        response_pattern = "@@chenglve_discard!";
    }


    bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
        return selected.length() < Self->getMark("chenglveDiscardNum") && !Self->isJilei(to_select) && !to_select->isEquipped();
    }

    const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.length() == Self->getMark("chenglveDiscardNum")) {
            DummyCard *discard = new DummyCard;
            discard->addSubcards(cards);
            return discard;
        }

        return NULL;
    }
};

ChenglveCard::ChenglveCard()
{
    target_fixed = true;
}

void ChenglveCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const
{
    int y = 1, x = 2;
    if (source->getMark("chenglveTransformed")%2)
        qSwap(x, y);

    room->addPlayerMark(source, "chenglveTransformed");

    source->drawCards(y, "chenglve");
    QList<int> all_cards = source->forceToDiscard(10086, false);
    QList<int> to_discard = source->forceToDiscard(x, false);
    if (all_cards.length() > x){
        room->setPlayerMark(source, "chenglveDiscardNum", x);
        const Card *card = room->askForCard(source, "@@chenglve_discard!", "@chenglve-discard:::" + QString::number(x), QVariant(), Card::MethodNone);
        room->setPlayerMark(source, "chenglveDiscardNum", 0);
        if (card != NULL && card->subcardsLength() == x) {
            to_discard = card->getSubcards();
        }
    }
    if (to_discard.isEmpty()) return;
    QStringList suitlist;
    foreach(int card_id, to_discard){
        const Card *card = Sanguosha->getCard(card_id);
        QString suit = card->getSuitString();
        if (!suitlist.contains(suit))
            suitlist << suit;
    }

    DummyCard *dummy_card = new DummyCard(to_discard);
    dummy_card->deleteLater();
    CardMoveReason mreason(CardMoveReason::S_REASON_THROW, source->objectName(), QString(), "chenglve", QString());
    room->throwCard(dummy_card, mreason, source);

    QStringList suits = source->property("chenglveSuitList").toString().split("+");
    suits << suitlist;
    room->setPlayerProperty(source, "chenglveSuitList", suits.join("+"));
}

class ChenglveViewAsSkill : public ZeroCardViewAsSkill
{
public:
    ChenglveViewAsSkill() : ZeroCardViewAsSkill("chenglve")
    {

    }

    virtual const Card *viewAs() const
    {
        return new ChenglveCard;
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("ChenglveCard");
    }
};

class Chenglve : public TriggerSkill
{
public:
    Chenglve() : TriggerSkill("chenglve")
    {
        events << EventPhaseChanging;
        view_as_skill = new ChenglveViewAsSkill;
    }

    bool triggerable(const ServerPlayer *) const
    {
        return false;
    }

    virtual void record(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (data.value<PhaseChangeStruct>().from == Player::Play)
            room->setPlayerProperty(player, "chenglveSuitList", QVariant());
    }
};

class ChenglveTargetMod : public TargetModSkill
{
public:
    ChenglveTargetMod() : TargetModSkill("#chenglve-target")
    {
        pattern = "^SkillCard";
    }

    virtual int getDistanceLimit(const Player *from, const Card *card, const Player *) const
    {
        QStringList suits = from->property("chenglveSuitList").toString().split("+");
        if (suits.contains(card->getSuitString()))
            return 1000;
        else
            return 0;
    }

    int getResidueNum(const Player *from, const Card *card, const Player *) const
    {
        //for cheaking

        QStringList suits = from->property("chenglveSuitList").toString().split("+");
        if (suits.contains(card->getSuitString()))
            return 1000;
        else
            return 0;
    }
};

class Shicai : public TriggerSkill
{
public:
    Shicai() : TriggerSkill("shicai")
    {
        events << CardFinished << TargetConfirmed;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        const Card *card = NULL;
        CardUseStruct use = data.value<CardUseStruct>();
        if (triggerEvent == TargetConfirmed) {
            if (use.card->getTypeId() == Card::TypeEquip)
                card = use.card;
        } else if (triggerEvent == CardFinished) {
            if (use.card->getTypeId() == Card::TypeBasic || use.card->isNDTrick())
                card = use.card;
        }
        if (card && card->hasFlag("ShicaiCanInvoke") && room->isAllOnPlace(card, Player::PlaceTable))
            return QStringList(objectName());

        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        const Card *card = data.value<CardUseStruct>().card;
        if (card && room->isAllOnPlace(card, Player::PlaceTable) && room->askForSkillInvoke(player, objectName(), data)) {
            player->broadcastSkillInvoke(objectName());
            CardMoveReason reason(CardMoveReason::S_REASON_PUT, player->objectName(), objectName(), QString());
            if (card->subcardsLength() == 1)
                room->moveCardTo(card, NULL, Player::DrawPile, reason);
            else {
                AskForMoveCardsStruct result = room->askForArrangeCards(player, card->getSubcards(), Room::GuanxingUpOnly);
                QList<int> top_cards = result.top;
                DummyCard *dummy = new DummyCard(top_cards);
                room->moveCardTo(dummy, NULL, Player::DrawPile, reason);
                delete dummy;
            }

            player->drawCards(1, objectName());
        }
        return false;
    }
};

class Mingren : public TriggerSkill
{
public:
    Mingren() : TriggerSkill("mingren")
    {
        events << TurnStart << EventPhaseStart;
    }

    virtual TriggerList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        TriggerList skill_list;
        if (triggerEvent == TurnStart && room->getTag("FirstRound").toBool()) {
            QList<ServerPlayer *> luzhis = room->findPlayersBySkillName(objectName());
            foreach (ServerPlayer *luzhi, luzhis)
                skill_list.insert(luzhi, QStringList("mingren!"));

        } else if (triggerEvent == EventPhaseStart && TriggerSkill::triggerable(player) && player->getPhase() == Player::Finish){
            if (!player->isKongcheng() && !player->getPile("mingren_pile").isEmpty())
                skill_list.insert(player, QStringList(objectName()));
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer *luzhi) const
    {
        if (triggerEvent == TurnStart) {
            room->sendCompulsoryTriggerLog(luzhi, objectName());
            luzhi->broadcastSkillInvoke(objectName());
            luzhi->drawCards(1, objectName());
            if (!luzhi->isKongcheng()) {
                const Card *card = room->askForExchange(luzhi, objectName(), 1, 1, false, "@mingren-push");
                luzhi->addToPile("mingren_pile", card);
            }
        } else if (triggerEvent == EventPhaseStart) {
            const Card *ren = Sanguosha->getCard(player->getPile("mingren_pile").first());
            const Card *card = room->askForExchange(player, objectName(), 1, 1, false, "@mingren-exchange:::"+ren->objectName(), true);
            if (card) {
                LogMessage log;
                log.from = player;
                log.type = "#InvokeSkill";
                log.arg = objectName();
                room->sendLog(log);
                room->notifySkillInvoked(player, objectName());
                player->broadcastSkillInvoke(objectName());

                player->addToPile("mingren_pile", card);
                CardMoveReason reason(CardMoveReason::S_REASON_EXCHANGE_FROM_PILE, player->objectName());
                room->obtainCard(player, ren, reason, true);
            }

        }
        return false;
    }
};

ZhenliangCard::ZhenliangCard()
{
}

bool ZhenliangCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    int x = qMax(qAbs(to_select->getHp() - Self->getHp()), 1);
    return (targets.isEmpty() && x == subcardsLength() && Self->inMyAttackRange(to_select));
}

void ZhenliangCard::extraCost(Room *room, const CardUseStruct &card_use) const
{
    room->addPlayerMark(card_use.from, "zhenliangTransformed");
    SkillCard::extraCost(room, card_use);
}

void ZhenliangCard::onEffect(const CardEffectStruct &effect) const
{
    effect.from->getRoom()->damage(DamageStruct("zhenliang", effect.from, effect.to));
}

class ZhenliangViewAsSkill : public ViewAsSkill
{
public:
    ZhenliangViewAsSkill() : ViewAsSkill("zhenliang")
    {
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("ZhenliangCard") && !player->getPile("mingren_pile").isEmpty() && player->getMark("zhenliangTransformed")%2==0;
    }

    bool viewFilter(const QList<const Card *> &, const Card *to_select) const
    {
        int id = Self->getPile("mingren_pile").first();
        return !Self->isJilei(to_select) && Sanguosha->getCard(id)->sameColorWith(to_select);
    }

    const Card *viewAs(const QList<const Card *> &cards) const
    {
        ZhenliangCard *duwu = new ZhenliangCard;
        if (!cards.isEmpty())
            duwu->addSubcards(cards);
        return duwu;
    }
};

class Zhenliang : public TriggerSkill
{
public:
    Zhenliang() : TriggerSkill("zhenliang")
    {
        events << CardFinished << CardRespondedFinished;
        view_as_skill = new ZhenliangViewAsSkill;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(player) || player->getMark("zhenliangTransformed")%2 == 0
                || player->getPile("mingren_pile").isEmpty() || player->getPhase() != Player::NotActive)
            return QStringList();

        int id = player->getPile("mingren_pile").first();
        const Card *card = NULL;
        if (triggerEvent == CardFinished)
            card = data.value<CardUseStruct>().card;
        else {
            card = data.value<CardResponseStruct>().m_card;
        }
        if (card && card->getTypeId() == Sanguosha->getCard(id)->getTypeId())
            return QStringList(objectName());

        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        ServerPlayer *target = room->askForPlayerChosen(player, room->getAlivePlayers(), objectName(), "@zhenliang-invoke", true, true);
        if (target) {
            room->addPlayerMark(player, "zhenliangTransformed");
            player->broadcastSkillInvoke(objectName());
            target->drawCards(1, objectName());
        }
        return false;
    }
};

ShadowPackage::ShadowPackage()
    : Package("shadow")
{
    General *yanyan = new General(this, "yanyan", "shu");
    yanyan->addSkill(new Juzhan);
    yanyan->addSkill(new JuzhanProhibit);
    related_skills.insertMulti("juzhan", "#juzhan-prohibit");

    General *wangping = new General(this, "wangping", "shu");
    wangping->addSkill(new Feijun);
    wangping->addSkill(new Skill("binglve", Skill::Compulsory)); // in FeijunCard()

    General *luji = new General(this, "luji", "wu", 3);
    luji->addSkill(new Huaiju);
    luji->addSkill(new Weili);
    luji->addSkill(new Zhenglun);

    General *sunliang = new General(this, "sunliang$", "wu", 3);
    sunliang->addSkill(new Kuizhu);
    sunliang->addSkill(new Chezheng);
    sunliang->addSkill(new ChezhengProhibit);
    sunliang->addSkill(new Lijun);
    related_skills.insertMulti("chezheng", "#chezheng-prohibit");

    General *wangji = new General(this, "wangji", "wei", 3);
    wangji->addSkill(new Qizhi);
    wangji->addSkill(new Jinqu);

    General *erkuai = new General(this, "kuaiyuekuailiang", "wei", 3);
    erkuai->addSkill(new Jianxiang);
    erkuai->addSkill(new Shenshi);

    General *xuyou = new General(this, "xuyou", "qun", 3);
    xuyou->addSkill(new Chenglve);
    xuyou->addSkill(new ChenglveTargetMod);
    xuyou->addSkill(new Shicai);
    xuyou->addSkill(new Skill("cunmu", Skill::Compulsory)); // in Room::drawCards()
    related_skills.insertMulti("chenglve", "#chenglve-target");

    General *luzhi = new General(this, "luzhi", "qun", 3);
    luzhi->addSkill(new Mingren);
    luzhi->addSkill(new DetachEffectSkill("mingren", "mingren_pile"));
    related_skills.insertMulti("mingren", "#mingren-clear");
    luzhi->addSkill(new Zhenliang);

    addMetaObject<FeijunCard>();
    addMetaObject<KuizhuCard>();
    addMetaObject<ShenshiCard>();
    addMetaObject<ChenglveCard>();
    addMetaObject<ZhenliangCard>();

    skills << new ChenglveDiscard;
}

ADD_PACKAGE(Shadow)

