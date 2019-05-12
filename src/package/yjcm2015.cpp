#include "yjcm2015.h"
#include "general.h"
#include "player.h"
#include "structs.h"
#include "room.h"
#include "skill.h"
#include "standard.h"
#include "engine.h"
#include "clientplayer.h"
#include "clientstruct.h"
#include "settings.h"
#include "wrapped-card.h"
#include "roomthread.h"
#include "standard-equips.h"
#include "standard-skillcards.h"
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

class Huituo : public MasochismSkill
{
public:
    Huituo() : MasochismSkill("huituo")
    {
        view_as_skill = new dummyVS;
    }

    void onDamaged(ServerPlayer *target, const DamageStruct &damage) const
    {
        Room *room = target->getRoom();
        JudgeStruct j;

        j.who = room->askForPlayerChosen(target, room->getAlivePlayers(), objectName(), "@huituo-select", true, true);
        if (j.who == NULL)
            return;
		
        target->broadcastSkillInvoke(objectName());

        j.pattern = ".";
        j.reason = "huituo";
        j.play_animation = false;
        j.patterns << ".|red" << ".|black";
        room->judge(j);

        if (j.pattern == ".|red")
            room->recover(j.who, RecoverStruct(target));
        else if (j.pattern == ".|black")
            room->drawCards(j.who, damage.damage, objectName());
    }
};

MingjianCard::MingjianCard()
{
}

bool MingjianCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select != Self;
}

void MingjianCard::extraCost(Room *room, const CardUseStruct &card_use) const
{
    CardMoveReason reason(CardMoveReason::S_REASON_GIVE, card_use.from->objectName(), card_use.to.first()->objectName(), "mingjian", QString());
    room->obtainCard(card_use.to.first(), card_use.from->wholeHandCards(), reason, false);
}

void MingjianCard::onEffect(const CardEffectStruct &effect) const
{
	effect.from->getRoom()->addPlayerMark(effect.to, "@mingjian");
}

class MingjianViewAsSkill : public ZeroCardViewAsSkill
{
public:
    MingjianViewAsSkill() : ZeroCardViewAsSkill("mingjian")
    {
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->isKongcheng() && !player->hasUsed("MingjianCard");
    }

    const Card *viewAs() const
    {
        return new MingjianCard;
    }
};

class Mingjian : public TriggerSkill
{
public:
    Mingjian() : TriggerSkill("mingjian")
    {
        events << EventPhaseStart;
        view_as_skill = new MingjianViewAsSkill;
    }

    bool triggerable(const ServerPlayer *) const
    {
        return false;
    }

    virtual void record(TriggerEvent , Room *room, ServerPlayer *target, QVariant &) const
    {
        if (target->getPhase() == Player::NotActive)
            room->setPlayerMark(target, "@mingjian", 0);
    }
};

class MingjianTargetMod : public TargetModSkill
{
public:
    MingjianTargetMod() : TargetModSkill("#mingjian-target")
    {
    }

    virtual int getResidueNum(const Player *from, const Card *, const Player *) const
    {
        return from->getMark("@mingjian");
    }
};

class MingjianMaxCards : public MaxCardsSkill
{
public:
    MingjianMaxCards() : MaxCardsSkill("#mingjian-maxcard")
    {
    }

    virtual int getExtra(const Player *target) const
    {
        return target->getMark("@mingjian");
    }
};

class Xingshuai : public TriggerSkill
{
public:
    Xingshuai() : TriggerSkill("xingshuai$")
    {
        events << Dying << QuitDying;
        limit_mark = "@vicissitude";
        frequency = Limited;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        DyingStruct dying = data.value<DyingStruct>();
        if (triggerEvent == Dying && dying.who == player) {
            if (player != NULL && player->isAlive() && player->hasLordSkill(this) && player->getMark(limit_mark) > 0 && hasWeiGens(player))
                return QStringList(objectName());
        } else if (triggerEvent == QuitDying) {
            QVariantList responsers = player->tag[objectName()].toList();
            if (!responsers.isEmpty())
                return QStringList("xingshuai!");

        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        if (triggerEvent == Dying) {
            if (player->askForSkillInvoke(this, data)) {
                player->broadcastSkillInvoke(objectName());

                room->removePlayerMark(player, limit_mark);

                QList<ServerPlayer *> weis = room->getLieges("wei", player);

                room->sortByActionOrder(weis);

                foreach (ServerPlayer *wei, weis)
                    room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), wei->objectName());

                QVariantList responsers = room->getTag(objectName()).toList();
                foreach (ServerPlayer *wei, weis) {
                    if (wei->askForSkillInvoke("_xingshuai", "xing:" + player->objectName())) {
                        responsers << QVariant::fromValue(wei);
                        room->recover(player, RecoverStruct(wei));
                    }
                }
                player->tag[objectName()] = QVariant::fromValue(responsers);
            }
		} else if (triggerEvent == QuitDying) {
            QVariantList responsers = player->tag[objectName()].toList();
            player->tag.remove(objectName());
            foreach (QVariant responser, responsers) {
                ServerPlayer *wei = responser.value<ServerPlayer *>();
                room->damage(DamageStruct(objectName(), NULL, wei));
            }
		}
        return false;
    }

