#include "boss.h"
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

class BossGuimei : public ProhibitSkill
{
public:
    BossGuimei() : ProhibitSkill("bossguimei")
    {
    }

    virtual bool isProhibited(const Player *, const Player *to, const Card *card, const QList<const Player *> &) const
    {
        return to->hasSkill(this) && card->isKindOf("DelayedTrick");
    }
};

class BossDidong : public PhaseChangeSkill
{
public:
    BossDidong() : PhaseChangeSkill("bossdidong")
    {
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
        if (target->getPhase() != Player::Finish) return false;
        Room *room = target->getRoom();

        ServerPlayer *player = room->askForPlayerChosen(target, room->getOtherPlayers(target), objectName(), "bossdidong-invoke", true, true);
        if (player) {
            target->broadcastSkillInvoke(objectName());
            player->turnOver();
        }
        return false;
    }
};

class BossShanbeng : public TriggerSkill
{
public:
    BossShanbeng() : TriggerSkill("bossshanbeng")
    {
        events << Death;
        frequency = Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target && target->hasSkill(this);
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        DeathStruct death = data.value<DeathStruct>();
        if (player != death.who) return false;

        player->broadcastSkillInvoke(objectName());
        room->sendCompulsoryTriggerLog(player, objectName());
        foreach (ServerPlayer *p, room->getOtherPlayers(player)) {
            if (p->getEquips().isEmpty()) continue;
            p->throwAllEquips();
        }
        return false;
    }
};

class BossBeiming : public TriggerSkill
{
public:
    BossBeiming() :TriggerSkill("bossbeiming")
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
        if (death.who != player)
            return false;
        ServerPlayer *killer = death.damage ? death.damage->from : NULL;
        if (killer && killer != player) {
            LogMessage log;
            log.type = "#BeimingThrow";
            log.from = player;
            log.to << killer;
            log.arg = objectName();
            room->sendLog(log);
            room->notifySkillInvoked(player, objectName());

            player->broadcastSkillInvoke(objectName());

            killer->throwAllHandCards();
        }

        return false;
    }
};

class BossLuolei : public PhaseChangeSkill
{
public:
    BossLuolei() : PhaseChangeSkill("bossluolei")
    {
		view_as_skill = new dummyVS;
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
        if (target->getPhase() != Player::Start) return false;
        Room *room = target->getRoom();

        ServerPlayer *player = room->askForPlayerChosen(target, room->getOtherPlayers(target), objectName(), "bossluolei-invoke", true, true);
        if (player) {
            target->broadcastSkillInvoke(objectName());
            room->damage(DamageStruct(objectName(), target, player, 1, DamageStruct::Thunder));
        }
        return false;
    }
};

class BossGuihuo : public PhaseChangeSkill
{
public:
    BossGuihuo() : PhaseChangeSkill("bossguihuo")
    {
		view_as_skill = new dummyVS;
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
        if (target->getPhase() != Player::Start) return false;
        Room *room = target->getRoom();

        ServerPlayer *player = room->askForPlayerChosen(target, room->getOtherPlayers(target), objectName(), "bossguihuo-invoke", true, true);
        if (player) {
            target->broadcastSkillInvoke(objectName());
            room->damage(DamageStruct(objectName(), target, player, 1, DamageStruct::Fire));
        }
        return false;
    }
};

class BossMingbao : public TriggerSkill
{
public:
    BossMingbao() : TriggerSkill("bossmingbao")
    {
        events << Death;
        frequency = Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target && target->hasSkill(this);
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        DeathStruct death = data.value<DeathStruct>();
        if (player != death.who) return false;

        player->broadcastSkillInvoke(objectName());
        room->sendCompulsoryTriggerLog(player, objectName());

        foreach(ServerPlayer *p, room->getOtherPlayers(player))
            room->damage(DamageStruct(objectName(), NULL, p, 1, DamageStruct::Fire));
        return false;
    }
};

class BossBaolian : public PhaseChangeSkill
{
public:
    BossBaolian() : PhaseChangeSkill("bossbaolian")
    {
        frequency = Compulsory;
    }

    virtual int getPriority(TriggerEvent) const
    {
        return 4;
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
        if (target->getPhase() != Player::Finish) return false;
        Room *room = target->getRoom();

        target->broadcastSkillInvoke(objectName());
        room->sendCompulsoryTriggerLog(target, objectName());

        target->drawCards(2, objectName());
        return false;
    }
};

class BossManjia : public TriggerSkill
{
public:
    BossManjia() : TriggerSkill("bossmanjia")
    {
        events << DamageInflicted << SlashEffected << CardEffected;
        frequency = Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return TriggerSkill::triggerable(target) && !target->getArmor() && target->hasArmorEffect("vine");
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == SlashEffected) {
            SlashEffectStruct effect = data.value<SlashEffectStruct>();
            if (effect.nature == DamageStruct::Normal) {
                player->broadcastSkillInvoke(objectName());
                room->sendCompulsoryTriggerLog(player, objectName());

                room->setEmotion(player, "armor/vine");
                LogMessage log;
                log.from = player;
                log.type = "#ArmorNullify";
                log.arg = objectName();
                log.arg2 = effect.slash->objectName();
                room->sendLog(log);

                effect.to->setFlags("Global_NonSkillNullify");
                return true;
            }
        } else if (triggerEvent == CardEffected) {
            CardEffectStruct effect = data.value<CardEffectStruct>();
            if (effect.card->isKindOf("AOE")) {
                player->broadcastSkillInvoke(objectName());
                room->sendCompulsoryTriggerLog(player, objectName());

                room->setEmotion(player, "armor/vine");
                LogMessage log;
                log.from = player;
                log.type = "#ArmorNullify";
                log.arg = objectName();
                log.arg2 = effect.card->objectName();
                room->sendLog(log);

                effect.to->setFlags("Global_NonSkillNullify");
                return true;
            }
        } else if (triggerEvent == DamageInflicted) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.nature == DamageStruct::Fire) {
                player->broadcastSkillInvoke(objectName());
                room->sendCompulsoryTriggerLog(player, objectName());

                room->setEmotion(player, "armor/vineburn");
                LogMessage log;
                log.type = "#VineDamage";
                log.from = player;
                log.arg = QString::number(damage.damage);
                log.arg2 = QString::number(++damage.damage);
                room->sendLog(log);

                data = QVariant::fromValue(damage);
            }
        }

        return false;
    }
};

