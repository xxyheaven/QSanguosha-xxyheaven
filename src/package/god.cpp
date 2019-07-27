#include "god.h"
#include "standard-skillcards.h"
#include "client.h"
#include "engine.h"
#include "maneuvering.h"
#include "general.h"
#include "settings.h"

#include <QGroupBox>
#include <QAbstractButton>
#include <QButtonGroup>
#include <QDialog>

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

class Wushen : public FilterSkill
{
public:
    Wushen() : FilterSkill("wushen")
    {
    }

    virtual bool viewFilter(const Card *to_select) const
    {
        Room *room = Sanguosha->currentRoom();
        Player::Place place = room->getCardPlace(to_select->getEffectiveId());
        return to_select->getSuit() == Card::Heart && place == Player::PlaceHand;
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

class WushenTargetMod : public TargetModSkill
{
public:
    WushenTargetMod() : TargetModSkill("#wushen-target")
    {
    }

    virtual int getDistanceLimit(const Player *from, const Card *card, const Player *) const
    {
        if (from->hasSkill("wushen") && card->getSuit() == Card::Heart)
            return 1000;
        else
            return 0;
    }
};

class Wuhun : public TriggerSkill
{
public:
    Wuhun() : TriggerSkill("wuhun")
    {
        events << Damaged << Death;
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (triggerEvent == Damaged && TriggerSkill::triggerable(player)) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.from && damage.from->isAlive())
                return QStringList(objectName());
        } else if (triggerEvent == Death && player != NULL && player->hasSkill(objectName())) {
            DeathStruct death = data.value<DeathStruct>();
            if (death.who == player) {
                QList<ServerPlayer *> players = room->getOtherPlayers(player);
                foreach(ServerPlayer *p, players) {
                    if (p->getMark("@nightmare") > 0)
                        return QStringList(objectName());
                }
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *shenguanyu, QVariant &data, ServerPlayer *) const
    {
        if (triggerEvent == Damaged) {
            DamageStruct damage = data.value<DamageStruct>();

            if (damage.from && damage.from->isAlive()) {
                room->sendCompulsoryTriggerLog(shenguanyu, objectName());
                shenguanyu->broadcastSkillInvoke(objectName());
                damage.from->gainMark("@nightmare", damage.damage);
            }
        } else if (triggerEvent == Death) {
            QList<ServerPlayer *> players = room->getOtherPlayers(shenguanyu);

            int max = 0;
            foreach(ServerPlayer *player, players)
                max = qMax(max, player->getMark("@nightmare"));
            if (max == 0) return false;

            QList<ServerPlayer *> foes;
            foreach (ServerPlayer *player, players) {
                if (player->getMark("@nightmare") == max)
                    foes << player;
            }

            if (foes.isEmpty())
                return false;
            room->sendCompulsoryTriggerLog(shenguanyu, objectName());
            shenguanyu->broadcastSkillInvoke(objectName());
            ServerPlayer *foe = NULL;
            if (foes.length() == 1)
                foe = foes.first();
            else
                foe = room->askForPlayerChosen(shenguanyu, foes, "wuhun", "@wuhun-revenge");

            JudgeStruct judge;
            judge.pattern = "Peach,GodSalvation";
            judge.good = true;
            judge.negative = true;
            judge.reason = "wuhun";
            judge.who = foe;

            room->judge(judge);

            if (judge.isBad()) {
                room->killPlayer(foe);
            }
        }

        return false;
    }
};

/*
static bool CompareBySuit(int card1, int card2)
{
    const Card *c1 = Sanguosha->getCard(card1);
    const Card *c2 = Sanguosha->getCard(card2);

    int a = static_cast<int>(c1->getSuit());
    int b = static_cast<int>(c2->getSuit());

    return a < b;
}
*/
class Shelie : public PhaseChangeSkill
{
public:
    Shelie() : PhaseChangeSkill("shelie")
    {
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Draw;
    }

    virtual bool onPhaseChange(ServerPlayer *shenlvmeng) const
    {
        Room *room = shenlvmeng->getRoom();
        if (!shenlvmeng->askForSkillInvoke(this)) return false;
        shenlvmeng->broadcastSkillInvoke(objectName());
        QList<int> card_ids = room->getNCards(5);

		QSet<Card::Suit> suits;
		foreach(int card_id, card_ids)
			suits << Sanguosha->getCard(card_id)->getSuit();

        AskForMoveCardsStruct result = room->askForMoveCards(shenlvmeng, card_ids, QList<int>(), true, objectName(), "differentsuit", suits.size(), 0, false, true);
		QList<int> selected = result.bottom;
		DummyCard *dummy = new DummyCard(selected);
		room->obtainCard(shenlvmeng, dummy, true);
		QList<int> card_to_throw = result.top;
		dummy = new DummyCard(card_to_throw);
		CardMoveReason reason(CardMoveReason::S_REASON_NATURAL_ENTER, shenlvmeng->objectName(), "shelie", QString());
		room->throwCard(dummy, reason, NULL);
		dummy->deleteLater();
        return true;
    }
};

GongxinCard::GongxinCard()
{
}

bool GongxinCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select != Self;
}

void GongxinCard::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.from->getRoom();
	if (!effect.to->isKongcheng()) {
        QList<int> ids;
        foreach (const Card *card, effect.to->getHandcards()) {
            if (card->getSuit() == Card::Heart)
                ids << card->getEffectiveId();
        }

        int card_id = room->doGongxin(effect.from, effect.to, ids);
        if (card_id == -1) return;

        QString result = room->askForChoice(effect.from, "gongxin", "discard+put");
        effect.from->tag.remove("gongxin");
        if (result == "discard") {
            CardMoveReason reason(CardMoveReason::S_REASON_DISMANTLE, effect.from->objectName(), QString(), "gongxin", QString());
            room->throwCard(Sanguosha->getCard(card_id), reason, effect.to, effect.from);
        } else {
            effect.from->setFlags("Global_GongxinOperator");
            CardMoveReason reason(CardMoveReason::S_REASON_PUT, effect.from->objectName(), QString(), "gongxin", QString());
            room->moveCardTo(Sanguosha->getCard(card_id), effect.to, NULL, Player::DrawPile, reason, true);
            effect.from->setFlags("-Global_GongxinOperator");
        }
    }
}

class Gongxin : public ZeroCardViewAsSkill
{
public:
    Gongxin() : ZeroCardViewAsSkill("gongxin")
    {
    }

    virtual const Card *viewAs() const
    {
        return new GongxinCard;
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("GongxinCard");
    }
};

void YeyanCard::damage(ServerPlayer *shenzhouyu, ServerPlayer *target, int point) const
{
    shenzhouyu->getRoom()->damage(DamageStruct("yeyan", shenzhouyu, target, point, DamageStruct::Fire));
}

GreatYeyanCard::GreatYeyanCard()
{
    m_skillName = "yeyan";
}

bool GreatYeyanCard::targetFilter(const QList<const Player *> &, const Player *, const Player *) const
{
    Q_ASSERT(false);
    return false;
}

bool GreatYeyanCard::targetsFeasible(const QList<const Player *> &targets, const Player *) const
{
    if (subcards.length() != 4) return false;
    QList<Card::Suit> allsuits;
    foreach (int cardId, subcards) {
        const Card *card = Sanguosha->getCard(cardId);
        if (allsuits.contains(card->getSuit())) return false;
        allsuits.append(card->getSuit());
    }

    //We can only assign 2 damage to one player
    //If we select only one target only once, we assign 3 damage to the target
    if (targets.toSet().size() == 1)
        return true;
    else if (targets.toSet().size() == 2)
        return targets.size() == 3;
    return false;
}

bool GreatYeyanCard::targetFilter(const QList<const Player *> &targets, const Player *to_select,
    const Player *, int &maxVotes) const
{
    int i = 0;
    foreach(const Player *player, targets)
        if (player == to_select) i++;
    maxVotes = qMax(3 - targets.size(), 0) + i;
    return maxVotes > 0;
}