private:
    static bool hasWeiGens(const Player *lord)
    {
        QList<const Player *> sib = lord->getAliveSiblings();
        foreach (const Player *p, sib) {
            if (p->getKingdom() == "wei")
                return true;
        }

        return false;
    }
};

class Qianju : public DistanceSkill
{
public:
    Qianju() : DistanceSkill("qianju")
    {
    }

    virtual int getCorrect(const Player *from, const Player *) const
    {
        if (from->hasSkill(this))
            return -from->getLostHp();
        else
            return 0;
    }
};

class Qingxi : public TriggerSkill
{
public:
    Qingxi() : TriggerSkill("qingxi")
    {
        events << DamageCaused;
    }

//    virtual int getPriority(TriggerEvent triggerEvent) const
//    {
//        if (triggerEvent == DamageCaused)
//            return 5;
//        return TriggerSkill::getPriority(triggerEvent);
//    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (TriggerSkill::triggerable(player) && player->getWeapon() && damage.card && damage.card->isKindOf("Slash")
                && !damage.chain && !damage.transfer && damage.to->isAlive())
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        ServerPlayer *target = damage.to;
        if (target->isAlive() && room->askForSkillInvoke(player, objectName(), QVariant::fromValue(target))) {
            player->broadcastSkillInvoke(objectName());
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), target->objectName());
            const Weapon *card = qobject_cast<const Weapon *>(player->getWeapon()->getRealCard());
            int x = card->getRange();
            if (room->askForDiscard(target, objectName(), x, x, true, false, "@qingxi-discard")) {
                if (player->getWeapon())
                    room->throwCard(player->getWeapon(), player, target);
            } else {
                ++damage.damage;
                data = QVariant::fromValue(damage);
            }
        }
        return false;
    }
};

HuaiyiCard::HuaiyiCard()
{
    target_fixed = true;
}

void HuaiyiCard::extraCost(Room *room, const CardUseStruct &card_use) const
{
    room->showAllCards(card_use.from);
    room->getThread()->delay(3000);
}

void HuaiyiCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const
{
    QList<int> blacks;
    QList<int> reds;
    foreach (const Card *c, source->getHandcards()) {
        if (c->isRed())
            reds << c->getId();
        else
            blacks << c->getId();
    }

    if (reds.isEmpty() || blacks.isEmpty())
        return;

    QString to_discard = room->askForChoice(source, "huaiyi", "black+red", QVariant(), "@huaiyi-choose");
    QList<int> *pile = NULL;
    if (to_discard == "black")
        pile = &blacks;
    else
        pile = &reds;

    int n = pile->length();

    room->setPlayerMark(source, "huaiyi_num", n);

    DummyCard dm(*pile);
    room->throwCard(&dm, source);

    room->askForUseCard(source, "@@huaiyi", "@huaiyi:::" + QString::number(n), QVariant(), Card::MethodNone);
}

HuaiyiSnatchCard::HuaiyiSnatchCard()
{
    handling_method = Card::MethodNone;
    m_skillName = "_huaiyi";
}

bool HuaiyiSnatchCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    int n = Self->getMark("huaiyi_num");
    if (targets.length() >= n)
        return false;

    if (to_select == Self)
        return false;

    if (to_select->isNude())
        return false;

    return true;
}

void HuaiyiSnatchCard::onUse(Room *room, const CardUseStruct &card_use) const
{
    ServerPlayer *player = card_use.from;

    QList<ServerPlayer *> to = card_use.to;

    room->sortByActionOrder(to);

    foreach (ServerPlayer *p, to) {
        int id = room->askForCardChosen(player, p, "he", "huaiyi");
        player->obtainCard(Sanguosha->getCard(id), false);
    }

    if (to.length() >= 2)
        room->loseHp(player);
}

class Huaiyi : public ZeroCardViewAsSkill
{
public:
    Huaiyi() : ZeroCardViewAsSkill("huaiyi")
    {

    }

    const Card *viewAs() const
    {
        if (Sanguosha->currentRoomState()->getCurrentCardUsePattern() == "@@huaiyi")
            return new HuaiyiSnatchCard;
        else
            return new HuaiyiCard;
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("HuaiyiCard");
    }

    bool isEnabledAtResponse(const Player *, const QString &pattern) const
    {
        return pattern == "@@huaiyi";
    }
};

class Jigong : public PhaseChangeSkill
{
public:
    Jigong() : PhaseChangeSkill("jigong")
    {

    }

    bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Play;
    }

    bool onPhaseChange(ServerPlayer *target) const
    {
		Room *room = target->getRoom();
        if (target->askForSkillInvoke(this)) {
            target->broadcastSkillInvoke(objectName());
			target->drawCards(2, "jigong");
            room->setPlayerFlag(target, "jigong");
        }

        return false;
    }
};