class BossXiaoshou : public PhaseChangeSkill
{
public:
    BossXiaoshou() : PhaseChangeSkill("bossxiaoshou")
    {
		view_as_skill = new dummyVS;
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
        if (target->getPhase() != Player::Finish) return false;
        Room *room = target->getRoom();

        QList<ServerPlayer *> players;
        foreach (ServerPlayer *p, room->getAlivePlayers()) {
            if (p->getHp() > target->getHp())
                players << p;
        }
        if (players.isEmpty()) return false;
        ServerPlayer *player = room->askForPlayerChosen(target, players, objectName(), "bossxiaoshou-invoke", true, true);
        if (player) {
            player->broadcastSkillInvoke(objectName());
            room->damage(DamageStruct(objectName(), target, player, 2));
        }
        return false;
    }
};

class BossGuiji : public TriggerSkill
{
public:
    BossGuiji() : TriggerSkill("bossguiji")
    {
        events << EventPhaseEnd;
        frequency = Compulsory;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (player->getPhase() != Player::Start || player->getJudgingArea().isEmpty())
            return false;

        player->broadcastSkillInvoke(objectName());
        room->sendCompulsoryTriggerLog(player, objectName());

        QList<const Card *> dtricks = player->getJudgingArea();
        int index = qrand() % dtricks.length();
        room->throwCard(dtricks.at(index), NULL, player);
        return false;
    }
};

class BossLianyu : public PhaseChangeSkill
{
public:
    BossLianyu() : PhaseChangeSkill("bosslianyu")
    {
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
        if (target->getPhase() != Player::Finish)
            return false;

        Room *room = target->getRoom();
        if (room->askForSkillInvoke(target, objectName())) {
            target->broadcastSkillInvoke(objectName());
            foreach(ServerPlayer *p, room->getOtherPlayers(target))
                room->damage(DamageStruct(objectName(), target, p, 1, DamageStruct::Fire));
        }
        return false;
    }
};

class BossTaiping : public DrawCardsSkill
{
public:
    BossTaiping() : DrawCardsSkill("bosstaiping")
    {
        frequency = Compulsory;
    }

    virtual int getDrawNum(ServerPlayer *player, int n) const
    {
        Room *room = player->getRoom();

        player->broadcastSkillInvoke(objectName());
        room->sendCompulsoryTriggerLog(player, objectName());

        return n + 2;
    }
};

class BossSuoming : public PhaseChangeSkill
{
public:
    BossSuoming() : PhaseChangeSkill("bosssuoming")
    {
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
        if (target->getPhase() != Player::Finish) return false;
        Room *room = target->getRoom();

        QList<ServerPlayer *> to_chain;
        foreach (ServerPlayer *p, room->getOtherPlayers(target)) {
            if (!p->isChained())
                to_chain << p;
        }

        if (!to_chain.isEmpty() && room->askForSkillInvoke(target, objectName())) {
            target->broadcastSkillInvoke(objectName());

            foreach (ServerPlayer *p, to_chain) {
                if (p->isChained()) continue;
                p->setChained(true);
                room->broadcastProperty(p, "chained");
                room->setEmotion(p, "chain");
                room->getThread()->trigger(ChainStateChanged, room, p);
            }
        }
        return false;
    }
};

class BossXixing : public PhaseChangeSkill
{
public:
    BossXixing() : PhaseChangeSkill("bossxixing")
    {
		view_as_skill = new dummyVS;
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
        if (target->getPhase() != Player::Start) return false;
        Room *room = target->getRoom();

        QList<ServerPlayer *> chain;
        foreach (ServerPlayer *p, room->getOtherPlayers(target)) {
            if (p->isChained())
                chain << p;
        }
        if (chain.isEmpty()) return false;
        ServerPlayer *player = room->askForPlayerChosen(target, chain, objectName(), "bossxixing-invoke", true, true);
        if (player) {
            target->broadcastSkillInvoke(objectName());
            target->setFlags("bossxixing");
            try {
                room->damage(DamageStruct(objectName(), target, player, 1, DamageStruct::Thunder));
                if (target->isAlive() && target->hasFlag("bossxixing")) {
                    target->setFlags("-bossxixing");
                    if (target->isWounded())
                        room->recover(target, RecoverStruct(target));
                }
            }
            catch (TriggerEvent triggerEvent) {
                if (triggerEvent == TurnBroken || triggerEvent == StageChange)
                    target->setFlags("-bossxixing");
                throw triggerEvent;
            }
        }
        return false;
    }
};

