#include "standard.h"
#include "standard-equips.h"
#include "maneuvering.h"
#include "general.h"
#include "engine.h"
#include "client.h"
#include "room.h"
#include "ai.h"
#include "settings.h"

Slash::Slash(Suit suit, int number) : BasicCard(suit, number)
{
    setObjectName("slash");
    nature = DamageStruct::Normal;
    specific_assignee = QStringList();
}

bool Slash::IsAvailable(const Player *player, const Card *slash, bool considerSpecificAssignee)
{
    Slash *newslash = new Slash(Card::NoSuit, 0);
    newslash->setFlags("Global_SlashAvailabilityChecking");
    newslash->deleteLater();
#define THIS_SLASH (slash == NULL ? newslash : slash)
    if (player->isCardLimited(THIS_SLASH, Card::MethodUse))
        return false;

    if (Sanguosha->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_PLAY || Sanguosha->getCurrentCardUsePattern() == "@@rende_basic") {
        QList<int> ids;
        if (slash) {
            if (slash->isVirtualCard()) {
                if (slash->subcardsLength() > 0)
                    ids = slash->getSubcards();
            } else {
                ids << slash->getEffectiveId();
            }
        }
        int used = player->getSlashCount();
        int valid = 1 + Sanguosha->correctCardTarget(TargetModSkill::Residue, player, THIS_SLASH, NULL);
        if (used < valid) return true;

        if (considerSpecificAssignee) {
            QList<const Player *> all_players = player->getSiblings();
            foreach (const Player *p, all_players) {
                if (used < 1 + Sanguosha->correctCardTarget(TargetModSkill::Residue, player, THIS_SLASH, p))
                    return true;
            }
        }
        return false;
    } else {
        return true;
    }
#undef THIS_SLASH
}

bool Slash::IsSpecificAssignee(const Player *player, const Player *from, const Card *slash)
{
    if (from->hasFlag("slashTargetFix") && player->hasFlag("SlashAssignee"))
        return true;
    else if (from->getPhase() == Player::Play && Sanguosha->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_PLAY
        && !Slash::IsAvailable(from, slash, false)) {
        return from->getSlashCount() < 1 + Sanguosha->correctCardTarget(TargetModSkill::Residue, from, slash, player);
    } else {
        const Slash *s = qobject_cast<const Slash *>(slash);
        if (s && s->hasSpecificAssignee(player))
            return true;
    }
    return false;
}

bool Slash::isAvailable(const Player *player) const
{
    return IsAvailable(player, this) && BasicCard::isAvailable(player);
}

QString Slash::getSubtype() const
{
    return "attack_card";
}

void Slash::onUse(Room *room, const CardUseStruct &card_use) const
{
    CardUseStruct use = card_use;
    ServerPlayer *player = use.from;

    if (player->hasFlag("slashTargetFix")) {
        room->setPlayerFlag(player, "-slashTargetFix");
        room->setPlayerFlag(player, "-slashTargetFixToOne");
        foreach(ServerPlayer *target, room->getAlivePlayers())
            if (target->hasFlag("SlashAssignee"))
                room->setPlayerFlag(target, "-SlashAssignee");
    }

    if (player->hasFlag("slashNoDistanceLimit")){
        room->setPlayerFlag(player, "-slashNoDistanceLimit");
        room->setCardFlag(this, "slashNoDistanceLimit");
    }

    if (player->hasFlag("slashDisableExtraTarget")){
        room->setPlayerFlag(player, "-slashDisableExtraTarget");
        room->setCardFlag(this, "slashDisableExtraTarget");
    }

    if (player->getPhase() == Player::Play && player->hasFlag("Global_MoreSlashInOneTurn")) {
        QString name;
        if (player->hasSkill("paoxiao"))
            name = "paoxiao";
        if (!name.isEmpty()) {
            player->setFlags("-Global_MoreSlashInOneTurn");
            if (name == "paoxiao") {
                player->broadcastSkillInvoke(name);
                room->notifySkillInvoked(player, name);
            }
        }
    }
    if (use.to.size() > 1 && player->hasSkill("shenji")) {
        player->broadcastSkillInvoke("shenji");
        room->notifySkillInvoked(player, "shenji");
    }

    int rangefix = 0;
    if (use.card->isVirtualCard()) {
        if (use.from->getWeapon() && use.card->getSubcards().contains(use.from->getWeapon()->getId())) {
            const Weapon *weapon = qobject_cast<const Weapon *>(use.from->getWeapon()->getRealCard());
            rangefix += weapon->getRange() - use.from->getAttackRange(false);
        }
        if (use.from->getOffensiveHorse() && use.card->getSubcards().contains(use.from->getOffensiveHorse()->getId()))
            rangefix += 1;
    }
    foreach (ServerPlayer *p, use.to) {
        if (p->hasSkill("tongji") && p->getHandcardNum() > p->getHp() && use.from->inMyAttackRange(p, rangefix)) {
            p->broadcastSkillInvoke("tongji");
            room->notifySkillInvoked(p, "tongji");
            break;
        }
    }

    if (use.from->hasFlag("BladeUse")) {
        use.from->setFlags("-BladeUse");
        if (player->getWeapon())
            room->setCardFlag(player->getWeapon()->getId(), "-using");
        room->setEmotion(player, "weapon/blade");

        LogMessage log;
        log.type = "#BladeUse";
        log.from = use.from;
        log.to << use.to;
        room->sendLog(log);
    } else if (use.to.size() > 1 && player->hasWeapon("halberd") && player->isLastHandCard(this)) {
        room->setEmotion(player, "weapon/halberd");
        room->notifySkillInvoked(player, "halberd");
    }
    if (player->getPhase() == Player::Play
        && player->hasFlag("Global_MoreSlashInOneTurn")
        && player->hasWeapon("Crossbow") && !player->canSlashWithoutCrossbow(this)) {
        player->setFlags("-Global_MoreSlashInOneTurn");
        room->sendCompulsoryTriggerLog(player, "crossbow", false);
        room->setEmotion(player, "weapon/crossbow");
    }
    if (use.m_isOwnerUse)
        room->setCardEmotion(player, this);

    int x = this->tag["addcardinality"].toInt();
    if (use.from->getMark("drank") > 0) {
        room->setCardFlag(use.card, "drank");
        x += use.from->getMark("drank");
        room->setPlayerMark(use.from, "drank", 0);
    }
    if (use.from->getMark("#kannan") > 0) {
        x += use.from->getMark("#kannan");
        room->setPlayerMark(use.from, "#kannan", 0);
    }

    this->setTag("addcardinality", x);

    BasicCard::onUse(room, use);
}

