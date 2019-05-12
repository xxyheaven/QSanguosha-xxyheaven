#ifndef SHADOW
#define SHADOW

#include "package.h"
#include "card.h"

class ShadowPackage : public Package
{
    Q_OBJECT

public:
    ShadowPackage();
};


class FeijunCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE FeijunCard();

    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const;
};

class KuizhuCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE KuizhuCard();
    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual bool targetsFeasible(const QList<const Player *> &targets, const Player *Self) const;
    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class ChenglveCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE ChenglveCard();

    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class ShenshiCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE ShenshiCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void extraCost(Room *room, const CardUseStruct &card_use) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class ZhenliangCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE ZhenliangCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void extraCost(Room *room, const CardUseStruct &card_use) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};















#endif // SHADOW

