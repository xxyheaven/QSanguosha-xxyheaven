#include "standard.h"
#include "standard-skillcards.h"
#include "room.h"
#include "clientplayer.h"
#include "engine.h"
#include "client.h"
#include "json.h"
#include "settings.h"

ZhihengCard::ZhihengCard()
{
    target_fixed = true;
}

void ZhihengCard::extraCost(Room *room, const CardUseStruct &card_use) const
{
    bool all_handcards = true;
    foreach (const Card *c, card_use.from->getHandcards()) {
        if (!usecontains(c)) {
            all_handcards = false;
            break;
        }
    }
    if (all_handcards)
        setFlags("ZhihengAllHandcards");

    CardMoveReason reason(CardMoveReason::S_REASON_THROW, card_use.from->objectName(), QString(), card_use.card->getSkillName(), QString());
    room->moveCardTo(this, NULL, Player::DiscardPile, reason, true);
}

void ZhihengCard::use(Room *, ServerPlayer *source, QList<ServerPlayer *> &) const
{
    if (source->isAlive())
        source->drawCards(subcards.length()+(hasFlag("ZhihengAllHandcards")?1:0), "zhiheng");
}

RendeCard::RendeCard()
{
    will_throw = false;
    handling_method = Card::MethodNone;
}

bool RendeCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
	QStringList rende_prop = Self->property("rende").toString().split("+");
    if (rende_prop.contains(to_select->objectName()))
        return false;

    return targets.isEmpty() && to_select != Self;
}

void RendeCard::extraCost(Room *room, const CardUseStruct &card_use) const
{
	room->addPlayerMark(card_use.to.first(), "rende" + card_use.from->objectName());
    CardMoveReason reason(CardMoveReason::S_REASON_GIVE, card_use.from->objectName(), card_use.to.first()->objectName(), "rende", QString());
    room->obtainCard(card_use.to.first(), this, reason, false);
}

void RendeCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
	ServerPlayer *target = targets.first();
    int old_value = source->getMark("rende");
    int new_value = old_value + subcards.length();
    room->setPlayerMark(source, "rende", new_value);

    if (old_value < 2 && new_value >= 2)
        room->askForUseCard(source, "@@rende_basic", "@rende-basic");

    QSet<QString> rende_prop = source->property("rende").toString().split("+").toSet();
    rende_prop.insert(target->objectName());
    room->setPlayerProperty(source, "rende", QStringList(rende_prop.toList()).join("+"));
}

YijueCard::YijueCard()
{

}

bool YijueCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *) const
{
    return targets.isEmpty() && to_select != Self && !to_select->isKongcheng();
}

void YijueCard::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *source = effect.from;
    Room *room = source->getRoom();
    ServerPlayer *target = effect.to;
    if (target->isDead() || target->isKongcheng()) return;
    const Card *card = room->askForCardShow(target, source, "yijue");
    room->showCard(target, card->getEffectiveId());
    room->getThread()->delay(1000);
    if (card->isBlack()) {
        if (target->getMark("yijue") == 0) {
            target->addMark("yijue");

            room->setPlayerCardLimitation(target, "use,response", ".|.|.|hand", true);
            room->addPlayerMark(target, "skill_invalidity");
            room->addPlayerTip(target, "#yijue");

            QStringList assignee_list = source->property("yijue_targets").toString().split("+");
            assignee_list << target->objectName();
            room->setPlayerProperty(source, "yijue_targets", assignee_list.join("+"));

            foreach(ServerPlayer *p, room->getAllPlayers())
                room->filterCards(p, p->getCards("he"), true);
            JsonArray args;
            args << QSanProtocol::S_GAME_EVENT_UPDATE_SKILL;
            room->doBroadcastNotify(QSanProtocol::S_COMMAND_LOG_EVENT, args);
        }
    } else {
        CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, source->objectName());
        room->obtainCard(source, card, reason, false);
        if (target->isWounded()) {
            target->setFlags("YijueTarget");
            if (room->askForSkillInvoke(source, "yijue_recover", "prompt:" + target->objectName()))
                room->recover(target, RecoverStruct(source));
            target->setFlags("-YijueTarget");
        }
    }
}

JieyinCard::JieyinCard()
{
    m_skillName = "jieyin";
    will_throw = false;
}