void Slash::onEffect(const CardEffectStruct &effect) const
{
    if (!effect.to->isAlive()) return;

    SlashEffectStruct slash_effect;
    slash_effect.from = effect.from;
    slash_effect.nature = nature;
    slash_effect.slash = this;

    slash_effect.to = effect.to;
    slash_effect.drank = this->tag["drank"].toInt();
    slash_effect.index = effect.index;

    Room *room = effect.from->getRoom();

    QString slasher = effect.from->objectName();
    int index = effect.index;

    QVariantList jink_list = this->tag["Jink_List"].toList();
    QVariantList damage_list = this->tag["Damage_List"].toList();
    int jink_num = jink_list[index].toInt();

    if (tag["Fuji_tag"].toStringList().contains(effect.to->objectName()) || tag["Fuji_tag"].toStringList().contains("_ALL_PLAYERS"))
        jink_num = 0;


    int damage = damage_list[index].toInt();

    slash_effect.cardinality = damage + this->tag["addcardinality"].toInt();

    QVariant data = QVariant::fromValue(slash_effect);


    if (jink_num == 1) {
        const Card *jink = room->askForCard(effect.to, "jink", "slash-jink:" + slasher, data, Card::MethodUse, effect.from);
        room->slashResult(slash_effect, jink);
    } else {
        const Card *asked_jink = NULL;
        for (int i = jink_num; i > 0; i--) {
            QString prompt = QString("@multi-jink%1:%2::%3").arg(i == jink_num ? "-start" : QString())
                .arg(slasher).arg(i);
            asked_jink = room->askForCard(effect.to, "jink", prompt, data, Card::MethodUse, effect.from);
            if (asked_jink == NULL) {
                room->slashResult(slash_effect, NULL);
                return;
            }
        }
        room->slashResult(slash_effect, asked_jink);
    }
}

bool Slash::targetsFeasible(const QList<const Player *> &targets, const Player *) const
{
    return !targets.isEmpty();
}

bool Slash::targetRated(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    int slash_targets = 1 + Sanguosha->correctCardTarget(TargetModSkill::ExtraTarget, Self, this, to_select);
    bool distance_limit = ((1 + Sanguosha->correctCardTarget(TargetModSkill::DistanceLimit, Self, this, to_select)) < 500);
    if (Self->hasFlag("slashNoDistanceLimit"))
        distance_limit = false;

    int rangefix = 0;
    if (Self->getWeapon() && (subcards.contains(Self->getWeapon()->getId()) || costcards.contains(Self->getWeapon()->getId()))) {
        const Weapon *weapon = qobject_cast<const Weapon *>(Self->getWeapon()->getRealCard());
        rangefix += weapon->getRange() - Self->getAttackRange(false);
    }

    if (Self->getOffensiveHorse() && (subcards.contains(Self->getOffensiveHorse()->getId()) || costcards.contains(Self->getOffensiveHorse()->getId())))
        rangefix += 1;

    if (!Self->canSlash(to_select, this, distance_limit, rangefix, targets)) return false;
    return targets.length() < slash_targets;
}

bool Slash::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    if (!targetRated(targets, to_select, Self)) return false;
    if (targets.isEmpty()) {
        QList<const Player *> all_players = Self->getSiblings();
        all_players.append(Self);
        foreach (const Player *p, all_players) {
            if (Slash::IsSpecificAssignee(p, Self, this)) {
                return Slash::IsSpecificAssignee(to_select, Self, this);
            }
        }
    }
    return true;
}

Jink::Jink(Suit suit, int number) : BasicCard(suit, number)
{
    setObjectName("jink");
    target_fixed = true;
}

QString Jink::getSubtype() const
{
    return "defense_card";
}

bool Jink::isAvailable(const Player *) const
{
    return false;
}

void Jink::onEffect(const CardEffectStruct &effect) const
{
    if (effect.to_card)
        effect.to_card->setFlags("Global_Jink_Effected");
}

Peach::Peach(Suit suit, int number) : BasicCard(suit, number)
{
    setObjectName("peach");
    target_fixed = true;
}

QString Peach::getSubtype() const
{
    return "recover_card";
}

bool Peach::targetRated(const QList<const Player *> &targets, const Player *to_select, const Player *) const
{
    return !hasFlag("UsedBySecondWay") && targets.isEmpty() && to_select->isWounded();
}

void Peach::onUse(Room *room, const CardUseStruct &card_use) const
{
    room->setCardEmotion(card_use.from, this);
    BasicCard::onUse(room, card_use);
}

QList<ServerPlayer *> Peach::defaultTargets(Room *, ServerPlayer *source) const
{
    QList<ServerPlayer *> targets;
    if (!hasFlag("UsedBySecondWay"))
        targets << source;
    return targets;
}

void Peach::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.to->getRoom();
    RecoverStruct recover;
    recover.card = this;
    recover.recover = 1;
    recover.effect = effect;
    room->recover(effect.to, recover);
}

bool Peach::isAvailable(const Player *player) const
{
    return player->isWounded() && !player->isProhibited(player, this) && BasicCard::isAvailable(player);
}

Crossbow::Crossbow(Suit suit, int number)
    : Weapon(suit, number, 1)
{
    setObjectName("crossbow");
}

class CrossbowSkill : public TargetModSkill
{
public:
    CrossbowSkill() : TargetModSkill("crossbow")
    {
    }

    virtual int getResidueNum(const Player *from, const Card *slash, const Player *) const
    {
        if (from->hasWeapon(objectName()) && !slash->usecontains(from->getWeapon())
                && !from->hasFlag("Global_CrossbowCheaking"))
            return 10000;
        else
            return 0;
    }
};

class DoubleSwordSkill : public WeaponSkill
{
public:
    DoubleSwordSkill() : WeaponSkill("double_sword")
    {
        events << TargetSpecified;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (WeaponSkill::triggerable(player)) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card != NULL && use.card->isKindOf("Slash")) {
                ServerPlayer *to = use.to.at(use.index);

                if (to && to->isAlive() && to->getMark("Equips_of_Others_Nullified_to_You") == 0) {
                    if ((player->isMale() && to->isFemale()) || (player->isFemale() && to->isMale()))
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
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), to->objectName());
            room->setEmotion(player, "weapon/double_sword");

            if (!room->askForCard(to, ".", "double-sword-card:" + player->objectName(), data))
                player->drawCards(1, objectName());
        }

        return false;
    }
};

DoubleSword::DoubleSword(Suit suit, int number)
    : Weapon(suit, number, 2)
{
    setObjectName("double_sword");
}