class JigongMax : public MaxCardsSkill
{
public:
    JigongMax() : MaxCardsSkill("#jigong")
    {

    }

    int getFixed(const Player *target) const
    {
        if (target->hasFlag("jigong"))
            return target->getMark("damage_point_play_phase");
        
        return -1;
    }
};

ShifeiCard::ShifeiCard()
{
	target_fixed = true;
}

const Card *ShifeiCard::validateInResponse(ServerPlayer *user) const
{
    Room *room = user->getRoom();
    ServerPlayer *current = room->getCurrent();
    if (current == NULL || current->isDead() || current->getPhase() == Player::NotActive)
        return NULL;

	LogMessage log;
    log.from = user;
    log.type = "#UseCard";
    log.card_str = toString();
    room->sendLog(log);
	room->notifySkillInvoked(user, "shifei");
    user->broadcastSkillInvoke("shifei");
	
	current->drawCards(1, "shifei");
	QList<ServerPlayer *> mosts;
    int most = -1;
    foreach (ServerPlayer *p, room->getAlivePlayers()) {
        int h = p->getHandcardNum();
        if (h > most) {
            mosts.clear();
            most = h;
            mosts << p;
        } else if (most == h)
            mosts << p;
    }
	if (most > 0 && !mosts.isEmpty() && (!mosts.contains(current) || mosts.length() > 1)){
		QList<ServerPlayer *> mosts_copy = mosts;
        foreach (ServerPlayer *p, mosts_copy) {
            if (!user->canDiscard(p, "he"))
                mosts.removeOne(p);
        }
		if (!mosts.isEmpty()){
			ServerPlayer *vic = room->askForPlayerChosen(user, mosts, "shifei", "@shifei-dis");
			int id = room->askForCardChosen(user, vic, "he", "shifei", false, Card::MethodDiscard);
            room->throwCard(id, vic, user);
			Jink *jink = new Jink(Card::NoSuit, 0);
            jink->setSkillName("_shifei");
            return jink;
		}
	}
    room->setPlayerFlag(user, "Global_ShifeiFailed");
    return NULL;
}

class Shifei : public ZeroCardViewAsSkill
{
public:
    Shifei() : ZeroCardViewAsSkill("shifei")
    {
    }

    bool isEnabledAtPlay(const Player *) const
    {
        return false;
    }

    bool isEnabledAtResponse(const Player *player, const QString &pattern) const
    {
        if (player->hasFlag("Global_ShifeiFailed") || pattern != "jink")
			return false;
		QList<const Player *> players = player->getAliveSiblings();
        players.append(player);
        foreach (const Player *p, players) {
            if (p->getPhase() != Player::NotActive) {
                return true;
            }
        }
        return false;
    }

    virtual const Card *viewAs() const
    {
        return new ShifeiCard;
    }
};

ZhanjueCard::ZhanjueCard()
{
    handling_method = Card::MethodNone;
    will_throw = false;
}

bool ZhanjueCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    Duel *duel = new Duel(Card::SuitToBeDecided, -1);
    duel->addSubcards(Self->getHandcards());
    duel->setSkillName("zhanjue");

    return duel && duel->targetFilter(targets, to_select, Self) && !Self->isProhibited(to_select, duel, targets);
}

void ZhanjueCard::onUse(Room *room, const CardUseStruct &card_use) const
{
    ServerPlayer *player = card_use.from;

    QList<ServerPlayer *> to = card_use.to;

    Duel *duel = new Duel(Card::SuitToBeDecided, -1);
    duel->addSubcards(player->getHandcards());
    duel->setSkillName("zhanjue");
    room->useCard(CardUseStruct(duel, player, to));

    room->addPlayerMark(player, "zhanjuedraw", 1);
    player->drawCards(1, "zhanjue");
    QStringList damaged_tag = duel->tag["GlobalCardDamagedTag"].toStringList();

    QList<ServerPlayer *> allplayers = room->getAlivePlayers();
    foreach (ServerPlayer *p, allplayers) {
        if (damaged_tag.contains(p->objectName())) {
            if (p == player)
                room->addPlayerMark(player, "zhanjuedraw", 1);
            p->drawCards(1, "zhanjue");
        }
    }

}

class ZhanjueVS : public ZeroCardViewAsSkill
{
public:
    ZhanjueVS() : ZeroCardViewAsSkill("zhanjue")
    {

    }

    const Card *viewAs() const
    {

        return new ZhanjueCard;
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return player->getMark("zhanjuedraw") < 2 && !player->isKongcheng();
    }
};

class Zhanjue : public TriggerSkill
{
public:
    Zhanjue() : TriggerSkill("zhanjue")
    {
        view_as_skill = new ZhanjueVS;
        events << EventPhaseChanging;
    }

    bool triggerable(const ServerPlayer *) const
    {
        return false;
    }

