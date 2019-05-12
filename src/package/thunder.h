#ifndef THUNDER
#define THUNDER

#include "package.h"
#include "card.h"

class ThunderPackage : public Package
{
    Q_OBJECT

public:
    ThunderPackage();
};

class JueyanCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE JueyanCard();

    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class HuairouCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE HuairouCard();
    void onUse(Room *room, const CardUseStruct &card_use) const;
};

class HongjuCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE HongjuCard();
    virtual void onUse(Room *room, const CardUseStruct &card_use) const;
};

class QingceCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE QingceCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class XiongluanCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE XiongluanCard();

    virtual void extraCost(Room *room, const CardUseStruct &card_use) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class CongjianCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE CongjianCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};















#endif // THUNDER

