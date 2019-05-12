#include "trialofgod.h"
#include "settings.h"
#include "skill.h"
#include "standard.h"
#include "client.h"
#include "clientplayer.h"
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

class BossTengyun : public TriggerSkill
{
public:
    BossTengyun() : TriggerSkill("bosstengyun")
    {
        events << TargetConfirming;
        frequency = Compulsory;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.from != player && use.to.contains(player) && player->getMark("Flamc") && use.card->getTypeId() != Card::TypeSkill) {
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            use.to.removeAll(player);
            data = QVariant::fromValue(use);
            return true;
        }
        return false;
    }
};

class BossBuchun : public PhaseChangeSkill
{
public:
    BossBuchun() : PhaseChangeSkill("bossbuchun")
    {
        frequency = Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    virtual bool onPhaseChange(ServerPlayer *player) const
    {
        Room *room = player->getRoom();
        if (player->getPhase() != Player::Start) return false;
        if (player->getMark("buchuninvoked") > 0)
            room->setPlayerMark(player, "buchuninvoked", 0);
        else if (TriggerSkill::triggerable(player)) {
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            room->setPlayerMark(player, "buchuninvoked", 1);
            QList<ServerPlayer *> deadfriends, enemies;
            foreach (ServerPlayer *p, room->getAllPlayers(true)) {
                if (p->getKingdom() == "god") {
                    if (p->isDead())
                        deadfriends << p;
                }
                else
                    enemies << p;
            }
            if (deadfriends.isEmpty()) {
                if (enemies.isEmpty()) return false;
                ServerPlayer *target = room->askForPlayerChosen(player, enemies, objectName(), "@buchu-choose");
                room->damage(DamageStruct(objectName(), player, target, 2));
            } else {
                foreach (ServerPlayer *p, deadfriends) {
                    if (p->isDead() && p->getMaxHp() > 0) {
                        room->revivePlayer(p);
                        room->setPlayerProperty(p, "hp", 1);
                        p->drawCards(p->getMaxHp(), objectName());
                    }
                }
            }
        }
        return false;
    }
};

class BossCuidu : public TriggerSkill
{
public:
    BossCuidu() : TriggerSkill("bosscuidu")
    {
        events << Damage;
        frequency = Compulsory;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        ServerPlayer *target = damage.to;
        if (target->isAlive() && target->getKingdom() != "god" && !target->hasSkill("bosszhongdu", true)) {
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), target->objectName());
            room->acquireSkill(target, "bosszhongdu");
            ServerPlayer *mushen = room->findPlayerByGeneralName("mushengoumang");
            if (mushen)
                mushen->drawCards(1, objectName());
        }
        return false;
    }
};

class BossZhongdu : public PhaseChangeSkill
{
public:
    BossZhongdu() : PhaseChangeSkill("bosszhongdu")
    {
        frequency = Compulsory;
    }

    virtual bool onPhaseChange(ServerPlayer *player) const
    {
        Room *room = player->getRoom();
        if (player->getPhase() == Player::Start) {
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            JudgeStruct judge;
            judge.pattern = ".|diamond";
            judge.good = false;
            judge.negative = true;
            judge.reason = objectName();
            judge.who = player;

            room->judge(judge);

            if (judge.isBad())
                room->loseHp(player);
            else
                room->detachSkillFromPlayer(player, objectName());
        }
        return false;
    }
};

class BossShenen : public DrawCardsSkill
{
public:
    BossShenen() : DrawCardsSkill("bossshenen")
    {
        frequency = Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    virtual int getDrawNum(ServerPlayer *player, int n) const
    {
        Room *room = player->getRoom();
        foreach (ServerPlayer *god, room->getAlivePlayers()) {
            if (player->getKingdom() == "god") break;
            if (!TriggerSkill::triggerable(god)) continue;
            room->sendCompulsoryTriggerLog(god, objectName());
            god->broadcastSkillInvoke(objectName());
            n++;
        }
        return n;
    }
};

class BossShenenTarget : public TargetModSkill
{
public:
    BossShenenTarget() : TargetModSkill("#bossshenen-target")
    {
        pattern = "^SkillCard";
    }