    virtual void record(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data) const
    {
        PhaseChangeStruct change = data.value<PhaseChangeStruct>();
        if (change.from == Player::Play)
            room->setPlayerMark(player, "zhanjuedraw", 0);
    }
};

QinwangCard::QinwangCard()
{
    mute = true;
}

bool QinwangCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    Slash *slash = new Slash(NoSuit, 0);
    slash->addCostcard(this);
    slash->deleteLater();
    return slash->targetFilter(targets, to_select, Self);
}

const Card *QinwangCard::validate(CardUseStruct &cardUse) const
{
    cardUse.m_isOwnerUse = false;
    ServerPlayer *liubei = cardUse.from;
    QList<ServerPlayer *> targets = cardUse.to;
    Room *room = liubei->getRoom();

    liubei->broadcastSkillInvoke("qinwang");
    room->notifySkillInvoked(liubei, "qinwang");

    LogMessage log;
    log.from = liubei;
    log.to = targets;
    log.type = "#UseCard";
    log.card_str = toString();
    room->sendLog(log);
	
	CardMoveReason reason(CardMoveReason::S_REASON_THROW, liubei->objectName(), QString(), "qinwang", QString());
    room->moveCardTo(this, NULL, Player::DiscardPile, reason, true);

    const Card *slash = NULL;

    QList<ServerPlayer *> lieges = room->getLieges("shu", liubei);
    foreach(ServerPlayer *target, targets)
        target->setFlags("JijiangTarget");
    foreach (ServerPlayer *liege, lieges) {
        try {
            slash = room->askForCard(liege, "slash", "@jijiang-slash:" + liubei->objectName(),
                QVariant(), Card::MethodResponse, liubei, false, QString(), true);
        }
        catch (TriggerEvent triggerEvent) {
            if (triggerEvent == TurnBroken || triggerEvent == StageChange) {
                foreach(ServerPlayer *target, targets)
                    target->setFlags("-JijiangTarget");
            }
            throw triggerEvent;
        }

        if (slash) {
            foreach(ServerPlayer *target, targets)
                target->setFlags("-JijiangTarget");

			slash->tag["QinwangResponser"] = QVariant::fromValue(liege);
            return slash;
        }
    }
    foreach(ServerPlayer *target, targets)
        target->setFlags("-JijiangTarget");
    return NULL;
}

class Qinwang : public OneCardViewAsSkill
{
public:
    Qinwang() : OneCardViewAsSkill("qinwang$")
    {
        filter_pattern = ".!";
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        Jijiang jj;
        return jj.isEnabledAtPlay(player);
    }

    bool isEnabledAtResponse(const Player *player, const QString &pattern) const
    {
        Jijiang jj;
        return jj.isEnabledAtResponse(player, pattern);
    }

    const Card *viewAs(const Card *originalCard) const
    {
        QinwangCard *qw = new QinwangCard;
        qw->addSubcard(originalCard);
        return qw;
    }
};

class Yaoming : public TriggerSkill
{
public:
    Yaoming() : TriggerSkill("yaoming")
    {
        events << Damage << Damaged ;
        view_as_skill = new dummyVS;
    }

    virtual QStringList triggerable(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer * &) const
    {
        if (TriggerSkill::triggerable(player) && !player->hasFlag("YaomingInvoked")) {
            foreach (ServerPlayer *p, room->getAlivePlayers()) {
                if (p->getHandcardNum() != player->getHandcardNum())
                    return QStringList(objectName());
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        QList<ServerPlayer *> targets;
        foreach (ServerPlayer *p, room->getAlivePlayers()) {
            if (p->getHandcardNum() != player->getHandcardNum())
                targets << p;
        }
        if (targets.isEmpty()) return false;
        ServerPlayer *target = room->askForPlayerChosen(player, targets, objectName(), "yaoming-invoke", true, true);
        if (target) {
            target->broadcastSkillInvoke(objectName());
            room->setPlayerFlag(player, "YaomingInvoked");
            if (target->getHandcardNum() > player->getHandcardNum()) {
                int to_throw = room->askForCardChosen(player, target, "h", objectName(), false, Card::MethodDiscard);
                room->throwCard(to_throw, target, player);
            } else if (target->getHandcardNum() < player->getHandcardNum())
                target->drawCards(1, "yaoming");
        }
        return false;
    }
};

YanzhuCard::YanzhuCard()
{

}

bool YanzhuCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select != Self && !to_select->isNude();
}

void YanzhuCard::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *target = effect.to;
    Room *r = target->getRoom();

    if (!r->askForDiscard(target, "yanzhu", 1, 1, !target->getEquips().isEmpty(), true, "@yanzhu-discard")) {
        if (!target->getEquips().isEmpty()) {
            DummyCard dummy;
            dummy.addSubcards(target->getEquips());
            r->obtainCard(effect.from, &dummy);
        }

        r->detachSkillFromPlayer(effect.from, "yanzhu");
    }
}