class BossQiangzheng : public PhaseChangeSkill
{
public:
    BossQiangzheng() : PhaseChangeSkill("bossqiangzheng")
    {
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
        if (target->getPhase() != Player::Finish) return false;
        Room *room = target->getRoom();

        bool can_invoke = false;
        foreach (ServerPlayer *p, room->getOtherPlayers(target)) {
            if (!p->isKongcheng()) {
                can_invoke = true;
                break;
            }
        }
        if (!can_invoke) return false;

        if (room->askForSkillInvoke(target, objectName())) {
            target->broadcastSkillInvoke(objectName());
            foreach (ServerPlayer *p, room->getOtherPlayers(target)) {
                if (p->isAlive() && !p->isKongcheng()) {
                    int card_id = room->askForCardChosen(target, p, "h", "bossqiangzheng");

                    CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, target->objectName());
                    room->obtainCard(target, Sanguosha->getCard(card_id), reason, false);
                }
            }
        }
        return false;
    }
};

class BossZuijiu : public TriggerSkill
{
public:
    BossZuijiu() : TriggerSkill("bosszuijiu")
    {
        events << ConfirmDamage;
        frequency = Compulsory;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *, QVariant &data) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (damage.card && damage.card->isKindOf("Slash")) {
            LogMessage log;
            log.type = "#ZuijiuBuff";
            log.from = damage.from;
            log.to << damage.to;
            log.arg = QString::number(damage.damage);
            log.arg2 = QString::number(++damage.damage);
            room->sendLog(log);
            damage.from->broadcastSkillInvoke(objectName());
            room->notifySkillInvoked(damage.from, objectName());

            data = QVariant::fromValue(damage);
        }

        return false;
    }
};

class BossModao : public PhaseChangeSkill
{
public:
    BossModao() : PhaseChangeSkill("bossmodao")
    {
        frequency = Compulsory;
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
        if (target->getPhase() != Player::Start) return false;
        Room *room = target->getRoom();

        target->broadcastSkillInvoke(objectName());
        room->sendCompulsoryTriggerLog(target, objectName());

        target->drawCards(2, objectName());
        return false;
    }
};

class BossQushou : public PhaseChangeSkill
{
public:
    BossQushou() : PhaseChangeSkill("bossqushou")
    {
		frequency = Compulsory;
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
        if (target->getPhase() != Player::Play) return false;
        Room *room = target->getRoom();

        SavageAssault *sa = new SavageAssault(Card::NoSuit, 0);
        sa->setSkillName("_" + objectName());
        bool can_invoke = false;
        if (!target->isCardLimited(sa, Card::MethodUse)) {
            foreach (ServerPlayer *p, room->getOtherPlayers(target)) {
                if (!room->isProhibited(target, p, sa)) {
                    can_invoke = true;
                    break;
                }
            }
        }
        if (!can_invoke) {
            delete sa;
            return false;
        }
        room->sendCompulsoryTriggerLog(target, objectName());
        room->useCard(CardUseStruct(sa, target, QList<ServerPlayer *>()));
        return false;
    }
};

class BossMoyan : public TriggerSkill
{
public:
    BossMoyan() : TriggerSkill("bossmoyan")
    {
        events << CardsMoveOneTime;
        frequency = Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return TriggerSkill::triggerable(target) && target->getPhase() == Player::NotActive;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
        if (move.from == player && (move.from_places.contains(Player::PlaceHand) || move.from_places.contains(Player::PlaceEquip))
            && !(move.to == player && (move.to_place == Player::PlaceHand || move.to_place == Player::PlaceEquip))) {
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            JudgeStruct judge;
            judge.pattern = ".|red";
            judge.good = true;
            judge.reason = objectName();
            judge.who = player;
            room->judge(judge);
			if (judge.isGood()) {
				ServerPlayer *to = room->askForPlayerChosen(player, room->getOtherPlayers(player), objectName(), "moyan-damage");
				if (to)
					room->damage(DamageStruct(objectName(), player, to, 2, DamageStruct::Fire));
			}
        }
        return false;
    }
};

class BossMojian : public PhaseChangeSkill
{
public:
    BossMojian() : PhaseChangeSkill("bossmojian")
    {
		frequency = Compulsory;
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
        if (target->getPhase() != Player::Play) return false;
        Room *room = target->getRoom();

        ArcheryAttack *aa = new ArcheryAttack(Card::NoSuit, 0);
        aa->setSkillName("_" + objectName());
        bool can_invoke = false;
        if (!target->isCardLimited(aa, Card::MethodUse)) {
            foreach (ServerPlayer *p, room->getOtherPlayers(target)) {
                if (!room->isProhibited(target, p, aa)) {
                    can_invoke = true;
                    break;
                }
            }
        }
        if (!can_invoke) {
            delete aa;
            return false;
        }
        room->sendCompulsoryTriggerLog(target, objectName());
        room->useCard(CardUseStruct(aa, target, QList<ServerPlayer *>()));
        return false;
    }
};

class BossDanshu : public TriggerSkill
{
public:
    BossDanshu() : TriggerSkill("bossdanshu")
    {
        events << CardsMoveOneTime;
        frequency = Compulsory;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
        if (player != move.from || player->getPhase() != Player::NotActive
            || (!move.from_places.contains(Player::PlaceHand) && !move.from_places.contains(Player::PlaceEquip)))
            return false;
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());
        JudgeStruct judge;
        judge.who = player;
        judge.reason = objectName();
        judge.good = true;
        judge.pattern = ".|red";
        room->judge(judge);

        if (judge.isGood() && player->isAlive() && player->isWounded())
            room->recover(player, RecoverStruct(player));

        return false;
    }
};

class BossYingzi : public DrawCardsSkill
{
public:
    BossYingzi() : DrawCardsSkill("bossyingzi")
    {
        frequency = Compulsory;
    }

