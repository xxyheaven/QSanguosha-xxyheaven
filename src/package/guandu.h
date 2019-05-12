#ifndef GUANDU
#define GUANDU

#include "standard.h"

class GuanduWarPackage : public Package
{
    Q_OBJECT

public:
    GuanduWarPackage();
};

class YuanlveCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE YuanlveCard();

    virtual void onEffect(const CardEffectStruct &effect) const;
};

class MoushiCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE MoushiCard();

    virtual void onEffect(const CardEffectStruct &effect) const;
};












#endif // GUANDU