bool JieyinCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    if (!targets.isEmpty() || !to_select->isMale()) return false;
    QString choice = Self->tag["jieyin"].toString();
    return choice != "putequip" || to_select->canSetEquip(Sanguosha->getCard(getEffectiveId()));
}

void JieyinCard::extraCost(Room *room, const CardUseStruct &card_use) const
{
    ServerPlayer *sunshangxiang = card_use.from;
    if (user_string == "discard") {
        CardMoveReason reason(CardMoveReason::S_REASON_THROW, sunshangxiang->objectName(), QString(), "jieyin", QString());
        room->moveCardTo(this, sunshangxiang, NULL, Player::DiscardPile, reason, true);
    } else if (user_string == "putequip") {
        LogMessage log;
        log.type = "$PutEquip";
        log.from = sunshangxiang;
        log.to = card_use.to;
        log.card_str = QString::number(getEffectiveId());
        room->sendLog(log);

        room->moveCardTo(this, sunshangxiang, card_use.to.first(), Player::PlaceEquip,
            CardMoveReason(CardMoveReason::S_REASON_PUT, sunshangxiang->objectName(), "jieyin", QString()));
    }
}

void JieyinCard::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.from->getRoom();

    int x1 = effect.from->getHp();
    int x2 = effect.to->getHp();

    RecoverStruct recover(effect.from);

    if (x1 > x2) {
        effect.from->drawCards(1, objectName());
        room->recover(effect.to, recover);
    } else if (x1 < x2) {
        effect.to->drawCards(1, objectName());
        room->recover(effect.from, recover);
    }
}

FanjianCard::FanjianCard()
{
    will_throw = false;
    handling_method = Card::MethodNone;
}

void FanjianCard::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *zhouyu = effect.from;
    ServerPlayer *target = effect.to;
    Room *room = zhouyu->getRoom();
    Card::Suit suit = getSuit();

    CardMoveReason reason(CardMoveReason::S_REASON_GIVE, zhouyu->objectName(), target->objectName(), "fanjian", QString());
    room->obtainCard(target, this, reason);

    if (target->isAlive()) {
        if (target->isKongcheng()) {
            room->loseHp(target);
        } else {
            target->setMark("FanjianSuit", int(suit)); // For AI
            if (room->askForSkillInvoke(target, "fanjian_discard", "prompt:::" + Card::Suit2String(suit))) {
                room->showAllCards(target);
                room->getThread()->delay(3000);
                DummyCard *dummy = new DummyCard;
                foreach (const Card *card, target->getCards("he")) {
                    if (card->getSuit() == suit)
                        dummy->addSubcard(card);
                }
                if (dummy->subcardsLength() > 0)
                    room->throwCard(dummy, target);
                delete dummy;
            } else {
                room->loseHp(target);
            }
        }
    }
}

KurouCard::KurouCard()
{
    target_fixed = true;
}

void KurouCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const
{
    room->loseHp(source);
}

LianyingCard::LianyingCard()
{
}

bool LianyingCard::targetFilter(const QList<const Player *> &targets, const Player *, const Player *Self) const
{
    return targets.length() < Self->getMark("lianying");
}

void LianyingCard::onEffect(const CardEffectStruct &effect) const
{
    effect.to->drawCards(1, "lianying");
}

LijianCard::LijianCard(bool cancelable) : duel_cancelable(cancelable)
{
    will_sort = false;
}

bool LijianCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    if (!to_select->isMale())
        return false;

    Duel *duel = new Duel(Card::NoSuit, 0);
    duel->deleteLater();
    if (targets.isEmpty() && Sanguosha->isProhibited(NULL, to_select, duel))
        return false;

    if (targets.length() == 1 && to_select->isCardLimited(duel, Card::MethodUse))
        return false;

    return targets.length() < 2 && to_select != Self;
}

bool LijianCard::targetsFeasible(const QList<const Player *> &targets, const Player *) const
{
    return targets.length() == 2;
}

void LijianCard::use(Room *room, ServerPlayer *, QList<ServerPlayer *> &targets) const
{
    ServerPlayer *to = targets.at(0);
    ServerPlayer *from = targets.at(1);

    Duel *duel = new Duel(Card::NoSuit, 0);
    duel->setCancelable(duel_cancelable);
    duel->setSkillName(QString("_%1").arg(getSkillName()));
    if (!from->isCardLimited(duel, Card::MethodUse) && !from->isProhibited(to, duel))
        room->useCard(CardUseStruct(duel, from, to));
    else
        delete duel;
}

