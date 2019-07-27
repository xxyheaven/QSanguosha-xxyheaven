#include "maneuvering.h"
#include "client.h"
#include "engine.h"
#include "general.h"
#include "room.h"

NatureSlash::NatureSlash(Suit suit, int number, DamageStruct::Nature nature)
    : Slash(suit, number)
{
    this->nature = nature;
}

bool NatureSlash::match(const QString &pattern) const
{
    QStringList patterns = pattern.split("+");
    if (patterns.contains("slash"))
        return true;
    else
        return Slash::match(pattern);
}

ThunderSlash::ThunderSlash(Suit suit, int number)
    : NatureSlash(suit, number, DamageStruct::Thunder)
{
    setObjectName("thunder_slash");
}

FireSlash::FireSlash(Suit suit, int number)
    : NatureSlash(suit, number, DamageStruct::Fire)
{
    setObjectName("fire_slash");
    nature = DamageStruct::Fire;
}

Analeptic::Analeptic(Card::Suit suit, int number)
    : BasicCard(suit, number)
{
    setObjectName("analeptic");
    target_fixed = true;
}

QString Analeptic::getSubtype() const
{
    return "buff_card";
}

bool Analeptic::targetRated(const QList<const Player *> &targets, const Player *, const Player *) const
{
    return !hasFlag("UsedBySecondWay") && targets.isEmpty();
}

bool Analeptic::IsAvailable(const Player *player, const Card *analeptic)
{
    Analeptic *newanaleptic = new Analeptic(Card::NoSuit, 0);
    newanaleptic->deleteLater();
#define THIS_ANALEPTIC (analeptic == NULL ? newanaleptic : analeptic)
    if (player->isCardLimited(THIS_ANALEPTIC, Card::MethodUse) || player->isProhibited(player, THIS_ANALEPTIC))
        return false;

    return player->getMark("AnalepticUsedTimes") <= Sanguosha->correctCardTarget(TargetModSkill::Residue, player, THIS_ANALEPTIC, player);
#undef THIS_ANALEPTIC
}

bool Analeptic::isAvailable(const Player *player) const
{
    return IsAvailable(player, this) && BasicCard::isAvailable(player);
}

void Analeptic::onUse(Room *room, const CardUseStruct &card_use) const
{
    room->setCardEmotion(card_use.from, this);
    BasicCard::onUse(room, card_use);
}

QList<ServerPlayer *> Analeptic::defaultTargets(Room *, ServerPlayer *source) const
{
    QList<ServerPlayer *> targets;
    if (!hasFlag("UsedBySecondWay"))
        targets << source;
    return targets;
}

void Analeptic::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    BasicCard::use(room, source, targets);
}

void Analeptic::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.to->getRoom();
    if (hasFlag("UsedBySecondWay"))
        room->recover(effect.to, RecoverStruct(effect.from, this));
    else
        room->addPlayerMark(effect.to, "drank");
}

class FanSkill : public WeaponSkill
{
public:
    FanSkill() : WeaponSkill("fan")
    {
        events << ConfirmCardUsed;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        if (WeaponSkill::triggerable(player) && use.card->objectName() == "slash")
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        if (player->askForSkillInvoke(this)) {
            room->setEmotion(player, "weapon/fan");
            FireSlash *fire_slash = new FireSlash(use.card->getSuit(), use.card->getNumber());
            fire_slash->copyFrom(use.card);
            use.card = fire_slash;
            data = QVariant::fromValue(use);
        }
        return false;
    }
};

Fan::Fan(Suit suit, int number)
    : Weapon(suit, number, 4)
{
    setObjectName("fan");
}

