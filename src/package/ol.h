#ifndef OL_PACKAGE_H
#define OL_PACKAGE_H

#include "package.h"
#include "card.h"
#include "standard.h"

class OLPackage : public Package
{
    Q_OBJECT

public:
    OLPackage();
};

class AocaiCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE AocaiCard();

    virtual bool targetFixed() const;
    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual bool targetsFeasible(const QList<const Player *> &targets, const Player *Self) const;

    virtual const Card *validateInResponse(ServerPlayer *user) const;
    virtual const Card *validate(CardUseStruct &cardUse) const;
};

class DuwuCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE DuwuCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class ShefuCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE ShefuCard();
    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class GusheCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE GusheCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class LianzhuCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE LianzhuCard();
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class LianjiCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE LianjiCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual bool targetsFeasible(const QList<const Player *> &targets, const Player *Self) const;
    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class SmileDagger : public SingleTargetTrick
{
    Q_OBJECT

public:
    Q_INVOKABLE SmileDagger(Card::Suit suit, int number);

    virtual void onEffect(const CardEffectStruct &effect) const;
};

class HoneyTrap : public SingleTargetTrick
{
    Q_OBJECT

public:
    Q_INVOKABLE HoneyTrap(Card::Suit suit, int number);

    virtual bool targetRated(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class JingongCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE JingongCard();

    bool targetFixed() const;
    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    bool targetsFeasible(const QList<const Player *> &targets, const Player *Self) const;

    const Card *validate(CardUseStruct &card_use) const;
};

class QianyaCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE QianyaCard();

    virtual void onEffect(const CardEffectStruct &effect) const;
};

class CanshiiCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE CanshiiCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class JianjieCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE JianjieCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual bool targetsFeasible(const QList<const Player *> &targets, const Player *Self) const;
    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class YeyaniCard : public SkillCard
{
    Q_OBJECT

public:
    void damage(ServerPlayer *shenzhouyu, ServerPlayer *target, int point) const;
};

class GreatYeyaniCard : public YeyaniCard
{
    Q_OBJECT

public:
    Q_INVOKABLE GreatYeyaniCard();

    virtual bool targetFilter(const QList<const Player *> &targets,const Player *to_select, const Player *Self) const;
    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select,const Player *Self, int &maxVotes) const;
    virtual bool targetsFeasible(const QList<const Player *> &targets, const Player *Self) const;
    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class SmallYeyaniCard : public YeyaniCard
{
    Q_OBJECT

public:
    Q_INVOKABLE SmallYeyaniCard();
    virtual bool targetsFeasible(const QList<const Player *> &targets, const Player *Self) const;
    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

#endif