QingnangCard::QingnangCard()
{
}

bool QingnangCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *) const
{
    return targets.isEmpty() && to_select->isWounded();
}

void QingnangCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    ServerPlayer *target = targets.value(0, source);
    room->cardEffect(this, source, target);
}

void QingnangCard::onEffect(const CardEffectStruct &effect) const
{
    effect.to->getRoom()->recover(effect.to, RecoverStruct(effect.from));
}

LiuliCard::LiuliCard()
{
}

bool LiuliCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    if (!targets.isEmpty()) return false;

    QStringList available_targets = Self->property("liuli_available_targets").toString().split("+");

    if (!available_targets.contains(to_select->objectName())) return false;

    int card_id = subcards.first();
    int range_fix = 0;
    if (Self->getWeapon() && Self->getWeapon()->getId() == card_id) {
        const Weapon *weapon = qobject_cast<const Weapon *>(Self->getWeapon()->getRealCard());
        range_fix += weapon->getRange() - Self->getAttackRange(false);
    } else if (Self->getOffensiveHorse() && Self->getOffensiveHorse()->getId() == card_id) {
        range_fix += 1;
    }

    return Self->inMyAttackRange(to_select, range_fix);
}

void LiuliCard::onEffect(const CardEffectStruct &effect) const
{
	effect.to->setFlags("LiuliTarget");
}

GuoseCard::GuoseCard()
{
    handling_method = Card::MethodNone;
    m_skillName = "guose";
}

bool GuoseCard::willThrow() const
{
    return user_string == "dis_indulgence";
}

bool GuoseCard::isMute() const
{
    return user_string == "use_indulgence";
}

bool GuoseCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    if (user_string == "use_indulgence") {
        Indulgence *indulgence = new Indulgence(getSuit(), getNumber());
        indulgence->addSubcard(getEffectiveId());
        indulgence->setSkillName("guose");
        return indulgence->targetFilter(targets, to_select, Self) && !Self->isProhibited(to_select, indulgence, targets);
    } else if (user_string == "dis_indulgence" && targets.isEmpty()) {
        QList<const Card *> tricks = to_select->getJudgingArea();
        foreach (const Card *judge, tricks) {
            if (judge->isKindOf("Indulgence") && Self->canDiscard(to_select, judge->getEffectiveId()))
                return true;
        }
    }
    return false;
}

void GuoseCard::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.from->getRoom();
    if (user_string == "use_indulgence") {
        Indulgence *indulgence = new Indulgence(getSuit(), getNumber());
        indulgence->addSubcard(getEffectiveId());
        indulgence->setSkillName("guose");
        room->useCard(CardUseStruct(indulgence, effect.from, effect.to));
        effect.from->drawCards(1, "guose");
    } else if (user_string == "dis_indulgence") {
        foreach (const Card *judge, effect.to->getJudgingArea()) {
            if (judge->isKindOf("Indulgence") && effect.from->canDiscard(effect.to, judge->getEffectiveId())) {
                room->throwCard(judge, NULL, effect.from);
                effect.from->drawCards(1, "guose");
                return;
            }
        }
    }
}

JijiangCard::JijiangCard()
{
}

bool JijiangCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    Slash *slash = new Slash(NoSuit, 0);
    slash->deleteLater();
    return slash->targetFilter(targets, to_select, Self);
}

const Card *JijiangCard::validate(CardUseStruct &cardUse) const
{
    cardUse.m_isOwnerUse = false;
    ServerPlayer *liubei = cardUse.from;
    QList<ServerPlayer *> targets = cardUse.to;
    Room *room = liubei->getRoom();

    liubei->broadcastSkillInvoke("jijiang");

    room->notifySkillInvoked(liubei, "jijiang");

    LogMessage log;
    log.from = liubei;
    log.to = targets;
    log.type = "#UseCard";
    log.card_str = toString();
    room->sendLog(log);

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

            return slash;
        }
    }
    foreach(ServerPlayer *target, targets)
        target->setFlags("-JijiangTarget");
    room->setPlayerFlag(liubei, "Global_JijiangFailed");
    return NULL;
}
