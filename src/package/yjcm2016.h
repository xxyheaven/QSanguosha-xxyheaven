#ifndef _YJCM2016_H
#define _YJCM2016_H

#include "package.h"
#include "card.h"

class YJCM2016Package : public Package
{
    Q_OBJECT

public:
    YJCM2016Package();
};

class ZhigeCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE ZhigeCard();

	virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class JisheCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE JisheCard();

    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class JisheChainCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE JisheChainCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class QinqingCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE QinqingCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class JiyuCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE JiyuCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class TaoluanCard : public SkillCard
{
    Q_OBJECT 

public:
    Q_INVOKABLE TaoluanCard();
    bool targetFixed() const;
    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    bool targetsFeasible(const QList<const Player *> &targets, const Player *Self) const;
    const Card *validate(CardUseStruct &cardUse) const;
    const Card *validateInResponse(ServerPlayer *user) const;
};

#endif
