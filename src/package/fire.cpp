#include "fire.h"
#include "general.h"
#include "skill.h"
#include "standard.h"
#include "client.h"
#include "engine.h"
#include "maneuvering.h"

class dummyVS : public ZeroCardViewAsSkill
{
public:
    dummyVS() : ZeroCardViewAsSkill("dummy")
    {
    }

    virtual const Card *viewAs() const
    {
        return NULL;
    }
};

QuhuCard::QuhuCard()
{
}

bool QuhuCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select->getHp() > Self->getHp() && Self->canPindian(to_select);
}

void QuhuCard::use(Room *room, ServerPlayer *xunyu, QList<ServerPlayer *> &targets) const
{
    ServerPlayer *tiger = targets.first();
    bool success = xunyu->pindian(tiger, "quhu", NULL);
    if (success) {
        QList<ServerPlayer *> players = room->getOtherPlayers(tiger), wolves;
        foreach (ServerPlayer *player, players) {
            if (tiger->inMyAttackRange(player))
                wolves << player;
        }

        if (wolves.isEmpty()) {
            LogMessage log;
            log.type = "#QuhuNoWolf";
            log.from = xunyu;
            log.to << tiger;
            room->sendLog(log);

            return;
        }

        ServerPlayer *wolf = room->askForPlayerChosen(xunyu, wolves, "quhu", QString("@quhu-damage:%1").arg(tiger->objectName()));
        room->damage(DamageStruct("quhu", tiger, wolf));
    } else {
        room->damage(DamageStruct("quhu", tiger, xunyu));
    }
}

class Jieming : public MasochismSkill
{
public:
    Jieming() : MasochismSkill("jieming")
    {
		view_as_skill = new dummyVS;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (TriggerSkill::triggerable(player)) {
            DamageStruct damage = data.value<DamageStruct>();
            QStringList trigger_list;
            for (int i = 1; i <= damage.damage; i++) {
                trigger_list << objectName();
            }
            return trigger_list;
        }

        return QStringList();
    }

    virtual void onDamaged(ServerPlayer *xunyu, const DamageStruct &) const
    {
        Room *room = xunyu->getRoom();
        ServerPlayer *to = room->askForPlayerChosen(xunyu, room->getAlivePlayers(), objectName(), "jieming-invoke", true, true);
        if (to != NULL) {
            xunyu->broadcastSkillInvoke(objectName());
            int upper = qMin(5, to->getMaxHp());
            int x = upper - to->getHandcardNum();
            if (x > 0)
                to->drawCards(x, objectName());
        }
    }
};

class Quhu : public ZeroCardViewAsSkill
{
public:
    Quhu() : ZeroCardViewAsSkill("quhu")
    {
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("QuhuCard") && !player->isKongcheng();
    }

    virtual const Card *viewAs() const
    {
        return new QuhuCard;
    }
};

QiangxiCard::QiangxiCard()
{
}

bool QiangxiCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    if (!targets.isEmpty() || to_select == Self)
        return false;

    int rangefix = 0;
    if (!subcards.isEmpty() && Self->getWeapon() && Self->getWeapon()->getId() == subcards.first()) {
        const Weapon *card = qobject_cast<const Weapon *>(Self->getWeapon()->getRealCard());
        rangefix += card->getRange() - Self->getAttackRange(false);;
    }

    return Self->inMyAttackRange(to_select, rangefix);
}

void QiangxiCard::extraCost(Room *room, const CardUseStruct &card_use) const
{
	if (subcards.isEmpty())
	    room->loseHp(card_use.from);
	else {
		CardMoveReason reason(CardMoveReason::S_REASON_THROW, card_use.from->objectName(), QString(), card_use.card->getSkillName(), QString());
        room->moveCardTo(this, NULL, Player::DiscardPile, reason, true);
	}
}

void QiangxiCard::onEffect(const CardEffectStruct &effect) const
{
    effect.to->getRoom()->damage(DamageStruct("qiangxi", effect.from, effect.to));
}

class Qiangxi : public ViewAsSkill
{
public:
    Qiangxi() : ViewAsSkill("qiangxi")
    {
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("QiangxiCard");
    }