    virtual int getDrawNum(ServerPlayer *player, int n) const
    {
        Room *room = player->getRoom();
        LogMessage log;
        log.type = "#YongsiGood";
        log.from = player;
        log.arg = "2";
        log.arg2 = objectName();
        room->sendLog(log);
        room->notifySkillInvoked(player, objectName());
        player->broadcastSkillInvoke(objectName());
        return n + 2;
    }
};

class BossMengtai : public TriggerSkill
{
public:
    BossMengtai() : TriggerSkill("bossmengtai")
    {
        events << EventPhaseStart << EventPhaseChanging;
		frequency = Compulsory;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseChanging) {
			if (data.value<PhaseChangeStruct>().to != Player::Discard) return false;
            if (player->hasSkipped(Player::Play) && !player->isSkipped(Player::Discard)) {
				room->sendCompulsoryTriggerLog(player, objectName());
                player->broadcastSkillInvoke(objectName());
				player->skip(Player::Discard);
			}
		} else if (triggerEvent == EventPhaseStart) {
			if (player->getPhase() != Player::Finish) return false;
            if (player->hasSkipped(Player::Draw)) {
				room->sendCompulsoryTriggerLog(player, objectName());
                player->broadcastSkillInvoke(objectName());
				player->drawCards(3, objectName());
			}
		}
        return false;
    }
};

class BossDongmian : public ProhibitSkill
{
public:
    BossDongmian() : ProhibitSkill("bossdongmian")
    {
    }

    bool isProhibited(const Player *from, const Player *to, const Card *card, const QList<const Player *> & /* = QList<const Player *>() */) const
    {
        if (card->getTypeId() == Card::TypeSkill)
            return false;

        return to->hasSkill(objectName()) && from && from != to && to->getHp() <= 6;
    }
};

class RuizhiSelect : public ViewAsSkill
{
public:
    RuizhiSelect() : ViewAsSkill("ruizhi_select")
    {
        response_pattern = "@@ruizhi_select!";
    }

    virtual bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
		return selected.isEmpty() || (selected.length() == 1 && to_select->isEquipped() != selected.first()->isEquipped());
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const
    {
		bool ok = false;
        if (cards.length() == 1) {
            if (cards.first()->isEquipped())
				ok = Self->isKongcheng();
			else
			    ok = !Self->hasEquip();
        } else if (cards.length() == 2) {
            ok = true;
		}

        if (!ok)
            return NULL;

        DummyCard *dummy = new DummyCard;
        dummy->addSubcards(cards);
        return dummy;
    }
};

class BossRuizhi : public PhaseChangeSkill
{
public:
    BossRuizhi() : PhaseChangeSkill("bossruizhi")
    {
        frequency = Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->getPhase() == Player::Start;
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
        Room *room = target->getRoom();
		foreach (ServerPlayer *nien, room->getOtherPlayers(target)) {
            if (target->isDead() || (target->getHandcardNum() < 2 && target->getEquips().length() < 2)) break;
			if (!TriggerSkill::triggerable(nien)) continue;
            nien->broadcastSkillInvoke(objectName());
            room->sendCompulsoryTriggerLog(nien, objectName());
			room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, nien->objectName(), target->objectName());
			QList<int> to_remain;
			if (!target->isKongcheng())
				to_remain << target->handCards().first();
			if (target->hasEquip())
                to_remain << target->getEquips().first()->getEffectiveId();
			const Card *card = room->askForCard(target, "@@ruizhi_select!", "@ruizhi-select:" + nien->objectName(), QVariant(), Card::MethodNone);
			if (card != NULL) {
				to_remain = card->getSubcards();
			}
            DummyCard *to_discard = new DummyCard;
            to_discard->deleteLater();
			foreach (const Card *c, target->getCards("he")) {
                if (!target->isJilei(c) && !to_remain.contains(c->getEffectiveId()))
                    to_discard->addSubcard(c);
            }
			if (to_discard->subcardsLength() > 0) {
				CardMoveReason mreason(CardMoveReason::S_REASON_THROW, target->objectName(), QString(), objectName(), QString());
                room->throwCard(to_discard, mreason, target);
			}
        }
        return false;
    }
};

class BossJingjue : public TriggerSkill
{
public:
    BossJingjue() : TriggerSkill("bossjingjue")
    {
        events << CardsMoveOneTime;
		frequency = Compulsory;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
        if (player->isWounded() && move.from && move.from == player
                && ((move.reason.m_reason & CardMoveReason::S_MASK_BASIC_REASON) == CardMoveReason::S_REASON_DISCARD
                || (move.to && move.to != player && move.to_place == Player::PlaceHand
                && move.reason.m_reason != CardMoveReason::S_REASON_GIVE))) {
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            room->recover(player, RecoverStruct(player));
        }
        return false;
    }
};

class BossRenxing : public TriggerSkill
{
public:
    BossRenxing() : TriggerSkill("bossrenxing")
    {
        events << HpRecover << Damaged;
		frequency = Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *, QVariant &data) const
    {
		int n = 0;
		if (triggerEvent == HpRecover) {
			n = data.value<RecoverStruct>().recover;
		} else if (triggerEvent == Damaged) {
			n = data.value<DamageStruct>().damage;
		}
		for (int i = 0; i < n; i++) {
		    foreach (ServerPlayer *nien, room->getAlivePlayers()) {
                if (!TriggerSkill::triggerable(nien) || nien->getPhase() != Player::NotActive) continue;
				room->sendCompulsoryTriggerLog(nien, objectName());
                nien->broadcastSkillInvoke(objectName());
				nien->drawCards(1, objectName());
			}
        }
        return false;
    }
};