class GudingBladeSkill : public WeaponSkill
{
public:
    GudingBladeSkill() : WeaponSkill("guding_blade")
    {
        events << DamageCaused;
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (!WeaponSkill::triggerable(player)) return QStringList();
        DamageStruct damage = data.value<DamageStruct>();
        if (damage.card && damage.card->isKindOf("Slash") && damage.by_user && !damage.chain && !damage.transfer
            && damage.to && damage.to->isAlive() && damage.to->getMark("Equips_of_Others_Nullified_to_You") == 0 && damage.to->isKongcheng())
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        room->setEmotion(player, "weapon/guding_blade");
        room->notifySkillInvoked(player, objectName());
        room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), damage.to->objectName());

        LogMessage log;
        log.type = "#GudingBladeEffect";
        log.from = player;
        log.to << damage.to;
        log.arg = QString::number(damage.damage);
        log.arg2 = QString::number(++damage.damage);
        room->sendLog(log);

        data = QVariant::fromValue(damage);
        return false;
    }
};

GudingBlade::GudingBlade(Suit suit, int number)
    : Weapon(suit, number, 2)
{
    setObjectName("guding_blade");
}

class VineSkill : public ArmorSkill
{
public:
    VineSkill() : ArmorSkill("vine")
    {
        events << DamageInflicted << CardEffected;
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (!ArmorSkill::triggerable(player)) return QStringList();
        if (triggerEvent == CardEffected) {
            CardEffectStruct effect = data.value<CardEffectStruct>();
            if (effect.card->isKindOf("AOE") || effect.card->objectName() == "slash")
                return QStringList(objectName());
        } else if (triggerEvent == DamageInflicted) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.nature == DamageStruct::Fire)
                return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        if (triggerEvent == CardEffected) {
            CardEffectStruct effect = data.value<CardEffectStruct>();

            room->setEmotion(player, "armor/vine");

            room->sendCompulsoryTriggerLog(player, objectName());

            effect.nullified = true;

            data = QVariant::fromValue(effect);


        } else if (triggerEvent == DamageInflicted) {
            DamageStruct damage = data.value<DamageStruct>();

            room->setEmotion(player, "armor/vineburn");

            room->sendCompulsoryTriggerLog(player, objectName());
            damage.damage++;

            data = QVariant::fromValue(damage);

        }

        return false;
    }
};

Vine::Vine(Suit suit, int number)
    : Armor(suit, number)
{
    setObjectName("vine");
}

class SilverLionSkill : public ArmorSkill
{
public:
    SilverLionSkill() : ArmorSkill("silver_lion")
    {
        events << DamageInflicted << CardsMoveOneTime;
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (triggerEvent == DamageInflicted) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.from && damage.from->ingoreArmor(player)) return QStringList();
            if (ArmorSkill::triggerable(player) && damage.damage > 1)
                return QStringList(objectName());
        } else if (triggerEvent == CardsMoveOneTime && player->getMark("Armor_Nullified") == 0) {
            QVariantList move_datas = data.toList();
            foreach(QVariant move_data, move_datas) {
                CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
                QString source_name = move.reason.m_playerId;
                ServerPlayer *source = room->findPlayer(source_name);
                if (source && source->ingoreArmor(player)) continue;
                if (move.from != player || !move.from_places.contains(Player::PlaceEquip)) return QStringList();
                for (int i = 0; i < move.card_ids.size(); i++) {
                    if (move.from_places[i] != Player::PlaceEquip) continue;
                    const Card *card = Sanguosha->getEngineCard(move.card_ids[i]);
                    if (card->objectName() == objectName()) {
                        if (player->isWounded()) {
                            return QStringList(objectName());
                        }
                    }
                }
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        if (triggerEvent == DamageInflicted) {
            DamageStruct damage = data.value<DamageStruct>();
            room->sendCompulsoryTriggerLog(player, objectName(), false);
            room->setEmotion(player, "armor/silver_lion");
            damage.damage = 1;
            data = QVariant::fromValue(damage);
        } else {
            room->sendCompulsoryTriggerLog(player, objectName(), false);
            room->setEmotion(player, "armor/silver_lion");
            RecoverStruct recover;
            room->recover(player, recover);
        }
        return false;
    }
};

SilverLion::SilverLion(Suit suit, int number)
    : Armor(suit, number)
{
    setObjectName("silver_lion");
}

