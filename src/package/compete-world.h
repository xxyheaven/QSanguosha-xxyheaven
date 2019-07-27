#ifndef COMPETEWORLD
#define COMPETEWORLD

#include "standard.h"



class Demobilizing : public SingleTargetTrick
{
    Q_OBJECT

public:
    Q_INVOKABLE Demobilizing(Card::Suit suit, int number);

    virtual bool targetRated(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class FloweringTree : public SingleTargetTrick
{
    Q_OBJECT

public:
    Q_INVOKABLE FloweringTree(Card::Suit suit, int number);
    virtual QList<ServerPlayer *> defaultTargets(Room *room, ServerPlayer *source) const;
    virtual bool targetRated(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
    virtual bool isAvailable(const Player *player) const;
};


class BrokenHalberd : public Weapon
{
    Q_OBJECT

public:
    Q_INVOKABLE BrokenHalberd(Card::Suit suit = Club, int number = 5);
};

class WomenDress : public Armor
{
    Q_OBJECT

public:
    Q_INVOKABLE WomenDress(Card::Suit suit = Heart, int number = 10);

};






class QianxiniCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE QianxiniCard();
    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class TunanCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE TunanCard();
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class FuhaiCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE FuhaiCard();
    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};



class CompeteWorldCardPackage : public Package
{
    Q_OBJECT

public:
    CompeteWorldCardPackage();
};

class CompeteWorldPackage : public Package
{
    Q_OBJECT

public:
    CompeteWorldPackage();
};



#endif // COMPETEWORLD