    virtual int getDistanceLimit(const Player *from, const Card *, const Player *) const
    {
        if (from->getKingdom() == "god") {
            if (from->hasSkill("bossshenen")) return 1000;
            QList<const Player *> players = from->getAliveSiblings();
            foreach (const Player *player, players) {
                if (player->hasSkill("bossshenen"))
                    return 1000;
            }
        }
        return 0;
    }
};

class BossShenenMaxCards : public MaxCardsSkill
{
public:
    BossShenenMaxCards() : MaxCardsSkill("#bossshenen-maxcards")
    {
    }

    virtual int getExtra(const Player *target) const
    {
        int n = 0;
        if (target->getKingdom() != "god") {
            if (target->hasSkill("bossshenen")) n++;
            QList<const Player *> players = target->getAliveSiblings();
            foreach (const Player *player, players) {
                if (player->hasSkill("bossshenen"))
                    n++;
            }
        }
        return n;
    }
};

class BossQingyi : public TriggerSkill
{
public:
    BossQingyi() : TriggerSkill("bossqingyi")
    {
        events << RoundStart;
        frequency = Compulsory;
    }

    virtual bool trigger(TriggerEvent , Room *room, ServerPlayer *taihao, QVariant &data) const
    {
        int turns = data.toInt();
        if (turns == 3) {
            room->sendCompulsoryTriggerLog(taihao, objectName());
            taihao->broadcastSkillInvoke(objectName());
            QList<ServerPlayer *> friends;
            foreach (ServerPlayer *p, room->getAlivePlayers()) {
                if (p->getKingdom() == "god") {
                    room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, taihao->objectName(), p->objectName());
                    friends << p;
                }
            }
            foreach (ServerPlayer *p, friends) {
                if (p->isAlive()) {
                    LogMessage log;
                    log.type = "#GainMaxHp";
                    log.from = p;
                    log.arg = "1";
                    log.arg2 = QString::number(p->getMaxHp() + 1);
                    room->sendLog(log);
                    room->setPlayerProperty(p, "maxhp", p->getMaxHp() + 1);
                    if (p->isWounded())
                        room->recover(p, RecoverStruct(taihao));
                }
            }

        } else if (turns == 6) {
            room->sendCompulsoryTriggerLog(taihao, objectName());
            taihao->broadcastSkillInvoke(objectName());
            QList<ServerPlayer *> enemies;
            foreach (ServerPlayer *p, room->getAlivePlayers()) {
                if (p->getKingdom() != "god") {
                    room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, taihao->objectName(), p->objectName());
                    enemies << p;
                }
            }
            foreach (ServerPlayer *p, enemies) {
                if (p->isAlive()) {
                    room->loseHp(p);
                }
            }
        } else if (turns == 9) {
            room->sendCompulsoryTriggerLog(taihao, objectName());
            taihao->broadcastSkillInvoke(objectName());
            QList<ServerPlayer *> deadfriends;
            foreach (ServerPlayer *p, room->getAllPlayers(true)) {
                if (p->getKingdom() == "god") {
                    if (p->isDead())
                        deadfriends << p;
                }
            }
            foreach (ServerPlayer *p, deadfriends) {
                if (p->isDead() && p->getMaxHp() > 0) {
                    room->revivePlayer(p);
                    room->setPlayerProperty(p, "hp", p->getMaxHp());
                    p->drawCards(4, objectName());
                }
            }
            QList<ServerPlayer *> friends;
            foreach (ServerPlayer *p, room->getAlivePlayers()) {
                if (p->getKingdom() == "god") {
                    friends << p;
                }
            }
            foreach (ServerPlayer *p, friends) {
                if (p->isAlive()) {
                    room->acquireSkill(p, "nosqingnang");
                }
            }
        }
        return false;
    }
};

BossFentianCard::BossFentianCard()
{
}

void BossFentianCard::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *zhuque = effect.from;
    ServerPlayer *target = effect.to;
    Room *room = zhuque->getRoom();

    room->damage(DamageStruct("bossfentian", zhuque, target, 1, DamageStruct::Fire));
}

class BossFentianViewAsSkill : public ZeroCardViewAsSkill
{
public:
    BossFentianViewAsSkill() : ZeroCardViewAsSkill("bossfentian")
    {
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return player->usedTimes("BossFentianCard") <= player->getMark("BossFentianTimes");
    }

    virtual const Card *viewAs() const
    {
        return new BossFentianCard;
    }
};