class QinggangSwordSkill : public WeaponSkill
{
public:
    QinggangSwordSkill() : WeaponSkill("qinggang_sword")
    {
        events << TargetSpecified;
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (WeaponSkill::triggerable(player)) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card != NULL && use.card->isKindOf("Slash")) {
                ServerPlayer *to = use.to.at(use.index);
                if (to && to->isAlive() && to->getMark("Equips_of_Others_Nullified_to_You") == 0)
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
        room->sendCompulsoryTriggerLog(player, objectName());
        room->setEmotion(player, "weapon/qinggang_sword");
        room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), to->objectName());


        QVariantList qinggang_list = use.card->tag["Qinggang_List"].toList();
        qinggang_list[index] = true;
        use.card->setTag("Qinggang_List", qinggang_list);

        room->addPlayerMark(to, "Armor_Nullified");

        return false;
    }
};

QinggangSword::QinggangSword(Suit suit, int number)
    : Weapon(suit, number, 2)
{
    setObjectName("qinggang_sword");
}

class BladeSkill : public WeaponSkill
{
public:
    BladeSkill() : WeaponSkill("blade")
    {
        events << SlashMissed;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (!WeaponSkill::triggerable(player)) return QStringList();
        SlashEffectStruct effect = data.value<SlashEffectStruct>();
        if (effect.to->isAlive() && effect.to->getMark("Equips_of_Others_Nullified_to_You") == 0 &&
                effect.from->canSlash(effect.to, NULL, false))
            return QStringList(objectName());

        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        SlashEffectStruct effect = data.value<SlashEffectStruct>();
        int weapon_id = player->getWeapon()->getId();
        room->setCardFlag(weapon_id, "using");
        player->setFlags("BladeUse");
        bool use = room->askForUseSlashTo(player, effect.to, QString("blade-slash:%1").arg(effect.to->objectName()), false, true);
        if (!use) player->setFlags("-BladeUse");
        room->setCardFlag(weapon_id, "-using");
        return false;
    }
};

Blade::Blade(Suit suit, int number)
    : Weapon(suit, number, 3)
{
    setObjectName("blade");
}

class SpearSkill : public ViewAsSkill
{
public:
    SpearSkill() : ViewAsSkill("spear")
    {
        response_or_use = true;
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return Slash::IsAvailable(player) && player->getMark("Equips_Nullified_to_Yourself") == 0;
    }

    virtual bool isEnabledAtResponse(const Player *player, const QString &pattern) const
    {
        return pattern == "slash" && player->getMark("Equips_Nullified_to_Yourself") == 0;
    }

    virtual bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
        return selected.length() < 2 && !to_select->isEquipped();
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.length() != 2)
            return NULL;

        Slash *slash = new Slash(Card::SuitToBeDecided, 0);
        slash->setSkillName(objectName());
        slash->addSubcards(cards);

        return slash;
    }
};

Spear::Spear(Suit suit, int number)
    : Weapon(suit, number, 3)
{
    setObjectName("spear");
}

class AxeViewAsSkill : public ViewAsSkill
{
public:
    AxeViewAsSkill() : ViewAsSkill("axe")
    {
        response_pattern = "@axe";
    }

    virtual bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
        return selected.length() < 2 && to_select != Self->getWeapon() && !Self->isJilei(to_select);
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.length() != 2)
            return NULL;

        DummyCard *card = new DummyCard;
        card->setSkillName(objectName());
        card->addSubcards(cards);
        return card;
    }
};

class AxeSkill : public WeaponSkill
{
public:
    AxeSkill() : WeaponSkill("axe")
    {
        events << SlashMissed;
        view_as_skill = new AxeViewAsSkill;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (!WeaponSkill::triggerable(player)) return QStringList();
        SlashEffectStruct effect = data.value<SlashEffectStruct>();
        if (effect.to->isAlive() && effect.to->getMark("Equips_of_Others_Nullified_to_You") == 0 && player->getCardCount() >= 3)
            return QStringList(objectName());

        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        SlashEffectStruct effect = data.value<SlashEffectStruct>();
        if (room->askForCard(player, "@axe", "@axe:" + effect.to->objectName(), data, objectName())) {
            room->setEmotion(player, "weapon/axe");
            effect.flags << "axe";
            data = QVariant::fromValue(effect);
        }
        return false;
    }
};

Axe::Axe(Suit suit, int number)
    : Weapon(suit, number, 3)
{
    setObjectName("axe");
}

class HalberdSkill : public TargetModSkill
{
public:
    HalberdSkill() : TargetModSkill("halberd")
    {
    }

    virtual int getExtraTargetNum(const Player *from, const Card *card) const
    {
        if (from->hasWeapon("halberd") && from->isLastHandCard(card))
            return 2;
        else
            return 0;
    }
};

Halberd::Halberd(Suit suit, int number)
    : Weapon(suit, number, 4)
{
    setObjectName("halberd");
}

