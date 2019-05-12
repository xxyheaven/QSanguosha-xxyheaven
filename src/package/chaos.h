#ifndef CHAOS
#define CHAOS

#include "package.h"
#include "card.h"
#include "standard.h"

class ChaosPackage : public Package
{
    Q_OBJECT

public:
    ChaosPackage();
};

class SidaoCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE SidaoCard();

    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    bool targetsFeasible(const QList<const Player *> &targets, const Player *Self) const;

    const Card *validate(CardUseStruct &card_use) const;
};

class TanbeiCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE TanbeiCard();
    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    void onEffect(const CardEffectStruct &effect) const;
};

class LvemingCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE LvemingCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class TunjunCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE TunjunCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void extraCost(Room *room, const CardUseStruct &card_use) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class XionghuoCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE XionghuoCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void extraCost(Room *room, const CardUseStruct &card_use) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

#endif // CHAOS