class BossBaonu : public TriggerSkill
{
public:
    BossBaonu() : TriggerSkill("bossbaonu")
    {
        events << DrawNCards << CardUsed << ConfirmDamage;
		frequency = Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->isAlive();
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
		if (triggerEvent == DrawNCards) {
			if (TriggerSkill::triggerable(player)) {
				room->sendCompulsoryTriggerLog(player, objectName());
                player->broadcastSkillInvoke(objectName());
				int n = player->getHp()-4;
			    if (n < 1)
				    data = 4;
			    else
				    data = qrand() % n + 5;
			}
		} else if (triggerEvent == CardUsed) {
			CardUseStruct use = data.value<CardUseStruct>();
            if (TriggerSkill::triggerable(use.from) && use.from->getHp() < 5) {
                if (use.card != NULL && use.card->isKindOf("Slash"))
                    use.card->setFlags("baonu_slash");
            }
		} else if (triggerEvent == ConfirmDamage) {
			DamageStruct damage = data.value<DamageStruct>();
            if (damage.card != NULL && damage.card->hasFlag("baonu_slash")) {
                ++damage.damage;
                data = QVariant::fromValue(damage);
            }
		}
        return false;
    }
};

class BaonuTarget : public TargetModSkill
{
public:
    BaonuTarget() : TargetModSkill("#baonu-target")
    {
    }

    virtual int getResidueNum(const Player *from, const Card *, const Player *) const
    {
        if (from->hasSkill("bossbaonu") && from->getHp() < 5)
            return 1000;
        else
            return 0;
    }
};

class BossShouyi : public TargetModSkill
{
public:
    BossShouyi() : TargetModSkill("bossshouyi")
    {
        pattern = "^SkillCard";
    }

    virtual int getDistanceLimit(const Player *from, const Card *, const Player *) const
    {
        if (from->hasSkill(this))
            return 1000;
        else
            return 0;
    }
};

class BossChange : public TriggerSkill
{
public:
    BossChange() : TriggerSkill("#bosschange")
    {
        events << RoundStart;
		global = true;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &, ServerPlayer * &) const
    {
        if (player->getGeneralName() == "best_nien") return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *, ServerPlayer *best_nien, QVariant &, ServerPlayer *) const
    {
        BossChangeState(best_nien);
        return false;
    }

private:
    static void BossChangeState(ServerPlayer *boss)
    {
        Room *room = boss->getRoom();
        QString last_state = "bossdongmian";
		switch (boss->getMark("BossCurrentState")) {
        case 1: last_state = "bossruizhi"; break;
        case 2: last_state = "bossjingjue"; break;
        case 3: last_state = "bossrenxing"; break;
        case 4: last_state = "bossbaonu"; break;
	    default:
			break;
        }
		QString current_state = "bossdongmian";
		switch (boss->getMark("BossNextState")) {
        case 1: current_state = "bossruizhi"; break;
        case 2: current_state = "bossjingjue"; break;
		case 3: current_state = "bossrenxing"; break;
        case 4: current_state = "bossbaonu"; break;
	    default:
			break;
        }
		boss->setMark("BossCurrentState", boss->getMark("BossNextState"));
		QStringList boss_states = boss->tag["BossStates"].toStringList();
		if (boss_states.isEmpty()) {
			boss_states << "bossruizhi";
			boss_states << "bossjingjue";
			boss_states << "bossrenxing";
			boss_states << "bossbaonu";
		}
		QString next_state = boss_states.takeAt(qrand() % boss_states.length());
		boss->tag["BossStates"] = QVariant::fromValue(boss_states);
		
		if (next_state == "bossruizhi")
			boss->setMark("BossNextState", 1);
		else if (next_state == "bossjingjue")
			boss->setMark("BossNextState", 2);
		else if (next_state == "bossrenxing")
			boss->setMark("BossNextState", 3);
		else if (next_state == "bossbaonu")
			boss->setMark("BossNextState", 4);
		else
			boss->setMark("BossNextState", 0);

		LogMessage log;
        log.type = "#BossChange";
        log.from = boss;
        log.arg = current_state;
	    log.arg2 = next_state;
        room->sendLog(log);
        room->removePlayerTip(boss, "#" + last_state);
        room->addPlayerTip(boss, "#" + current_state);

        if (current_state == "bossbaonu")
            current_state = "bossbaonu|bossshouyi";
        else if (current_state == "bossruizhi")
            current_state = "bossruizhi|weimu";

        if (last_state == "bossbaonu")
            last_state = "-bossbaonu|-bossshouyi";
        else if (last_state == "bossruizhi")
            last_state = "-bossruizhi|-weimu";
        else
            last_state = "-" + last_state;

	    room->handleAcquireDetachSkills(boss, last_state + "|" + current_state);
    }
};

class BossXiongshou : public TriggerSkill
{
public:
    BossXiongshou() : TriggerSkill("bossxiongshou")
    {
        events << DamageCaused;
        frequency = Compulsory;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *boss, QVariant &data) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (boss->getHp() > damage.to->getHp() && damage.card && damage.card->isKindOf("Slash")) {
            boss->broadcastSkillInvoke(objectName());
            room->sendCompulsoryTriggerLog(boss, objectName());
            damage.damage++;
            data = QVariant::fromValue(damage);
        }

        return false;
    }
};

class BossXiongshouDistance : public DistanceSkill
{
public:
    BossXiongshouDistance() : DistanceSkill("#bossxiongshou-distance")
    {
    }

    virtual int getCorrect(const Player *from, const Player *) const
    {
        if (from->hasSkill(this))
            return -1;
        else
            return 0;
    }
};