void GreatYeyanCard::use(Room *room, ServerPlayer *shenzhouyu, QList<ServerPlayer *> &targets) const
{
    int criticaltarget = 0;
    int totalvictim = 0;
    QMap<ServerPlayer *, int> map;

    foreach(ServerPlayer *sp, targets)
        map[sp]++;

    if (targets.size() == 1)
        map[targets.first()] += 2;

    foreach (ServerPlayer *sp, map.keys()) {
        if (map[sp] > 1) criticaltarget++;
        totalvictim++;
    }
    if (criticaltarget > 0) {
        room->removePlayerMark(shenzhouyu, "@flame");
        room->loseHp(shenzhouyu, 3);

        QList<ServerPlayer *> targets = map.keys();
        room->sortByActionOrder(targets);
        foreach(ServerPlayer *sp, targets)
            damage(shenzhouyu, sp, map[sp]);
    }
}

SmallYeyanCard::SmallYeyanCard()
{
    m_skillName = "yeyan";
}

bool SmallYeyanCard::targetsFeasible(const QList<const Player *> &targets, const Player *) const
{
    return !targets.isEmpty();
}

bool SmallYeyanCard::targetFilter(const QList<const Player *> &targets, const Player *, const Player *) const
{
    return targets.length() < 3;
}

void SmallYeyanCard::use(Room *room, ServerPlayer *shenzhouyu, QList<ServerPlayer *> &targets) const
{
    room->removePlayerMark(shenzhouyu, "@flame");
    Card::use(room, shenzhouyu, targets);
}

void SmallYeyanCard::onEffect(const CardEffectStruct &effect) const
{
    damage(effect.from, effect.to, 1);
}

class Yeyan : public ViewAsSkill
{
public:
    Yeyan() : ViewAsSkill("yeyan")
    {
        frequency = Limited;
        limit_mark = "@flame";
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return player->getMark("@flame") >= 1;
    }

    virtual bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
        if (selected.length() >= 4)
            return false;

        if (to_select->isEquipped())
            return false;

        if (Self->isJilei(to_select))
            return false;

        foreach (const Card *item, selected) {
            if (to_select->getSuit() == item->getSuit())
                return false;
        }

        return true;
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.length() == 0)
            return new SmallYeyanCard;
        if (cards.length() != 4)
            return NULL;

        GreatYeyanCard *card = new GreatYeyanCard;
        card->addSubcards(cards);

        return card;
    }
};

class Qinyin : public TriggerSkill
{
public:
    Qinyin() : TriggerSkill("qinyin")
    {
        events << EventPhaseEnd;
		view_as_skill = new dummyVS;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return TriggerSkill::triggerable(target) && target->getPhase() == Player::Discard && target->getMark("GlobalRuleDiscardCount") > 1;
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *shenzhouyu, QVariant &, ServerPlayer *) const
    {
        if (!room->askForSkillInvoke(shenzhouyu, "skill_ask", "prompt:::"+objectName())) return false;
        QStringList choices;
        choices << "down" << "cancel";
        QList<ServerPlayer *> all_players = room->getAllPlayers();
        foreach (ServerPlayer *player, all_players) {
            if (player->isWounded()) {
                choices.append("up");
                break;
            }
        }
        QString result = room->askForChoice(shenzhouyu, objectName(), choices.join("+"), QVariant(), QString(), "up+down+cancel");
        if (result == "cancel")
            return false;
        LogMessage log;
        log.type = "#InvokeSkill";
        log.from = shenzhouyu;
        log.arg = objectName();
        room->sendLog(log);
        room->notifySkillInvoked(shenzhouyu, objectName());
        if (result == "up") {
            shenzhouyu->broadcastSkillInvoke(objectName(), 1);
            foreach(ServerPlayer *player, all_players)
                room->recover(player, RecoverStruct(shenzhouyu));
        } else if (result == "down") {
            shenzhouyu->broadcastSkillInvoke(objectName(), 2);
            foreach(ServerPlayer *player, all_players) {
                room->getThread()->delay(100);
                room->loseHp(player);
            }
        }
        return false;
    }
};

class Guixin : public MasochismSkill
{
public:
    Guixin() : MasochismSkill("guixin")
    {
    }

    virtual QStringList triggerable(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (TriggerSkill::triggerable(player)) {
            QList<ServerPlayer *> players = room->getOtherPlayers(player);
            bool can_invoke = false;
            foreach (ServerPlayer *player, players) {
                if (!player->isAllNude())
                    can_invoke = true;
            }
            if (!can_invoke) return QStringList();

            DamageStruct damage = data.value<DamageStruct>();
            QStringList trigger_list;
            for (int i = 1; i <= damage.damage; i++) {
                trigger_list << objectName();
            }

            return trigger_list;
        }
        return QStringList();
    }

    virtual void onDamaged(ServerPlayer *shencc, const DamageStruct &damage) const
    {
        Room *room = shencc->getRoom();
        if (shencc->askForSkillInvoke(this, QVariant::fromValue(damage))) {
            shencc->broadcastSkillInvoke(objectName());
            QList<ServerPlayer *> players = room->getOtherPlayers(shencc);
            foreach (ServerPlayer *player, players)
                room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, shencc->objectName(), player->objectName());

            QString place = room->askForChoice(shencc, objectName(), "hand+equip+judge", QVariant(), "@guixin-choose");

            foreach (ServerPlayer *player, players) {
                if (player->isAlive() && !player->isAllNude()) {
                    QStringList flags;
                    if (place == "equip")
                        flags << "e" << "h" << "j";
                    else if (place == "judge")
                        flags << "j" << "h" << "e";
                    else
                        flags << "h" << "e" << "j";
                    const Card *to_obtain = NULL;
                    foreach (QString flag, flags) {
                        QList<const Card *> cards = player->getCards(flag);
                        if (cards.isEmpty()) continue;
                        to_obtain = cards.at(qrand() % cards.length());
                        break;
                    }
                    if (to_obtain) {
                        CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, shencc->objectName());
                        room->obtainCard(shencc, to_obtain, reason, false);
                    }
                }
            }
            shencc->turnOver();
        }
    }
};

class Feiying : public DistanceSkill
{
public:
    Feiying() : DistanceSkill("feiying")
    {
    }

    virtual int getCorrect(const Player *, const Player *to) const
    {
        if (to->hasSkill(this))
            return +1;
        else
            return 0;
    }
};

class Kuangbao : public TriggerSkill
{
public:
    Kuangbao() : TriggerSkill("kuangbao")
    {
        events << TurnStart << Damaged << Damage;
        frequency = Compulsory;
    }

    virtual TriggerList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        TriggerList skill_list;
        if (triggerEvent == TurnStart) {
            if (!room->getTag("FirstRound").toBool()) return skill_list;
            QList<ServerPlayer *> lvbus = room->findPlayersBySkillName(objectName());
            foreach (ServerPlayer *lvbu, lvbus)
                skill_list.insert(lvbu, QStringList(objectName()));

        } else if (TriggerSkill::triggerable(player)) {
            skill_list.insert(player, QStringList(objectName()));
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *shenlvbu) const
    {
        if (triggerEvent == TurnStart) {
            room->sendCompulsoryTriggerLog(shenlvbu, objectName());
            shenlvbu->broadcastSkillInvoke(objectName());
            shenlvbu->gainMark("@wrath", 2);

        } else {
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            DamageStruct damage = data.value<DamageStruct>();
            player->gainMark("@wrath", damage.damage);
		}
        return false;
    }
};

class Wumou : public TriggerSkill
{
public:
    Wumou() : TriggerSkill("wumou")
    {
        frequency = Compulsory;
        events << CardUsed;
    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card != NULL && use.card->isNDTrick())
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());

        int num = player->getMark("@wrath");
        if (num >= 1 && room->askForChoice(player, objectName(), "discard+losehp") == "discard") {
            player->loseMark("@wrath");
        } else
            room->loseHp(player);

        return false;
    }
};

class Shenfen : public ZeroCardViewAsSkill
{
public:
    Shenfen() : ZeroCardViewAsSkill("shenfen")
    {
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return player->getMark("@wrath") >= 6 && !player->hasUsed("ShenfenCard");
    }

    virtual const Card *viewAs() const
    {
        return new ShenfenCard;
    }
};

ShenfenCard::ShenfenCard()
{
    target_fixed = true;
}

void ShenfenCard::onUse(Room *room, const CardUseStruct &card_use) const
{
    CardUseStruct use = card_use;
    foreach (ServerPlayer *p, room->getOtherPlayers(use.from))
        use.to << p;
    SkillCard::onUse(room, use);
}