    virtual bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
        return selected.isEmpty() && to_select->isKindOf("Weapon") && !Self->isJilei(to_select);
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.isEmpty())
            return new QiangxiCard;
        else if (cards.length() == 1) {
            QiangxiCard *card = new QiangxiCard;
            card->addSubcards(cards);

            return card;
        } else
            return NULL;
    }
};

class Luanji : public ViewAsSkill
{
public:
    Luanji() : ViewAsSkill("luanji")
    {
        response_or_use = true;
    }

    virtual bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
        if (selected.isEmpty())
            return !to_select->isEquipped();
        else if (selected.length() == 1) {
            const Card *card = selected.first();
            return !to_select->isEquipped() && to_select->getSuit() == card->getSuit();
        } else
            return false;
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.length() == 2) {
            ArcheryAttack *aa = new ArcheryAttack(Card::SuitToBeDecided, 0);
            aa->addSubcards(cards);
            aa->setSkillName(objectName());
            return aa;
        } else
            return NULL;
    }
};

class Xueyi : public MaxCardsSkill
{
public:
    Xueyi() : MaxCardsSkill("xueyi$")
    {
    }

    virtual int getExtra(const Player *target) const
    {
        if (target->hasLordSkill(this)) {
            int extra = 0;
            QList<const Player *> players = target->getAliveSiblings();
            foreach (const Player *player, players) {
                if (player->getKingdom() == "qun")
                    extra += 2;
            }
            return extra;
        } else
            return 0;
    }
};

class ShuangxiongViewAsSkill : public OneCardViewAsSkill
{
public:
    ShuangxiongViewAsSkill() : OneCardViewAsSkill("shuangxiong")
    {
        response_or_use = true;
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return player->getMark("shuangxiong") != 0;
    }

    virtual bool viewFilter(const Card *card) const
    {
        if (card->isEquipped())
            return false;

        int value = Self->getMark("shuangxiong");
        if (value == 1)
            return card->isBlack();
        else if (value == 2)
            return card->isRed();

        return false;
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        Duel *duel = new Duel(originalCard->getSuit(), originalCard->getNumber());
        duel->addSubcard(originalCard);
        duel->setSkillName(objectName());
        return duel;
    }
};

class Shuangxiong : public TriggerSkill
{
public:
    Shuangxiong() : TriggerSkill("shuangxiong")
    {
        events << EventPhaseStart << FinishJudge;
        view_as_skill = new ShuangxiongViewAsSkill;
    }

    int getPriority(TriggerEvent triggerEvent) const
    {
        if (triggerEvent == FinishJudge)
            return 5;
        return TriggerSkill::getPriority(triggerEvent);
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *shuangxiong, QVariant &) const
    {
        if (triggerEvent == EventPhaseStart && shuangxiong->getPhase() == Player::NotActive) {
            room->setPlayerMark(shuangxiong, "shuangxiong", 0);
            room->setPlayerMark(shuangxiong, "ViewAsSkill_shuangxiongEffect", 0);
        }
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (triggerEvent == EventPhaseStart && player->getPhase() == Player::Draw) {
            if (TriggerSkill::triggerable(player))
                return QStringList(objectName());
        } else if (triggerEvent == FinishJudge) {
            JudgeStruct *judge = data.value<JudgeStruct *>();
            if (judge->reason == objectName() && judge->isGood() && room->getCardPlace(judge->card->getEffectiveId()) == Player::PlaceJudge)
                return QStringList("shuangxiong!");
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *shuangxiong, QVariant &data, ServerPlayer *) const
    {
        if (triggerEvent == EventPhaseStart) {
            if (shuangxiong->askForSkillInvoke(this)) {
                room->setPlayerFlag(shuangxiong, "shuangxiong");
                shuangxiong->broadcastSkillInvoke("shuangxiong");
                JudgeStruct judge;
                judge.good = true;
                judge.reason = objectName();
                judge.pattern = ".";
                judge.who = shuangxiong;
                judge.play_animation = false;
                room->judge(judge);
                room->setPlayerMark(shuangxiong, "shuangxiong", judge.card->isRed() ? 1 : 2);
                room->setPlayerMark(shuangxiong, "ViewAsSkill_shuangxiongEffect", 1);
                return true;
            }
        } else if (triggerEvent == FinishJudge) {
            JudgeStruct *judge = data.value<JudgeStruct *>();
            const Card *card = judge->card;
            shuangxiong->obtainCard(card);
        }
        return false;
    }
};

class Jianchu : public TriggerSkill
{
public:
    Jianchu() : TriggerSkill("jianchu")
    {
        events << TargetSpecified;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (TriggerSkill::triggerable(player)) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card != NULL && use.card->isKindOf("Slash")) {
                ServerPlayer *to = use.to.at(use.index);

                if (to && to->isAlive() && player->canDiscard(to, "he"))
                    return QStringList(objectName());
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *, QVariant &data, ServerPlayer *player) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        int index = use.index;
        ServerPlayer *to = use.to.at(index);
        if (player->askForSkillInvoke(this, QVariant::fromValue(to))) {
            player->broadcastSkillInvoke(objectName());
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), to->objectName());