class Yanzhu : public ZeroCardViewAsSkill
{
public:
    Yanzhu() : ZeroCardViewAsSkill("yanzhu")
    {

    }

    const Card *viewAs() const
    {
        return new YanzhuCard;
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("YanzhuCard");
    }
};

XingxueCard::XingxueCard()
{

}

bool XingxueCard::targetFilter(const QList<const Player *> &targets, const Player *, const Player *Self) const
{
    int n = Self->hasSkill("yanzhu", true) ? Self->getHp() : Self->getMaxHp();
    return targets.length() < n;
}

void XingxueCard::onEffect(const CardEffectStruct &effect) const
{
	ServerPlayer *target = effect.to;
    Room *room = target->getRoom();
    target->drawCards(1, "xingxue");
    if (target->isAlive() && !target->isNude()) {
        const Card *c = room->askForExchange(target, "xingxue", 1, 1, true, "@xingxue-put");
        CardMoveReason reason(CardMoveReason::S_REASON_PUT, target->objectName(), QString(), "xingxue", QString());
        room->moveCardTo(c, target, NULL, Player::DrawPile, reason, false);
    }
}

class XingxueVS : public ZeroCardViewAsSkill
{
public:
    XingxueVS() : ZeroCardViewAsSkill("xingxue")
    {
        response_pattern = "@@xingxue";
    }

    const Card *viewAs() const
    {
        return new XingxueCard;
    }
};

class Xingxue : public PhaseChangeSkill
{
public:
    Xingxue() : PhaseChangeSkill("xingxue")
    {
        view_as_skill = new XingxueVS;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Finish;
    }

    bool onPhaseChange(ServerPlayer *target) const
    {
		int n = target->hasSkill("yanzhu", true) ? target->getHp() : target->getMaxHp();
        target->getRoom()->askForUseCard(target, "@@xingxue", "@xingxue:::" + QString::number(n));
        return false;
    }
};

class Qiaoshi : public TriggerSkill
{
public:
    Qiaoshi() : TriggerSkill("qiaoshi")
    {
        events << EventPhaseStart;
    }

    virtual TriggerList triggerable(TriggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        TriggerList skill_list;
        if (player != NULL && player->isAlive() && player->getPhase() == Player::Finish) {
            QList<ServerPlayer *> xiahoushis = room->findPlayersBySkillName(objectName());
            foreach (ServerPlayer *xiahoushi, xiahoushis) {
                if (xiahoushi != NULL && player != xiahoushi && xiahoushi->getHandcardNum() == player->getHandcardNum())
                    skill_list.insert(xiahoushi, QStringList(objectName()));
            }
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent, Room *, ServerPlayer *player, QVariant &, ServerPlayer *xiahoushi) const
    {
        if (xiahoushi->askForSkillInvoke(objectName(), QVariant::fromValue(player))) {
            xiahoushi->broadcastSkillInvoke(objectName());
            player->drawCards(1, objectName());
            xiahoushi->drawCards(1, objectName());
        }
        return false;
    }
};

YanyuCard::YanyuCard()
{
    will_throw = false;
    can_recast = true;
    handling_method = Card::MethodRecast;
    target_fixed = true;
}

void YanyuCard::onUse(Room *room, const CardUseStruct &card_use) const
{
    ServerPlayer *xiahou = card_use.from;

    LogMessage log;
    log.type = "$RecastCardWithSkill";
    log.from = xiahou;
    log.arg = getSkillName();
    log.card_str = QString::number(card_use.card->getSubcards().first());
    room->sendLog(log);
    xiahou->broadcastSkillInvoke(getSkillName());
    room->notifySkillInvoked(xiahou, getSkillName());

    CardMoveReason reason(CardMoveReason::S_REASON_RECAST, xiahou->objectName());
    reason.m_skillName = getSkillName();
    room->moveCardTo(this, xiahou, NULL, Player::DiscardPile, reason);

    xiahou->drawCards(1, "recast");

    xiahou->addMark("yanyu");
}

class YanyuVS : public OneCardViewAsSkill
{
public:
    YanyuVS() : OneCardViewAsSkill("yanyu")
    {
        filter_pattern = "Slash";
    }