void ShenfenCard::extraCost(Room *, const CardUseStruct &card_use) const
{
	card_use.from->loseMark("@wrath", 6);
}

void ShenfenCard::use(Room *room, ServerPlayer *shenlvbu, QList<ServerPlayer *> &) const
{
    shenlvbu->setFlags("ShenfenUsing");

    try {
        QList<ServerPlayer *> players = room->getOtherPlayers(shenlvbu);
        foreach (ServerPlayer *player, players) {
            room->damage(DamageStruct("shenfen", shenlvbu, player));
            room->getThread()->delay(150);
        }

        foreach (ServerPlayer *player, players) {
            QList<const Card *> equips = player->getEquips();
            player->throwAllEquips();
            if (!equips.isEmpty())
                room->getThread()->delay(150);
        }

        foreach (ServerPlayer *player, players) {
            bool delay = !player->isKongcheng();
            room->askForDiscard(player, "shenfen", 4, 4);
            if (delay)
                room->getThread()->delay(150);
        }

        shenlvbu->turnOver();
        shenlvbu->setFlags("-ShenfenUsing");
    }
    catch (TriggerEvent triggerEvent) {
        if (triggerEvent == TurnBroken || triggerEvent == StageChange)
            shenlvbu->setFlags("-ShenfenUsing");
        throw triggerEvent;
    }
}

WuqianCard::WuqianCard()
{
}

bool WuqianCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select != Self;
}

void WuqianCard::extraCost(Room *, const CardUseStruct &card_use) const
{
	card_use.from->loseMark("@wrath", 2);
}

void WuqianCard::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.to->getRoom();

    room->acquireSkill(effect.from, "wushuang", true, true);
    effect.from->setFlags("WuqianSource");
    effect.to->setFlags("WuqianTarget");
    room->addPlayerMark(effect.to, "Armor_Nullified");
}

class WuqianViewAsSkill : public ZeroCardViewAsSkill
{
public:
    WuqianViewAsSkill() : ZeroCardViewAsSkill("wuqian")
    {
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return player->getMark("@wrath") >= 2;
    }

    virtual const Card *viewAs() const
    {
        return new WuqianCard;
    }
};

class Wuqian : public TriggerSkill
{
public:
    Wuqian() : TriggerSkill("wuqian")
    {
        events << EventPhaseStart;
        view_as_skill = new WuqianViewAsSkill;
    }

    virtual bool triggerable(const ServerPlayer *) const
    {
        return false;
    }

    virtual void record(TriggerEvent , Room *room, ServerPlayer *player, QVariant &) const
    {
        if (player->getPhase() != Player::NotActive) return;
        foreach (ServerPlayer *p, room->getAllPlayers()) {
            if (p->hasFlag("WuqianTarget")) {
                if (p->getMark("Armor_Nullified") > 0)
                    room->removePlayerMark(p, "Armor_Nullified");
            }
        }
    }
};

QixingCard::QixingCard()
{
    will_throw = false;
    handling_method = Card::MethodNone;
    target_fixed = true;
}

void QixingCard::onUse(Room *room, const CardUseStruct &card_use) const
{
    QList<int> pile = card_use.from->getPile("stars");
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

    card_use.from->broadcastSkillInvoke("qixing");
    room->notifySkillInvoked(card_use.from, "qixing");

    card_use.from->addToPile("stars", to_pile, false);

    DummyCard *to_handcard_x = new DummyCard(to_handcard);
    CardMoveReason reason(CardMoveReason::S_REASON_EXCHANGE_FROM_PILE, card_use.from->objectName());
    room->obtainCard(card_use.from, to_handcard_x, reason, false);
    to_handcard_x->deleteLater();
}

class QixingVS : public ViewAsSkill
{
public:
    QixingVS() : ViewAsSkill("qixing")
    {
        response_pattern = "@@qixing";
        expand_pile = "stars";
    }

    virtual bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
        if (selected.length() < Self->getPile("stars").length())
            return !to_select->isEquipped();

        return false;
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.length() == Self->getPile("stars").length()) {
            QixingCard *c = new QixingCard;
            c->addSubcards(cards);
            return c;
        }

        return NULL;
    }
};


class Qixing : public TriggerSkill
{
public:
    Qixing() : TriggerSkill("qixing")
    {
        events << TurnStart << EventPhaseEnd;
    }

    virtual TriggerList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        TriggerList skill_list;
        if (triggerEvent == TurnStart && room->getTag("FirstRound").toBool()) {
            QList<ServerPlayer *> shenzhuges = room->findPlayersBySkillName(objectName());
            foreach (ServerPlayer *shenzhuge, shenzhuges)
                skill_list.insert(shenzhuge, QStringList("qixing!"));

        } else if (triggerEvent == EventPhaseEnd && TriggerSkill::triggerable(player) && player->getPhase() == Player::Draw){
            if (!player->isKongcheng() && !player->getPile("stars").isEmpty())
                skill_list.insert(player, QStringList(objectName()));
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer *shenzhugeliang) const
    {
        if (triggerEvent == TurnStart) {
            room->sendCompulsoryTriggerLog(shenzhugeliang, objectName());
            shenzhugeliang->broadcastSkillInvoke(objectName());
            shenzhugeliang->addToPile("stars", room->getNCards(7), false, QList<ServerPlayer *>() << shenzhugeliang);

        } else if (triggerEvent == EventPhaseEnd) {


        }
        return false;
    }
};

KuangfengCard::KuangfengCard()
{
}

bool KuangfengCard::targetFilter(const QList<const Player *> &targets, const Player *, const Player *) const
{
    return targets.isEmpty();
}

void KuangfengCard::onEffect(const CardEffectStruct &effect) const
{
    effect.from->tag["Qixing_user"] = true;
    effect.to->gainMark("@gale");
}

class KuangfengViewAsSkill : public OneCardViewAsSkill
{
public:
    KuangfengViewAsSkill() : OneCardViewAsSkill("kuangfeng")
    {
        response_pattern = "@@kuangfeng";
        filter_pattern = ".|.|.|stars";
        expand_pile = "stars";
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        KuangfengCard *kf = new KuangfengCard;
        kf->addSubcard(originalCard);
        return kf;
    }
};

class Kuangfeng : public TriggerSkill
{
public:
    Kuangfeng() : TriggerSkill("kuangfeng")
    {
        events << DamageForseen;
        view_as_skill = new KuangfengViewAsSkill;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->getMark("@gale") > 0;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (damage.nature == DamageStruct::Fire) {
            LogMessage log;
            log.type = "#GalePower";
            log.from = player;
            log.arg = QString::number(damage.damage);
            log.arg2 = QString::number(++damage.damage);
            room->sendLog(log);

            data = QVariant::fromValue(damage);
        }

        return false;
    }
};

DawuCard::DawuCard()
{
}

bool DawuCard::targetFilter(const QList<const Player *> &targets, const Player *, const Player *) const
{
    return targets.length() < subcards.length();
}

bool DawuCard::targetsFeasible(const QList<const Player *> &targets, const Player *) const
{
    return targets.length() == subcards.length();
}

void DawuCard::onEffect(const CardEffectStruct &effect) const
{
    effect.from->tag["Qixing_user"] = true;
    effect.to->gainMark("@fog");
}

class DawuViewAsSkill : public ViewAsSkill
{
public:
    DawuViewAsSkill() : ViewAsSkill("dawu")
    {
        response_pattern = "@@dawu";
        expand_pile = "stars";
    }

    virtual bool viewFilter(const QList<const Card *> &, const Card *to_select) const
    {
        return Self->getPile("stars").contains(to_select->getId());
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (!cards.isEmpty()) {
            DawuCard *dw = new DawuCard;
            dw->addSubcards(cards);
            return dw;
        }

        return NULL;
    }
};

class Dawu : public TriggerSkill
{
public:
    Dawu() : TriggerSkill("dawu")
    {
        events << DamageForseen;
        view_as_skill = new DawuViewAsSkill;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->getMark("@fog") > 0;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (damage.nature != DamageStruct::Thunder) {
            LogMessage log;
            log.type = "#FogProtect";
            log.from = player;
            log.arg = QString::number(damage.damage);
            if (damage.nature == DamageStruct::Normal)
                log.arg2 = "normal_nature";
            else if (damage.nature == DamageStruct::Fire)
                log.arg2 = "fire_nature";
            room->sendLog(log);

            return true;
        } else
            return false;
    }
};