            int to_throw = room->askForCardChosen(player, to, "he", objectName(), false, Card::MethodDiscard);
            bool no_jink = Sanguosha->getCard(to_throw)->getTypeId() == Card::TypeEquip;
            room->throwCard(to_throw, to, player);
            if (no_jink) {
                LogMessage log;
                log.type = "#NoJink";
                log.from = to;
                room->sendLog(log);
                QVariantList jink_list = use.card->tag["Jink_List"].toList();
                jink_list[index] = 0;
                use.card->setTag("Jink_List", jink_list);

            } else if (room->isAllOnPlace(use.card, Player::PlaceTable))
                to->obtainCard(use.card);
        }

        return false;
    }
};

class Lianhuan : public OneCardViewAsSkill
{
public:
    Lianhuan() : OneCardViewAsSkill("lianhuan")
    {
        filter_pattern = ".|club|.|hand";
        response_or_use = true;
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        IronChain *chain = new IronChain(originalCard->getSuit(), originalCard->getNumber());
        chain->addSubcard(originalCard);
        chain->setSkillName(objectName());
        return chain;
    }
};

class Niepan : public TriggerSkill
{
public:
    Niepan() : TriggerSkill("niepan")
    {
        events << AskForPeaches;
        frequency = Limited;
        limit_mark = "@nirvana";
    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *target, QVariant &data, ServerPlayer * &) const
    {
        if (TriggerSkill::triggerable(target) && target->getMark("@nirvana") > 0) {
            DyingStruct dying_data = data.value<DyingStruct>();

            if (target->getHp() > 0)
                return QStringList();

            if (dying_data.who != target)
                return QStringList();
            return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *pangtong, QVariant &, ServerPlayer *) const
    {
        if (pangtong->askForSkillInvoke(this)) {
            pangtong->broadcastSkillInvoke(objectName());

            room->removePlayerMark(pangtong, "@nirvana");

            pangtong->throwAllHandCardsAndEquips();
            QList<const Card *> tricks = pangtong->getJudgingArea();
            foreach (const Card *trick, tricks) {
                CardMoveReason reason(CardMoveReason::S_REASON_NATURAL_ENTER, pangtong->objectName());
                room->throwCard(trick, reason, NULL);
            }

            if (pangtong->isChained())
                room->setPlayerProperty(pangtong, "chained", false);

            if (!pangtong->faceUp())
                pangtong->turnOver();

            pangtong->drawCards(3, objectName());
            room->recover(pangtong, RecoverStruct(pangtong, NULL, 3 - pangtong->getHp()));

        }

        return false;
    }
};

class Huoji : public OneCardViewAsSkill
{
public:
    Huoji() : OneCardViewAsSkill("huoji")
    {
        filter_pattern = ".|red|.|hand";
        response_or_use = true;
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        FireAttack *fire_attack = new FireAttack(originalCard->getSuit(), originalCard->getNumber());
        fire_attack->addSubcard(originalCard->getId());
        fire_attack->setSkillName(objectName());
        return fire_attack;
    }
};

class Bazhen : public ViewHasSkill
{
public:
    Bazhen() : ViewHasSkill("bazhen")
    {
    }
    virtual bool ViewHas(const Player *player, const QString &skill_name, const QString &flag) const
    {
        if (flag == "armor" && skill_name == "eight_diagram" && player->isAlive() && player->hasSkill(objectName()) && !player->getArmor())
            return true;
        return false;
    }
};

class Kanpo : public OneCardViewAsSkill
{
public:
    Kanpo() : OneCardViewAsSkill("kanpo")
    {
        filter_pattern = ".|black|.|hand";
        response_pattern = "nullification";
        response_or_use = true;
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        Card *ncard = new Nullification(originalCard->getSuit(), originalCard->getNumber());
        ncard->addSubcard(originalCard);
        ncard->setSkillName(objectName());
        return ncard;
    }

