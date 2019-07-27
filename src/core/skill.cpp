#include "skill.h"
#include "settings.h"
#include "engine.h"
#include "player.h"
#include "room.h"
#include "client.h"
#include "standard.h"
#include "scenario.h"
#include "guhuodialog.h"

#include <QFile>

Skill::Skill(const QString &name, Frequency frequency)
    : frequency(frequency), limit_mark(QString()), lord_skill(false), attached_lord_skill(false), owner(QString())
{
    static QChar lord_symbol('$');
    static QChar attached_lord_symbol('&');

    if (name.endsWith(lord_symbol)) {
        QString copy = name;
        copy.remove(lord_symbol);
        setObjectName(copy);
        lord_skill = true;
    } else if (name.endsWith(attached_lord_symbol)) {
        QString copy = name;
        copy.remove(attached_lord_symbol);
        setObjectName(copy);
        attached_lord_skill = true;
    } else {
        setObjectName(name);
    }
}

bool Skill::isLordSkill() const
{
    return lord_skill;
}

bool Skill::isAttachedLordSkill() const
{
    return attached_lord_skill;
}

bool Skill::shouldBeVisible(const Player *Self) const
{
    return Self != NULL;
}

QString Skill::getDescription() const
{
    bool normal_game = ServerInfo.DuringGame && (isNormalGameMode(ServerInfo.GameMode) || ServerInfo.GameMode == "08_zdyj");
    QString name = QString("%1%2").arg(objectName()).arg(normal_game ? "_p" : "");
    QString des_src = Sanguosha->translate(":" + name);
    if (normal_game && des_src.startsWith(":"))
        des_src = Sanguosha->translate(":" + objectName());
    if (des_src.startsWith(":"))
        return QString();
    if (des_src.startsWith("[NoAutoRep]"))
        return des_src.mid(11);

    if (Config.value("AutoSkillTypeColorReplacement", true).toBool()) {
        QMap<QString, QColor> skilltype_color_map = Sanguosha->getSkillTypeColorMap();
        foreach (QString skill_type, skilltype_color_map.keys()) {
            QString type_name = Sanguosha->translate(skill_type);
            QString color_name = skilltype_color_map[skill_type].name();
            des_src.replace(type_name, QString("<font color=%1><b>%2</b></font>").arg(color_name).arg(type_name));
        }
    }
    if (Config.value("AutoSuitReplacement", true).toBool()) {
        for (int i = 0; i <= 3; i++) {
            Card::Suit suit = (Card::Suit)i;
            QString suit_name = Sanguosha->translate(Card::Suit2String(suit));
            QString suit_char = Sanguosha->translate(Card::Suit2String(suit) + "_char");
            QString colored_suit_char;
            if (i < 2)
                colored_suit_char = suit_char;
            else
                colored_suit_char = QString("<font color=#FF0000>%1</font>").arg(suit_char);
            des_src.replace(suit_char, colored_suit_char);
            des_src.replace(suit_name, colored_suit_char);
        }
    }
    return des_src;
}

QString Skill::getNotice(int index) const
{
    if (index == -1)
        return Sanguosha->translate("~" + objectName());

    return Sanguosha->translate(QString("~%1%2").arg(objectName()).arg(index));
}

bool Skill::isVisible() const
{
    return !objectName().startsWith("#");
}

int Skill::getEffectIndex(const ServerPlayer *, const Card *) const
{
    return -1;
}

int Skill::getEffectIndex(const ServerPlayer *, const QString &) const
{
    return -1;
}

void Skill::initMediaSource()
{
    sources.clear();
    for (int i = 1;; i++) {
        QString effect_file = QString("audio/skill/%1%2.ogg").arg(objectName()).arg(QString::number(i));
        if (QFile::exists(effect_file))
            sources << effect_file;
        else
            break;
    }

    if (sources.isEmpty()) {
        QString effect_file = QString("audio/skill/%1.ogg").arg(objectName());
        if (QFile::exists(effect_file))
            sources << effect_file;
    }
}