class BossFentian : public TriggerSkill
{
public:
    BossFentian() : TriggerSkill("bossfentian")
    {
        events << Death << EventPhaseChanging;
        view_as_skill = new BossFentianViewAsSkill;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *zhuque, QVariant &data) const
    {
        if (triggerEvent == Death) {
            DeathStruct death = data.value<DeathStruct>();
            if (death.damage && zhuque == death.damage->from && death.damage->reason == objectName()) {
                if (zhuque->getPhase() == Player::Play)
                    room->addPlayerMark(zhuque, "BossFentianTimes");
            }
        } else if (triggerEvent == EventPhaseChanging) {
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (change.to != Player::Play && change.from != Player::Play) return false;
            room->setPlayerMark(zhuque, "BossFentianTimes", 0);
        }
        return false;
    }
};

class BossXingxia : public PhaseChangeSkill
{
public:
    BossXingxia() : PhaseChangeSkill("bossxingxia")
    {
        frequency = Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    virtual bool onPhaseChange(ServerPlayer *player) const
    {
        Room *room = player->getRoom();
        if (player->getPhase() != Player::Play) return false;
        if (player->getMark("xingxiainvoked") > 0)
            room->setPlayerMark(player, "xingxiainvoked", 0);
        else if (TriggerSkill::triggerable(player)) {
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            room->setPlayerMark(player, "xingxiainvoked", 1);
            QList<ServerPlayer *> friends, enemies;
            foreach (ServerPlayer *p, room->getOtherPlayers(player)) {
                if (p->getKingdom() == "god")
                    friends << p;
            }
            if (friends.isEmpty()) return false;
            ServerPlayer *target = room->askForPlayerChosen(player, friends, objectName(), "@xingxia-choose");
            room->damage(DamageStruct(objectName(), player, target, 2, DamageStruct::Fire));
            foreach (ServerPlayer *p, room->getAlivePlayers()) {
                if (p->getKingdom() != "god")
                    enemies << p;
            }
            if (enemies.isEmpty()) return false;
            foreach (ServerPlayer *p, enemies) {
                if (p->isAlive()) {
                    if (!room->askForCard(p, ".|red", "@xingxia-discard", QVariant()))
                        room->damage(DamageStruct(objectName(), player, p, 1, DamageStruct::Fire));
                }
            }
        }
        return false;
    }
};

class BossHuihuo : public TriggerSkill
{
public:
    BossHuihuo() : TriggerSkill("bosshuihuo")
    {
        events << Death;
        frequency = Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->hasSkill(this);
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        DeathStruct death = data.value<DeathStruct>();
        if (death.who != player) return false;
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());
        QList<ServerPlayer *> enemies;
        foreach (ServerPlayer *p, room->getAlivePlayers()) {
            if (p->getKingdom() != "god") {
                room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), p->objectName());
                enemies << p;
            }
        }
        foreach (ServerPlayer *p, enemies) {
            if (p->isAlive())
                room->damage(DamageStruct(objectName(), player, p, 3, DamageStruct::Fire));
        }
        return false;
    }
};

class BossHuihuoTarget : public TargetModSkill
{
public:
    BossHuihuoTarget() : TargetModSkill("#bosshuihuo-target")
    {
    }

    virtual int getResidueNum(const Player *from, const Card *, const Player *) const
    {
        if (from->hasSkill("bosshuihuo"))
            return 1;
        else
            return 0;
    }
};

class BossFuran : public TriggerSkill
{
public:
    BossFuran() : TriggerSkill("bossfuran")
    {
        events << GameStart << EventAcquireSkill << EventLoseSkill << Debut;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if ((triggerEvent == GameStart && TriggerSkill::triggerable(player))
            || (triggerEvent == EventAcquireSkill && data.toString() == objectName())) {
            foreach (ServerPlayer *p, room->getOtherPlayers(player)) {
                if (!p->hasSkill("bossfuran_save"))
                    room->attachSkillToPlayer(p, "bossfuran_save");
            }
        } else if (triggerEvent == EventLoseSkill && data.toString() == objectName()) {
            foreach (ServerPlayer *p, room->getOtherPlayers(player)) {
                if (p->hasSkill("xiansi_slash"))
                    room->detachSkillFromPlayer(p, "bossfuran_save", true);
            }
        } else if (triggerEvent == Debut) {
            QList<ServerPlayer *> yanlings = room->findPlayersBySkillName(objectName());
            foreach (ServerPlayer *yanling, yanlings) {
                if (player != yanling && !player->hasSkill("bossfuran_save")) {
                    room->attachSkillToPlayer(player, "bossfuran_save");
                    break;
                }
            }
        }
        return false;
    }
};

