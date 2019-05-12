#ifndef _SP_H
#define _SP_H

#include "package.h"
#include "card.h"
#include "standard.h"

class SPPackage : public Package
{
    Q_OBJECT

public:
    SPPackage();
};

class MiscellaneousPackage : public Package
{
    Q_OBJECT

public:
    MiscellaneousPackage();
};

class JiwuCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE JiwuCard();

    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class ShichouCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE ShichouCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class JianshuCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE JianshuCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual bool targetsFeasible(const QList<const Player *> &targets, const Player *Self) const;
	virtual void extraCost(Room *room, const CardUseStruct &card_use) const;
    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class YuanhuCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE YuanhuCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class BifaCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE BifaCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class SongciCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE SongciCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onUse(Room *room, const CardUseStruct &card_use) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class ZhoufuCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE ZhoufuCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class FenxunCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE FenxunCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class MumuCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE MumuCard();

    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const;
};

class XintanCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE XintanCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *, const Player *) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class LihunCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE LihunCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void extraCost(Room *room, const CardUseStruct &card_use) const;
	virtual void onEffect(const CardEffectStruct &effect) const;
};

class WeikuiCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE WeikuiCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void extraCost(Room *room, const CardUseStruct &card_use) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class LizhanCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE LizhanCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

#endif