void Skill::playAudioEffect(int index, bool superpose) const
{
    if (!sources.isEmpty()) {
        if (index == -1)
            index = qrand() % sources.length();
        else
            index--;

        // check length
        QString filename;
        if (index >= 0 && index < sources.length())
            filename = sources.at(index);
        else if (index >= sources.length()) {
            while (index >= sources.length())
                index -= sources.length();
            filename = sources.at(index);
        } else
            filename = sources.first();

        Sanguosha->playAudioEffect(filename, superpose);
    }
}

Skill::Frequency Skill::getFrequency(const Player *) const
{
    return frequency;
}

QString Skill::getLimitMark() const
{
    return limit_mark;
}

void Skill::setOwner(QString general_name)
{
    owner = general_name;
}

QString Skill::getOwner() const
{
    return owner;
}

QStringList Skill::getInheritSkill() const
{
    return inherit_skills;
}

QStringList Skill::getSources() const
{
    return sources;
}

GuhuoDialog *Skill::getDialog() const
{
    return NULL;
}

QString Skill::getSelectBox() const
{
    return "";
}

bool Skill::buttonEnabled(const QString &button_name, const QList<const Card *> &, const QList<const Player *> &) const
{
    if (button_name.isEmpty()) {
        if (Sanguosha->currentRoomState()->getCurrentCardUseReason() != CardUseStruct::CARD_USE_REASON_PLAY) {
            QString pattern = Sanguosha->currentRoomState()->getCurrentCardUsePattern();
            if (pattern.startsWith(".") || pattern.startsWith("@"))
                return false;
        }
        return true;
    }

    const Card *card = Sanguosha->cloneCard(button_name, Card::NoSuit, 0);
    if (card == NULL)
        return true;
    if (Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_PLAY)
        return !Self->isCardLimited(card, Card::MethodUse, true) && card->isAvailable(Self);
    else {
        QString pattern = Sanguosha->currentRoomState()->getCurrentCardUsePattern();
        if (pattern.startsWith(".") || pattern.startsWith("@"))
            return false;
        if (pattern == "slash") {
            return card->isKindOf("Slash");
        } else
            return pattern.contains(button_name);
    }
    return false;
}

ViewAsSkill::ViewAsSkill(const QString &name)
    : Skill(name), response_pattern(QString()), response_or_use(false), expand_pile(QString()), guhuo_type(QString()), guhuo_dialog_type(QString())
{
}

bool ViewAsSkill::isAvailable(const Player *invoker,
    CardUseStruct::CardUseReason reason,
    const QString &pattern) const
{
    if (!invoker->hasSkill(this) && !invoker->hasLordSkill(this)
            && invoker->getMark("ViewAsSkill_" + objectName() + "Effect") == 0   // For skills like Shuangxiong(ViewAsSkill effect remains even if the player has lost the skill)
            && !invoker->hasFlag("RoomScene_" + objectName() + "TempUse")) // for RoomScene Temp Use
        return false;
    switch (reason) {
    case CardUseStruct::CARD_USE_REASON_PLAY: return isEnabledAtPlay(invoker);
    case CardUseStruct::CARD_USE_REASON_RESPONSE:
    case CardUseStruct::CARD_USE_REASON_RESPONSE_USE: return isEnabledAtResponse(invoker, pattern);
    default:
        return false;
    }
}

bool ViewAsSkill::isEnabledAtPlay(const Player *) const
{
    return response_pattern.isEmpty();
}

bool ViewAsSkill::isEnabledAtResponse(const Player *, const QString &pattern) const
{
    if (!response_pattern.isEmpty())
        return pattern == response_pattern;
    return false;
}

bool ViewAsSkill::isEnabledAtNullification(const ServerPlayer *) const
{
    return false; // return response_pattern.contains("nullification");
}

