#ifndef SPARK
#define SPARK

#include "package.h"
#include "card.h"
#include "standard.h"

class Spark1Package : public Package
{
    Q_OBJECT

public:
    Spark1Package();
};

class Spark2Package : public Package
{
    Q_OBJECT

public:
    Spark2Package();
};

class Spark3Package : public Package
{
    Q_OBJECT

public:
    Spark3Package();
};

class Spark4Package : public Package
{
    Q_OBJECT

public:
    Spark4Package();
};

class Spark5Package : public Package
{
    Q_OBJECT

public:
    Spark5Package();
};

class Spark6Package : public Package
{
    Q_OBJECT

public:
    Spark6Package();
};

class QiangwuCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE QiangwuCard();

    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class KuangbiCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE KuangbiCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class DingpanCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE DingpanCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class MizhaoCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE MizhaoCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class HuiminGraceCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE HuiminGraceCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};


class QujiCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE QujiCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *) const;
    virtual void extraCost(Room *room, const CardUseStruct &card_use) const;
    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class LimuCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE LimuCard();

    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const;
};

class TianbianCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE TianbianCard();

    virtual const Card *validateInResponse(ServerPlayer *user) const;
};

class JiaozhaoCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE JiaozhaoCard();

    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    bool targetsFeasible(const QList<const Player *> &targets, const Player *Self) const;
    void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class DenglouUseCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE DenglouUseCard();

    bool targetFixed() const;
    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    bool targetsFeasible(const QList<const Player *> &targets, const Player *Self) const;

    const Card *validate(CardUseStruct &card_use) const;
};

class TongboCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE TongboCard();
    virtual void onUse(Room *room, const CardUseStruct &card_use) const;
};

class JujianCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE JujianCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class GuolunCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE GuolunCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class JuesiCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE JuesiCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class JixuCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE JixuCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class DuliangCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE DuliangCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class ZiyuanCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE ZiyuanCard();

    virtual void extraCost(Room *room, const CardUseStruct &card_use) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class FumanCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE FumanCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void extraCost(Room *room, const CardUseStruct &card_use) const;
    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class DuanfaCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE DuanfaCard();
    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class ShanxiCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE ShanxiCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};

class KannanCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE KannanCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void onEffect(const CardEffectStruct &effect) const;
};


class XuejiCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE XuejiCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class WenguaCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE WenguaCard();

    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class WenguaAttachCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE WenguaAttachCard();

    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
};

class ZengdaoCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE ZengdaoCard();
    virtual void extraCost(Room *room, const CardUseStruct &card_use) const;
};

class LuanzhanCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE LuanzhanCard();

    virtual bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    virtual void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};





#endif // SPARK