    const Card *viewAs(const Card *originalCard) const
    {
        if (Self->isCardLimited(originalCard, Card::MethodRecast))
            return NULL;

        YanyuCard *recast = new YanyuCard;
        recast->addSubcard(originalCard);
        return recast;
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        Slash *s = new Slash(Card::NoSuit, 0);
        s->deleteLater();
        return !player->isCardLimited(s, Card::MethodRecast);
    }
};

class Yanyu : public TriggerSkill
{
public:
    Yanyu() : TriggerSkill("yanyu")
    {
        view_as_skill = new YanyuVS;
        events << EventPhaseEnd << EventPhaseStart;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (triggerEvent == EventPhaseStart && player->getPhase() == Player::Play)
            room->setPlayerMark(player, "yanyu", 0);
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer * &) const
    {
        if (triggerEvent == EventPhaseEnd && TriggerSkill::triggerable(player)) {
            if (player->getPhase() == Player::Play && player->getMark("yanyu") > 1) {
                foreach (ServerPlayer *p, room->getAlivePlayers()) {
                    if (p->isMale())
                        return QStringList(objectName());
                }
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        QList<ServerPlayer *> malelist;
        foreach (ServerPlayer *p, room->getAlivePlayers()) {
            if (p->isMale())
                malelist << p;
        }
        if (malelist.isEmpty()) return false;
        ServerPlayer *male = room->askForPlayerChosen(player, malelist, objectName(), "@yanyu-give", true, true);
        if (male != NULL){
            player->broadcastSkillInvoke(objectName());
            male->drawCards(2, objectName());
		}
        return false;
    }
};

FurongCard::FurongCard()
{
    handling_method = Card::MethodNone;
}

bool FurongCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.length() == 0 && to_select != Self && !to_select->isKongcheng();
}

void FurongCard::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.from->getRoom();

    QList<const Card *> cards = room->askForFurong(effect.from, effect.to, "furong");
    const Card *card1 = cards.first();
    const Card *card2 = cards.last();

    room->showCard(effect.from, card1->getEffectiveId());
    room->showCard(effect.to, card2->getEffectiveId());

    if (card1->isKindOf("Slash") && !card2->isKindOf("Jink")) {
        room->throwCard(card1, effect.from);
        room->damage(DamageStruct(objectName(), effect.from, effect.to));
    } else if (!card1->isKindOf("Slash") && card2->isKindOf("Jink")) {
        room->throwCard(card1, effect.from);
        if (!effect.to->isNude()) {
            int id = room->askForCardChosen(effect.from, effect.to, "he", objectName());
            room->obtainCard(effect.from, id, false);
        }
    }
}

class Furong : public ZeroCardViewAsSkill
{
public:
    Furong() : ZeroCardViewAsSkill("furong")
    {

    }

    const Card *viewAs() const
    {
        return new FurongCard;
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("FurongCard") && !player->isKongcheng();
    }
};

class Shizhi : public TriggerSkill
{
public:
    Shizhi() : TriggerSkill("#shizhi")
    {
        events << HpChanged << MaxHpChanged << EventAcquireSkill << EventLoseSkill << GameStart;
    }

    bool triggerable(const ServerPlayer *) const
    {
        return false;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
//        if (triggerEvent == EventAcquireSkill || triggerEvent == EventLoseSkill) {
//            if (data.toString() != "shizhi")
//                return;
//        }

//        if (triggerEvent == HpChanged || triggerEvent == MaxHpChanged || triggerEvent == GameStart) {
//            if (!player->hasSkill(this))
//                return;
//        }

//        bool skillStateBefore = false;
//        if (triggerEvent != EventAcquireSkill && triggerEvent != GameStart)
//            skillStateBefore = player->getMark("shizhi") > 0;

//        bool skillStateAfter = false;
//        if (triggerEvent == EventLoseSkill)
//            skillStateAfter = true;
//        else
//            skillStateAfter = player->getHp() == 1;

//        if (skillStateAfter != skillStateBefore)
//            room->filterCards(player, player->getCards("he"), true);

//        player->setMark("shizhi", skillStateAfter ? 1 : 0);

    }
};

class ShizhiFilter : public FilterSkill
{
public:
    ShizhiFilter() : FilterSkill("shizhi")
    {

    }

    bool viewFilter(const Card *to_select) const
    {
        Room *room = Sanguosha->currentRoom();
        ServerPlayer *player = room->getCardOwner(to_select->getId());
        return player != NULL && player->getHp() == 1 && to_select->isKindOf("Jink");
    }

    const Card *viewAs(const Card *originalCard) const
    {
        Slash *slash = new Slash(originalCard->getSuit(), originalCard->getNumber());
        slash->setSkillName(objectName());
        WrappedCard *card = Sanguosha->getWrappedCard(originalCard->getId());
        card->takeOver(slash);
        return card;
    }
};

HuomoCard::HuomoCard()
{
    will_throw = false;
    handling_method = Card::MethodNone;
    m_skillName = "huomo";
}

bool HuomoCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    Card *card = Sanguosha->cloneCard(user_string, Card::NoSuit, 0);
    if (card == NULL)
        return false;

    card->addCostcard(this);
    card->setCanRecast(false);
    card->deleteLater();
    return card && card->targetFilter(targets, to_select, Self) && !Self->isProhibited(to_select, card, targets);
}

bool HuomoCard::targetFixed() const
{
    Card *card = Sanguosha->cloneCard(user_string, Card::NoSuit, 0);
    if (card == NULL)
        return false;

    card->setCanRecast(false);
    card->deleteLater();
    return card && card->targetFixed();
}

bool HuomoCard::targetsFeasible(const QList<const Player *> &targets, const Player *Self) const
{
    Card *card = Sanguosha->cloneCard(user_string, Card::NoSuit, 0);
    if (card == NULL)
        return false;

    card->setCanRecast(false);
    card->deleteLater();
    return card && card->targetsFeasible(targets, Self);
}