class Renjie : public TriggerSkill
{
public:
    Renjie() : TriggerSkill("renjie")
    {
        events << Damaged << CardsMoveOneTime;
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        if (triggerEvent == CardsMoveOneTime) {
            if (player->getPhase() == Player::Discard) {
                QVariantList move_datas = data.toList();
                foreach(QVariant move_data, move_datas) {
                    CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
                    if (move.from == player && move.reason.m_playerId == player->objectName() && move.from_places.contains(Player::PlaceHand)
                            && move.reason.m_reason == CardMoveReason::S_REASON_RULEDISCARD) {
                        return QStringList(objectName());
                    }
                }
            }
            return QStringList();
        }
        return QStringList(objectName());
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());
        int n = 0;
        if (triggerEvent == CardsMoveOneTime) {
            QVariantList move_datas = data.toList();
            foreach(QVariant move_data, move_datas) {
                CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
                if (move.from == player && move.reason.m_playerId == player->objectName() &&
                        move.reason.m_reason == CardMoveReason::S_REASON_RULEDISCARD) {
                    n = n + move.from_places.count(Player::PlaceHand);
                }
            }
        } else if (triggerEvent == Damaged)
            n = data.value<DamageStruct>().damage;
        player->gainMark("@bear", n);
        return false;
    }
};

class Baiyin : public PhaseChangeSkill
{
public:
    Baiyin() : PhaseChangeSkill("baiyin")
    {
        frequency = Wake;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && PhaseChangeSkill::triggerable(target)
            && target->getPhase() == Player::Start
            && target->getMark("baiyin") == 0
            && target->getMark("@bear") >= 4;
    }

    virtual bool onPhaseChange(ServerPlayer *shensimayi) const
    {
        Room *room = shensimayi->getRoom();
        shensimayi->broadcastSkillInvoke(objectName());
        room->sendCompulsoryTriggerLog(shensimayi, objectName());

        room->setPlayerMark(shensimayi, "baiyin", 1);
        if (room->changeMaxHpForAwakenSkill(shensimayi) && shensimayi->getMark("baiyin") == 1)
            room->acquireSkill(shensimayi, "jilve");

        return false;
    }
};

JilveCard::JilveCard()
{
    target_fixed = true;
}

void JilveCard::onUse(Room *room, const CardUseStruct &card_use) const
{
    ServerPlayer *shensimayi = card_use.from;
	LogMessage log;
    log.from = shensimayi;
    log.type = "#InvokeSkill";
    log.arg = "jilve";
    room->sendLog(log);
	room->notifySkillInvoked(shensimayi, "jilve");

	shensimayi->loseMark("@bear");
	if (subcards.length() == 0){
		room->setPlayerFlag(shensimayi, "JilveWansha");
        shensimayi->broadcastSkillInvoke("wansha");
        room->acquireSkill(shensimayi, "wansha", true, true);
	}else{
		room->addPlayerHistory(shensimayi, "ZhihengCard", 1);
		room->setPlayerFlag(shensimayi, "JilveZhiheng");
		LogMessage log;
        log.from = shensimayi;
        log.type = "#UseCard";
        log.card_str = QString("%1[%2:%3]=%4").arg("@ZhihengCard").arg(getSuitString()).arg(getNumberString()).arg(subcardString());
        room->sendLog(log);
        shensimayi->broadcastSkillInvoke("zhiheng");

        bool all_handcards = true;
        foreach (const Card *c, shensimayi->getHandcards()) {
            if (!usecontains(c)) {
                all_handcards = false;
                break;
            }
        }

		CardMoveReason reason(CardMoveReason::S_REASON_THROW, shensimayi->objectName(), QString(), "zhiheng", QString());
        room->moveCardTo(this, shensimayi, NULL, Player::DiscardPile, reason, true);
		ZhihengCard *zhiheng_card = new ZhihengCard;
        zhiheng_card->addSubcards(subcards);
        if (all_handcards)
            zhiheng_card->setFlags("ZhihengAllHandcards");
        QList<ServerPlayer *> targets;
        zhiheng_card->use(room, shensimayi, targets);
        delete zhiheng_card;
		room->setPlayerFlag(shensimayi, "-JilveZhiheng");
	}
}

class JilveViewAsSkill : public ViewAsSkill
{
public: // wansha & zhiheng
    JilveViewAsSkill() : ViewAsSkill("jilve")
    {
    }

    virtual bool viewFilter(const QList<const Card *> &, const Card *to_select) const
    {
        QString choice = Self->tag["jilve"].toString();
        if (choice == "zhiheng")
            return !Self->isJilei(to_select);
		return false;
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const
    {
        QString choice = Self->tag["jilve"].toString();
        if (choice == "wansha")
            return new JilveCard;
        else if (choice == "zhiheng"){
			if (cards.isEmpty())
                return NULL;

            JilveCard *zhiheng_card = new JilveCard;
            zhiheng_card->addSubcards(cards);
            zhiheng_card->setSkillName(objectName());
            return zhiheng_card;
		} else
			return NULL;
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !(player->hasFlag("JilveWansha") & player->hasUsed("ZhihengCard")) && player->getMark("@bear") > 0;
    }

};

class Jilve : public TriggerSkill
{
public:
    Jilve() : TriggerSkill("jilve")
    {
        events << CardUsed // JiZhi
            << AskForRetrial // GuiCai
            << Damaged // FangZhu
            << EventPhaseStart; //record
        view_as_skill = new JilveViewAsSkill;
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

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (!TriggerSkill::triggerable(player) || player->getMark("@bear") == 0 || triggerEvent == EventPhaseStart) return QStringList();
        if (triggerEvent == CardUsed) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (!use.card->isNDTrick()) return QStringList();
        } else if (triggerEvent == AskForRetrial) {
            if (player->isNude()) return QStringList();
        }
        return QStringList(objectName());
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        if (triggerEvent == CardUsed) {
            if (room->askForSkillInvoke(player, "jilve_jizhi", data)) {
                player->broadcastSkillInvoke("jizhi");
                player->loseMark("@bear");
                bool from_up = true;
                if (player->hasSkill("cunmu")) {
                    room->sendCompulsoryTriggerLog(player, "cunmu");
                    player->broadcastSkillInvoke("cunmu");
                    from_up = false;
                }
                int id = room->drawCard(from_up);
                const Card *card = Sanguosha->getCard(id);
                CardMoveReason reason(CardMoveReason::S_REASON_DRAW, player->objectName(), objectName(), QString());
                room->obtainCard(player, card, reason, false);
                if (card->getTypeId() == Card::TypeBasic && player->handCards().contains(id)
                        && room->askForChoice(player, objectName(),"yes+no", QVariant::fromValue(card), "@jizhi-discard:::"+card->objectName()) == "yes") {
                    room->throwCard(card, player);
                    room->addPlayerMark(player, "#jizhi");
                    room->addPlayerMark(player, "Global_MaxcardsIncrease");
                }
            }
        } else if (triggerEvent == AskForRetrial) {
            JudgeStruct *judge = data.value<JudgeStruct *>();
            QStringList prompt_list;
            prompt_list << "@guicai-card" << judge->who->objectName()
                << "jilve_guicai" << judge->reason << QString::number(judge->card->getEffectiveId());
            QString prompt = prompt_list.join(":");
            const Card *card = room->askForCard(player, "..", prompt, data, Card::MethodResponse, judge->who, true);
            if (card) {
                player->broadcastSkillInvoke("guicai");
                player->loseMark("@bear");
                if (judge->who)
                    room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), judge->who->objectName());
                room->retrial(card, player, judge, "guicai");
            }
        } else if (triggerEvent == Damaged) {
            ServerPlayer *to = room->askForPlayerChosen(player, room->getOtherPlayers(player), "fangzhu",
                "@jilve_fangzhu-invoke:::" + QString::number(player->getLostHp()), true, true);
            if (to) {
                player->broadcastSkillInvoke("fangzhu");
                player->loseMark("@bear");
                to->turnOver();
                if (player->isAlive() && player->getLostHp() > 0)
                    to->drawCards(player->getLostHp(), "fangzhu");
            }
        }
        return false;
    }

    QString getSelectBox() const
    {
        return "zhiheng+wansha";
    }

    bool buttonEnabled(const QString &button_name, const QList<const Card *> &, const QList<const Player *> &) const
    {
        if (button_name.isEmpty())
            return true;

        return ((button_name == "wansha" && !Self->hasFlag("JilveWansha")) ||
            (button_name == "zhiheng" && !Self->hasUsed("ZhihengCard")));
    }
};