class BossFuranSave : public OneCardViewAsSkill
{
public:
    BossFuranSave() : OneCardViewAsSkill("bossfuran_save")
    {
        filter_pattern = ".|red";
        response_or_use = true;
        attached_lord_skill = true;
    }

    virtual bool isEnabledAtPlay(const Player *) const
    {
        return false;
    }

    virtual bool isEnabledAtResponse(const Player *player, const QString &pattern) const
    {
        if (!pattern.contains("peach")) return false;
        foreach(const Player *sib, player->getAliveSiblings()) {
            if (sib->hasFlag("Global_Dying") && sib->hasSkill("bossfuran"))
                return true;
        }
        return false;
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        Peach *peach = new Peach(originalCard->getSuit(), originalCard->getNumber());
        peach->addSubcard(originalCard->getId());
        peach->setSkillName(objectName());
        return peach;
    }
};

class BossChiyi : public TriggerSkill
{
public:
    BossChiyi() : TriggerSkill("bosschiyi")
    {
        events << ConfirmDamage << RoundStart;
        frequency = Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *yandi, QVariant &data) const
    {
        if (triggerEvent == ConfirmDamage) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.to->getKingdom() != "god" && room->getTurn() >= 3) {
                foreach (ServerPlayer *p, room->getAlivePlayers()) {
                    if (TriggerSkill::triggerable(p))
                        ++damage.damage;
                }
                data = QVariant::fromValue(damage);
            }
        } else if (triggerEvent == RoundStart && TriggerSkill::triggerable(yandi)) {
            int turns = data.toInt();
            if (turns == 6) {
                room->sendCompulsoryTriggerLog(yandi, objectName());
                yandi->broadcastSkillInvoke(objectName());
                QList<ServerPlayer *> otherplayers = room->getOtherPlayers(yandi);
                foreach (ServerPlayer *p, otherplayers) {
                    if (p->isAlive())
                        room->damage(DamageStruct(objectName(), yandi, p, 1, DamageStruct::Fire));
                }
            } else if (turns == 9) {
                room->sendCompulsoryTriggerLog(yandi, objectName());
                yandi->broadcastSkillInvoke(objectName());
                QList<ServerPlayer *> yanlings;
                foreach (ServerPlayer *p, room->getAlivePlayers()) {
                    if (p->getGeneralName() == "yanling" || p->getGeneral2Name() == "yanling") {
                        yanlings << p;
                    }
                }
                foreach (ServerPlayer *yanling, yanlings) {
                    if (yanling->isAlive())
                        room->killPlayer(yanling);
                }
            }
        }
        return false;
    }
};

class BossKuangxiao : public TriggerSkill
{
public:
    BossKuangxiao() : TriggerSkill("bosskuangxiao")
    {
        events << TargetChosed;
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(player) || player->getPhase() == Player::NotActive) return QStringList();
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card->isKindOf("Slash") && !player->getUseExtraTargets(use, true).isEmpty())
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        QList<ServerPlayer *> available_targets = player->getUseExtraTargets(use, true);
        if (available_targets.isEmpty()) return false;
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());
        foreach (ServerPlayer *p, available_targets) {
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), p->objectName());
            use.to.append(p);
        }
        room->sortByActionOrder(use.to);
        data = QVariant::fromValue(use);
        return false;
    }
};

class BossKuangxiaoTarget : public TargetModSkill
{
public:
    BossKuangxiaoTarget() : TargetModSkill("#bosskuangxiao-target")
    {

    }

    virtual int getDistanceLimit(const Player *from, const Card *, const Player *) const
    {
        if (from->hasSkill("bosskuangxiao") && from->getPhase() != Player::NotActive)
            return 1000;
        return 0;
    }
};