const Card *HuomoCard::validate(CardUseStruct &card_use) const
{
    ServerPlayer *zhongyao = card_use.from;
    Room *room = zhongyao->getRoom();

    QString user_str = user_string;
    CardMoveReason reason(CardMoveReason::S_REASON_PUT, zhongyao->objectName(), QString(), "huomo", QString());
    room->moveCardTo(this, NULL, Player::DrawPile, reason, true);

    Card *c = Sanguosha->cloneCard(user_str, Card::NoSuit, 0);

    c->setSkillName("huomo");
    c->deleteLater();
    c->addCostcard(this);
    return c;
}

const Card *HuomoCard::validateInResponse(ServerPlayer *zhongyao) const
{
    Room *room = zhongyao->getRoom();

    CardMoveReason reason(CardMoveReason::S_REASON_PUT, zhongyao->objectName(), QString(), "huomo", QString());
    room->moveCardTo(this, NULL, Player::DrawPile, reason, true);

    QString user_str = user_string;
    Card *c = Sanguosha->cloneCard(user_str, Card::NoSuit, 0);
    c->setSkillName("huomo");
    c->deleteLater();
    return c;

}

class HuomoVS : public OneCardViewAsSkill
{
public:
    HuomoVS() : OneCardViewAsSkill("huomo")
    {
        filter_pattern = "^BasicCard|black";
    }

    const Card *viewAs(const Card *originalCard) const
    {
        HuomoCard *hm = new HuomoCard;
        hm->addSubcard(originalCard);
        return hm;
    }

    bool isEnabledAtPlay(const Player *) const
    {
        return true; // for DIY!!!!!!!
    }

    bool isEnabledAtResponse(const Player *player, const QString &pattern) const
    {
        QList<const Player *> sib = player->getAliveSiblings();
        if (player->isAlive())
            sib << player;

        bool noround = true;

        foreach (const Player *p, sib) {
            if (p->getPhase() != Player::NotActive) {
                noround = false;
                break;
            }
        }

        if (noround) return false;

        if (Sanguosha->currentRoomState()->getCurrentCardUseReason() != CardUseStruct::CARD_USE_REASON_RESPONSE_USE)
            return false;

#define HUOMO_CAN_USE(x) (!player->hasFlag("HuomoHasUsed_" #x))

        if (pattern == "slash")
            return HUOMO_CAN_USE(Slash);
        else if (pattern == "peach")
            return HUOMO_CAN_USE(Peach);
        else if (pattern.contains("analeptic"))
            return HUOMO_CAN_USE(Peach) || HUOMO_CAN_USE(Analeptic);
        else if (pattern == "jink")
            return HUOMO_CAN_USE(Jink);

#undef HUOMO_CAN_USE

        return false;
    }
};

class Huomo : public TriggerSkill
{
public:
    Huomo() : TriggerSkill("huomo")
    {
        view_as_skill = new HuomoVS;
    }

    QString getSelectBox() const
    {
        return "guhuo_b";
    }

    bool buttonEnabled(const QString &button_name, const QList<const Card *> &, const QList<const Player *> &) const
    {
        if (button_name.isEmpty()) return true;
        Card *card = Sanguosha->cloneCard(button_name, Card::NoSuit, 0);
        if (card == NULL) return false;
        QString classname = card->getClassName();
        if (card->isKindOf("Slash"))
            classname = "Slash";
        if (Self->hasFlag("HuomoHasUsed_" + classname)) return false;
        return Skill::buttonEnabled(button_name);
    }

    bool trigger(TriggerEvent, Room *, ServerPlayer *, QVariant &) const
    {
        return false;
    }
};

class Zuoding : public TriggerSkill
{
public:
    Zuoding() : TriggerSkill("zuoding")
    {
        events << TargetSpecified;
		view_as_skill = new dummyVS;
    }

    virtual TriggerList triggerable(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        TriggerList skill_list;
        if (room->getTag("ZuodingCannot").toBool() || player->getPhase() != Player::Play) return skill_list;
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card == NULL || use.card->getTypeId() == Card::TypeSkill || use.card->getSuit() != Card::Spade
                || use.to.isEmpty() || use.index > 0) return skill_list;
        QList<ServerPlayer *> zhongyaos = room->findPlayersBySkillName(objectName());
        foreach (ServerPlayer *zhongyao, zhongyaos) {
            if (zhongyao != NULL && player != zhongyao)
                skill_list.insert(zhongyao, QStringList(objectName()));
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *, QVariant &data, ServerPlayer *zhongyao) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        ServerPlayer *p = room->askForPlayerChosen(zhongyao, use.to, objectName(), "@zuoding", true, true);
        if (p != NULL){
            zhongyao->broadcastSkillInvoke(objectName());
            p->drawCards(1, objectName());
        }
        return false;
    }
};