FireAttack::FireAttack(Card::Suit suit, int number)
    : SingleTargetTrick(suit, number)
{
    setObjectName("fire_attack");
}

bool FireAttack::targetRated(const QList<const Player *> &targets, const Player *to_select, const Player *) const
{
    return targets.isEmpty() && !to_select->isKongcheng();
}

void FireAttack::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.from->getRoom();
    if (effect.to->isKongcheng())
        return;

    const Card *card = room->askForCardShow(effect.to, effect.from, objectName());
    room->showCard(effect.to, card->getEffectiveId());

    QString suit_str = card->getSuitString();
    QString pattern = QString(".%1").arg(suit_str.at(0).toUpper());
    QString prompt = QString("@fire-attack:%1::%2").arg(effect.to->objectName()).arg(suit_str);
    if (effect.from->isAlive()) {
        const Card *card_to_throw = room->askForCard(effect.from, pattern, prompt);
        if (card_to_throw)
            room->damage(DamageStruct(this, effect.from, effect.to, 1, DamageStruct::Fire));
        else
            effect.from->setFlags("FireAttackFailed_" + effect.to->objectName()); // For AI
    }

    if (card->isVirtualCard())
        delete card;
}

IronChain::IronChain(Card::Suit suit, int number)
    : TrickCard(suit, number)
{
    setObjectName("iron_chain");
    can_recast = true;
}

QString IronChain::getSubtype() const
{
    return "damage_spread";
}

bool IronChain::targetRated(const QList<const Player *> &targets, const Player *, const Player *Self) const
{
    return targets.length() < 2 && !Self->isCardLimited(this, Card::MethodUse);
}

bool IronChain::targetsFeasible(const QList<const Player *> &targets, const Player *Self) const
{
    bool rec = (Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_PLAY) && can_recast;
    QList<int> sub;
    if (isVirtualCard())
        sub = subcards;
    else
        sub << getEffectiveId();
    foreach (int id, sub) {
        if (Self->getHandPile().contains(id)) {
            rec = false;
            break;
        }
    }

    if (rec && Self->isCardLimited(this, Card::MethodUse))
        return targets.length() == 0;
    if (!rec)
        return targets.length() > 0 && targets.length() <= 2;
    else
        return targets.length() <= 2;
}

void IronChain::onUse(Room *room, const CardUseStruct &card_use) const
{
    if (card_use.to.isEmpty()) {

        LogMessage log;
        log.type = "#UseCard_Recast";
        log.from = card_use.from;
        log.card_str = card_use.card->toString();
        room->sendLog(log);

        if (this->isVirtualCard())
            card_use.from->broadcastSkillInvoke(this->getSkillName());
        else
            card_use.from->broadcastSkillInvoke("@recast");

        if (!card_use.card->getSkillName().isEmpty()) {
            QString name = card_use.card->getSkillName();
            room->addPlayerHistory(card_use.from, "ViewAsSkill_" + name + "Card");
        }

        CardMoveReason reason(CardMoveReason::S_REASON_RECAST, card_use.from->objectName());
        reason.m_skillName = this->getSkillName();
        room->moveCardTo(this, card_use.from, NULL, Player::DiscardPile, reason);

        card_use.from->drawCards(1, "recast");
		room->addPlayerHistory(NULL, "pushPile");
    } else{
		foreach(ServerPlayer *player, card_use.to)
            room->setEmotion(player, "chain");
		TrickCard::onUse(room, card_use);
	}
}

void IronChain::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.to->getRoom();

    room->setPlayerProperty(effect.to, "chained", !effect.to->isChained());
}

SupplyShortage::SupplyShortage(Card::Suit suit, int number)
    : DelayedTrick(suit, number)
{
    setObjectName("supply_shortage");

    judge.pattern = ".|club";
    judge.good = true;
    judge.reason = objectName();

    turn_skills << "yearyangshou" << "yearyinshou" << "yearxiongshou";
}