class BossWake : public TriggerSkill
{
public:
    BossWake() : TriggerSkill("#bosswake")
    {
        events << HpChanged << MaxHpChanged;
        global = true;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->getHp() <= target->getMaxHp()/2 && target->getGeneralName().startsWith("fierce");
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        foreach (const QString &skill_name, player->getGeneral()->getRelatedSkillNames()) {
            const Skill *skill = Sanguosha->getSkill(skill_name);
            if (skill && !player->hasSkill(skill, true))
                room->acquireSkill(player, skill_name);
        }
        return false;
    }
};

class BossWuzang : public DrawCardsSkill
{
public:
    BossWuzang() : DrawCardsSkill("#bosswuzang-draw")
    {
        frequency = Compulsory;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target && target->isAlive() && target->hasSkill("bosswuzang");
    }

    virtual int getDrawNum(ServerPlayer *player, int n) const
    {
        if (n <= 0) return n;
        player->getRoom()->sendCompulsoryTriggerLog(player, "bosswuzang");
        player->broadcastSkillInvoke("bosswuzang");
        return qMax(player->getHp() / 2, 5);
    }
};

class BossWuzangKeep : public MaxCardsSkill
{
public:
    BossWuzangKeep() : MaxCardsSkill("bosswuzang")
    {
    }

    virtual int getFixed(const Player *target) const
    {
        if (target->hasSkill(objectName()))
            return 0;
        else
            return -1;
    }
};

class BossXiangde : public TriggerSkill
{
public:
    BossXiangde() : TriggerSkill("bossxiangde")
    {
        events << DamageInflicted;
        frequency = Compulsory;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (!damage.from || !damage.from->getWeapon()) return false;
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());
        damage.damage++;
        data = QVariant::fromValue(damage);
        return false;
    }
};

class BossYinzei : public MasochismSkill
{
public:
    BossYinzei() : MasochismSkill("bossyinzei")
    {
        frequency = Compulsory;
    }

    virtual void onDamaged(ServerPlayer *boss, const DamageStruct &damage) const
    {
        ServerPlayer *from = damage.from;
        Room *room = boss->getRoom();
        if (boss->isKongcheng() && from && from->isAlive()) {
            QList<int> ids = from->forceToDiscard(1, true);
            if (ids.isEmpty()) return;
            room->sendCompulsoryTriggerLog(boss, objectName());
            boss->broadcastSkillInvoke(objectName());
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, boss->objectName(), from->objectName());
            room->throwCard(ids.first(), from);
        }
    }
};

class BossZhue : public TriggerSkill
{
public:
    BossZhue() : TriggerSkill("bosszhue")
    {
        events << Damage;
        frequency = Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *target, QVariant &) const
    {
        foreach (ServerPlayer *boss, room->getOtherPlayers(target)) {
            if (!TriggerSkill::triggerable(boss)) continue;
            room->sendCompulsoryTriggerLog(boss, objectName());
            boss->broadcastSkillInvoke(objectName());
            if (target->isAlive())
                room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, boss->objectName(), target->objectName());
            boss->drawCards(1, objectName());
            target->drawCards(1, objectName());
        }
        return false;
    }
};

class BossFutai : public TriggerSkill
{
public:
    BossFutai() : TriggerSkill("bossfutai")
    {
        events << EventPhaseStart << AskForPeaches;
        frequency = Compulsory;
    }

    int getPriority(TriggerEvent triggerEvent) const
    {
        if (triggerEvent == AskForPeaches)
            return 1;
        else
            return TriggerSkill::getPriority(triggerEvent);
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseStart && player->getPhase() == Player::RoundStart && TriggerSkill::triggerable(player)) {
            QList<ServerPlayer *> woundeds;
            foreach (ServerPlayer *p, room->getAlivePlayers()) {
                if (p->isWounded())
                    woundeds << p;
            }
            if (woundeds.isEmpty()) return false;
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            foreach (ServerPlayer *p, woundeds)
                room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), p->objectName());
            foreach (ServerPlayer *p, woundeds)
                room->recover(p, RecoverStruct(player));
        } else if (triggerEvent == AskForPeaches) {
            DyingStruct dying_data = data.value<DyingStruct>();
            if (dying_data.who == player) return false;
            foreach (ServerPlayer *p, room->findPlayersBySkillName(objectName())) {
                if (p->getPhase() == Player::NotActive && p != player) {
                    room->setTag("SkipGameRule", true);
                    break;
                }
            }
        }
        return false;
    }
};

class BossFutaiCardLimited : public CardLimitedSkill
{
public:
    BossFutaiCardLimited() : CardLimitedSkill("#bossfutai-limited")
    {
    }
    virtual bool isCardLimited(const Player *player, const Card *card, Card::HandlingMethod method) const
    {
        if (!card->isKindOf("Peach") || method != Card::MethodUse) return false;
        foreach(const Player *sib, player->getAliveSiblings()) {
            if (sib->hasSkill("bossfutai") && sib->getPhase() == Player::NotActive)
                return true;
        }
        return false;
    }
};

class BossYandu : public PhaseChangeSkill
{
public:
    BossYandu() : PhaseChangeSkill("bossyandu")
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
        if (player->getPhase() == Player::NotActive && player->getMark("damage_point_round") == 0) {
            foreach (ServerPlayer *p, room->getOtherPlayers(player)) {
                if (TriggerSkill::triggerable(p) && player->isAlive() && !player->isNude()) {
                    room->sendCompulsoryTriggerLog(p, objectName());
                    p->broadcastSkillInvoke(objectName());
                    room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, p->objectName(), player->objectName());
                    int card_id = room->askForCardChosen(p, player, "he", objectName());
                    CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, p->objectName());
                    room->obtainCard(p, Sanguosha->getCard(card_id),
                        reason, room->getCardPlace(card_id) != Player::PlaceHand);
                }
            }
        }
        return false;
    }
};

