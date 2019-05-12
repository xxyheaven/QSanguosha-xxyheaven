#ifndef TRIALOFGOD
#define TRIALOFGOD

#include "package.h"
#include "card.h"

class TrialOfGodPackage : public Package
{
    Q_OBJECT

public:
    TrialOfGodPackage();
};

class BossFentianCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE BossFentianCard();
    virtual void onEffect(const CardEffectStruct &effect) const;
};

#endif // TRIALOFGOD