class Lianpo : public TriggerSkill
{
public:
    Lianpo() : TriggerSkill("lianpo")
    {
        events << EventPhaseChanging;
        frequency = Frequent;
    }

    virtual TriggerList triggerable(TriggerEvent , Room *room, ServerPlayer *, QVariant &data) const
    {
        TriggerList skill_list;

        if (data.value<PhaseChangeStruct>().to == Player::NotActive) {
            foreach (ServerPlayer *p, room->getAllPlayers()) {
                if (p->getMark("GlobalKilledCount") > 0 && TriggerSkill::triggerable(p)) {
                    skill_list.insert(p, QStringList(objectName()));
                }
            }
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent, Room *, ServerPlayer *, QVariant &, ServerPlayer *shensimayi) const
    {
        if (shensimayi->askForSkillInvoke(objectName())) {
            shensimayi->broadcastSkillInvoke(objectName());
            shensimayi->gainAnExtraTurn();
        }
        return false;
    }
};

class Juejing : public TriggerSkill
{
public:
    Juejing() : TriggerSkill("juejing")
    {
        events << Dying << QuitDying;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *shenzhaoyun, QVariant &data, ServerPlayer* &) const
    {
        if (TriggerSkill::triggerable(shenzhaoyun)) {
            if (triggerEvent == Dying) {
                DyingStruct dying = data.value<DyingStruct>();
                if (dying.who != shenzhaoyun) return QStringList();
            }
            return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *shenzhaoyun, QVariant &, ServerPlayer *) const
    {
        room->sendCompulsoryTriggerLog(shenzhaoyun, objectName());
        shenzhaoyun->broadcastSkillInvoke(objectName());
        shenzhaoyun->drawCards(1, objectName());
    }
};

class JuejingKeep : public MaxCardsSkill
{
public:
    JuejingKeep() : MaxCardsSkill("#juejing-keep")
    {
    }