const ViewAsSkill *ViewAsSkill::parseViewAsSkill(const Skill *skill)
{
    if (skill == NULL) return NULL;
    if (skill->inherits("ViewAsSkill")) {
        const ViewAsSkill *view_as_skill = qobject_cast<const ViewAsSkill *>(skill);
        return view_as_skill;
    }
    if (skill->inherits("TriggerSkill")) {
        const TriggerSkill *trigger_skill = qobject_cast<const TriggerSkill *>(skill);
        Q_ASSERT(trigger_skill != NULL);
        const ViewAsSkill *view_as_skill = trigger_skill->getViewAsSkill();
        if (view_as_skill != NULL) return view_as_skill;
    }
    return NULL;
}

ZeroCardViewAsSkill::ZeroCardViewAsSkill(const QString &name)
    : ViewAsSkill(name)
{
}

const Card *ZeroCardViewAsSkill::viewAs(const QList<const Card *> &cards) const
{
    if (cards.isEmpty())
        return viewAs();
    else
        return NULL;
}

bool ZeroCardViewAsSkill::viewFilter(const QList<const Card *> &, const Card *) const
{
    return false;
}

OneCardViewAsSkill::OneCardViewAsSkill(const QString &name)
    : ViewAsSkill(name), filter_pattern(QString())
{
}

bool OneCardViewAsSkill::viewFilter(const QList<const Card *> &selected, const Card *to_select) const
{
    return selected.isEmpty() && !to_select->hasFlag("using") && viewFilter(to_select);
}

bool OneCardViewAsSkill::viewFilter(const Card *to_select) const
{
    if (!inherits("FilterSkill") && !filter_pattern.isEmpty()) {
        QString pat = filter_pattern;
        if (pat.endsWith("!")) {
            if (Self->isJilei(to_select)) return false;
            pat.chop(1);
        } else if (response_or_use && pat.contains("hand")) {
            QStringList handlist;
            handlist.append("hand");
            foreach (const QString &pile, Self->getPileNames()) {
                if (pile.startsWith("&") || pile == "wooden_ox")
                    handlist.append(pile);
            }
            pat.replace("hand", handlist.join(","));
        }
        ExpPattern pattern(pat);
        return pattern.match(Self, to_select);
    }
    return false;
}

const Card *OneCardViewAsSkill::viewAs(const QList<const Card *> &cards) const
{
    if (cards.length() != 1)
        return NULL;
    else
        return viewAs(cards.first());
}

FilterSkill::FilterSkill(const QString &name)
    : OneCardViewAsSkill(name)
{
    frequency = Compulsory;
}

TriggerSkill::TriggerSkill(const QString &name)
    : Skill(name), view_as_skill(NULL), global(false), current_priority(0.0)
{
    priority.clear();
}

const ViewAsSkill *TriggerSkill::getViewAsSkill() const
{
    return view_as_skill;
}

QList<TriggerEvent> TriggerSkill::getTriggerEvents() const
{
    return events;
}

int TriggerSkill::getPriority(TriggerEvent) const
{
    return 3;
}

double TriggerSkill::getDynamicPriority(TriggerEvent e) const
{
    if (priority.keys().contains(e))
        return priority.key(e);
    else
        return this->getPriority(e);
}

TriggerList TriggerSkill::triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
{
    TriggerList skill_lists;
    if (objectName() == "game_rule") return skill_lists;
    ServerPlayer *ask_who = player;
    QStringList skill_list = triggerable(triggerEvent, room, player, data, ask_who);
    if (!skill_list.isEmpty())
        skill_lists.insert(ask_who, skill_list);
    return skill_lists;
}

bool TriggerSkill::triggerable(const ServerPlayer *target) const
{
    return target != NULL && (global || (target->isAlive() && target->hasSkill(this)));
}

void TriggerSkill::insertPriority(TriggerEvent e, double value)
{
    priority.insert(e, value);
}

void TriggerSkill::record(TriggerEvent, Room *, ServerPlayer *, QVariant &) const
{

}