class BossXingqiu : public PhaseChangeSkill
{
public:
    BossXingqiu() : PhaseChangeSkill("bossxingqiu")
    {
        frequency = Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    virtual bool onPhaseChange(ServerPlayer *player) const
    {
        Room *room = player->getRoom();
        if (player->getPhase() != Player::Play) return false;
        if (player->getMark("xingqiuinvoked") > 0)
            room->setPlayerMark(player, "xingqiuinvoked", 0);
        else if (TriggerSkill::triggerable(player)) {
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            room->setPlayerMark(player, "xingqiuinvoked", 1);
            QList<ServerPlayer *> enemies, mingxingzhus;
            foreach (ServerPlayer *p, room->getAlivePlayers()) {
                if (p->getKingdom() != "god")
                    enemies << p;
            }
            foreach (ServerPlayer *p, enemies) {
                if (p->isAlive() && !p->isChained())
                    room->setPlayerProperty(p, "chained", true);
            }
            foreach (ServerPlayer *p, room->getAlivePlayers()) {
                if (p->getGeneralName() == "mingxingzhu" || p->getGeneral2Name() == "mingxingzhu")
                    mingxingzhus << p;
            }
            foreach (ServerPlayer *p, mingxingzhus) {
                if (p->isAlive() && !p->hasSkill("bossjiding", true))
                    room->acquireSkill(p, "bossjiding");
            }
        }
        return false;
    }
};

class BossQingzhu : public TriggerSkill
{
public:
    BossQingzhu() : TriggerSkill("bossqingzhu")
    {
        events << EventPhaseChanging;
        frequency = Compulsory;
    }

    virtual bool trigger(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data) const
    {
        PhaseChangeStruct change = data.value<PhaseChangeStruct>();
        if (change.to == Player::Discard) {
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            player->skip(Player::Discard);
        }
        return false;
    }
};

class BossQingzhuCardLimited : public CardLimitedSkill
{
public:
    BossQingzhuCardLimited() : CardLimitedSkill("#bossqingzhu-cardlimited")
    {
    }
    virtual bool isCardLimited(const Player *player, const Card *card, Card::HandlingMethod method) const
    {
        return (method == Card::MethodUse && player->hasSkill("bossqingzhu") && card->isKindOf("Slash")
                && !player->hasSkill("bossjiding", true) && player->getPhase() == Player::Play);
    }
};

class BossJiazu : public TriggerSkill
{
public:
    BossJiazu() : TriggerSkill("bossjiazu")
    {
        events << EventPhaseStart;
        frequency = Compulsory;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (player->getPhase() != Player::RoundStart) return false;
        QList<ServerPlayer *> targets;
        foreach (ServerPlayer *p, room->getAlivePlayers()) {
            if ((p->getNextAlive() == player || player->getNextAlive() == p) && p->getKingdom() != "god"
                    && (p->getOffensiveHorse() || p->getDefensiveHorse()))
                targets << p;
        }
        if (targets.isEmpty()) return false;
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());
        foreach (ServerPlayer *p, targets)
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), p->objectName());
        foreach (ServerPlayer *p, targets) {
            DummyCard *dummy = new DummyCard;
            foreach (const Card *card, p->getEquips()) {
                if (card->isKindOf("Horse"))
                    dummy->addSubcard(card);
            }
            if (dummy->subcardsLength() > 0)
                room->throwCard(dummy, p);
            delete dummy;
        }
        return false;
    }
};

class BossJiding : public TriggerSkill
{
public:
    BossJiding() : TriggerSkill("bossjiding")
    {
        events << Damaged << Damage;
        frequency = Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (triggerEvent == Damaged) {
            ServerPlayer *target = damage.from;
            QList<ServerPlayer *> mingxingzhus = room->getOtherPlayers(player);
            foreach (ServerPlayer *mingxingzhu, mingxingzhus) {
                if (!target || target->isDead() || target->getKingdom() == "god" || player->getKingdom() != "god") break;
                if (!TriggerSkill::triggerable(mingxingzhu)) continue;
                room->sendCompulsoryTriggerLog(mingxingzhu, objectName());
                mingxingzhu->broadcastSkillInvoke(objectName());
                room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, mingxingzhu->objectName(), target->objectName());
                room->detachSkillFromPlayer(mingxingzhu, objectName());
                ThunderSlash *slash = new ThunderSlash(Card::NoSuit, 0);
                slash->setSkillName("_bossjiding");
                room->useCard(CardUseStruct(slash, mingxingzhu, target));
            }
        } else if (triggerEvent == Damage) {
            if (damage.card && damage.card->getSkillName() == objectName()) {
                QList<ServerPlayer *> jinshen = room->getAlivePlayers();
                foreach (ServerPlayer *p, jinshen) {
                    if (!player || player->isDead()) break;
                    if (p->getGeneralName() == "jinshenrushou" || p->getGeneral2Name() == "jinshenrushou") {
                        if (p->isWounded()){
                            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), p->objectName());
                            room->recover(p, RecoverStruct(player));
                        }
                    }
                }
            }
        }
        return false;
    }
};