class KylinBowSkill : public WeaponSkill
{
public:
    KylinBowSkill() : WeaponSkill("kylin_bow")
    {
        events << DamageCaused;
    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (!WeaponSkill::triggerable(player)) return QStringList();
        DamageStruct damage = data.value<DamageStruct>();
        if (damage.card && damage.card->isKindOf("Slash") && damage.by_user && !damage.chain && !damage.transfer &&
                damage.to->getMark("Equips_of_Others_Nullified_to_You") == 0) {
            if ((damage.to->getDefensiveHorse() && damage.from->canDiscard(damage.to, damage.to->getDefensiveHorse()->getEffectiveId())) ||
                    (damage.to->getOffensiveHorse() && damage.from->canDiscard(damage.to, damage.to->getOffensiveHorse()->getEffectiveId())))
                return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        DamageStruct damage = data.value<DamageStruct>();

        QStringList horses;

        if (damage.to->getDefensiveHorse() && damage.from->canDiscard(damage.to, damage.to->getDefensiveHorse()->getEffectiveId()))
            horses << "dhorse";
        if (damage.to->getOffensiveHorse() && damage.from->canDiscard(damage.to, damage.to->getOffensiveHorse()->getEffectiveId()))
            horses << "ohorse";

        if (horses.isEmpty())
            return false;

        if (player->askForSkillInvoke(this, QVariant::fromValue(damage.to))) {
            room->setEmotion(player, "weapon/kylin_bow");

            QString horse_type = room->askForChoice(player, objectName(), horses.join("+"));

            if (horse_type == "dhorse")
                room->throwCard(damage.to->getDefensiveHorse(), damage.to, damage.from);
            else if (horse_type == "ohorse")
                room->throwCard(damage.to->getOffensiveHorse(), damage.to, damage.from);
        }
        return false;
    }
};

KylinBow::KylinBow(Suit suit, int number)
    : Weapon(suit, number, 5)
{
    setObjectName("kylin_bow");
}

class EightDiagramSkill : public ArmorSkill
{
public:
    EightDiagramSkill() : ArmorSkill("eight_diagram")
    {
        events << CardAsked;
        global = true;
    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (ArmorSkill::triggerable(player) && data.toStringList().first() == "jink") {
            Jink *jink = new Jink(Card::NoSuit, 0);
            jink->setSkillName(objectName());
            if (!player->isLocked(jink))
                return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        if (!ArmorSkill::cost(room, player, data)) return false;
        room->setEmotion(player, "armor/eight_diagram");
        int armor_id = -1;
        if (player->getArmor()) {
            armor_id = player->getArmor()->getId();
            room->setCardFlag(armor_id, "using");
        }
        JudgeStruct judge;
        judge.pattern = ".|red";
        judge.good = true;
        judge.reason = objectName();
        judge.who = player;

        room->judge(judge);

        if (armor_id != -1)
            room->setCardFlag(armor_id, "-using");

        if (judge.isGood()) {
            Jink *jink = new Jink(Card::NoSuit, 0);
            jink->setSkillName(objectName());
            room->provide(jink);

            return true;
        }
        return false;
    }
};

EightDiagram::EightDiagram(Suit suit, int number)
    : Armor(suit, number)
{
    setObjectName("eight_diagram");
}

AmazingGrace::AmazingGrace(Suit suit, int number)
    : GlobalEffect(suit, number)
{
    setObjectName("amazing_grace");
    has_preact = true;
}

void AmazingGrace::clearRestCards(Room *room) const
{
    room->clearAG();
    QVariantList ag_list = room->getTag("AmazingGrace").toList();
    if (ag_list.isEmpty()) return;
    DummyCard *dummy = new DummyCard(VariantList2IntList(ag_list));
    CardMoveReason reason(CardMoveReason::S_REASON_NATURAL_ENTER, QString(), "amazing_grace", QString());
    room->throwCard(dummy, reason, NULL);
    delete dummy;
}

void AmazingGrace::doPreAction(Room *room, const CardUseStruct &card_use) const
{
    QList<int> card_ids = room->getNCards(card_use.to.length());
    room->fillAG(card_ids);
    room->setTag("AmazingGrace", IntList2VariantList(card_ids));
}

void AmazingGrace::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    try {
        GlobalEffect::use(room, source, targets);
        clearRestCards(room);
    }
    catch (TriggerEvent triggerEvent) {
        if (triggerEvent == TurnBroken || triggerEvent == StageChange)
            clearRestCards(room);
        throw triggerEvent;
    }
}

void AmazingGrace::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.from->getRoom();
    QVariantList ag_list = room->getTag("AmazingGrace").toList();
    QList<int> card_ids;
    foreach(QVariant card_id, ag_list)
        card_ids << card_id.toInt();

    if (card_ids.isEmpty())
        return;

    int card_id = room->askForAG(effect.to, card_ids, false, objectName());
    card_ids.removeOne(card_id);

    room->takeAG(effect.to, card_id);
    ag_list.removeOne(card_id);

    room->setTag("AmazingGrace", ag_list);
}

GodSalvation::GodSalvation(Suit suit, int number)
    : GlobalEffect(suit, number)
{
    setObjectName("god_salvation");
}

bool GodSalvation::isCancelable(const CardEffectStruct &effect) const
{
    return effect.to->isWounded() && TrickCard::isCancelable(effect);
}

void GodSalvation::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.to->getRoom();
    if (!effect.to->isWounded())
        room->setEmotion(effect.to, "skill_nullify");
    else
        room->recover(effect.to, RecoverStruct(effect.from, this));
}

SavageAssault::SavageAssault(Suit suit, int number)
    : AOE(suit, number)
{
    setObjectName("savage_assault");
}

void SavageAssault::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.to->getRoom();
    bool damage = true;
    if (!tag["Fuji_tag"].toStringList().contains(effect.to->objectName()) && !tag["Fuji_tag"].toStringList().contains("_ALL_PLAYERS")) {
        damage = (room->askForCard(effect.to, "slash",
            "savage-assault-slash:" + effect.from->objectName(),
            QVariant::fromValue(effect), Card::MethodResponse,
            effect.from->isAlive() ? effect.from : NULL) == NULL);
    }
    if (damage) {
        ServerPlayer *from = tag["GlobalDamageSource"].value<ServerPlayer *>();

        QVariantList damage_list = this->tag["Damage_List"].toList();
        int x = damage_list[effect.index].toInt() + this->tag["addcardinality"].toInt();

        room->damage(DamageStruct(this, from == NULL ? effect.from : from, effect.to, x));
    }
}

ArcheryAttack::ArcheryAttack(Card::Suit suit, int number)
    : AOE(suit, number)
{
    setObjectName("archery_attack");
}

void ArcheryAttack::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.to->getRoom();
    bool damage = true;
    if (!tag["Fuji_tag"].toStringList().contains(effect.to->objectName()) && !tag["Fuji_tag"].toStringList().contains("_ALL_PLAYERS")) {
        damage = (room->askForCard(effect.to, "jink",
            "archery-attack-jink:" + effect.from->objectName(),
            QVariant::fromValue(effect), Card::MethodResponse,
            effect.from->isAlive() ? effect.from : NULL) == NULL);
    }
    if (damage) {

        QVariantList damage_list = this->tag["Damage_List"].toList();
        int x = damage_list[effect.index].toInt() + this->tag["addcardinality"].toInt();

        room->damage(DamageStruct(this, effect.from->isAlive() ? effect.from : NULL, effect.to, x));
    }
}

Collateral::Collateral(Card::Suit suit, int number)
    : SingleTargetTrick(suit, number)
{
    setObjectName("collateral");
}

bool Collateral::isAvailable(const Player *player) const
{
    return SingleTargetTrick::isAvailable(player);
}

bool Collateral::targetsFeasible(const QList<const Player *> &targets, const Player *) const
{
    return targets.length() == 2;
}