class BossMingwan : public TriggerSkill
{
public:
    BossMingwan() : TriggerSkill("bossmingwan")
    {
        events << Damage << CardUsed << CardResponded << EventPhaseStart;
        frequency = Compulsory;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == Damage) {
            DamageStruct damage = data.value<DamageStruct>();
            QStringList assignee_list = player->property("bossmingwan_targets").toString().split("+");
            if (TriggerSkill::triggerable(player) && !assignee_list.contains(damage.to->objectName()) && player->getPhase() != Player::NotActive) {
                room->sendCompulsoryTriggerLog(player, objectName());
                player->broadcastSkillInvoke(objectName());
                room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), damage.to->objectName());
                assignee_list << damage.to->objectName();
                room->setPlayerProperty(player, "bossmingwan_targets", assignee_list.join("+"));
            }
        } else if (triggerEvent == EventPhaseStart) {
            if (player->getPhase() == Player::NotActive)
                room->setPlayerProperty(player, "bossmingwan_targets", QString());
        } else {
            const Card *card = NULL;
            if (triggerEvent == CardUsed)
                card = data.value<CardUseStruct>().card;
            else {
                CardResponseStruct response = data.value<CardResponseStruct>();
                if (response.m_isUse)
                    card = response.m_card;
            }
            if (card && card->getTypeId() != Card::TypeSkill && card->getHandlingMethod() == Card::MethodUse) {
                if (player->property("bossmingwan_targets").toString().isEmpty()) return false;
                room->sendCompulsoryTriggerLog(player, objectName());
                player->broadcastSkillInvoke(objectName());
                player->drawCards(1, objectName());
            }
        }
        return false;
    }
};

class BossMingwanProhibit : public ProhibitSkill
{
public:
    BossMingwanProhibit() : ProhibitSkill("#bossmingwan-prohibit")
    {
    }

    bool isProhibited(const Player *from, const Player *to, const Card *card, const QList<const Player *> & /* = QList<const Player *>() */) const
    {
        if (!from || card->isKindOf("SkillCard") || from->property("bossmingwan_targets").toString().isEmpty()
                || !to || from == to) return false;
        return !from->property("bossmingwan_targets").toString().split("+").contains(to->objectName());
    }
};

class BossNitai : public TriggerSkill
{
public:
    BossNitai() : TriggerSkill("bossnitai")
    {
        events << DamageInflicted;
        frequency = Compulsory;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (player->getPhase() == Player::NotActive && damage.nature != DamageStruct::Fire) return false;
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());
        if (player->getPhase() == Player::NotActive) {
            damage.damage++;
            data = QVariant::fromValue(damage);
        } else
            return true;
        return false;
    }
};

class BossLuanchang : public TriggerSkill
{
public:
    BossLuanchang() : TriggerSkill("bossluanchang")
    {
        events << EventPhaseStart << EventPhaseChanging;
        frequency = Compulsory;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *boss, QVariant &data) const
    {
        if (triggerEvent == EventPhaseChanging) {
            if (data.value<PhaseChangeStruct>().to != Player::NotActive) return false;
            ArcheryAttack *aa = new ArcheryAttack(Card::NoSuit, 0);
            aa->setSkillName("_" + objectName());
            if (!aa->isAvailable(boss)) return false;
            room->sendCompulsoryTriggerLog(boss, objectName());
            boss->broadcastSkillInvoke(objectName());
            room->useCard(CardUseStruct(aa, boss, QList<ServerPlayer *>()));

        } else if (triggerEvent == EventPhaseStart) {
            if (boss->getPhase() != Player::RoundStart) return false;
            SavageAssault *sa = new SavageAssault(Card::NoSuit, 0);
            sa->setSkillName("_" + objectName());
            if (!sa->isAvailable(boss)) return false;
            room->sendCompulsoryTriggerLog(boss, objectName());
            boss->broadcastSkillInvoke(objectName());
            room->useCard(CardUseStruct(sa, boss, QList<ServerPlayer *>()));
        }
        return false;
    }
};

class BossTanyu : public TriggerSkill
{
public:
    BossTanyu() : TriggerSkill("bosstanyu")
    {
        events << EventPhaseStart << EventPhaseChanging;
        frequency = Compulsory;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseChanging) {
            if (data.value<PhaseChangeStruct>().to != Player::Discard || player->isSkipped(Player::Discard)) return false;
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            player->skip(Player::Discard);
        } else if (triggerEvent == EventPhaseStart) {
            if (player->getPhase() != Player::Finish) return false;
            foreach (ServerPlayer *p, room->getOtherPlayers(player)) {
                if (p->getHandcardNum() > player->getHandcardNum())
                    return false;
            }
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            room->loseHp(player);

        }
        return false;
    }
};

class BossCangmu : public DrawCardsSkill
{
public:
    BossCangmu() : DrawCardsSkill("bosscangmu")
    {
        frequency = Compulsory;
    }

    virtual int getDrawNum(ServerPlayer *player, int n) const
    {
        if (n <= 0) return n;
        player->getRoom()->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());
        return player->getRoom()->alivePlayerCount();
    }
};

class BossJicai : public TriggerSkill
{
public:
    BossJicai() : TriggerSkill("bossjicai")
    {
        events << HpRecover;
        frequency = Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->isAlive();
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        foreach (ServerPlayer *boss, room->getAlivePlayers()) {
            if (!TriggerSkill::triggerable(boss)) continue;
            room->sendCompulsoryTriggerLog(boss, objectName());
            boss->broadcastSkillInvoke(objectName());
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, boss->objectName(), player->objectName());
            boss->drawCards(1, objectName());
            if (boss != player)
                player->drawCards(1, objectName());
        }
        return false;
    }
};

