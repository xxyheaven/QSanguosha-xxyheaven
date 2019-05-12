#include "aux-skills.h"
#include "clientplayer.h"
#include "nostalgia.h"
#include "engine.h"

DiscardSkill::DiscardSkill()
    : ViewAsSkill("discard"), card(new DummyCard),
    num(0), include_equip(false), is_discard(true), is_gamerule(false)
{
    card->setParent(this);
}

void DiscardSkill::setNum(int num)
{
    this->num = num;
}

void DiscardSkill::setMinNum(int minnum)
{
    this->minnum = minnum;
}

void DiscardSkill::setIncludeEquip(bool include_equip)
{
    this->include_equip = include_equip;
}

void DiscardSkill::setIsDiscard(bool is_discard)
{
    this->is_discard = is_discard;
}

void DiscardSkill::setIsGamerule(bool is_gamerule)
{
    this->is_gamerule = is_gamerule;
}

void DiscardSkill::setPattern(const QString &pattern)
{
    this->pattern = pattern;
}

bool DiscardSkill::viewFilter(const QList<const Card *> &selected, const Card *card) const
{
    if (selected.length() >= num)
        return false;

    if (!Sanguosha->matchExpPattern(pattern, Self, card))
        return false;

    if (!include_equip && card->isEquipped())
        return false;

    if (is_discard && Self->isCardLimited(card, Card::MethodDiscard))
        return false;

    if (is_gamerule && Sanguosha->isCardHided(Self, card))
        return false;

    return true;
}

const Card *DiscardSkill::viewAs(const QList<const Card *> &cards) const
{
    if (cards.length() >= minnum) {
        card->clearSubcards();
        card->addSubcards(cards);
        return card;
    } else
        return NULL;
}

// -------------------------------------------

ResponseSkill::ResponseSkill()
    : OneCardViewAsSkill("response-skill")
{
    request = Card::MethodResponse;
}

void ResponseSkill::setPattern(const QString &pattern)
{
    this->pattern = Sanguosha->getPattern(pattern);
}

void ResponseSkill::setRequest(const Card::HandlingMethod request)
{
    this->request = request;
}

bool ResponseSkill::matchPattern(const Player *player, const Card *card) const
{
    if (request != Card::MethodNone && player->isCardLimited(card, request))
        return false;
    if (pattern) {
        QString pat = pattern->getPatternString();
        if ((request == Card::MethodUse || request == Card::MethodResponse) && pat.contains("hand")) {
            pat.replace("hand", player->getHandPileList().join(","));
        }
        ExpPattern exp_pattern(pat);
        return exp_pattern.match(player, card);
    }
    return false;
}

bool ResponseSkill::viewFilter(const Card *card) const
{
    return matchPattern(Self, card);
}

const Card *ResponseSkill::viewAs(const Card *originalCard) const
{
    return originalCard;
}

// -------------------------------------------

ShowOrPindianSkill::ShowOrPindianSkill()
{
    setObjectName("showorpindian-skill");
    request = Card::MethodNone;
}

bool ShowOrPindianSkill::matchPattern(const Player *player, const Card *card) const
{
    return pattern && pattern->match(player, card);
}

// -------------------------------------------

class NosYijiCard : public NosRendeCard
{
public:
    NosYijiCard()
    {
        target_fixed = false;
    }

    void setPlayerNames(const QStringList &names)
    {
        set = names.toSet();
    }

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *) const
    {
        return targets.isEmpty() && set.contains(to_select->objectName());
    }

private:
    QSet<QString> set;
};

NosYijiViewAsSkill::NosYijiViewAsSkill()
    : ViewAsSkill("askforyiji")
{
    card = new NosYijiCard;
    card->setParent(this);
}

void NosYijiViewAsSkill::initialize(const QString &card_str, int max_num, const QStringList &player_names, const QString &expand_pile)
{
    QStringList cards = card_str.split("+");
    ids = StringList2IntList(cards);

    this->max_num = max_num;

    card->setPlayerNames(player_names);

    this->expand_pile = expand_pile;
}

bool NosYijiViewAsSkill::viewFilter(const QList<const Card *> &selected, const Card *card) const
{
    return ids.contains(card->getId()) && selected.length() < max_num;
}

const Card *NosYijiViewAsSkill::viewAs(const QList<const Card *> &cards) const
{
    if (cards.isEmpty() || cards.length() > max_num)
        return NULL;

    card->clearSubcards();
    card->addSubcards(cards);
    return card;
}

// ------------------------------------------------

class ChoosePlayerCard : public DummyCard
{
public:
    ChoosePlayerCard()
    {
        target_fixed = false;
    }

    void setPlayerNames(const QStringList &names, int max, int min)
    {
        set = names.toSet();
        this->max = max;
        this->min = min;
    }

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *) const
    {
        return targets.length() < max && set.contains(to_select->objectName());
    }

    virtual bool targetsFeasible(const QList<const Player *> &targets, const Player *) const
    {
        return targets.length() >= min && !targets.isEmpty();
    }

private:
    QSet<QString> set;
    int max;
    int min;
};

ChoosePlayerSkill::ChoosePlayerSkill()
    : ZeroCardViewAsSkill("choose_player")
{
    card = new ChoosePlayerCard;
    card->setParent(this);
}

void ChoosePlayerSkill::setPlayerNames(const QStringList &names, int max, int min)
{
    card->setPlayerNames(names, max, min);
}

const Card *ChoosePlayerSkill::viewAs() const
{
    return card;
}