bool Collateral::targetRated(const QList<const Player *> &targets,
    const Player *to_select, const Player *Self) const
{
    if (!targets.isEmpty()) {
        // @todo: fix this. We should probably keep the codes here, but change the code in
        // roomscene such that if it is collateral, then targetFilter's result is overrode
        Q_ASSERT(targets.length() <= 2);
        if (targets.length() == 2) return false;
        const Player *slashFrom = targets[0];
        /* @todo: develop a new mechanism of filtering targets
                    to remove the coupling here and to fix the similar bugs caused by TongJi */
        return slashFrom->canSlash(to_select);
    } else {
        if (!to_select->getWeapon() || to_select == Self)
            return false;
        foreach (const Player *p, to_select->getAliveSiblings()) {
            if (to_select->canSlash(p))
                return true;
        }
    }
    return false;
}

void Collateral::onUse(Room *room, const CardUseStruct &card_use) const
{
    Q_ASSERT(card_use.to.length() == 2);
    ServerPlayer *killer = card_use.to.at(0);
    ServerPlayer *victim = card_use.to.at(1);

    CardUseStruct new_use = card_use;
    new_use.to.removeAt(1);
    killer->tag["collateralVictim"] = QVariant::fromValue(victim);

    SingleTargetTrick::onUse(room, new_use);
}

bool Collateral::doCollateral(Room *room, ServerPlayer *killer, ServerPlayer *victim, const QString &prompt) const
{
    bool useSlash = false;
    if (killer->canSlash(victim, NULL, false))
        useSlash = room->askForUseSlashTo(killer, victim, prompt);
    return useSlash;
}

void Collateral::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *source = effect.from;
    Room *room = source->getRoom();
    ServerPlayer *killer = effect.to;
    ServerPlayer *victim = killer->tag["collateralVictim"].value<ServerPlayer *>();
    killer->tag.remove("collateralVictim");

    if (!victim || victim->isDead()) {
        if (source->isAlive() && killer->isAlive() && killer->getWeapon())
            source->obtainCard(killer->getWeapon());
    } else if (tag["Fuji_tag"].toStringList().contains(killer->objectName()) || tag["Fuji_tag"].toStringList().contains("_ALL_PLAYERS")) {
        if (source->isAlive() && killer->isAlive() && killer->getWeapon())
            source->obtainCard(killer->getWeapon());
    } else {
        QString prompt = QString("collateral-slash:%1:%2").arg(victim->objectName()).arg(source->objectName());
        if (!killer->isDead() && !(killer->canSlash(victim, NULL, false)
                && room->askForUseSlashTo(killer, victim, prompt, true, false, false, QVariant::fromValue(effect)))) {
            if (source->isAlive() && killer->getWeapon())
                source->obtainCard(killer->getWeapon());
        }
    }
}

Nullification::Nullification(Suit suit, int number)
    : SingleTargetTrick(suit, number)
{
    target_fixed = true;
    setObjectName("nullification");
}

void Nullification::onEffect(const CardEffectStruct &effect) const
{
    if (effect.to_card)
        effect.to_card->setFlags("Global_Nullification_Effected");
}

bool Nullification::isAvailable(const Player *) const
{
    return false;
}

ExNihilo::ExNihilo(Suit suit, int number)
    : SingleTargetTrick(suit, number)
{
    setObjectName("ex_nihilo");
    target_fixed = true;
}

bool ExNihilo::targetRated(const QList<const Player *> &targets, const Player *, const Player *) const
{
    return targets.isEmpty();
}

QList<ServerPlayer *> ExNihilo::defaultTargets(Room *, ServerPlayer *source) const
{
    return QList<ServerPlayer *>() << source;
}

bool ExNihilo::isAvailable(const Player *player) const
{
    return !player->isProhibited(player, this) && TrickCard::isAvailable(player);
}

void ExNihilo::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.to->getRoom();
    int extra = 0;
    if (room->getMode() == "06_3v3" && Config.value("3v3/OfficialRule", "2013").toString() == "2013") {
        int friend_num = 0, enemy_num = 0;
        foreach (ServerPlayer *p, room->getAllPlayers()) {
            if (AI::GetRelation3v3(effect.to, p) == AI::Friend)
                friend_num++;
            else
                enemy_num++;
        }
        if (friend_num < enemy_num) extra = 1;
    }
    effect.to->drawCards(2 + extra, "ex_nihilo");
}

Duel::Duel(Suit suit, int number)
    : SingleTargetTrick(suit, number)
{
    setObjectName("duel");
}

bool Duel::targetRated(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select != Self;
}

void Duel::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *first = effect.to;
    ServerPlayer *second = effect.from;
    Room *room = first->getRoom();

    room->setEmotion(first, "duel");
    room->setEmotion(second, "duel");

    QVariantList wushuang1_list = this->tag["Wushuang1_List"].toList();
    QVariantList wushuang2_list = this->tag["Wushuang2_List"].toList();
    int index = effect.index;

    bool wushuang1 = wushuang1_list[index].toBool();
    bool wushuang2 = wushuang2_list[index].toBool();


    forever{
        if (!first->isAlive() || tag["Fuji_tag"].toStringList().contains(first->objectName()) || tag["Fuji_tag"].toStringList().contains("_ALL_PLAYERS")) break;
        if (wushuang1) {
            const Card *slash = room->askForCard(first,
                "slash",
                "@wushuang-slash-1:" + second->objectName(),
                QVariant::fromValue(effect),
                Card::MethodResponse,
                second);
            if (slash == NULL)
                break;

            slash = room->askForCard(first, "slash",
                "@wushuang-slash-2:" + second->objectName(),
                QVariant::fromValue(effect),
                Card::MethodResponse,
                second);
            if (slash == NULL)
                break;
        } else {
            const Card *slash = room->askForCard(first,
                "slash",
                "duel-slash:" + second->objectName(),
                QVariant::fromValue(effect),
                Card::MethodResponse,
                second);
            if (slash == NULL)
                break;
        }

        qSwap(first, second);
        qSwap(wushuang1, wushuang2);
    }

    QVariantList damage_list = this->tag["Damage_List"].toList();
    int x = damage_list[index].toInt() + this->tag["addcardinality"].toInt();

    DamageStruct damage(this, second->isAlive() ? second : NULL, first, x);
    if (second != effect.from)
        damage.by_user = false;
    room->damage(damage);
}

Snatch::Snatch(Suit suit, int number)
    : SingleTargetTrick(suit, number)
{
    setObjectName("snatch");
}

