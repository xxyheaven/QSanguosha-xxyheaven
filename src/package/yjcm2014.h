#ifndef _YJCM2014_H
#define _YJCM2014_H

#include "package.h"
#include "card.h"

class DingpinCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE DingpinCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onUse(Room *room, const CardUseStruct &card_use) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class ShenxingCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE ShenxingCard();
    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class XianzhouCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE XianzhouCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
	virtual void extraCost(Room *room, const CardUseStruct &card_use) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class XianzhouDamageCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE XianzhouDamageCard();

    virtual void onUse(Room *room, const CardUseStruct &use) const;
    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
};

class PingkouCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE PingkouCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class YJCM2014Package : public Package
{
    Q_OBJECT

public:
    YJCM2014Package();
};

#endif
