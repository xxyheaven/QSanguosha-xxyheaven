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

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Finish;
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
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

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (player == NULL || !player->hasSkill(objectName())) return QStringList();
        DeathStruct death = data.value<DeathStruct>();

        if (death.who == player)
            return QStringList(objectName());

        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        player->broadcastSkillInvoke(objectName());
        room->sendCompulsoryTriggerLog(player, objectName());
        QList<ServerPlayer *> players = room->getOtherPlayers(player);
        foreach (ServerPlayer *p, players)
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), p->objectName());
        foreach (ServerPlayer *p, players)
            p->throwAllEquips();
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

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (player == NULL || !player->hasSkill(objectName())) return QStringList();
        DeathStruct death = data.value<DeathStruct>();

        if (death.who == player && death.damage && death.damage->from && death.damage->from->isAlive())
            return QStringList(objectName());

        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        DeathStruct death = data.value<DeathStruct>();

        ServerPlayer *killer = death.damage ? death.damage->from : NULL;
        if (killer && killer != player) {
            LogMessage log;
            log.type = "#BeimingThrow";
            log.from = player;
            log.to << killer;
            log.arg = objectName();
            room->sendLog(log);
            room->notifySkillInvoked(player, objectName());
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), killer->objectName());

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

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Start;
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
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

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Start;
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
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

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (player == NULL || !player->hasSkill(objectName())) return QStringList();
        DeathStruct death = data.value<DeathStruct>();

        if (death.who == player)
            return QStringList(objectName());

        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        player->broadcastSkillInvoke(objectName());
        room->sendCompulsoryTriggerLog(player, objectName());
        QList<ServerPlayer *> players = room->getOtherPlayers(player);
        foreach (ServerPlayer *p, players)
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), p->objectName());
        foreach (ServerPlayer *p, players)
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

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Finish;
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
        Room *room = target->getRoom();

        target->broadcastSkillInvoke(objectName());
        room->sendCompulsoryTriggerLog(target, objectName());

        target->drawCards(2, objectName());
        return false;
    }
};

class BossManjia : public ViewHasSkill
{
public:
    BossManjia() : ViewHasSkill("bossmanjia")
    {
    }
    virtual bool ViewHas(const Player *player, const QString &skill_name, const QString &flag) const
    {
        if (flag == "armor" && skill_name == "vine" && player->isAlive() && player->hasSkill(objectName()) && !player->getArmor())
            return true;
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

    virtual QStringList triggerable(TriggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer* &) const
    {
        if (!PhaseChangeSkill::triggerable(player)) return QStringList();
        if (player->getPhase() == Player::Finish) {
            QList<ServerPlayer *> players = room->getOtherPlayers(player);
            foreach(ServerPlayer *p, players)
                if (p->getHp() > player->getHp())
                    return QStringList(objectName());
        }

        return QStringList();
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
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

class BossGuiji : public PhaseChangeSkill
{
public:
    BossGuiji() : PhaseChangeSkill("bossguiji")
    {
        frequency = Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return TriggerSkill::triggerable(target) && target->getPhase() == Player::Start && !target->getJudgingArea().isEmpty();
    }

    virtual bool onPhaseChange(ServerPlayer *player) const
    {
        Room *room = player->getRoom();

        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());

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
        frequency = Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Finish;
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
        Room *room = target->getRoom();
        room->sendCompulsoryTriggerLog(target, objectName());
        target->broadcastSkillInvoke(objectName());

        QList<ServerPlayer *> players = room->getOtherPlayers(target);
        foreach (ServerPlayer *p, players)
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, target->objectName(), p->objectName());
        foreach (ServerPlayer *p, players)
            room->damage(DamageStruct(objectName(), target, p, 1, DamageStruct::Fire));

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
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer* &) const
    {
        if (!PhaseChangeSkill::triggerable(player)) return QStringList();
        if (player->getPhase() == Player::Finish) {
            QList<ServerPlayer *> players = room->getOtherPlayers(player);
            foreach(ServerPlayer *p, players)
                if (!p->isChained())
                    return QStringList(objectName());
        }

        return QStringList();
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
        Room *room = target->getRoom();
        room->sendCompulsoryTriggerLog(target, objectName());
        target->broadcastSkillInvoke(objectName());

        QList<ServerPlayer *> players = room->getOtherPlayers(target);

        foreach (ServerPlayer *p, players) {
            if (!p->isChained())
                room->setPlayerProperty(p, "chained", true);
        }
        return false;
    }
};

class BossXixing : public PhaseChangeSkill
{
public:
    BossXixing() : PhaseChangeSkill("bossxixing")
    {
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer* &) const
    {
        if (!PhaseChangeSkill::triggerable(player)) return QStringList();
        if (player->getPhase() == Player::Start) {
            QList<ServerPlayer *> players = room->getOtherPlayers(player);
            foreach(ServerPlayer *p, players)
                if (p->isChained())
                    return QStringList(objectName());
        }

        return QStringList();
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
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
            room->damage(DamageStruct(objectName(), target, player, 1, DamageStruct::Thunder));

            if (target->isWounded())
                room->recover(target, RecoverStruct(target));

        }
        return false;
    }
};