BossModePackage::BossModePackage()
    : Package("BossMode")
{
    General *chi = new General(this, "boss_chi", "god", 5, true, true);
    chi->addSkill(new BossGuimei);
    chi->addSkill(new BossDidong);
    chi->addSkill(new BossShanbeng);

    General *mei = new General(this, "boss_mei", "god", 5, true, true);
    mei->addSkill("bossguimei");
    mei->addSkill("nosenyuan");
    mei->addSkill(new BossBeiming);

    General *wang = new General(this, "boss_wang", "god", 5, true, true);
    wang->addSkill("bossguimei");
    wang->addSkill(new BossLuolei);
    wang->addSkill("huilei");

    General *liang = new General(this, "boss_liang", "god", 5, true, true);
    liang->addSkill("bossguimei");
    liang->addSkill(new BossGuihuo);
    liang->addSkill(new BossMingbao);

    General *niutou = new General(this, "boss_niutou", "god", 7, true, true);
    niutou->addSkill(new BossBaolian);
    niutou->addSkill("niepan");
    niutou->addSkill(new BossManjia);
    niutou->addSkill(new BossXiaoshou);

    General *mamian = new General(this, "boss_mamian", "god", 6, true, true);
    mamian->addSkill(new BossGuiji);
    mamian->addSkill("nosfankui");
    mamian->addSkill(new BossLianyu);
    mamian->addSkill("nosjuece");

    General *heiwuchang = new General(this, "boss_heiwuchang", "god", 9, true, true);
    heiwuchang->addSkill("bossguiji");
    heiwuchang->addSkill(new BossTaiping);
    heiwuchang->addSkill(new BossSuoming);
    heiwuchang->addSkill(new BossXixing);

    General *baiwuchang = new General(this, "boss_baiwuchang", "god", 9, true, true);
    baiwuchang->addSkill("bossbaolian");
    baiwuchang->addSkill(new BossQiangzheng);
    baiwuchang->addSkill(new BossZuijiu);
    baiwuchang->addSkill("nosjuece");

    General *luocha = new General(this, "boss_luocha", "god", 12, false, true);
    luocha->addSkill(new BossModao);
    luocha->addSkill(new BossQushou);
    luocha->addSkill("yizhong");
    luocha->addSkill(new BossMoyan);

    General *yecha = new General(this, "boss_yecha", "god", 11, true, true);
    yecha->addSkill("bossmodao");
    yecha->addSkill(new BossMojian);
    yecha->addSkill("bazhen");
    yecha->addSkill(new BossDanshu);

	General *nian = new General(this, "best_nien", "qun", 12, true, true);
	nian->addSkill(new BossYingzi);
    nian->addSkill(new BossMengtai);
	nian->addSkill(new BossChange);
	related_skills.insertMulti("bossbaonu", "#baonu-target");
    nian->addRelateSkill("bossdongmian");
    nian->addRelateSkill("bossruizhi");
    nian->addRelateSkill("weimu");
    nian->addRelateSkill("bossjingjue");
    nian->addRelateSkill("bossrenxing");
    nian->addRelateSkill("bossbaonu");
    nian->addRelateSkill("bossshouyi");

    General *boss_zhuyin = new General(this, "fierce_zhuyin", "qun", 4, true, true);
    boss_zhuyin->addSkill(new BossXiongshou);
    boss_zhuyin->addSkill(new BossXiongshouDistance);
    related_skills.insertMulti("bossxiongshou", "#bossxiongshou-distance");

    General *boss_hundun = new General(this, "fierce_hundun", "qun", 20, true, true);
    boss_hundun->addSkill("bossxiongshou");
    boss_hundun->addSkill(new BossWuzang);
    boss_hundun->addSkill(new BossWuzangKeep);
    related_skills.insertMulti("bosswuzang", "#bosswuzang-keep");
    boss_hundun->addSkill(new BossXiangde);
    boss_hundun->addRelateSkill("bossyinzei");

    General *boss_qiongqi = new General(this, "fierce_qiongqi", "qun", 16, true, true);
    boss_qiongqi->addSkill("bossxiongshou");
    boss_qiongqi->addSkill(new BossZhue);
    boss_qiongqi->addSkill(new BossFutai);
    boss_qiongqi->addSkill(new BossFutaiCardLimited);
    related_skills.insertMulti("bossfutai", "#bossfutai-limited");
    boss_qiongqi->addRelateSkill("bossyandu");

    General *boss_taowu = new General(this, "fierce_taowu", "qun", 16, true, true);
    boss_taowu->addSkill("bossxiongshou");
    boss_taowu->addSkill(new BossMingwan);
    boss_taowu->addSkill(new BossMingwanProhibit);
    related_skills.insertMulti("bossmingwan", "#bossmingwan-prohibit");
    boss_taowu->addSkill(new BossNitai);
    boss_taowu->addRelateSkill("bossluanchang");

    General *boss_taotie = new General(this, "fierce_taotie", "qun", 20, true, true);
    boss_taotie->addSkill("bossxiongshou");
    boss_taotie->addSkill(new BossTanyu);
    boss_taotie->addSkill(new BossCangmu);
    boss_taotie->addRelateSkill("bossjicai");

    skills << new BossDongmian << new BossRuizhi << new RuizhiSelect << new BossJingjue
           << new BossRenxing << new BossBaonu << new BaonuTarget << new BossShouyi
           << new BossWake << new BossYinzei << new BossYandu
           << new BossLuanchang << new BossJicai;
}

ADD_PACKAGE(BossMode)