class BossBaiyi : public TriggerSkill
{
public:
    BossBaiyi() : TriggerSkill("bossbaiyi")
    {
        events << RoundStart << DamageInflicted << DrawNCards;
        frequency = Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == RoundStart && TriggerSkill::triggerable(player) && data.toInt() == 5) {
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            QList<ServerPlayer *> enemies;
            foreach (ServerPlayer *p, room->getAlivePlayers()) {
                if (p->getKingdom() != "god") {
                    room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), p->objectName());
                    enemies << p;
                }
            }
            foreach (ServerPlayer *p, enemies) {
                if (p->isAlive())
                    room->askForDiscard(p, objectName(), 2, 2, false, true, "@baiyi-discard");
            }
        } else if (triggerEvent == DrawNCards) {
            int n = data.toInt();
            foreach (ServerPlayer *god, room->getAlivePlayers()) {
                if (player->getKingdom() == "god") break;
                if (!TriggerSkill::triggerable(god)) continue;
                room->sendCompulsoryTriggerLog(god, objectName());
                god->broadcastSkillInvoke(objectName());
                n--;
            }
            data = n;
        } else if (triggerEvent == DamageInflicted) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.nature == DamageStruct::Thunder && player->getKingdom() == "god" && room->getTurn() < 7) {
                ServerPlayer *shaohao = room->findPlayerBySkillName(objectName());
                if (!shaohao) return false;
                room->sendCompulsoryTriggerLog(shaohao, objectName());
                shaohao->broadcastSkillInvoke(objectName());
                room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, shaohao->objectName(), player->objectName());
                return true;
            }
        }
        return false;
    }
};

class BossLingqu : public TriggerSkill
{
public:
    BossLingqu() : TriggerSkill("bosslingqu")
    {
        events << Damaged << DamageInflicted;
        frequency = Compulsory;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (triggerEvent == Damaged) {
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            player->drawCards(1, objectName());
            room->addPlayerMark(player, "bosslingqu_times");
        } else if (triggerEvent == DamageInflicted) {
            if (damage.damage > 1) {
                room->sendCompulsoryTriggerLog(player, objectName());
                player->broadcastSkillInvoke(objectName());
                damage.damage = 1;
                data = QVariant::fromValue(damage);
            }
        }
        return false;
    }
};

class BossLingquMaxCards : public MaxCardsSkill
{
public:
    BossLingquMaxCards() : MaxCardsSkill("#bosslingqu-maxcards")
    {

    }

    virtual int getExtra(const Player *target) const
    {
        return target->getMark("bosslingqu_times");
    }
};

class BossZirun : public TriggerSkill
{
public:
    BossZirun() : TriggerSkill("bosszirun")
    {
        events << EventPhaseStart;
        frequency = Compulsory;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (player->getPhase() != Player::Start) return false;
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());
        QList<ServerPlayer *> all_players = room->getAlivePlayers(), eme;
        foreach (ServerPlayer *p, all_players) {
            if (p->isAlive())
                p->drawCards(1, objectName());
            if (p->isAlive() && p->hasEquip())
                p->drawCards(1, objectName());
        }
        return false;
    }
};

class BossJuehong : public TriggerSkill
{
public:
    BossJuehong() : TriggerSkill("bossjuehong")
    {
        events << EventPhaseStart;
        frequency = Compulsory;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (player->getPhase() != Player::Start) return false;
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());
        QList<ServerPlayer *> all_players = room->getAlivePlayers(), enemies;
        foreach (ServerPlayer *p, all_players) {
            if (p->getKingdom() != "god") {
                room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), p->objectName());
                enemies << p;
            }
        }
        foreach (ServerPlayer *p, enemies) {
            if (p->isAlive()) {
                if (p->hasEquip()) {
                    DummyCard *dummy = new DummyCard;
                    foreach (const Card *card, p->getEquips()) {
                        dummy->addSubcard(card);
                    }
                    if (dummy->subcardsLength() > 0)
                        room->throwCard(dummy, p);
                    delete dummy;
                } else if (player->isAlive() && player->canDiscard(p, "h")) {
                    int to_throw = room->askForCardChosen(player, p, "h", objectName(), false, Card::MethodDiscard);
                    room->throwCard(to_throw, p, player);
                }
            }
        }
        return false;
    }
};