    virtual bool isEnabledAtNullification(const ServerPlayer *player) const
    {
        return !player->isKongcheng() || !player->getHandPile().isEmpty();
    }
};

TianyiCard::TianyiCard()
{
}

bool TianyiCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && Self->canPindian(to_select);
}

void TianyiCard::use(Room *room, ServerPlayer *taishici, QList<ServerPlayer *> &targets) const
{
    bool success = taishici->pindian(targets.first(), "tianyi", NULL);
    if (success)
        room->setPlayerFlag(taishici, "TianyiSuccess");
    else
        room->setPlayerCardLimitation(taishici, "use", "Slash", true);
}

class Tianyi : public ZeroCardViewAsSkill
{
public:
    Tianyi() : ZeroCardViewAsSkill("tianyi")
    {
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("TianyiCard") && !player->isKongcheng();
    }

    virtual const Card *viewAs() const
    {
        return new TianyiCard;
    }
};

class TianyiTargetMod : public TargetModSkill
{
public:
    TianyiTargetMod() : TargetModSkill("#tianyi-target")
    {
        frequency = NotFrequent;
    }

    virtual int getResidueNum(const Player *from, const Card *, const Player *) const
    {
        if (from->hasFlag("TianyiSuccess"))
            return 1;
        else
            return 0;
    }

    virtual int getDistanceLimit(const Player *from, const Card *, const Player *) const
    {
        if (from->hasFlag("TianyiSuccess"))
            return 1000;
        else
            return 0;
    }

    virtual int getExtraTargetNum(const Player *from, const Card *) const
    {
        if (from->hasFlag("TianyiSuccess"))
            return 1;
        else
            return 0;
    }
};

FirePackage::FirePackage()
    : Package("fire")
{
    General *dianwei = new General(this, "dianwei", "wei"); // WEI 012
    dianwei->addSkill(new Qiangxi);

    General *xunyu = new General(this, "xunyu", "wei", 3); // WEI 013
    xunyu->addSkill(new Quhu);
    xunyu->addSkill(new Jieming);

    General *pangtong = new General(this, "pangtong", "shu", 3); // SHU 010
    pangtong->addSkill(new Lianhuan);
    pangtong->addSkill(new Niepan);

    General *wolong = new General(this, "wolong", "shu", 3); // SHU 011
    wolong->addSkill(new Bazhen);
    wolong->addSkill(new Huoji);
    wolong->addSkill(new Kanpo);

    General *taishici = new General(this, "taishici", "wu"); // WU 012
    taishici->addSkill(new Tianyi);
    taishici->addSkill(new TianyiTargetMod);
    related_skills.insertMulti("tianyi", "#tianyi-target");

    General *yuanshao = new General(this, "yuanshao$", "qun"); // QUN 004
    yuanshao->addSkill(new Luanji);
    yuanshao->addSkill(new Xueyi);

    General *yanliangwenchou = new General(this, "yanliangwenchou", "qun"); // QUN 005
    yanliangwenchou->addSkill(new Shuangxiong);

    General *pangde = new General(this, "pangde", "qun"); // QUN 008
    pangde->addSkill("mashu");
    pangde->addSkill(new Jianchu);

    addMetaObject<QuhuCard>();
    addMetaObject<QiangxiCard>();
    addMetaObject<TianyiCard>();
}

ADD_PACKAGE(Fire)