class BossQiangzheng : public PhaseChangeSkill
{
public:
    BossQiangzheng() : PhaseChangeSkill("bossqiangzheng")
    {
        frequency = Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Finish;
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
        Room *room = target->getRoom();
        room->sendCompulsoryTriggerLog(target, objectName());
        target->broadcastSkillInvoke(objectName());

        QList<ServerPlayer *> players = room->getOtherPlayers(target);
        foreach (ServerPlayer *p, players)
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, target->objectName(), p->objectName());
        foreach (ServerPlayer *p, players) {
            if (target->isAlive() && target->canGetCard(p, "h")) {
                const Card *card = p->getRandomHandCard();
                CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, target->objectName());
                room->obtainCard(target, card, reason, false);
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
        events << PreCardUsed;
        frequency = Compulsory;
    }

    virtual void record(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == PreCardUsed && TriggerSkill::triggerable(player)) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card != NULL && use.card->isKindOf("Slash"))
                use.card->setTag("addcardinality", use.card->tag["addcardinality"].toInt() + 1);
        }
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *, QVariant &, ServerPlayer* &) const
    {
        return QStringList();
    }

};

class BossModao : public PhaseChangeSkill
{
public:
    BossModao() : PhaseChangeSkill("bossmodao")
    {
        frequency = Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Start;
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
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

    virtual bool triggerable(const ServerPlayer *target) const
    {
        if (PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Play) {
            SavageAssault *sa = new SavageAssault(Card::NoSuit, 0);
            sa->setSkillName("_" + objectName());
            if (sa->isAvailable(target))
                return true;
        }
        return false;
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
        Room *room = target->getRoom();
        room->sendCompulsoryTriggerLog(target, objectName());
        target->broadcastSkillInvoke(objectName());
        SavageAssault *sa = new SavageAssault(Card::NoSuit, 0);
        sa->setSkillName("_" + objectName());
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

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (TriggerSkill::triggerable(player) && player->getPhase() == Player::NotActive) {
            QVariantList move_datas = data.toList();
            foreach(QVariant move_data, move_datas) {
                CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
                if (move.from == player && (move.from_places.contains(Player::PlaceHand) || move.from_places.contains(Player::PlaceEquip))
                    && !(move.to == player && (move.to_place == Player::PlaceHand || move.to_place == Player::PlaceEquip))) {
                    return QStringList(objectName());
                }
            }
        }

        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
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

    virtual bool triggerable(const ServerPlayer *target) const
    {
        if (PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Play) {
            ArcheryAttack *aa = new ArcheryAttack(Card::NoSuit, 0);
            aa->setSkillName("_" + objectName());
            if (aa->isAvailable(target))
                return true;
        }
        return false;
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
        Room *room = target->getRoom();
        room->sendCompulsoryTriggerLog(target, objectName());
        target->broadcastSkillInvoke(objectName());
        ArcheryAttack *aa = new ArcheryAttack(Card::NoSuit, 0);
        aa->setSkillName("_" + objectName());
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

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (TriggerSkill::triggerable(player) && player->getPhase() == Player::NotActive) {
            QVariantList move_datas = data.toList();
            foreach(QVariant move_data, move_datas) {
                CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
                if (move.from == player && (move.from_places.contains(Player::PlaceHand) || move.from_places.contains(Player::PlaceEquip))
                    && !(move.to == player && (move.to_place == Player::PlaceHand || move.to_place == Player::PlaceEquip))) {
                    return QStringList(objectName());
                }
            }
        }

        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());
        JudgeStruct judge;
        judge.pattern = ".|red";
        judge.good = true;
        judge.reason = objectName();
        judge.who = player;
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

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        if (triggerEvent == EventPhaseChanging) {
            if (data.value<PhaseChangeStruct>().to == Player::Discard &&
                    player->hasSkipped(Player::Play) && !player->isSkipped(Player::Discard))
                return QStringList(objectName());
        } else if (triggerEvent == EventPhaseStart) {
            if (player->getPhase() == Player::Finish && player->hasSkipped(Player::Draw))
                return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());
        if (triggerEvent == EventPhaseChanging)
            player->skip(Player::Discard);
        else if (triggerEvent == EventPhaseStart)
            player->drawCards(3, objectName());
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

    virtual TriggerList triggerable(TriggerEvent , Room *room, ServerPlayer *player, QVariant &) const
    {
        TriggerList skill_list;
        if (player == NULL || player->isDead() || (player->getHandcardNum() < 2 && player->getEquips().length() < 2)) return skill_list;
        if (player->getPhase() != Player::Start) return skill_list;
        QList<ServerPlayer *> niens = room->findPlayersBySkillName(objectName());
        foreach (ServerPlayer *nien, niens) {
            if (nien != player)
                skill_list.insert(nien, QStringList(objectName()));
        }

        return skill_list;
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *target, QVariant &, ServerPlayer *nien) const
    {
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

        return false;
    }

    virtual bool onPhaseChange(ServerPlayer *) const
    {
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

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (!TriggerSkill::triggerable(player) || !player->isWounded()) return QStringList();
        QVariantList move_datas = data.toList();
        foreach(QVariant move_data, move_datas) {
            CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
            if (move.from && move.from == player
                    && ((move.reason.m_reason & CardMoveReason::S_MASK_BASIC_REASON) == CardMoveReason::S_REASON_DISCARD
                    || (move.to && move.to != player && move.to_place == Player::PlaceHand
                    && move.reason.m_reason != CardMoveReason::S_REASON_GIVE))) {
                return QStringList(objectName());
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());
        room->recover(player, RecoverStruct(player));
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

    virtual TriggerList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *, QVariant &data) const
    {
        TriggerList skill_list;
        int n = 0;
        if (triggerEvent == HpRecover) {
            n = data.value<RecoverStruct>().recover;
        } else if (triggerEvent == Damaged) {
            n = data.value<DamageStruct>().damage;
        }
        if (n > 0) {
            QList<ServerPlayer *> niens = room->findPlayersBySkillName(objectName());
            foreach (ServerPlayer *nien, niens) {
                if (nien->getPhase() == Player::NotActive) {
                    QStringList trigger_list;
                    for (int i = 1; i <= n; i++) {
                        trigger_list << objectName();
                    }
                    skill_list.insert(nien, trigger_list);
                }
            }
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *, QVariant &, ServerPlayer *nien) const
    {
        room->sendCompulsoryTriggerLog(nien, objectName());
        nien->broadcastSkillInvoke(objectName());
        nien->drawCards(1, objectName());
        return false;
    }
};

class BossBaonu : public TriggerSkill
{
public:
    BossBaonu() : TriggerSkill("bossbaonu")
    {
        events << DrawNCards << PreCardUsed;
		frequency = Compulsory;
    }

    virtual void record(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == PreCardUsed && TriggerSkill::triggerable(player) && player->getHp() < 5) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card != NULL && use.card->isKindOf("Slash"))
                use.card->setTag("addcardinality", use.card->tag["addcardinality"].toInt() + 1);
        }
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &, ServerPlayer* &) const
    {
        if (triggerEvent == DrawNCards && TriggerSkill::triggerable(player)) return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());
        int n = player->getHp()-4;
        if (n < 1)
            data = 4;
        else
            data = qrand() % n + 5;

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

    //    virtual int getPriority(TriggerEvent triggerEvent) const
    //    {
    //        if (triggerEvent == DamageCaused)
    //            return 5;
    //        return TriggerSkill::getPriority(triggerEvent);
    //    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *boss, QVariant &data, ServerPlayer * &) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (TriggerSkill::triggerable(boss) && damage.to && boss->getHp() > damage.to->getHp() && damage.card && damage.card->isKindOf("Slash"))
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *boss, QVariant &data, ServerPlayer *) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        boss->broadcastSkillInvoke(objectName());
        room->sendCompulsoryTriggerLog(boss, objectName());
        damage.damage++;
        data = QVariant::fromValue(damage);
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