bool Snatch::targetRated(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    bool include_judging = !(ServerInfo.GameMode == "02_1v1" && ServerInfo.GameRuleMode != "Classical");
    if (!targets.isEmpty() || to_select->getCardCount(true, include_judging) == 0 || to_select == Self)
        return false;

    int distance_limit = 1 + Sanguosha->correctCardTarget(TargetModSkill::DistanceLimit, Self, this, to_select);
    int rangefix = 0;
    if (Self->getOffensiveHorse() && usecontains(Self->getOffensiveHorse()))
        rangefix += 1;
    if (getSkillName() == "jixi")
        rangefix += 1;

    if (Self->distanceTo(to_select, rangefix) > distance_limit)
        return false;

    return true;
}

void Snatch::onEffect(const CardEffectStruct &effect) const
{
    if (effect.from->isDead())
        return;
    if (effect.to->isAllNude())
        return;

    Room *room = effect.to->getRoom();
    bool using_2013 = (room->getMode() == "02_1v1" && Config.value("1v1/Rule", "2013").toString() != "Classical");
    QString flag = using_2013 ? "he" : "hej";
    int card_id = room->askForCardChosen(effect.from, effect.to, flag, objectName());
    CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, effect.from->objectName());
    room->obtainCard(effect.from, Sanguosha->getCard(card_id), reason, room->getCardPlace(card_id) != Player::PlaceHand);
}

Dismantlement::Dismantlement(Suit suit, int number)
    : SingleTargetTrick(suit, number)
{
    setObjectName("dismantlement");
}

bool Dismantlement::targetRated(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    bool include_judging = !(ServerInfo.GameMode == "02_1v1" && ServerInfo.GameRuleMode != "Classical");
    return targets.isEmpty() && to_select->getCardCount(true, include_judging) > 0 && to_select != Self;
}

void Dismantlement::onEffect(const CardEffectStruct &effect) const
{
    if (effect.from->isDead())
        return;

    Room *room = effect.to->getRoom();
    bool using_2013 = (room->getMode() == "02_1v1" && Config.value("1v1/Rule", "2013").toString() != "Classical");

    int card_id = -1;
    if (!using_2013) {
        if (!effect.from->canDiscard(effect.to, "hej")) return;
        card_id = room->askForCardChosen(effect.from, effect.to, "hej", objectName(), false, Card::MethodDiscard);
    } else {
        QStringList choices;
        if (effect.from->canDiscard(effect.to, "h"))
            choices << "h";

        if (effect.from->canDiscard(effect.to, "e"))
            choices << "e";
        if (choices.isEmpty()) return;
        QString flag = room->askForChoice(effect.from, objectName(), choices.join("+"), QVariant(), QString(), "h+e");
        if (flag == "h") {
            LogMessage log;
            log.type = "$ViewAllCards";
            log.from = effect.from;
            log.to << effect.to;
            log.card_str = IntList2StringList(effect.to->handCards()).join("+");
            room->sendLog(log, effect.from);
        }
        card_id = room->askForCardChosen(effect.from, effect.to, flag, objectName(), true, Card::MethodDiscard);
    }
    room->throwCard(card_id, room->getCardPlace(card_id) == Player::PlaceDelayedTrick ? NULL : effect.to, effect.from);
}

Indulgence::Indulgence(Suit suit, int number)
    : DelayedTrick(suit, number)
{
    setObjectName("indulgence");

    judge.pattern = ".|heart";
    judge.good = true;
    judge.reason = objectName();
}

bool Indulgence::targetRated(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select != Self;
}

void Indulgence::takeEffect(ServerPlayer *target) const
{
    target->clearHistory();
    target->broadcastSkillInvoke("@indulgence");
    target->skip(Player::Play);
}

Disaster::Disaster(Card::Suit suit, int number)
    : DelayedTrick(suit, number, true)
{
    target_fixed = true;
}

void Disaster::onUse(Room *room, const CardUseStruct &card_use) const
{
    CardUseStruct use = card_use;
    if (use.to.isEmpty())
        use.to << use.from;
    DelayedTrick::onUse(room, use);
}

bool Disaster::isAvailable(const Player *player) const
{
    return !player->isProhibited(player, this) && DelayedTrick::isAvailable(player);
}

Lightning::Lightning(Suit suit, int number) :Disaster(suit, number)
{
    setObjectName("lightning");

    judge.pattern = ".|spade|2~9";
    judge.good = false;
    judge.reason = objectName();
}

void Lightning::takeEffect(ServerPlayer *target) const
{
    target->broadcastSkillInvoke("@lightning");
    target->getRoom()->damage(DamageStruct(this, NULL, target, 3, DamageStruct::Thunder));
}

// EX cards

class IceSwordSkill : public WeaponSkill
{
public:
    IceSwordSkill() : WeaponSkill("ice_sword")
    {
        events << DamageCaused;
    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (!WeaponSkill::triggerable(player)) return QStringList();
        DamageStruct damage = data.value<DamageStruct>();
        if (damage.card && damage.card->isKindOf("Slash") && damage.by_user && !damage.chain && !damage.transfer
                && damage.to->getMark("Equips_of_Others_Nullified_to_You") == 0 && !damage.to->isNude())
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        DamageStruct damage = data.value<DamageStruct>();

        if (player->askForSkillInvoke("ice_sword", data)) {
            room->setEmotion(player, "weapon/ice_sword");

            room->preventDamage(damage);

            if (damage.from->canDiscard(damage.to, "he")) {
                int card_id = room->askForCardChosen(player, damage.to, "he", "ice_sword", false, Card::MethodDiscard);
                room->throwCard(Sanguosha->getCard(card_id), damage.to, damage.from);

                if (damage.from->isAlive() && damage.to->isAlive() && damage.from->canDiscard(damage.to, "he")) {
                    card_id = room->askForCardChosen(player, damage.to, "he", "ice_sword", false, Card::MethodDiscard);
                    room->throwCard(Sanguosha->getCard(card_id), damage.to, damage.from);
                }
            }
            return true;
        }
        return false;
    }
};

IceSword::IceSword(Suit suit, int number)
    : Weapon(suit, number, 2)
{
    setObjectName("ice_sword");
}

class RenwangShieldSkill : public ArmorSkill
{
public:
    RenwangShieldSkill() : ArmorSkill("renwang_shield")
    {
        events << CardEffected;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (!ArmorSkill::triggerable(player)) return QStringList();
        CardEffectStruct effect = data.value<CardEffectStruct>();
        if (effect.card->isKindOf("Slash") && effect.card->isBlack())
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        CardEffectStruct effect = data.value<CardEffectStruct>();

        room->sendCompulsoryTriggerLog(player, objectName());

        room->setEmotion(player, "armor/renwang_shield");
        effect.to->setFlags("Global_NonSkillNullify");

        effect.nullified = true;

        data = QVariant::fromValue(effect);

        return false;
    }
};