QStringList TriggerSkill::triggerable(TriggerEvent, Room *, ServerPlayer *target, QVariant &, ServerPlayer* &) const
{
    if (triggerable(target))
        return QStringList(objectName());
    return QStringList();
}

bool TriggerSkill::trigger(TriggerEvent , Room *, ServerPlayer *, QVariant &) const
{
    return false;
}

bool TriggerSkill::effect(TriggerEvent e, Room *r, ServerPlayer *p, QVariant &d, ServerPlayer *) const
{
    return trigger(e, r, p, d);
}

ScenarioRule::ScenarioRule(Scenario *scenario)
    :TriggerSkill(scenario->objectName())
{
    setParent(scenario);
}

int ScenarioRule::getPriority(TriggerEvent) const
{
    return 0;
}

bool ScenarioRule::triggerable(const ServerPlayer *) const
{
    return true;
}

MasochismSkill::MasochismSkill(const QString &name)
    : TriggerSkill(name)
{
    events << Damaged;
}

bool MasochismSkill::trigger(TriggerEvent, Room *, ServerPlayer *player, QVariant &data) const
{
    DamageStruct damage = data.value<DamageStruct>();
    onDamaged(player, damage);

    return false;
}

PhaseChangeSkill::PhaseChangeSkill(const QString &name)
    : TriggerSkill(name)
{
    events << EventPhaseStart;
}

bool PhaseChangeSkill::trigger(TriggerEvent, Room *, ServerPlayer *player, QVariant &) const
{
    return onPhaseChange(player);
}

DrawCardsSkill::DrawCardsSkill(const QString &name, bool is_initial)
    : TriggerSkill(name), is_initial(is_initial)
{
    if (is_initial)
        events << DrawInitialCards;
    else
        events << DrawNCards;
}

bool DrawCardsSkill::trigger(TriggerEvent, Room *, ServerPlayer *player, QVariant &data) const
{
    int n = data.toInt();
    data = getDrawNum(player, n);
    return false;
}

GameStartSkill::GameStartSkill(const QString &name)
    : TriggerSkill(name)
{
    events << GameStart;
}

bool GameStartSkill::trigger(TriggerEvent, Room *, ServerPlayer *player, QVariant &) const
{
    onGameStart(player);
    return false;
}

ProhibitSkill::ProhibitSkill(const QString &name)
    : Skill(name, Skill::Compulsory)
{
}

DistanceSkill::DistanceSkill(const QString &name)
    : Skill(name, Skill::Compulsory)
{
}

MaxCardsSkill::MaxCardsSkill(const QString &name)
    : Skill(name, Skill::Compulsory)
{
}

int MaxCardsSkill::getExtra(const Player *) const
{
    return 0;
}

int MaxCardsSkill::getFixed(const Player *) const
{
    return -1;
}

TargetModSkill::TargetModSkill(const QString &name)
    : Skill(name, Skill::Compulsory)
{
    pattern = "Slash";
}

QString TargetModSkill::getPattern() const
{
    return pattern;
}

int TargetModSkill::getResidueNum(const Player *, const Card *, const Player *) const
{
    return 0;
}

int TargetModSkill::getDistanceLimit(const Player *, const Card *, const Player *) const
{
    return 0;
}

int TargetModSkill::getExtraTargetNum(const Player *, const Card *) const
{
    return 0;
}

SlashNoDistanceLimitSkill::SlashNoDistanceLimitSkill(const QString &skill_name)
    : TargetModSkill(QString("#%1-slash-ndl").arg(skill_name)), name(skill_name)
{
}

InvaliditySkill::InvaliditySkill(const QString &name)
    : Skill(name)
{
}

int SlashNoDistanceLimitSkill::getDistanceLimit(const Player *from, const Card *card, const Player *) const
{
    if (from->hasSkill(name) && card->getSkillName() == name)
        return 1000;
    else
        return 0;
}