    virtual int getExtra(const Player *target) const
    {
        if (target->hasSkill(this))
            return 2;
        else
            return 0;
    }
};

Longhun::Longhun() : ViewAsSkill("longhun")
{
    response_or_use = true;
}

bool Longhun::isEnabledAtResponse(const Player *, const QString &pattern) const
{
    return pattern == "slash" || pattern == "jink" || pattern.contains("peach") || pattern == "nullification";
}

bool Longhun::isEnabledAtPlay(const Player *player) const
{
    return player->isWounded() || Slash::IsAvailable(player);
}

bool Longhun::viewFilter(const QList<const Card *> &selected, const Card *card) const
{
    int n = qMax(1, Self->getHp());

    if (selected.length() >= n || card->hasFlag("using"))
        return false;

    if (n > 1 && !selected.isEmpty()) {
        Card::Suit suit = selected.first()->getSuit();
        return card->getSuit() == suit;
    }

    switch (Sanguosha->currentRoomState()->getCurrentCardUseReason()) {
    case CardUseStruct::CARD_USE_REASON_PLAY: {
        if (Self->isWounded() && card->getSuit() == Card::Heart)
            return true;
        else if (card->getSuit() == Card::Diamond) {
            FireSlash *slash = new FireSlash(Card::SuitToBeDecided, -1);
            slash->addSubcards(selected);
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

const Card *Longhun::viewAs(const QList<const Card *> &cards) const
{
    int n = getEffHp(Self);

    if (cards.length() != n)
        return NULL;

    const Card *card = cards.first();
    Card *new_card = NULL;

    switch (card->getSuit()) {
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
        new_card->addSubcards(cards);
    }

    return new_card;
}

bool Longhun::isEnabledAtNullification(const ServerPlayer *player) const
{
    return !player->isNude() || !player->getHandPile().isEmpty();
}

int Longhun::getEffHp(const Player *zhaoyun) const
{
    return qMax(1, zhaoyun->getHp());
}

class Longnu : public TriggerSkill
{
public:
    Longnu() : TriggerSkill("longnu")
    {
        events << EventPhaseStart << EventPhaseChanging;
        frequency = Compulsory;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *shenliubei, QVariant &data) const
    {
        if (triggerEvent == EventPhaseChanging && data.value<PhaseChangeStruct>().from == Player::Play) {
            room->setPlayerFlag(shenliubei, "-LongnuFire");
            room->setPlayerFlag(shenliubei, "-LongnuThunder");
            room->detachSkillFromPlayer(shenliubei, "#longnu-fire");
            room->detachSkillFromPlayer(shenliubei, "#longnu-thunder");
            room->filterCards(shenliubei, shenliubei->getCards("he"), true);
        }
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &, ServerPlayer * &) const
    {
        if (triggerEvent == EventPhaseStart && player->getPhase() == Player::Play && TriggerSkill::triggerable(player))
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *shenliubei, QVariant &, ServerPlayer *) const
    {
        room->sendCompulsoryTriggerLog(shenliubei, objectName());
        shenliubei->broadcastSkillInvoke(objectName());


        bool state1 = shenliubei->getMark("longnuTransformed")%2==0;

        room->addPlayerMark(shenliubei, "longnuTransformed");

        if (state1) {
            room->loseHp(shenliubei);
            shenliubei->drawCards(1, objectName());
            room->setPlayerFlag(shenliubei, "LongnuFire");
            room->acquireSkill(shenliubei, "#longnu-fire", false);
        } else {
            room->loseMaxHp(shenliubei);
            shenliubei->drawCards(1, objectName());
            room->setPlayerFlag(shenliubei, "LongnuThunder");
            room->acquireSkill(shenliubei, "#longnu-thunder", false);
        }
        room->filterCards(shenliubei, shenliubei->getCards("he"), false);
        return false;
    }
};

class LongnuTarget : public TargetModSkill
{
public:
    LongnuTarget() : TargetModSkill("#longnu-target")
    {
    }

    int getDistanceLimit(const Player *from, const Card *card, const Player *to) const
    {
        if (from->hasFlag("LongnuFire") && card->isKindOf("FireSlash"))
            return 1000;
        else
            return 0;
    }

    int getResidueNum(const Player *from, const Card *card, const Player *) const
    {
        if (from->hasFlag("LongnuThunder") && card->isKindOf("ThunderSlash"))
            return 1000;
        else
            return 0;
    }
};

class LongnuFire : public FilterSkill
{
public:
    LongnuFire() : FilterSkill("#longnu-fire")
    {

    }

    bool viewFilter(const Card *to_select) const
    {
        return to_select->isRed() && Sanguosha->currentRoom()->getCardPlace(to_select->getEffectiveId()) == Player::PlaceHand;
    }

    const Card *viewAs(const Card *originalCard) const
    {
        FireSlash *slash = new FireSlash(originalCard->getSuit(), originalCard->getNumber());
        slash->setSkillName("longnu");
        WrappedCard *card = Sanguosha->getWrappedCard(originalCard->getId());
        card->takeOver(slash);
        return card;
    }
};

class LongnuThunder : public FilterSkill
{
public:
    LongnuThunder() : FilterSkill("#longnu-thunder")
    {

    }

    bool viewFilter(const Card *to_select) const
    {
        return to_select->getTypeId() == Card::TypeTrick && Sanguosha->currentRoom()->getCardPlace(to_select->getEffectiveId()) == Player::PlaceHand;
    }

    const Card *viewAs(const Card *originalCard) const
    {
        ThunderSlash *slash = new ThunderSlash(originalCard->getSuit(), originalCard->getNumber());
        slash->setSkillName("longnu");
        WrappedCard *card = Sanguosha->getWrappedCard(originalCard->getId());
        card->takeOver(slash);
        return card;
    }
};

class Jieying : public TriggerSkill
{
public:
    Jieying() : TriggerSkill("jieying")
    {
        events << TurnStart << EventPhaseStart;
        frequency = Compulsory;
    }

    virtual TriggerList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        TriggerList skill_list;
        if (triggerEvent == TurnStart) {
            if (!room->getTag("FirstRound").toBool()) return skill_list;
            QList<ServerPlayer *> liubeis = room->findPlayersBySkillName(objectName());
            foreach (ServerPlayer *liubei, liubeis) {
                if (!liubei->isChained())
                    skill_list.insert(liubei, QStringList(objectName()));
            }

        } else if (TriggerSkill::triggerable(player) && player->getPhase() == Player::Finish) {
            QList<ServerPlayer *> players = room->getAlivePlayers();
            foreach (ServerPlayer *p, players) {
                if (!p->isChained()) {
                    skill_list.insert(player, QStringList(objectName()));
                    break;
                }
            }
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer *shenliubei) const
    {
        if (triggerEvent == TurnStart) {
            room->sendCompulsoryTriggerLog(shenliubei, objectName());
            shenliubei->broadcastSkillInvoke(objectName());
            if (!shenliubei->isChained())
                room->setPlayerProperty(shenliubei, "chained", true);
        } else {
            QList<ServerPlayer *> can_select;
            foreach (ServerPlayer *p, room->getOtherPlayers(shenliubei)) {
                if (!p->isChained())
                    can_select << p;
            }
            if (!can_select.isEmpty()) {
                room->sendCompulsoryTriggerLog(player, objectName());
                player->broadcastSkillInvoke(objectName());
                ServerPlayer *target = room->askForPlayerChosen(player, can_select, objectName(), "@jieying-target");
                room->setPlayerProperty(target, "chained", true);
            }
        }
        return false;
    }
};

class JieyingMaxCards : public MaxCardsSkill
{
public:
    JieyingMaxCards() : MaxCardsSkill("#jieying-maxcards")
    {
    }

    virtual int getExtra(const Player *target) const
    {
        if (target->isChained()) {
            int extra = 0;
            QList<const Player *> players = target->getAliveSiblings();
            players << target;
            foreach (const Player *player, players) {
                if (player->hasSkill("jieying"))
                    extra += 2;
            }
            return extra;
        } else
            return 0;
    }
};

class Junlve : public TriggerSkill
{
public:
    Junlve() : TriggerSkill("junlve")
    {
        events << Damaged << Damage;
        frequency = Compulsory;
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());
        DamageStruct damage = data.value<DamageStruct>();
        player->gainMark("@strategy", damage.damage);
        return false;
    }
};

class Cuike : public PhaseChangeSkill
{
public:
    Cuike() : PhaseChangeSkill("cuike")
    {

    }

    virtual QStringList triggerable(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer* &) const
    {
        if (!PhaseChangeSkill::triggerable(player) || player->getPhase() != Player::Play) return QStringList();
        if (player->getMark("@strategy")%2 == 0 && player->getMark("@strategy") < 8) {
            QList<ServerPlayer *> players = room->getAlivePlayers();
            foreach (ServerPlayer *p, players) {
                if (player->canDiscard(p, "hej"))
                    return QStringList("cuike!");
            }
            return QStringList();
        }
        return QStringList("cuike!");
    }

    virtual bool onPhaseChange(ServerPlayer *player) const
    {
        Room *room = player->getRoom();
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());
        if (player->getMark("@strategy")%2 == 0)  {
            QList<ServerPlayer *> can_select;
            foreach (ServerPlayer *p, room->getAlivePlayers()) {
                if (player->canDiscard(p, "hej") || !p->isChained())
                    can_select << p;
            }
            if (!can_select.isEmpty()) {
                ServerPlayer *target = room->askForPlayerChosen(player, can_select, objectName(), "@cuike-target", true);
                if (target) {
                    if (player->canDiscard(target, "hej")) {
                        int card_id = room->askForCardChosen(player, target, "hej", objectName(), false, Card::MethodDiscard);
                        room->throwCard(card_id, room->getCardPlace(card_id) == Player::PlaceDelayedTrick ? NULL : target, player);
                    }
                    if (!target->isChained())
                        room->setPlayerProperty(target, "chained", true);
                }
            }
        } else {
            ServerPlayer *to_damage = room->askForPlayerChosen(player, room->getAlivePlayers(), objectName(), "@cuike-damage", true);
            if (to_damage)
                room->damage(DamageStruct(objectName(), player, to_damage));

        }
        if (player->getMark("@strategy") > 7 && room->askForChoice(player, objectName(), "yes+no", QVariant(), "@cuike-dismark") == "yes") {
            player->loseAllMarks("@strategy");
            QList<ServerPlayer *> targets = room->getOtherPlayers(player);

            foreach (ServerPlayer *p, targets) {
                if (p->isAlive()) {
                    room->damage(DamageStruct(objectName(), player, p));
                    room->getThread()->delay(150);
                }
            }

        }
        return false;
    }
};

ZhanhuoCard::ZhanhuoCard()
{
}

bool ZhanhuoCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.length() < Self->getMark("@strategy") && to_select->isChained();
}

void ZhanhuoCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    room->removePlayerMark(source, "@blaze");
    source->loseAllMarks("@strategy");
    foreach (ServerPlayer *p, targets) {
        if (p->isAlive())
            p->throwAllEquips();
    }
    QList<ServerPlayer *> can_select;
    foreach (ServerPlayer *p, targets) {
        if (p->isAlive())
            can_select << p;
    }
    if (!can_select.isEmpty()) {
        ServerPlayer *to_damage = room->askForPlayerChosen(source, can_select, "zhanhuo", "@zhanhuo-target");
        room->damage(DamageStruct("zhanhuo", source, to_damage, 1, DamageStruct::Fire));
    }
}

class Zhanhuo : public ZeroCardViewAsSkill
{
public:
    Zhanhuo() : ZeroCardViewAsSkill("zhanhuo")
    {
        frequency = Limited;
        limit_mark = "@blaze";
    }

    virtual const Card *viewAs() const
    {
        return new ZhanhuoCard;
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return player->getMark("@blaze") > 0 && player->getMark("@strategy") > 0;
    }

};

class Duorui : public TriggerSkill
{
public:
    Duorui() : TriggerSkill("duorui")
    {
        events << Damage << EventPhaseStart << BuryVictim;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *target, QVariant &) const
    {
        if (triggerEvent == EventPhaseStart) {
            if (target->getPhase() != Player::NotActive) return;
        } else if (triggerEvent != BuryVictim) return;

        QStringList source_list = target->property("duorui_sources").toString().split("+");
        QStringList skill_list = target->property("duorui_skills").toString().split("+");

        QList<ServerPlayer *> all_players = room->getAlivePlayers();
        foreach (ServerPlayer *player, all_players) {
            if (source_list.contains(player->objectName())) {
                room->setPlayerMark(player, "DuoruiInvoked", 0);
                room->detachSkillFromPlayer(player, skill_list.at(source_list.indexOf(player->objectName())));
            }
        }
        room->setPlayerProperty(target, "duorui_sources", QString());
        room->setPlayerProperty(target, "duorui_skills", QString());
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (triggerEvent != Damage || !TriggerSkill::triggerable(player)) return QStringList();
        if (player->getPhase() != Player::Play || player->getMark("DuoruiInvoked") > 0) return QStringList();
        DamageStruct damage = data.value<DamageStruct>();
        ServerPlayer *target = damage.to;
        if (target && target->isAlive() && target != player && !target->hasFlag("Global_DebutFlag")) {
            if (player->getMark("WeaponSealed") == 0 || player->getMark("ArmorSealed") == 0 || player->getMark("TreasureSealed") == 0
                    || player->getMark("DefensiveHorseSealed") == 0 || player->getMark("OffensiveHorseSealed") == 0)
                return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        ServerPlayer *target = damage.to;
        if (room->askForSkillInvoke(player, objectName(), QVariant::fromValue(target))) {
            player->broadcastSkillInvoke(objectName());
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), target->objectName());
            QStringList area_names, choice_names, choices;
            area_names << "WeaponSealed" << "ArmorSealed" << "DefensiveHorseSealed" << "OffensiveHorseSealed" << "TreasureSealed";
            choice_names << "weapon" << "armor" << "defensive_horse" << "offensive_horse" << "treasure";
            for (int i = 0; i < S_EQUIP_AREA_LENGTH; i++) {
                if (player->getMark(area_names.at(i)) == 0)
                    choices << choice_names.at(i);
            }
            if (choices.isEmpty()) return false;
            QString choice = room->askForChoice(player, objectName(), choices.join("+"), data, "@duorui-area", choice_names.join("+"));
            QString area_name = area_names.at(choice_names.indexOf(choice));
            area_name.chop(6);
            player->sealAreas(area_name);
            QList<const Skill *> skills = target->getGeneral()->getSkillList();
            if (target->getGeneral2())
                skills.append(target->getGeneral2()->getSkillList());

            QStringList skill_list;
            foreach (const Skill *skill, skills) {
                if (skill->isVisible() && !skill->isAttachedLordSkill() && !skill->isLordSkill()
                        && skill->getFrequency() != Skill::Limited && skill->getFrequency() != Skill::Wake)
                    skill_list.append(skill->objectName());
            }
            if (!skill_list.isEmpty()) {
                QString skill_name = room->askForChoice(player, objectName(), skill_list.join("+"), data, "@duorui-skill::"+target->objectName());

                QStringList source_list = target->property("duorui_sources").toString().split("+");
                source_list << player->objectName();
                room->setPlayerProperty(target, "duorui_sources", source_list.join("+"));

                QStringList skill_list = target->property("duorui_skills").toString().split("+");
                skill_list << skill_name;
                room->setPlayerProperty(target, "duorui_skills", skill_list.join("+"));

                room->addPlayerMark(player, "DuoruiInvoked");
                room->acquireSkill(player, skill_name);
            }
        }
        return false;
    }
};


class DuoruiInvalidity : public InvaliditySkill
{
public:
    DuoruiInvalidity() : InvaliditySkill("#duorui-invalidity")
    {
    }