    //    virtual int getPriority(TriggerEvent triggerEvent) const
    //    {
    //        if (triggerEvent == DamageInflicted)
    //            return 5;
    //        return TriggerSkill::getPriority(triggerEvent);
    //    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *boss, QVariant &data, ServerPlayer * &) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (TriggerSkill::triggerable(boss) && damage.from && damage.from->getWeapon()) return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *boss, QVariant &data, ServerPlayer *) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        boss->broadcastSkillInvoke(objectName());
        room->sendCompulsoryTriggerLog(boss, objectName());
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

    virtual QStringList triggerable(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (TriggerSkill::triggerable(player) && player->isKongcheng()) {
            DamageStruct damage = data.value<DamageStruct>();
            ServerPlayer *from = damage.from;
            if (from && from->isAlive() && player->canDiscard(from, "he"))
                return QStringList(objectName());
        }

        return QStringList();
    }

    virtual void onDamaged(ServerPlayer *boss, const DamageStruct &damage) const
    {
        Room *room = boss->getRoom();
        room->sendCompulsoryTriggerLog(boss, objectName());
        boss->broadcastSkillInvoke(objectName());
        ServerPlayer *target = damage.from;
        if (target && target->isAlive()) {
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, boss->objectName(), target->objectName());
            QList<int> ids;
            QList<const Card *> allcards = target->getCards("he");
            foreach (const Card *card, allcards) {
                if (boss->canDiscard(target, card->getEffectiveId()))
                    ids << card->getEffectiveId();
            }
            if (!ids.isEmpty()) {
                int card_id = ids.at(qrand() % ids.length());
                room->throwCard(card_id, target, boss);
            }
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

    virtual TriggerList triggerable(TriggerEvent , Room *room, ServerPlayer *target, QVariant &) const
    {
        TriggerList skill_list;
        QList<ServerPlayer *> bosses = room->findPlayersBySkillName(objectName());
        foreach (ServerPlayer *boss, bosses) {
            if (boss != target)
                skill_list.insert(boss, QStringList(objectName()));
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *target, QVariant &, ServerPlayer *boss) const
    {
        room->sendCompulsoryTriggerLog(boss, objectName());
        boss->broadcastSkillInvoke(objectName());
        if (target->isAlive())
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, boss->objectName(), target->objectName());
        boss->drawCards(1, objectName());
        target->drawCards(1, objectName());
        return false;
    }
};

class BossFutai : public PhaseChangeSkill
{
public:
    BossFutai() : PhaseChangeSkill("bossfutai")
    {
        frequency = Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        if (PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::RoundStart) {
            Room *room = target->getRoom();
            foreach (ServerPlayer *p, room->getAlivePlayers()) {
                if (p->isWounded())
                    return true;
            }
        }
        return false;
    }