RenwangShield::RenwangShield(Suit suit, int number)
    : Armor(suit, number)
{
    setObjectName("renwang_shield");
}

class HorseSkill : public DistanceSkill
{
public:
    HorseSkill() : DistanceSkill("horse")
    {
    }

    virtual int getCorrect(const Player *from, const Player *to) const
    {
        int correct = 0;
        const Horse *horse = NULL;
        if (from->getOffensiveHorse() && from->getMark("Equips_Nullified_to_Yourself") == 0) {
            horse = qobject_cast<const Horse *>(from->getOffensiveHorse()->getRealCard());
            correct += horse->getCorrect();
        }
        if (to->getDefensiveHorse() && to->getMark("Equips_Nullified_to_Yourself") == 0) {
            horse = qobject_cast<const Horse *>(to->getDefensiveHorse()->getRealCard());
            correct += horse->getCorrect();
        }

        return correct;
    }
};

WoodenOxCard::WoodenOxCard()
{
    target_fixed = true;
    will_throw = false;
    handling_method = Card::MethodNone;
    m_skillName = "wooden_ox";
}

void WoodenOxCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const
{
    source->addToPile("wooden_ox", subcards, false);

    const Card *treasure = source->getTreasure();
    if (treasure) {
        QList<ServerPlayer *> targets;
        foreach (ServerPlayer *p, room->getOtherPlayers(source)) {
            if (p->canSetEquip(treasure))
                targets << p;
        }
        if (targets.isEmpty()) return;

        ServerPlayer *target = room->askForPlayerChosen(source, targets, "wooden_ox_move", "@wooden_ox-move", true);
        if (target) {
            room->moveCardTo(treasure, source, target, Player::PlaceEquip,
            CardMoveReason(CardMoveReason::S_REASON_TRANSFER, source->objectName(), "wooden_ox", QString()));
        }
    }

}

class WoodenOxViewAsSkill : public OneCardViewAsSkill
{
public:
    WoodenOxViewAsSkill() : OneCardViewAsSkill("wooden_ox")
    {
        filter_pattern = ".|.|.|hand";
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("WoodenOxCard") && player->getPile("wooden_ox").length() < 5;
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        WoodenOxCard *card = new WoodenOxCard;
        card->addSubcard(originalCard);
        card->setSkillName("wooden_ox");
        return card;
    }
};

class WoodenOxSkill : public TreasureSkill
{
public:
    WoodenOxSkill() : TreasureSkill("wooden_ox")
    {
        events << PreCardsMoveOneTime;
        view_as_skill = new WoodenOxViewAsSkill;
    }

    virtual QStringList triggerable(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        QVariantList move_datas = data.toList();
        foreach(QVariant move_data, move_datas) {
            CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
            for (int i = 0; i < move.card_ids.size(); i++) {
                const Card *card = Sanguosha->getEngineCard(move.card_ids[i]);
                if (card->objectName() == "wooden_ox") {
                    if (move.from_places[i] == Player::PlaceEquip) {
                        ServerPlayer *from = (ServerPlayer *) move.from;
                        if (from && from == player && !from->getPile("wooden_ox").isEmpty()) return QStringList("wooden_ox!");

                    } else if (move.from_places[i] == Player::PlaceTable) {
                        QVariantList record = room->getTag("wooden_ox_temp").toList();
                        foreach (QVariant card_data, record) {
                            int card_id = card_data.toInt();
                            if (room->getCardPlace(card_id) == Player::PlaceTable)
                                return QStringList("wooden_ox!");
                        }
                    }
                    break;
                }
            }
        }

        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *, QVariant &data, ServerPlayer *) const
    {
        QVariantList move_datas = data.toList();
        foreach(QVariant move_data, move_datas) {
            CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
            for (int i = 0; i < move.card_ids.size(); i++) {
                const Card *card = Sanguosha->getEngineCard(move.card_ids[i]);
                if (card->objectName() == "wooden_ox") {
                    if (move.from_places[i] == Player::PlaceEquip) {
                        ServerPlayer *player = (ServerPlayer *)move.from;
                        if (!player || player->getPile("wooden_ox").isEmpty()) return false;
                        ServerPlayer *to = (ServerPlayer *)move.to;
                        if (to && to->getTreasure() && to->getTreasure()->objectName() == "wooden_ox"
                            && move.to_place == Player::PlaceEquip) {
                            QList<ServerPlayer *> p_list;
                            p_list << to;
                            to->addToPile("wooden_ox", player->getPile("wooden_ox"), false, p_list);
                        } else if (move.to_place == Player::PlaceTable && move.reason.m_reason == CardMoveReason::S_REASON_SWAP) {
                            room->setTag("wooden_ox_temp", IntList2VariantList(player->getPile("wooden_ox")));
                            CardsMoveStruct move(player->getPile("wooden_ox"), NULL, Player::PlaceTable,
                                CardMoveReason(CardMoveReason::S_REASON_SECRETLY_PUT, player->objectName(), "wooden_ox", QString()));
                            room->moveCardsAtomic(move, false);
                        } else {
                            player->clearOnePrivatePile("wooden_ox");
                        }
                    } else if (move.from_places[i] == Player::PlaceTable) {
                        QVariantList record = room->getTag("wooden_ox_temp").toList();
                        QList<int> cardsToGet;
                        foreach (QVariant card_data, record) {
                            int card_id = card_data.toInt();
                            if (room->getCardPlace(card_id) == Player::PlaceTable)
                                cardsToGet << card_id;
                        }
                        if (cardsToGet.isEmpty()) return false;
                        ServerPlayer *to = (ServerPlayer *)move.to;
                        if (to && to->getTreasure() && to->getTreasure()->objectName() == "wooden_ox"
                            && move.to_place == Player::PlaceEquip) {
                            QList<ServerPlayer *> p_list;
                            p_list << to;
                            to->addToPile("wooden_ox", cardsToGet, false, p_list);
                        } else {
                            DummyCard *dummy = new DummyCard(cardsToGet);
                            dummy->deleteLater();
                            CardMoveReason reason(CardMoveReason::S_REASON_NATURAL_ENTER, QString());
                            room->throwCard(dummy, reason, NULL);
                        }
                    }
                    return false;
                }
            }
        }
        return false;
    }
};

WoodenOx::WoodenOx(Suit suit, int number)
    : Treasure(suit, number)
{
    setObjectName("wooden_ox");
}

void WoodenOx::onUninstall(ServerPlayer *player) const
{
    player->getRoom()->addPlayerHistory(player, "WoodenOxCard", 0);
    Treasure::onUninstall(player);
}