    virtual bool isSkillValid(const Player *target, const Skill *skill) const
    {
        QStringList duorui_list = target->property("duorui_skills").toString().split("+");
        return !duorui_list.contains(skill->objectName());
    }
};

class Zhiti : public TriggerSkill
{
public:
    Zhiti() : TriggerSkill("zhiti")
    {
        events << Damage << Damaged << Pindian;
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        ServerPlayer *target = NULL;
        if (triggerEvent == Damage) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.card && damage.card->isKindOf("Duel"))
                target = damage.to;
        } else if (triggerEvent == Damaged) {
            DamageStruct damage = data.value<DamageStruct>();
            target = damage.from;
        } else if (triggerEvent == Pindian) {
            PindianStruct *pindian = data.value<PindianStruct *>();
            if (pindian->from == player) {
                target = pindian->to;
            } else if (pindian->to == player) {
                target = pindian->from;
            }
        }

        if (target && target->isAlive() && player->inMyAttackRange(target) && target->isWounded()) {
            if (player->getMark("WeaponSealed") > 0 || player->getMark("ArmorSealed") > 0 || player->getMark("TreasureSealed") > 0
                    || player->getMark("DefensiveHorseSealed") > 0 || player->getMark("OffensiveHorseSealed") > 0)
                return QStringList(objectName());
        }

        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());

        QStringList area_names, choice_names, choices;
        area_names << "WeaponSealed" << "ArmorSealed" << "DefensiveHorseSealed" << "OffensiveHorseSealed" << "TreasureSealed";
        choice_names << "weapon" << "armor" << "defensive_horse" << "offensive_horse" << "treasure";
        for (int i = 0; i < S_EQUIP_AREA_LENGTH; i++) {
            if (player->getMark(area_names.at(i)) > 0)
                choices << choice_names.at(i);
        }
        if (choices.isEmpty()) return false;
        QString choice = room->askForChoice(player, objectName(), choices.join("+"), QVariant(), "@zhiti-area", choice_names.join("+"));
        QString area_name = area_names.at(choice_names.indexOf(choice));

        room->setPlayerMark(player, area_name, 0);

        return false;
    }
};


class ZhitiMaxCards : public MaxCardsSkill
{
public:
    ZhitiMaxCards() : MaxCardsSkill("#zhiti-maxcards")
    {
    }

    virtual int getExtra(const Player *target) const
    {
        if (target->isWounded()) {
            int decrese = 0;
            QList<const Player *> players = target->getAliveSiblings();
            foreach (const Player *player, players) {
                if (player->hasSkill("zhiti") && player->inMyAttackRange(target))
                    decrese += 1;
            }
            return -decrese;
        } else
            return 0;
    }
};

class PoxiChoose : public ViewAsSkill
{
public:
    PoxiChoose() : ViewAsSkill("poxi_choose")
    {
        response_pattern = "@@poxi_choose";
        expand_pile = "#poxi";
    }

    bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
        foreach (const Card *item, selected) {
            if (to_select->getSuit() == item->getSuit())
                return false;
        }
        return Self->getPile(expand_pile).contains(to_select->getEffectiveId()) || Self->getHandcards().contains(to_select);
    }

    const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.length() != 4) return NULL;
        DummyCard *dummy = new DummyCard;
        dummy->addSubcards(cards);
        return dummy;
    }
};

PoxiCard::PoxiCard()
{
}

bool PoxiCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select != Self && !to_select->isKongcheng();
}

void PoxiCard::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *source = effect.from;
    ServerPlayer *target = effect.to;
    Room *room = source->getRoom();

    QList<int> handcards = target->handCards();
    room->notifyMoveToPile(source, handcards, "poxi", Player::PlaceTable, true, true);
    const Card *card = room->askForCard(source, "@@poxi_choose", "@poxi-discard::" + target->objectName(), QVariant(), Card::MethodNone);
    room->notifyMoveToPile(source, handcards, "poxi", Player::PlaceTable, false, false);

    if (card != NULL && card->subcardsLength() > 0) {
        QList<int> cards1, cards2;
        foreach (int id, card->getSubcards()) {
            if (room->getCardOwner(id) == source)
                cards1 << id;
            else
                cards2 << id;
        }

        QList<CardsMoveStruct> moves;

        if (!cards1.isEmpty()) {
            LogMessage log;
            log.type = "$DiscardCard";
            log.from = source;
            log.card_str = IntList2StringList(cards1).join("+");
            room->sendLog(log);
            CardsMoveStruct move(cards1, NULL, Player::DiscardPile,
                CardMoveReason(CardMoveReason::S_REASON_THROW, source->objectName(), "poxi", QString()));
            moves.append(move);
        }
        if (!cards2.isEmpty()) {
            LogMessage log;
            log.type = "$DiscardCard";
            log.from = target;
            log.card_str = IntList2StringList(cards2).join("+");
            room->sendLog(log);
            CardsMoveStruct move(cards2, NULL, Player::DiscardPile,
                CardMoveReason(CardMoveReason::S_REASON_THROW, target->objectName(), "poxi", QString()));
            moves.append(move);
        }
        if (!moves.isEmpty())
            room->moveCardsAtomic(moves, true);

        switch (cards1.length()) {
        case 0:
            room->loseMaxHp(source);
            break;
        case 1:
            room->setPlayerFlag(source, "Global_PlayPhaseTerminated");
            if (source->getMaxCards() > 0)
                room->addPlayerMark(source, "Global_MaxcardsDecrease");
            break;
        case 3:
            if (source->isWounded())
                room->recover(source, RecoverStruct(source));
            break;
        case 4:
            source->drawCards(4, "poxi");
            break;
        default:
            break;
        }
    }
}

class Poxi : public ZeroCardViewAsSkill
{
public:
    Poxi() : ZeroCardViewAsSkill("poxi")
    {

    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("PoxiCard");
    }

    virtual const Card *viewAs() const
    {
        return new PoxiCard;
    }
};



class Robying : public TriggerSkill
{
public:
    Robying() : TriggerSkill("robying")
    {
        events << EventPhaseStart << DrawNCards;

    }