    virtual bool onPhaseChange(ServerPlayer *player) const
    {
        Room *room = player->getRoom();
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

class BossYandu : public TriggerSkill
{
public:
    BossYandu() : TriggerSkill("bossyandu")
    {
        events << EventPhaseChanging;
        frequency = Compulsory;
    }

    virtual TriggerList triggerable(TriggerEvent , Room *room, ServerPlayer *target, QVariant &data) const
    {
        TriggerList skill_list;
        if (data.value<PhaseChangeStruct>().to == Player::NotActive && target->getMark("damage_point_round") == 0) {
            QList<ServerPlayer *> bosses = room->findPlayersBySkillName(objectName());
            foreach (ServerPlayer *boss, bosses) {
                if (boss != target && boss->canGetCard(target, "he"))
                    skill_list.insert(boss, QStringList(objectName()));
            }
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *target, QVariant &, ServerPlayer *boss) const
    {
        room->sendCompulsoryTriggerLog(boss, objectName());
        boss->broadcastSkillInvoke(objectName());
        if (target->isAlive())
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, boss->objectName(), target->objectName());

        QList<int> ids;
        QList<const Card *> allcards = target->getCards("hej");
        foreach (const Card *card, allcards) {
            if (boss->canGetCard(target, card->getEffectiveId()))
                ids << card->getEffectiveId();
        }
        if (!ids.isEmpty()) {
            int card_id = ids.at(qrand() % ids.length());
            CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, boss->objectName());
            room->obtainCard(boss, Sanguosha->getCard(card_id), reason, false);
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

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (triggerEvent == EventPhaseStart && player->getPhase() == Player::NotActive)
            room->setPlayerProperty(player, "bossmingwan_targets", QVariant());
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (triggerEvent == Damage) {
            if (TriggerSkill::triggerable(player) && player->getPhase() != Player::NotActive) {
                DamageStruct damage = data.value<DamageStruct>();
                QStringList assignee_list = player->property("bossmingwan_targets").toString().split("+");
                if (damage.to && damage.to->isAlive() && !assignee_list.contains(damage.to->objectName()))
                    return QStringList(objectName());
            }
        } else if (triggerEvent == CardUsed || triggerEvent == CardResponded) {
            if (!player->property("bossmingwan_targets").toString().isEmpty()) {
                const Card *cardstar = NULL;
                if (triggerEvent == CardUsed) {
                    CardUseStruct use = data.value<CardUseStruct>();
                    cardstar = use.card;
                } else {
                    CardResponseStruct resp = data.value<CardResponseStruct>();
                    if (resp.m_isUse)
                        cardstar = resp.m_card;
                }
                if (cardstar && cardstar->getTypeId() != Card::TypeSkill && cardstar->getHandlingMethod() == Card::MethodUse)
                    return QStringList(objectName());
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        if (triggerEvent == Damage) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.to || damage.to->isDead()) return false;
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), damage.to->objectName());

            QStringList assignee_list = player->property("bossmingwan_targets").toString().split("+");
            assignee_list << damage.to->objectName();
            room->setPlayerProperty(player, "bossmingwan_targets", assignee_list.join("+"));

        } else {
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            player->drawCards(1, objectName());
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

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *boss, QVariant &data, ServerPlayer * &) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (TriggerSkill::triggerable(boss) && (boss->getPhase() != Player::NotActive || damage.nature == DamageStruct::Fire))
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());
        if (player->getPhase() == Player::NotActive) {
            damage.damage++;
            data = QVariant::fromValue(damage);
        } else {
            room->preventDamage(damage);
            return true;
        }
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

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *boss, QVariant &data, ServerPlayer * &) const
    {
        if (TriggerSkill::triggerable(boss)) {
            if (triggerEvent == EventPhaseChanging && data.value<PhaseChangeStruct>().to == Player::NotActive) {
                ArcheryAttack *aa = new ArcheryAttack(Card::NoSuit, 0);
                aa->setSkillName("_" + objectName());
                if (aa->isAvailable(boss)) return QStringList(objectName());
            } else if (triggerEvent == EventPhaseStart && boss->getPhase() == Player::RoundStart) {
                SavageAssault *sa = new SavageAssault(Card::NoSuit, 0);
                sa->setSkillName("_" + objectName());
                if (sa->isAvailable(boss)) return QStringList(objectName());
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *boss, QVariant &, ServerPlayer *) const
    {
        if (triggerEvent == EventPhaseChanging) {
            ArcheryAttack *aa = new ArcheryAttack(Card::NoSuit, 0);
            aa->setSkillName("_" + objectName());
            if (!aa->isAvailable(boss)) return false;
            room->sendCompulsoryTriggerLog(boss, objectName());
            boss->broadcastSkillInvoke(objectName());
            room->useCard(CardUseStruct(aa, boss, QList<ServerPlayer *>()));

        } else if (triggerEvent == EventPhaseStart) {
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

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *boss, QVariant &data, ServerPlayer * &) const
    {
        if (TriggerSkill::triggerable(boss)) {
            if (triggerEvent == EventPhaseChanging) {
                if (data.value<PhaseChangeStruct>().to == Player::Discard && !boss->isSkipped(Player::Discard))
                    return QStringList(objectName());
            } else if (triggerEvent == EventPhaseStart && boss->getPhase() == Player::Finish) {
                QList<ServerPlayer *> players = room->getOtherPlayers(boss);
                foreach (ServerPlayer *p, players) {
                    if (p->getHandcardNum() > boss->getHandcardNum())
                        return QStringList();
                }
                return QStringList(objectName());
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *boss, QVariant &, ServerPlayer *) const
    {
        room->sendCompulsoryTriggerLog(boss, objectName());
        boss->broadcastSkillInvoke(objectName());
        if (triggerEvent == EventPhaseChanging)
            boss->skip(Player::Discard);
        else if (triggerEvent == EventPhaseStart)
            room->loseHp(boss);
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

    virtual TriggerList triggerable(TriggerEvent , Room *room, ServerPlayer *target, QVariant &) const
    {
        TriggerList skill_list;
        QList<ServerPlayer *> bosses = room->findPlayersBySkillName(objectName());
        foreach (ServerPlayer *boss, bosses)
            skill_list.insert(boss, QStringList(objectName()));
        return skill_list;
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *target, QVariant &, ServerPlayer *boss) const
    {
        room->sendCompulsoryTriggerLog(boss, objectName());
        boss->broadcastSkillInvoke(objectName());
        if (target && target->isAlive())
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, boss->objectName(), target->objectName());
        boss->drawCards(1, objectName());
        if (boss != target && target && target->isAlive())
            target->drawCards(1, objectName());
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
