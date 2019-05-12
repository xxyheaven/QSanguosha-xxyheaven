#ifndef _YJCM2017_H
#define _YJCM2017_H

#include "package.h"
#include "card.h"

class YJCM2017Package : public Package
{
    Q_OBJECT

public:
    YJCM2017Package();
};

class ZhongjianCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE ZhongjianCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class QingxianCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE QingxianCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual bool targetsFeasible(const QList<const Player *> &targets, const Player *Self) const;
    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class CanyunCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE CanyunCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual bool targetsFeasible(const QList<const Player *> &targets, const Player *Self) const;
    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

#endif