    virtual TriggerList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        TriggerList skill_list;
        if (triggerEvent == EventPhaseStart) {
            if (player->getPhase() == Player::RoundStart && TriggerSkill::triggerable(player)) {
                foreach (ServerPlayer *p, room->getAlivePlayers()) {
                    if (p->getMark("@camp") > 0) return skill_list;
                }
                skill_list.insert(player, QStringList("robying!"));
            } else if (player->getPhase() == Player::Finish && player->getMark("@camp") > 0) {
                QList<ServerPlayer *> shengannings = room->findPlayersBySkillName(objectName());
                foreach (ServerPlayer *shenganning, shengannings) {
                    if (shenganning == player)
                        skill_list.insert(player, QStringList(objectName()));
                    else
                        skill_list.insert(shenganning, QStringList("robying!"));
                }
            }

        } else if (triggerEvent == DrawNCards && player->getMark("@camp") > 0) {
            QList<ServerPlayer *> shengannings = room->findPlayersBySkillName(objectName());
            foreach (ServerPlayer *shenganning, shengannings)
                skill_list.insert(shenganning, QStringList("robying!"));
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *shenganning) const
    {
        if (triggerEvent == EventPhaseStart) {
            if (player->getPhase() == Player::RoundStart) {
                room->sendCompulsoryTriggerLog(shenganning, objectName());
                shenganning->broadcastSkillInvoke(objectName());
                shenganning->gainMark("@camp");
            } else if (player->getPhase() == Player::Finish) {
                if (shenganning == player) {
                    ServerPlayer *target = room->askForPlayerChosen(shenganning, room->getOtherPlayers(shenganning), objectName(), "@robying-target", true, true);
                    if (target) {
                        shenganning->broadcastSkillInvoke(objectName());
                        shenganning->loseMark("@camp");
                        target->gainMark("@camp");
                    }
                } else {
                    room->sendCompulsoryTriggerLog(shenganning, objectName());
                    shenganning->broadcastSkillInvoke(objectName());
                    player->loseMark("@camp");
                    if (!player->isKongcheng()) {
                        DummyCard *dummy_card = new DummyCard(player->handCards());
                        dummy_card->deleteLater();
                        CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, shenganning->objectName());
                        room->obtainCard(shenganning, dummy_card, reason, false);
                    }
                }
            }
        } else if (triggerEvent == DrawNCards) {
            room->sendCompulsoryTriggerLog(shenganning, objectName());
            shenganning->broadcastSkillInvoke(objectName());
            data = data.toInt()+1;
        }
        return false;
    }
};

class RobyingTarget : public TargetModSkill
{
public:
    RobyingTarget() : TargetModSkill("#robying-target")
    {
    }

    virtual int getResidueNum(const Player *from, const Card *, const Player *) const
    {
        if (from->getMark("@camp") > 0) {
            int extra = 0;
            QList<const Player *> players = from->getAliveSiblings();
            players << from;
            foreach (const Player *player, players) {
                if (player->hasSkill("robying"))
                    extra += 1;
            }
            return extra;
        } else
            return 0;
    }
};

class RobyingMaxCards : public MaxCardsSkill
{
public:
    RobyingMaxCards() : MaxCardsSkill("#robying-maxcards")
    {
    }

    virtual int getExtra(const Player *target) const
    {
        if (target->getMark("@camp") > 0) {
            int extra = 0;
            QList<const Player *> players = target->getAliveSiblings();
            players << target;
            foreach (const Player *player, players) {
                if (player->hasSkill("robying"))
                    extra += 1;
            }
            return extra;
        } else
            return 0;
    }
};



GodPackage::GodPackage()
    : Package("god")
{
    General *shenguanyu = new General(this, "shenguanyu", "god", 5); // LE 001
    shenguanyu->addSkill(new Wushen);
    shenguanyu->addSkill(new WushenTargetMod);
    shenguanyu->addSkill(new Wuhun);
    shenguanyu->addSkill(new DetachEffectSkill("wuhun", QString(), "@nightmare"));
    related_skills.insertMulti("wushen", "#wushen-target");
    related_skills.insertMulti("wuhun", "#wuhun-clear");

    General *shenlvmeng = new General(this, "shenlvmeng", "god", 3); // LE 002
    shenlvmeng->addSkill(new Shelie);
    shenlvmeng->addSkill(new Gongxin);

    General *shenzhouyu = new General(this, "shenzhouyu", "god"); // LE 003
    shenzhouyu->addSkill(new Qinyin);
    shenzhouyu->addSkill(new Yeyan);

    General *shenzhugeliang = new General(this, "shenzhugeliang", "god", 3); // LE 004
    shenzhugeliang->addSkill(new Qixing);
    shenzhugeliang->addSkill(new Kuangfeng);
    shenzhugeliang->addSkill(new Dawu);

    General *shencaocao = new General(this, "shencaocao", "god", 3); // LE 005
    shencaocao->addSkill(new Guixin);
    shencaocao->addSkill(new Feiying);

    General *shenlvbu = new General(this, "shenlvbu", "god", 5); // LE 006
    shenlvbu->addSkill(new Kuangbao);
    shenlvbu->addSkill(new DetachEffectSkill("kuangbao", QString(), "@wrath"));
    related_skills.insertMulti("kuangbao", "#kuangbao-clear");
    shenlvbu->addSkill(new Wumou);
    shenlvbu->addSkill(new Wuqian);
    shenlvbu->addRelateSkill("wushuang");
    shenlvbu->addSkill(new Shenfen);

    General *shenzhaoyun = new General(this, "shenzhaoyun", "god", 2); // LE 007
    shenzhaoyun->addSkill(new JuejingKeep);
    shenzhaoyun->addSkill(new Juejing);
    shenzhaoyun->addSkill(new Longhun);
    related_skills.insertMulti("juejing", "#juejing-keep");

    General *shensimayi = new General(this, "shensimayi", "god", 4); // LE 008
    shensimayi->addSkill(new Renjie);
    shensimayi->addSkill(new DetachEffectSkill("renjie", QString(), "@bear"));
    related_skills.insertMulti("renjie", "#renjie-clear");
    shensimayi->addSkill(new Baiyin);
    shensimayi->addRelateSkill("jilve");
    shensimayi->addRelateSkill("guicai");
    shensimayi->addRelateSkill("fangzhu");
    shensimayi->addRelateSkill("jizhi");
    shensimayi->addRelateSkill("zhiheng");
    shensimayi->addRelateSkill("wansha");
    shensimayi->addSkill(new Lianpo);

    General *shenliubei = new General(this, "shenliubei", "god", 6);
    shenliubei->addSkill(new Longnu);
    shenliubei->addSkill(new LongnuTarget);
    shenliubei->addSkill(new Jieying);
    shenliubei->addSkill(new JieyingMaxCards);
    related_skills.insertMulti("longnu", "#longnu-target");
    related_skills.insertMulti("jieying", "#jieying-maxcards");

    General *shenluxun = new General(this, "shenluxun", "god");
    shenluxun->addSkill(new Junlve);
    shenluxun->addSkill(new Cuike);
    shenluxun->addSkill(new Zhanhuo);

    General *shenzhangliao = new General(this, "shenzhangliao", "god");
    shenzhangliao->addSkill(new Duorui);
    shenzhangliao->addSkill(new DuoruiInvalidity);
    related_skills.insertMulti("duorui", "#duorui-invalidity");
    shenzhangliao->addSkill(new Zhiti);
    shenzhangliao->addSkill(new ZhitiMaxCards);
    related_skills.insertMulti("zhiti", "#zhiti-maxcards");

    General *shenganning = new General(this, "shenganning", "god", 6);
    shenganning->addSkill(new Poxi);
    shenganning->addSkill(new Robying);
    shenganning->addSkill(new RobyingTarget);
    shenganning->addSkill(new RobyingMaxCards);
    related_skills.insertMulti("robying", "#robying-target");
    related_skills.insertMulti("robying", "#robying-maxcards");

    addMetaObject<GongxinCard>();
    addMetaObject<YeyanCard>();
    addMetaObject<ShenfenCard>();
    addMetaObject<GreatYeyanCard>();
    addMetaObject<SmallYeyanCard>();
    addMetaObject<QixingCard>();
    addMetaObject<KuangfengCard>();
    addMetaObject<DawuCard>();
    addMetaObject<WuqianCard>();
    addMetaObject<JilveCard>();
    addMetaObject<ZhanhuoCard>();
    addMetaObject<PoxiCard>();

    skills << new Jilve << new LongnuFire << new LongnuThunder << new PoxiChoose;
}

ADD_PACKAGE(God)