class BossZaoyiTrigger : public TriggerSkill
{
public:
    BossZaoyiTrigger() : TriggerSkill("#bosszaoyi-trigger")
    {
        events << Death << EventPhaseStart;

    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->isAlive() && target->hasSkill("bosszaoyi");
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *zhuanxu, QVariant &data) const
    {
        if (triggerEvent == Death) {
            DeathStruct death = data.value<DeathStruct>();
            ServerPlayer *player = death.who;
            bool gonggong = false, xuanming = false;
            if (player->getGeneralName() == "shuishengonggong" || player->getGeneral2Name() == "shuishengonggong")
                gonggong = true;
            if (player->getGeneralName() == "shuishenxuanming" || player->getGeneral2Name() == "shuishenxuanming")
                xuanming = true;
            if (gonggong)
                room->addPlayerMark(zhuanxu, "BossZaoyiGonggongDead");
            if (xuanming)
                room->addPlayerMark(zhuanxu, "BossZaoyiXuanmingDead");
            if (zhuanxu->getMark("BossZaoyiGonggongDead") > 0 && zhuanxu->getMark("BossZaoyiXuanmingDead") > 0) {
                room->sendCompulsoryTriggerLog(zhuanxu, "bosszaoyi");
                zhuanxu->broadcastSkillInvoke("bosszaoyi");
                zhuanxu->drawCards(4, "bosszaoyi");
            }
        } else if (triggerEvent == EventPhaseStart) {
            if (zhuanxu->getPhase() != Player::RoundStart) return false;
            if (zhuanxu->getMark("BossZaoyiGonggongDead") > 0 && zhuanxu->getMark("BossZaoyiXuanmingDead") > 0) {
                QList<ServerPlayer *> players = room->getAlivePlayers(), targets;
                int hp = 1000;
                foreach (ServerPlayer *p, players) {
                    if (p->getKingdom() != "god" && p->getHp() <= hp) {
                        if (p->getHp() < hp) {
                            hp = p->getHp();
                            targets.clear();
                        }
                        targets << p;
                    }
                }

                if (targets.isEmpty()) return false;
                room->sendCompulsoryTriggerLog(zhuanxu, "bosszaoyi");
                zhuanxu->broadcastSkillInvoke("bosszaoyi");
                ServerPlayer *target = room->askForPlayerChosen(zhuanxu, targets, "bosszaoyi", "@zaoyi-choose");
                if (target->getHp() > 0)
                    room->loseHp(target, target->getHp());
            }
        }
        return false;
    }
};

class BossZaoyi : public ProhibitSkill
{
public:
    BossZaoyi() : ProhibitSkill("bosszaoyi")
    {
    }

    virtual bool isProhibited(const Player *from, const Player *to, const Card *card, const QList<const Player *> &) const
    {
        if (from && from->getKingdom() != "god" && to->hasSkill("bosszaoyi")) {
            bool gonggong = false, xuanming= false;
            QList<const Player *> players = to->getAliveSiblings();
            foreach (const Player *player, players) {
                if (player->getGeneralName() == "shuishengonggong" || player->getGeneral2Name() == "shuishengonggong")
                    gonggong = true;
                if (player->getGeneralName() == "shuishenxuanming" || player->getGeneral2Name() == "shuishenxuanming")
                    xuanming = true;
            }
            return (gonggong && card->getTypeId() == Card::TypeBasic) || (xuanming && card->getTypeId() == Card::TypeTrick);
        }
        return false;
    }
};

