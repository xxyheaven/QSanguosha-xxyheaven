#ifndef _YJCM2015_H
#define _YJCM2015_H

#include "package.h"
#include "card.h"

class MingjianCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE MingjianCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
	virtual void extraCost(Room *room, const CardUseStruct &card_use) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class HuaiyiCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE HuaiyiCard();
	virtual void extraCost(Room *room, const CardUseStruct &card_use) const;
    void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class HuaiyiSnatchCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE HuaiyiSnatchCard();
    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    void onUse(Room *room, const CardUseStruct &card_use) const;
};

class ShifeiCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE ShifeiCard();

    virtual const Card *validateInResponse(ServerPlayer *user) const;
};

class ZhanjueCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE ZhanjueCard();
    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    void onUse(Room *room, const CardUseStruct &card_use) const;
};

class QinwangCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE QinwangCard();
    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    const Card *validate(CardUseStruct &cardUse) const;
};

class YanzhuCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE YanzhuCard();
    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    void onEffect(const CardEffectStruct &effect) const;
};

class XingxueCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE XingxueCard();
    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    void onEffect(const CardEffectStruct &effect) const;
};

class YanyuCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE YanyuCard();
    void onUse(Room *room, const CardUseStruct &card_use) const;
};

class FurongCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE FurongCard();

    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    void onEffect(const CardEffectStruct &effect) const;
};

class HuomoCard : public SkillCard
{
    Q_OBJECT 

public:
    Q_INVOKABLE HuomoCard();
    bool targetFixed() const;
    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    bool targetsFeasible(const QList<const Player *> &targets, const Player *Self) const;
    const Card *validate(CardUseStruct &cardUse) const;
    const Card *validateInResponse(ServerPlayer *user) const;
};

class AnguoCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE AnguoCard();
    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    void onEffect(const CardEffectStruct &effect) const;
};

class YJCM2015Package : public Package
{
    Q_OBJECT

public:
    YJCM2015Package();
};

#endif
