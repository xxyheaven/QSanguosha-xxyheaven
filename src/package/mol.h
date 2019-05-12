#ifndef MOL_PACKAGE_H
#define MOL_PACKAGE_H

#include "package.h"
#include "card.h"
#include "standard.h"

class MOLPackage : public Package
{
    Q_OBJECT

public:
    MOLPackage();
};

class DerivativeCardPackage : public Package
{
    Q_OBJECT

public:
    DerivativeCardPackage();
};

class ZhanyiViewAsBasicCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE ZhanyiViewAsBasicCard();

    virtual bool targetFixed() const;
    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual bool targetsFeasible(const QList<const Player *> &targets, const Player *Self) const;

    virtual const Card *validate(CardUseStruct &cardUse) const;
    virtual const Card *validateInResponse(ServerPlayer *user) const;
};

class ZhanyiCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE ZhanyiCard();

    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class Catapult : public Weapon
{
    Q_OBJECT

public:
    Q_INVOKABLE Catapult(Card::Suit suit = Diamond, int number = 9);
};

class PingcaiCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE PingcaiCard();

    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class PingcaiMoveCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE PingcaiMoveCard();

    virtual bool targetsFeasible(const QList<const Player *> &targets, const Player *Self) const;
    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onUse(Room *room, const CardUseStruct &use) const;
};

#endif