TrialOfGodPackage::TrialOfGodPackage()
    : Package("TrialOfGod")
{
    General *qinglong = new General(this, "qinglong", "god", 4, true, true);
    qinglong->addSkill(new Skill("bossshenyi", Skill::Compulsory));
    qinglong->addSkill(new BossTengyun);

    General *mushengoumang = new General(this, "mushengoumang", "god", 5, true, true);
    mushengoumang->addSkill("bossshenyi");
    mushengoumang->addSkill(new BossBuchun);

    General *shujing = new General(this, "shujing", "god", 2, false, true);
    shujing->addSkill(new BossCuidu);
    shujing->addRelateSkill("bosszhongdu");

    General *taihao = new General(this, "taihao", "god", 6, true, true);
    taihao->addSkill("bossshenyi");
    taihao->addSkill(new BossShenen);
    taihao->addSkill(new BossShenenTarget);
    taihao->addSkill(new BossShenenMaxCards);
    related_skills.insertMulti("bossshenen","#bossshenen-target");
    related_skills.insertMulti("bossshenen","#bossshenen-maxcards");
    taihao->addSkill(new BossQingyi);

    General *zhuque = new General(this, "zhuque", "god", 4, false, true);
    zhuque->addSkill("bossshenyi");
    zhuque->addSkill(new BossFentian);

    General *huoshenzhurong = new General(this, "huoshenzhurong", "god", 5, true, true);
    huoshenzhurong->addSkill("bossshenyi");
    huoshenzhurong->addSkill(new BossXingxia);

    General *yanling = new General(this, "yanling", "god", 4, true, true);
    yanling->addSkill(new BossHuihuo);
    yanling->addSkill(new BossHuihuoTarget);
    yanling->addSkill(new BossFuran);
    related_skills.insertMulti("bosshuihuo","#bosshuihuo-target");

    General *yandi = new General(this, "yandi", "god", 4, true, true);
    yandi->addSkill("bossshenyi");
    yandi->addSkill("bossshenen");
    yandi->addSkill("#bossshenen-target");
    yandi->addSkill("#bossshenen-maxcards");
    yandi->addSkill(new BossChiyi);

    General *baihu = new General(this, "baihu", "god", 4, true, true);
    baihu->addSkill("bossshenyi");
    baihu->addSkill(new BossKuangxiao);
    baihu->addSkill(new BossKuangxiaoTarget);
    related_skills.insertMulti("bosskuangxiao","#bosskuangxiao-target");

    General *jinshenrushou = new General(this, "jinshenrushou", "god", 5, false, true);
    jinshenrushou->addSkill("bossshenyi");
    jinshenrushou->addSkill(new BossXingqiu);

    General *mingxingzhu = new General(this, "mingxingzhu", "god", 3, false, true);
    mingxingzhu->addSkill(new BossQingzhu);
    mingxingzhu->addSkill(new BossQingzhuCardLimited);
    mingxingzhu->addSkill(new BossJiazu);
    mingxingzhu->addRelateSkill("bossjiding");
    related_skills.insertMulti("bossqingzhu","#bossqingzhu-cardlimited");

    General *shaohao = new General(this, "shaohao", "god", 6, true, true);
    shaohao->addSkill("bossshenyi");
    shaohao->addSkill("bossshenen");
    shaohao->addSkill("#bossshenen-target");
    shaohao->addSkill("#bossshenen-maxcards");
    shaohao->addSkill(new BossBaiyi);

    General *xuanwu = new General(this, "xuanwu", "god", 4, false, true);
    xuanwu->addSkill("bossshenyi");
    xuanwu->addSkill(new BossLingqu);
    xuanwu->addSkill(new BossLingquMaxCards);
    related_skills.insertMulti("bosslingqu","#bosslingqu-maxcards");

    General *shuishenxuanming = new General(this, "shuishenxuanming", "god", 5, false, true);
    shuishenxuanming->addSkill("bossshenyi");
    shuishenxuanming->addSkill(new BossZirun);

    General *shuishengonggong = new General(this, "shuishengonggong", "god", 5, true, true);
    shuishengonggong->addSkill("bossshenyi");
    shuishengonggong->addSkill(new BossJuehong);

    General *zhuanxu = new General(this, "zhuanxu", "god", 4, true, true);
    zhuanxu->addSkill("bossshenyi");
    zhuanxu->addSkill("bossshenen");
    zhuanxu->addSkill("#bossshenen-target");
    zhuanxu->addSkill("#bossshenen-maxcards");
    zhuanxu->addSkill(new BossZaoyi);
    zhuanxu->addSkill(new BossZaoyiTrigger);
    related_skills.insertMulti("bosszaoyi","#bosszaoyi-trigger");

    addMetaObject<BossFentianCard>();

    skills << new BossZhongdu << new BossFuranSave << new BossJiding;
}

ADD_PACKAGE(TrialOfGod)