bool SupplyShortage::targetRated(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    if (!targets.isEmpty() || to_select->containsTrick(objectName()) || to_select == Self)
        return false;

    int rangefix = 0;
    if (Self->getOffensiveHorse() && subcards.contains(Self->getOffensiveHorse()->getId()))
        rangefix += 1;

    int distance_limit = 1+Sanguosha->correctCardTarget(TargetModSkill::DistanceLimit, Self, this, to_select);
    if (Self->distanceTo(to_select, rangefix) > distance_limit)
        return false;

    return true;
}

void SupplyShortage::takeEffect(ServerPlayer *target) const
{
    target->broadcastSkillInvoke(QString("@%1").arg(objectName()));
    target->getRoom()->setEmotion(target, QString("@%1").arg(objectName()));
    target->skip(Player::Draw);
}

ManeuveringPackage::ManeuveringPackage()
    : Package("maneuvering", Package::CardPack)
{
    QList<Card *> cards;

    // spade
    cards << new GudingBlade(Card::Spade, 1)
        << new Vine(Card::Spade, 2)
        << new Analeptic(Card::Spade, 3)
        << new ThunderSlash(Card::Spade, 4)
        << new ThunderSlash(Card::Spade, 5)
        << new ThunderSlash(Card::Spade, 6)
        << new ThunderSlash(Card::Spade, 7)
        << new ThunderSlash(Card::Spade, 8)
        << new Analeptic(Card::Spade, 9)
        << new SupplyShortage(Card::Spade, 10)
        << new IronChain(Card::Spade, 11)
        << new IronChain(Card::Spade, 12)
        << new Nullification(Card::Spade, 13);
    // club
    cards << new SilverLion(Card::Club, 1)
        << new Vine(Card::Club, 2)
        << new Analeptic(Card::Club, 3)
        << new SupplyShortage(Card::Club, 4)
        << new ThunderSlash(Card::Club, 5)
        << new ThunderSlash(Card::Club, 6)
        << new ThunderSlash(Card::Club, 7)
        << new ThunderSlash(Card::Club, 8)
        << new Analeptic(Card::Club, 9)
        << new IronChain(Card::Club, 10)
        << new IronChain(Card::Club, 11)
        << new IronChain(Card::Club, 12)
        << new IronChain(Card::Club, 13);

    // heart
    cards << new Nullification(Card::Heart, 1)
        << new FireAttack(Card::Heart, 2)
        << new FireAttack(Card::Heart, 3)
        << new FireSlash(Card::Heart, 4)
        << new Peach(Card::Heart, 5)
        << new Peach(Card::Heart, 6)
        << new FireSlash(Card::Heart, 7)
        << new Jink(Card::Heart, 8)
        << new Jink(Card::Heart, 9)
        << new FireSlash(Card::Heart, 10)
        << new Jink(Card::Heart, 11)
        << new Jink(Card::Heart, 12)
        << new Nullification(Card::Heart, 13);

    // diamond
    cards << new Fan(Card::Diamond, 1)
        << new Peach(Card::Diamond, 2)
        << new Peach(Card::Diamond, 3)
        << new FireSlash(Card::Diamond, 4)
        << new FireSlash(Card::Diamond, 5)
        << new Jink(Card::Diamond, 6)
        << new Jink(Card::Diamond, 7)
        << new Jink(Card::Diamond, 8)
        << new Analeptic(Card::Diamond, 9)
        << new Jink(Card::Diamond, 10)
        << new Jink(Card::Diamond, 11)
        << new FireAttack(Card::Diamond, 12);

    DefensiveHorse *hualiu = new DefensiveHorse(Card::Diamond, 13);
    hualiu->setObjectName("hualiu");

    cards << hualiu;

    foreach(Card *card, cards)
        card->setParent(this);

    skills << new GudingBladeSkill << new FanSkill
        << new VineSkill << new SilverLionSkill;
}

ADD_PACKAGE(Maneuvering)