StandardCardPackage::StandardCardPackage()
    : Package("standard_cards", Package::CardPack)
{
    QList<Card *> cards;

    cards << new Slash(Card::Spade, 7)
        << new Slash(Card::Spade, 8)
        << new Slash(Card::Spade, 8)
        << new Slash(Card::Spade, 9)
        << new Slash(Card::Spade, 9)
        << new Slash(Card::Spade, 10)
        << new Slash(Card::Spade, 10)

        << new Slash(Card::Club, 2)
        << new Slash(Card::Club, 3)
        << new Slash(Card::Club, 4)
        << new Slash(Card::Club, 5)
        << new Slash(Card::Club, 6)
        << new Slash(Card::Club, 7)
        << new Slash(Card::Club, 8)
        << new Slash(Card::Club, 8)
        << new Slash(Card::Club, 9)
        << new Slash(Card::Club, 9)
        << new Slash(Card::Club, 10)
        << new Slash(Card::Club, 10)
        << new Slash(Card::Club, 11)
        << new Slash(Card::Club, 11)

        << new Slash(Card::Heart, 10)
        << new Slash(Card::Heart, 10)
        << new Slash(Card::Heart, 11)

        << new Slash(Card::Diamond, 6)
        << new Slash(Card::Diamond, 7)
        << new Slash(Card::Diamond, 8)
        << new Slash(Card::Diamond, 9)
        << new Slash(Card::Diamond, 10)
        << new Slash(Card::Diamond, 13)

        << new Jink(Card::Heart, 2)
        << new Jink(Card::Heart, 2)
        << new Jink(Card::Heart, 13)

        << new Jink(Card::Diamond, 2)
        << new Jink(Card::Diamond, 2)
        << new Jink(Card::Diamond, 3)
        << new Jink(Card::Diamond, 4)
        << new Jink(Card::Diamond, 5)
        << new Jink(Card::Diamond, 6)
        << new Jink(Card::Diamond, 7)
        << new Jink(Card::Diamond, 8)
        << new Jink(Card::Diamond, 9)
        << new Jink(Card::Diamond, 10)
        << new Jink(Card::Diamond, 11)
        << new Jink(Card::Diamond, 11)

        << new Peach(Card::Heart, 3)
        << new Peach(Card::Heart, 4)
        << new Peach(Card::Heart, 6)
        << new Peach(Card::Heart, 7)
        << new Peach(Card::Heart, 8)
        << new Peach(Card::Heart, 9)
        << new Peach(Card::Heart, 12)

        << new Peach(Card::Diamond, 12)

        << new Crossbow(Card::Club)
        << new Crossbow(Card::Diamond)
        << new DoubleSword
        << new QinggangSword
        << new Blade
        << new Spear
        << new Axe
        << new Halberd
        << new KylinBow

        << new EightDiagram(Card::Spade)
        << new EightDiagram(Card::Club);

    skills << new CrossbowSkill << new DoubleSwordSkill << new QinggangSwordSkill
        << new BladeSkill << new SpearSkill << new AxeSkill
        << new KylinBowSkill << new EightDiagramSkill
        << new HalberdSkill;

    QList<Card *> horses;
    horses << new DefensiveHorse(Card::Spade, 5)
        << new DefensiveHorse(Card::Club, 5)
        << new DefensiveHorse(Card::Heart, 13)
        << new OffensiveHorse(Card::Heart, 5)
        << new OffensiveHorse(Card::Spade, 13)
        << new OffensiveHorse(Card::Diamond, 13);

    horses.at(0)->setObjectName("jueying");
    horses.at(1)->setObjectName("dilu");
    horses.at(2)->setObjectName("zhuahuangfeidian");
    horses.at(3)->setObjectName("chitu");
    horses.at(4)->setObjectName("dayuan");
    horses.at(5)->setObjectName("zixing");

    cards << horses;

    skills << new HorseSkill;

    cards << new AmazingGrace(Card::Heart, 3)
        << new AmazingGrace(Card::Heart, 4)
        << new GodSalvation
        << new SavageAssault(Card::Spade, 7)
        << new SavageAssault(Card::Spade, 13)
        << new SavageAssault(Card::Club, 7)
        << new ArcheryAttack
        << new Duel(Card::Spade, 1)
        << new Duel(Card::Club, 1)
        << new Duel(Card::Diamond, 1)
        << new ExNihilo(Card::Heart, 7)
        << new ExNihilo(Card::Heart, 8)
        << new ExNihilo(Card::Heart, 9)
        << new ExNihilo(Card::Heart, 11)
        << new Snatch(Card::Spade, 3)
        << new Snatch(Card::Spade, 4)
        << new Snatch(Card::Spade, 11)
        << new Snatch(Card::Diamond, 3)
        << new Snatch(Card::Diamond, 4)
        << new Dismantlement(Card::Spade, 3)
        << new Dismantlement(Card::Spade, 4)
        << new Dismantlement(Card::Spade, 12)
        << new Dismantlement(Card::Club, 3)
        << new Dismantlement(Card::Club, 4)
        << new Dismantlement(Card::Heart, 12)
        << new Collateral(Card::Club, 12)
        << new Collateral(Card::Club, 13)
        << new Nullification(Card::Spade, 11)
        << new Nullification(Card::Club, 12)
        << new Nullification(Card::Club, 13)
        << new Indulgence(Card::Spade, 6)
        << new Indulgence(Card::Club, 6)
        << new Indulgence(Card::Heart, 6)
        << new Lightning(Card::Spade, 1);

    foreach(Card *card, cards)
        card->setParent(this);
}

StandardExCardPackage::StandardExCardPackage()
    : Package("standard_ex_cards", Package::CardPack)
{
    QList<Card *> cards;
    cards << new IceSword(Card::Spade, 2)
        << new RenwangShield(Card::Club, 2)
        << new Lightning(Card::Heart, 12)
        << new Nullification(Card::Diamond, 12);

    skills << new RenwangShieldSkill << new IceSwordSkill;

    foreach(Card *card, cards)
        card->setParent(this);
}

LimitationBrokenPackage::LimitationBrokenPackage()
    : Package("limitation_broken", Package::CardPack)
{
    QList<Card *> cards;
    cards << new WoodenOx(Card::Diamond, 5);

    skills << new WoodenOxSkill;

    foreach(Card *card, cards)
        card->setParent(this);

    addMetaObject<WoodenOxCard>();
}

ADD_PACKAGE(StandardCard)
ADD_PACKAGE(StandardExCard)
ADD_PACKAGE(LimitationBroken)