AnguoCard::AnguoCard()
{

}

bool AnguoCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select != Self;
}

void AnguoCard::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.to->getRoom();
    bool draw = true, recover = true, use_equip = true;
    QList<ServerPlayer *> players;
    players << effect.to;
    players << effect.from;
    foreach (ServerPlayer *target, players) {
        foreach (ServerPlayer *player, room->getOtherPlayers(target)) {
            if (player->getHandcardNum() < target->getHandcardNum()) {
                draw = false;
                break;
            }
        }
        if (draw) target->drawCards(1, "anguo");

        if (target->isDead() || !target->isWounded())
            recover = false;
        else {
            foreach (ServerPlayer *player, room->getOtherPlayers(target)) {
                if (player->getHp() < target->getHp()) {
                    recover = false;
                    break;
                }
            }
        }
        if (recover) room->recover(target, RecoverStruct(effect.from));

        if (target->isDead())
            use_equip = false;
        else {
            foreach (ServerPlayer *player, room->getOtherPlayers(target)) {
                if (player->getEquips().length() < target->getEquips().length()) {
                    use_equip = false;
                    break;
                }
            }
        }
        if (use_equip) {
            QList<int> equips;
            foreach (int card_id, room->getDrawPile()) {
                const Card *equip = Sanguosha->getCard(card_id);
                if (equip->getTypeId() == Card::TypeEquip || target->isLocked(equip))
                    equips << card_id;
            }
            if (!equips.isEmpty()){
                int index = qrand() % equips.length();
                int id = equips.at(index);
                const Card *equip = Sanguosha->getCard(id);
                room->useCard(CardUseStruct(equip, target, target));
            }
        }
        draw = !draw;
        recover = !recover;
        use_equip = !use_equip;
    }

}

class Anguo : public ZeroCardViewAsSkill
{
public:
    Anguo() : ZeroCardViewAsSkill("anguo")
    {

    }

    const Card *viewAs() const
    {
        return new AnguoCard;
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("AnguoCard");
    }
};

YJCM2015Package::YJCM2015Package()
    : Package("YJCM2015")
{

    General *caorui = new General(this, "caorui$", "wei", 3);
    caorui->addSkill(new Huituo);
    caorui->addSkill(new Mingjian);
	caorui->addSkill(new MingjianMaxCards);
	caorui->addSkill(new MingjianTargetMod);
	related_skills.insertMulti("mingjian", "#mingjian-maxcard");
	related_skills.insertMulti("mingjian", "#mingjian-target");
    caorui->addSkill(new Xingshuai);

    General *caoxiu = new General(this, "caoxiu", "wei");
    caoxiu->addSkill(new Qianju);
	caoxiu->addSkill(new Qingxi);

    General *gongsun = new General(this, "gongsunyuan", "qun");
    gongsun->addSkill(new Huaiyi);

    General *guopang = new General(this, "guotupangji", "qun", 3);
    guopang->addSkill(new Jigong);
    guopang->addSkill(new JigongMax);
    related_skills.insertMulti("jigong", "#jigong");
    guopang->addSkill(new Shifei);

    General *liuchen = new General(this, "liuchen$", "shu");
    liuchen->addSkill(new Zhanjue);
    liuchen->addSkill(new Qinwang);

    General *quancong = new General(this, "quancong", "wu");
    quancong->addSkill(new Yaoming);

    General *sunxiu = new General(this, "sunxiu$", "wu", 3);
    sunxiu->addSkill(new Yanzhu);
    sunxiu->addSkill(new Xingxue);
    sunxiu->addSkill(new Skill("zhaofu$", Skill::Compulsory));

    General *xiahou = new General(this, "xiahoushi", "shu", 3, false);
    xiahou->addSkill(new Qiaoshi);
    xiahou->addSkill(new Yanyu);

    General *zhangni = new General(this, "zhangni", "shu");
    zhangni->addSkill(new Furong);
    zhangni->addSkill(new Shizhi);
    zhangni->addSkill(new ShizhiFilter);
    related_skills.insertMulti("shizhi", "#shizhi");

    General *zhongyao = new General(this, "zhongyao", "wei", 3);
    zhongyao->addSkill(new Huomo);
    zhongyao->addSkill(new Zuoding);

    General *zhuzhi = new General(this, "zhuzhi", "wu");
    zhuzhi->addSkill(new Anguo);

    addMetaObject<HuaiyiCard>();
    addMetaObject<HuaiyiSnatchCard>();
	addMetaObject<ShifeiCard>();
    addMetaObject<ZhanjueCard>();
    addMetaObject<QinwangCard>();
    addMetaObject<YanzhuCard>();
    addMetaObject<XingxueCard>();
    addMetaObject<YanyuCard>();
    addMetaObject<FurongCard>();
    addMetaObject<HuomoCard>();
    addMetaObject<AnguoCard>();
	addMetaObject<MingjianCard>();

}
ADD_PACKAGE(YJCM2015)