AttackRangeSkill::AttackRangeSkill(const QString &name) : Skill(name, Skill::Compulsory)
{

}

int AttackRangeSkill::getExtra(const Player *, bool) const
{
    return 0;
}

int AttackRangeSkill::getFixed(const Player *, bool) const
{
    return -1;
}

DetachEffectSkill::DetachEffectSkill(const QString &skillname, const QString &pilename, const QString &markname)
    : TriggerSkill(QString("#%1-clear").arg(skillname)), name(skillname), pile_name(pilename), mark_name(markname)
{
    events << EventLoseSkill;
}

bool DetachEffectSkill::triggerable(const ServerPlayer *target) const
{
    return false;
}

void DetachEffectSkill::record(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data) const
{
    if (data.toString() == name) {
        if (!pile_name.isEmpty())
            player->clearOnePrivatePile(pile_name);
        if (!mark_name.isEmpty()) {
            foreach(ServerPlayer *p, room->getAllPlayers())
                room->setPlayerMark(p, mark_name, 0);
        }
        onSkillDetached(room, player);
    }
}

void DetachEffectSkill::onSkillDetached(Room *, ServerPlayer *) const
{
}

WeaponSkill::WeaponSkill(const QString &name)
    : TriggerSkill(name)
{
    global = true;
}

bool WeaponSkill::triggerable(const ServerPlayer *target) const
{
    if (target == NULL) return false;
    if (target->getMark("Equips_Nullified_to_Yourself") > 0) return false;
    return target->hasWeapon(objectName());
}

ArmorSkill::ArmorSkill(const QString &name)
    : TriggerSkill(name)
{
}

bool ArmorSkill::triggerable(const ServerPlayer *target) const
{
    if (target == NULL)
        return false;
    return target->hasArmorEffect(objectName());
}

bool ArmorSkill::cost(Room *room, ServerPlayer *target, QVariant &data) const
{
    bool has_skill = false;
    foreach (QString name, Sanguosha->getSkillNames()) {
        if (Sanguosha->getSkill(name)->inherits("ViewHasSkill")) {
            const ViewHasSkill *skill = qobject_cast<const ViewHasSkill *>(Sanguosha->getSkill(name));
            if (skill->ViewHas(target, objectName(), "armor") && target->hasSkill(skill)) {
                has_skill = true;
                const Skill *main_skill = Sanguosha->getMainSkill(skill->objectName());
                room->sendCompulsoryTriggerLog(target, main_skill->objectName());
                room->broadcastSkillInvoke(main_skill->objectName(), target);
            }
        }
    }
    if (has_skill || (target->getArmor() && target->getArmor()->objectName() == objectName())) {
        if (getFrequency() == Skill::Compulsory) {
            room->sendCompulsoryTriggerLog(target, objectName());
            return true;
        } else if (target->askForSkillInvoke(objectName(), data)) {
            return true;
        }
    }
    return false;
}

TreasureSkill::TreasureSkill(const QString &name)
    : TriggerSkill(name)
{
    global = true;
}

bool TreasureSkill::triggerable(const ServerPlayer *target) const
{
    if (target == NULL || target->getTreasure() == NULL)
        return false;
    return target->hasTreasure(objectName());
}

MarkAssignSkill::MarkAssignSkill(const QString &mark, int n)
    : GameStartSkill(QString("#%1-%2").arg(mark).arg(n)), mark_name(mark), n(n)
{
}

void MarkAssignSkill::onGameStart(ServerPlayer *player) const
{
    player->getRoom()->setPlayerMark(player, mark_name, player->getMark(mark_name) + n);
}

CardLimitedSkill::CardLimitedSkill(const QString &name)
    : Skill(name, Skill::Compulsory)
{
}

HideCardSkill::HideCardSkill(const QString &name)
    : Skill(name, Skill::Compulsory)
{
}

ViewHasSkill::ViewHasSkill(const QString &name)
    : Skill(name, Skill::Compulsory), global(false)
{
}
