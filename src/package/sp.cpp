#include "sp.h"
#include "client.h"
#include "general.h"
#include "skill.h"
#include "standard-skillcards.h"
#include "engine.h"
#include "maneuvering.h"

#include "settings.h"
#include "json.h"

class dummyVS : public ZeroCardViewAsSkill
{
public:
    dummyVS() : ZeroCardViewAsSkill("dummy")
    {
    }

    const Card *viewAs() const
    {
        return NULL;
    }
};

class Jilei : public TriggerSkill
{
public:
    Jilei() : TriggerSkill("jilei")
    {
        events << Damaged;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *yangxiu, QVariant &data) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        ServerPlayer *current = room->getCurrent();
        if (!current || current->getPhase() == Player::NotActive || current->isDead() || !damage.from)
            return false;

        if (room->askForSkillInvoke(yangxiu, objectName(), QVariant::fromValue(damage.from))) {
            yangxiu->broadcastSkillInvoke(objectName());
			room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, yangxiu->objectName(), damage.from->objectName());
            QString choice = room->askForChoice(yangxiu, objectName(), "BasicCard+EquipCard+TrickCard");
            

            LogMessage log;
            log.type = "#Jilei";
            log.from = damage.from;
            log.arg = choice;
            room->sendLog(log);

            QStringList jilei_list = damage.from->tag[objectName()].toStringList();
            if (jilei_list.contains(choice)) return false;
            jilei_list.append(choice);
            damage.from->tag[objectName()] = QVariant::fromValue(jilei_list);
            QString _type = choice + "|.|.|hand"; // Handcards only
            room->setPlayerCardLimitation(damage.from, "use,response,discard", _type, true);

            QString type_name = choice.replace("Card", "").toLower();
            room->addPlayerTip(damage.from, "#jilei_" + type_name);
        }

        return false;
    }
};

class JileiClear : public TriggerSkill
{
public:
    JileiClear() : TriggerSkill("#jilei-clear")
    {
        events << EventPhaseChanging << Death;
    }

    int getPriority(TriggerEvent) const
    {
        return 5;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *target, QVariant &data) const
    {
        if (triggerEvent == EventPhaseChanging) {
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (change.to != Player::NotActive)
                return false;
        } else if (triggerEvent == Death) {
            DeathStruct death = data.value<DeathStruct>();
            if (death.who != target || target != room->getCurrent())
                return false;
        }
        QList<ServerPlayer *> players = room->getAllPlayers();
        foreach (ServerPlayer *player, players) {
            QStringList jilei_list = player->tag["jilei"].toStringList();
            if (!jilei_list.isEmpty()) {
                LogMessage log;
                log.type = "#JileiClear";
                log.from = player;
                room->sendLog(log);

                foreach (QString jilei_type, jilei_list) {
                    room->removePlayerCardLimitation(player, "use,response,discard", jilei_type + "|.|.|hand$1");
                    QString type_name = jilei_type.replace("Card", "").toLower();
                    room->removePlayerTip(player, "#jilei_" + type_name);
                }
                player->tag.remove("jilei");
            }
        }

        return false;
    }
};

class Danlao : public TriggerSkill
{
public:
    Danlao() : TriggerSkill("danlao")
    {
        events << TargetConfirmed;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.to.length() <= 1 || !use.to.contains(player)
            || !use.card->isKindOf("TrickCard")
            || !room->askForSkillInvoke(player, objectName(), data))
            return false;

        player->broadcastSkillInvoke(objectName());
        player->setFlags("-DanlaoTarget");
        player->setFlags("DanlaoTarget");
        player->drawCards(1, objectName());
        if (player->isAlive() && player->hasFlag("DanlaoTarget")) {
            player->setFlags("-DanlaoTarget");
            use.nullified_list << player->objectName();
            data = QVariant::fromValue(use);
        }
        return false;
    }
};

/*
class SpJixi : public TriggerSkill
{
public:
    SpJixi() : TriggerSkill("spjixi")
    {
        events << EventPhaseChanging;
		frequency = Wake;
	}

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->hasSkill(objectName())
            && target->isAlive() && target->getMark("jixi_turns") > 1 && !target->hasFlag("JixiHasLoseHp")
            && target->getMark(objectName()) == 0;
    }

    virtual bool trigger(TriggerEvent , Room *room, ServerPlayer *yuanshu, QVariant &data) const
    {
		if (data.value<PhaseChangeStruct>().to == Player::NotActive) {
            room->sendCompulsoryTriggerLog(yuanshu, objectName());
            yuanshu->broadcastSkillInvoke(objectName());
			room->setPlayerMark(yuanshu, objectName(), 1);
            if (room->changeMaxHpForAwakenSkill(yuanshu, 1)) {
                room->recover(yuanshu, RecoverStruct(yuanshu));
                if (room->askForChoice(yuanshu, objectName(), "wangzun+draw", data) == "wangzun")
					room->acquireSkill(yuanshu, "wangzun");
				else {
					yuanshu->drawCards(2);
					if (!isNormalGameMode(room->getMode())) return false;
                    ServerPlayer *the_lord = room->getLord();
					if (the_lord == NULL) return false;
					QStringList acquireList;
					foreach (const Skill *skill, the_lord->getVisibleSkillList()) {
						if (!skill->isLordSkill()) continue;
						QString skill_name = skill->objectName();
						if (skill->getFrequency() == Skill::Wake && the_lord->getMark(skill_name) > 0) continue;
						if (skill->getFrequency() == Skill::Limited && the_lord->getMark(skill->getLimitMark()) < 1) continue;
						acquireList.append(skill_name);
					}
					if (!acquireList.isEmpty())
						room->handleAcquireDetachSkills(yuanshu, acquireList);
				}
            }
        }
        return false;
    }
};

class SpJixiRecord : public TriggerSkill
{
public:
    SpJixiRecord() : TriggerSkill("#spjixi-record")
    {
        events << EventPhaseChanging << HpLost;
        global = true;
    }

    int getPriority(TriggerEvent triggerEvent) const
    {
        if (triggerEvent == EventPhaseChanging)
            return 1;

        return TriggerSkill::getPriority(triggerEvent);
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *yuanshu, QVariant &data) const
    {
		if (triggerEvent == HpLost && yuanshu->getPhase() != Player::NotActive) {
            room->setPlayerFlag(yuanshu, "JixiHasLoseHp");
			room->setPlayerMark(yuanshu, "jixi_turns", 0);
		} else if (triggerEvent == EventPhaseChanging && !yuanshu->hasFlag("JixiHasLoseHp")) {
			if (data.value<PhaseChangeStruct>().to == Player::NotActive) {
				room->addPlayerMark(yuanshu, "jixi_turns");
			}
		}
        return false;
    }
};
*/

class Nuzhan : public TriggerSkill
{
public:
    Nuzhan() : TriggerSkill("nuzhan")
    {
        events << PreCardUsed << CardUsed << ConfirmDamage;
        frequency = Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->isAlive();
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *, QVariant &data) const
    {
        if (triggerEvent == PreCardUsed) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (TriggerSkill::triggerable(use.from)) {
                if (use.card != NULL && use.card->isKindOf("Slash") && use.card->isVirtualCard() && use.card->subcardsLength() == 1 && Sanguosha->getCard(use.card->getSubcards().first())->isKindOf("TrickCard") && use.m_addHistory) {
                    room->addPlayerHistory(use.from, use.card->getClassName(), -1);
                    use.m_addHistory = false;
                    data = QVariant::fromValue(use);
                }
            }
        } else if (triggerEvent == CardUsed) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (TriggerSkill::triggerable(use.from)) {
                if (use.card != NULL && use.card->isKindOf("Slash") && use.card->isVirtualCard() && use.card->subcardsLength() == 1 && Sanguosha->getCard(use.card->getSubcards().first())->isKindOf("EquipCard"))
                    use.card->setFlags("nuzhan_slash");
            }
        } else if (triggerEvent == ConfirmDamage) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.card != NULL && damage.card->hasFlag("nuzhan_slash")) {
                ++damage.damage;
                data = QVariant::fromValue(damage);
            }
        }
        return false;
    }
};

class Danji : public PhaseChangeSkill
{
public:
    Danji() : PhaseChangeSkill("danji")
    {
        frequency = Wake;
    }

    virtual bool triggerable(const ServerPlayer *guanyu, Room *room) const
    {
        return PhaseChangeSkill::triggerable(guanyu) && guanyu->getMark(objectName()) == 0 && guanyu->getPhase() == Player::Start && guanyu->getHandcardNum() > guanyu->getHp() && !lordIsLiubei(room);
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
        Room *room = target->getRoom();
		room->sendCompulsoryTriggerLog(target, objectName());
        target->broadcastSkillInvoke(objectName());
        room->setPlayerMark(target, objectName(), 1);
        if (room->changeMaxHpForAwakenSkill(target) && target->getMark(objectName()) > 0)
            room->handleAcquireDetachSkills(target, "mashu|nuzhan");

        return false;
    }

private:
    static bool lordIsLiubei(const Room *room)
    {
        if (room->getLord() != NULL) {
            const ServerPlayer *const lord = room->getLord();
            if (lord->getGeneral() && lord->getGeneralName().contains("liubei"))
                return true;

            if (lord->getGeneral2() && lord->getGeneral2Name().contains("liubei"))
                return true;
        }

        return false;
    }
};

ShichouCard::ShichouCard()
{
}

bool ShichouCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    QStringList available_targets = Self->property("shichou_available_targets").toString().split("+");
    return targets.length() < Self->getLostHp() && available_targets.contains(to_select->objectName());
}

void ShichouCard::use(Room *, ServerPlayer *, QList<ServerPlayer *> &targets) const
{
	foreach (ServerPlayer *p, targets)
	    p->setFlags("ShichouExtraTarget");
}

class ShichouViewAsSkill : public ZeroCardViewAsSkill
{
public:
    ShichouViewAsSkill() : ZeroCardViewAsSkill("shichou")
    {
		response_pattern = "@@shichou";
    }

    virtual const Card *viewAs() const
    {
        return new ShichouCard;
    }
};

class Shichou : public TriggerSkill
{
public:
    Shichou() : TriggerSkill("shichou")
    {
        events << TargetChosen;
		view_as_skill = new ShichouViewAsSkill;
    }

    virtual bool trigger(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data) const
    {
		CardUseStruct use = data.value<CardUseStruct>();
		if (use.card->isKindOf("Slash") && !use.card->hasFlag("slashDisableExtraTarget") && player->isWounded()) {
			QStringList available_targets;
			bool no_distance_limit = false;
			if (use.card->hasFlag("slashNoDistanceLimit")){
				no_distance_limit = true;
				room->setPlayerFlag(player, "slashNoDistanceLimit");
			}
			foreach (ServerPlayer *p, room->getAlivePlayers()) {
                if (use.to.contains(p) || room->isProhibited(player, p, use.card)) continue;
                if (use.card->targetFilter(QList<const Player *>(), p, player))
                    available_targets << p->objectName();
            }
			if (no_distance_limit)
				room->setPlayerFlag(player, "-slashNoDistanceLimit");

			if (available_targets.isEmpty())
				return false;
			room->setPlayerProperty(player, "shichou_available_targets", available_targets.join("+"));
			player->tag["shichou-use"] = data;
			room->askForUseCard(player, "@@shichou", "@shichou-add:::" + QString::number(player->getLostHp()));
			player->tag.remove("shichou-use");
			room->setPlayerProperty(player, "shichou_available_targets", QString());
			foreach (ServerPlayer *p, room->getAllPlayers()) {
                if (p->hasFlag("ShichouExtraTarget")) {
                    p->setFlags("-ShichouExtraTarget");
					use.to.append(p);
                }
            }
			room->sortByActionOrder(use.to);
		    data = QVariant::fromValue(use); 
		}
        return false;
    }
};

class Zhenlve : public ProhibitSkill
{
public:
    Zhenlve() : ProhibitSkill("zhenlve")
    {
    }

    virtual bool isProhibited(const Player *, const Player *to, const Card *card, const QList<const Player *> &) const
    {
        return to->hasSkill(this) && card->isKindOf("DelayedTrick");
    }
};

class ZhenlveTrick : public TriggerSkill
{
public:
    ZhenlveTrick() : TriggerSkill("#zhenlve-trick")
    {
        events << PreCardUsed << TrickCardCanceling;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *, ServerPlayer *, QVariant &data) const
    {
		if (triggerEvent == PreCardUsed) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.from && use.from->isAlive() && use.from->hasSkill("zhenlve")) {
                if (use.card != NULL && use.card->isNDTrick())
                    use.card->setFlags("ZhenlveEffect");
            }
        } else if (triggerEvent == TrickCardCanceling) {
			if (data.value<CardEffectStruct>().card->hasFlag("ZhenlveEffect"))
				return true;
        }
        return false;
    }
};

JianshuCard::JianshuCard()
{
    will_throw = false;
	will_sort = false;
    handling_method = Card::MethodNone;
}

bool JianshuCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return to_select != Self && (targets.isEmpty() || (targets.length() == 1 && to_select->inMyAttackRange(targets.first()) && !to_select->isKongcheng()));
}

bool JianshuCard::targetsFeasible(const QList<const Player *> &targets, const Player *) const
{
    return targets.length() == 2;
}

void JianshuCard::extraCost(Room *room, const CardUseStruct &card_use) const
{
	room->removePlayerMark(card_use.from, "@alienation");
	CardMoveReason reason(CardMoveReason::S_REASON_GIVE, card_use.from->objectName(), card_use.to.first()->objectName(), "jianshu", QString());
    room->obtainCard(card_use.to.first(), this, reason);
}

void JianshuCard::use(Room *room, ServerPlayer *, QList<ServerPlayer *> &targets) const
{
	ServerPlayer *from = targets.at(0);
    ServerPlayer *to = targets.at(1);
    if (from->canPindian(to)) {
        to->setFlags("JianshuPindianTarget");
		bool success = from->pindian(to, "jianshu", NULL);
        to->setFlags("-JianshuPindianTarget");
		if (from->hasFlag("JianshuSamePoint")){
			from->setFlags("-JianshuSamePoint");
			room->sortByActionOrder(targets);
			foreach (ServerPlayer *p, targets) {
				room->loseHp(p);
			}
		}else{
		    ServerPlayer *winner = NULL;
		    ServerPlayer *loser = NULL;
		    if (success) {
			    winner = from;
			    loser = to;
		    }else{
			    winner = to;
			    loser = from;
		    }
		    room->askForDiscard(winner, "jianshu", 2, 2, false, true);
			room->loseHp(loser);
		}
	}
}

class JianshuViewAsSkill : public OneCardViewAsSkill
{
public:
    JianshuViewAsSkill() : OneCardViewAsSkill("jianshu")
    {
        filter_pattern = ".|black|.|hand";
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return player->getMark("@alienation") > 0;
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        JianshuCard *skillcard = new JianshuCard;
        skillcard->addSubcard(originalCard);
        return skillcard;
    }
};

class Jianshu : public TriggerSkill
{
public:
    Jianshu() : TriggerSkill("jianshu")
    {
        events << Pindian;
        view_as_skill = new JianshuViewAsSkill;
		frequency = Limited;
        limit_mark = "@alienation";
    }

    int getPriority(TriggerEvent) const
    {
        return -1;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent, Room *, ServerPlayer *, QVariant &data) const
    {
        PindianStruct *pindian = data.value<PindianStruct *>();
        if (pindian->reason != objectName() || pindian->from_number != pindian->to_number)
            return false;

        pindian->from->setFlags("JianshuSamePoint");

        return false;
    }
};

class Yongdi : public TriggerSkill
{
public:
    Yongdi() : TriggerSkill("yongdi")
    {
        events << Damaged;
		frequency = Limited;
        limit_mark = "@advocacy";
    }

    bool trigger(TriggerEvent , Room *room, ServerPlayer *player, QVariant &) const
    {
		if (player->getMark(limit_mark) <= 0) return false;
		QList<ServerPlayer *> males;
        foreach (ServerPlayer *player, room->getOtherPlayers(player)) {
            if (player->isMale())
                males << player;
        }
        if (males.isEmpty()) return false;
        ServerPlayer *target = room->askForPlayerChosen(player, males, objectName(), "@yongdi", true, true);
		if (target) {
			room->removePlayerMark(player, limit_mark);
            player->broadcastSkillInvoke(objectName());

            LogMessage log;
            log.type = "#GainMaxHp";
            log.from = target;
            log.arg = "1";
            log.arg2 = QString::number(target->getMaxHp() + 1);
            room->sendLog(log);
            room->setPlayerProperty(target, "maxhp", target->getMaxHp() + 1);

			if (!target->isLord()) {
				QStringList skill_names;
                const General *general = target->getGeneral();
                foreach (const Skill *skill, general->getVisibleSkillList()) {
                    if (skill->isLordSkill())
                        skill_names << skill->objectName();
                }
				const General *general2 = target->getGeneral2();
				if (general2) {
					foreach (const Skill *skill, general2->getVisibleSkillList()) {
						if (skill->isLordSkill())
							skill_names << skill->objectName();
					}
				}
				if (!skill_names.isEmpty())
					room->handleAcquireDetachSkills(target, skill_names, true);
			}
		}
        return false;
    }
};

YuanhuCard::YuanhuCard()
{
    will_throw = false;
    handling_method = Card::MethodNone;
}

bool YuanhuCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *) const
{
    return targets.isEmpty() && to_select->canSetEquip(Sanguosha->getCard(getEffectiveId()));
}

void YuanhuCard::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *caohong = effect.from;
    Room *room = caohong->getRoom();
    room->moveCardTo(this, caohong, effect.to, Player::PlaceEquip,
        CardMoveReason(CardMoveReason::S_REASON_PUT, caohong->objectName(), "yuanhu", QString()));

    const Card *card = Sanguosha->getCard(subcards.first());

    LogMessage log;
    log.type = "$PutEquip";
    log.from = effect.to;
    log.card_str = QString::number(card->getEffectiveId());
    room->sendLog(log);

    if (card->isKindOf("Weapon")) {
        QList<ServerPlayer *> targets;
        foreach (ServerPlayer *p, room->getAllPlayers()) {
            if (effect.to->distanceTo(p) == 1 && caohong->canDiscard(p, "hej"))
                targets << p;
        }
        if (!targets.isEmpty()) {
            ServerPlayer *to_dismantle = room->askForPlayerChosen(caohong, targets, "yuanhu", "@yuanhu-discard:" + effect.to->objectName());
            int card_id = room->askForCardChosen(caohong, to_dismantle, "hej", "yuanhu", false, Card::MethodDiscard);
            room->throwCard(Sanguosha->getCard(card_id), to_dismantle, caohong);
        }
    } else if (card->isKindOf("Armor")) {
        effect.to->drawCards(1, "yuanhu");
    } else if (card->isKindOf("Horse")) {
        room->recover(effect.to, RecoverStruct(effect.from));
    }
}

class YuanhuViewAsSkill : public OneCardViewAsSkill
{
public:
    YuanhuViewAsSkill() : OneCardViewAsSkill("yuanhu")
    {
        filter_pattern = "EquipCard";
        response_pattern = "@@yuanhu";
    }

    const Card *viewAs(const Card *originalcard) const
    {
        YuanhuCard *first = new YuanhuCard;
        first->addSubcard(originalcard->getId());
        first->setSkillName(objectName());
        return first;
    }
};

class Yuanhu : public PhaseChangeSkill
{
public:
    Yuanhu() : PhaseChangeSkill("yuanhu")
    {
        view_as_skill = new YuanhuViewAsSkill;
    }

    bool onPhaseChange(ServerPlayer *target) const
    {
        Room *room = target->getRoom();
        if (target->getPhase() == Player::Finish && !target->isNude())
            room->askForUseCard(target, "@@yuanhu", "@yuanhu-equip", QVariant(), Card::MethodNone);
        return false;
    }

    int getEffectIndex(const ServerPlayer *, const Card *card) const
    {
        int index = -1;
		if (card->isKindOf("YuanhuCard")) {
			const Card *subcard = Sanguosha->getCard(card->getSubcards().first());
            if (subcard->isKindOf("Weapon"))
                index = 1;
            else if (subcard->isKindOf("Armor"))
                index = 2;
            else if (subcard->isKindOf("Horse"))
                index = 3;
		}
        return index;
    }
};

class Moukui : public TriggerSkill
{
public:
    Moukui() : TriggerSkill("moukui")
    {
        events << TargetSpecified << SlashMissed << CardFinished;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == TargetSpecified && TriggerSkill::triggerable(player)) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (!use.card->isKindOf("Slash"))
                return false;
            foreach (ServerPlayer *p, use.to) {
                if (player->askForSkillInvoke(this, QVariant::fromValue(p))) {
                    player->broadcastSkillInvoke(objectName());
					room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), p->objectName());
                    QString choice;
                    if (!player->canDiscard(p, "he"))
                        choice = "draw";
                    else
                        choice = room->askForChoice(player, objectName(), "draw+discard", QVariant::fromValue(p));
                    if (choice == "draw") {
                        player->drawCards(1, objectName());
                    } else {
                        room->setTag("MoukuiDiscard", data);
                        int disc = room->askForCardChosen(player, p, "he", objectName(), false, Card::MethodDiscard);
                        room->removeTag("MoukuiDiscard");
                        room->throwCard(disc, p, player);
                    }
                    room->addPlayerMark(p, objectName() + use.card->toString());
                }
            }
        } else if (triggerEvent == SlashMissed) {
            SlashEffectStruct effect = data.value<SlashEffectStruct>();
            if (effect.to->isDead() || effect.to->getMark(objectName() + effect.slash->toString()) <= 0)
                return false;
            if (!effect.from->isAlive() || !effect.to->isAlive() || !effect.to->canDiscard(effect.from, "he"))
                return false;
			room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, effect.to->objectName(), effect.from->objectName());
            int disc = room->askForCardChosen(effect.to, effect.from, "he", objectName(), false, Card::MethodDiscard);
            room->throwCard(disc, effect.from, effect.to);
            room->removePlayerMark(effect.to, objectName() + effect.slash->toString());
        } else if (triggerEvent == CardFinished) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (!use.card->isKindOf("Slash"))
                return false;
            foreach(ServerPlayer *p, room->getAllPlayers())
                room->setPlayerMark(p, objectName() + use.card->toString(), 0);
        }

        return false;
    }
};


class Jieyuan : public TriggerSkill
{
public:
    Jieyuan() : TriggerSkill("jieyuan")
    {
        events << DamageCaused << DamageInflicted;
		view_as_skill = new dummyVS;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (triggerEvent == DamageCaused) {
            if (damage.to && damage.to->isAlive() && damage.to != player && !player->isNude()) {
                if (player->getMark("jieyuan1_upgrade") == 0 && damage.to->getHp() < player->getHp()) return false;
                QString pattern = "..";
                if (player->getMark("jieyuan_upgrade") == 0) {
                    if (player->isKongcheng()) return false;
                    pattern = ".black";
                }
                if (room->askForCard(player, pattern, "@jieyuan-increase:" + damage.to->objectName(), data, objectName())) {
                    LogMessage log;
                    log.type = "#JieyuanIncrease";
                    log.from = player;
                    log.arg = QString::number(damage.damage);
                    log.arg2 = QString::number(++damage.damage);
                    room->sendLog(log);
                    data = QVariant::fromValue(damage);
                }
            }
        } else if (triggerEvent == DamageInflicted) {
            if (damage.from && damage.from->isAlive() && damage.from != player && !player->isNude()) {
                if (player->getMark("jieyuan2_upgrade") == 0 && damage.from->getHp() < player->getHp()) return false;
                QString pattern = "..";
                if (player->getMark("jieyuan_upgrade") == 0) {
                    if (player->isKongcheng()) return false;
                    pattern = ".red";
                }
                if (room->askForCard(player, pattern, "@jieyuan-decrease:" + damage.from->objectName(), data, objectName())) {
                    LogMessage log;
                    log.type = "#JieyuanDecrease";
                    log.from = player;
                    log.arg = QString::number(damage.damage);
                    log.arg2 = QString::number(--damage.damage);
                    room->sendLog(log);
                    data = QVariant::fromValue(damage);
                    if (damage.damage < 1)
                        return true;
                }
            }
        }

        return false;
    }

    int getEffectIndex(const ServerPlayer *, const QString &prompt) const
    {
        if (prompt.startsWith("@jieyuan-increase"))
			return 1;
		else if (prompt.startsWith("@jieyuan-decrease"))
			return 2;
		return -1;
    }
};

class Fenxin : public TriggerSkill
{
public:
    Fenxin() : TriggerSkill("fenxin")
    {
        events << Death;
        frequency = Compulsory;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *lingju, QVariant &data) const
    {
        DeathStruct death = data.value<DeathStruct>();
        ServerPlayer *player = death.who;
        if (!(isNormalGameMode(room->getMode()) || room->getMode() == "08_zdyj" || room->getMode() == "08_hongyan") || !lingju->hasSkill("jieyuan", true)) return false;
        QString role = player->getRole();
        if (role == "loyalist" && lingju->getMark("jieyuan2_upgrade") == 0) {
            room->sendCompulsoryTriggerLog(lingju, objectName());
            lingju->broadcastSkillInvoke(objectName());
            room->addPlayerMark(lingju, "jieyuan2_upgrade");
        } else if (role == "rebel" && lingju->getMark("jieyuan1_upgrade") == 0) {
            room->sendCompulsoryTriggerLog(lingju, objectName());
            lingju->broadcastSkillInvoke(objectName());
            room->addPlayerMark(lingju, "jieyuan1_upgrade");
        } else if (role == "renegade" && lingju->getMark("jieyuan_upgrade") == 0) {
            room->sendCompulsoryTriggerLog(lingju, objectName());
            lingju->broadcastSkillInvoke(objectName());
            room->addPlayerMark(lingju, "jieyuan_upgrade");
        }
        return false;
    }
};

class Baobian : public TriggerSkill
{
public:
    Baobian() : TriggerSkill("baobian")
    {
        events << GameStart << HpChanged << MaxHpChanged << EventAcquireSkill << EventLoseSkill;
        frequency = Compulsory;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventLoseSkill) {
            if (data.toString() == objectName()) {
                QStringList baobian_skills = player->tag["BaobianSkills"].toStringList();
                QStringList detachList;
                foreach(QString skill_name, baobian_skills)
                    detachList.append("-" + skill_name);
                room->handleAcquireDetachSkills(player, detachList);
                player->tag["BaobianSkills"] = QVariant();
            }
            return false;
        } else if (triggerEvent == EventAcquireSkill) {
            if (data.toString() != objectName()) return false;
        }

        if (!player->isAlive() || !player->hasSkill(this, true)) return false;

        acquired_skills.clear();
        detached_skills.clear();
        BaobianChange(room, player, 1, "shensu");
        BaobianChange(room, player, 2, "paoxiao");
        BaobianChange(room, player, 3, "tiaoxin");
        if (!acquired_skills.isEmpty() || !detached_skills.isEmpty()) {
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            room->handleAcquireDetachSkills(player, acquired_skills + detached_skills);
        }
        return false;
    }

private:
    void BaobianChange(Room *room, ServerPlayer *player, int hp, const QString &skill_name) const
    {
        QStringList baobian_skills = player->tag["BaobianSkills"].toStringList();
        if (player->getHp() <= hp) {
            if (!baobian_skills.contains(skill_name)) {
                room->notifySkillInvoked(player, "baobian");
                acquired_skills.append(skill_name);
                baobian_skills << skill_name;
            }
        } else {
            if (baobian_skills.contains(skill_name)) {
                detached_skills.append("-" + skill_name);
                baobian_skills.removeOne(skill_name);
            }
        }
        player->tag["BaobianSkills"] = QVariant::fromValue(baobian_skills);
    }

    mutable QStringList acquired_skills, detached_skills;
};

class Xiuluo : public PhaseChangeSkill
{
public:
    Xiuluo() : PhaseChangeSkill("xiuluo")
    {
		view_as_skill = new dummyVS;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target)
            && target->getPhase() == Player::Start
            && !target->isNude()
            && hasDelayedTrick(target);
    }

    bool onPhaseChange(ServerPlayer *target) const
    {
        Room *room = target->getRoom();
        while (hasDelayedTrick(target) && !target->isNude()) {
            QStringList suits;
            foreach (const Card *jcard, target->getJudgingArea()) {
                if (!suits.contains(jcard->getSuitString()))
                    suits << jcard->getSuitString();
            }

            const Card *card = room->askForCard(target, QString(".|%1|.|.").arg(suits.join(",")),
                "@xiuluo", QVariant(), objectName());
            if (!card || !hasDelayedTrick(target)) break;

            QList<int> avail_list, other_list;
            foreach (const Card *jcard, target->getJudgingArea()) {
                if (jcard->isKindOf("SkillCard")) continue;
                if (jcard->getSuit() == card->getSuit())
                    avail_list << jcard->getEffectiveId();
                else
                    other_list << jcard->getEffectiveId();
            }
            room->fillAG(avail_list + other_list, NULL, other_list);
            int id = room->askForAG(target, avail_list, false, objectName());
            room->clearAG();
            room->throwCard(id, NULL);
        }

        return false;
    }

private:
    static bool hasDelayedTrick(const ServerPlayer *target)
    {
        foreach(const Card *card, target->getJudgingArea())
            if (!card->isKindOf("SkillCard")) return true;
        return false;
    }
};

class Shenwei : public DrawCardsSkill
{
public:
    Shenwei() : DrawCardsSkill("shenwei")
    {
        frequency = Compulsory;
    }

    int getDrawNum(ServerPlayer *player, int n) const
    {
        Room *room = player->getRoom();

        player->broadcastSkillInvoke("shenwei");
        room->sendCompulsoryTriggerLog(player, "shenwei");

        return n + 3;
    }
};

class ShenweiKeep : public MaxCardsSkill
{
public:
    ShenweiKeep() : MaxCardsSkill("#shenwei-keep")
    {
    }

    int getExtra(const Player *target) const
    {
        if (target->hasSkill(this))
            return 3;
        else
            return 0;
    }
};

class Shenji : public TargetModSkill
{
public:
    Shenji() : TargetModSkill("shenji")
    {
    }

    virtual int getResidueNum(const Player *from, const Card *, const Player *) const
    {
        if (from->hasSkill(this) && from->getWeapon() == NULL)
            return 1;
        else
            return 0;
    }

    int getExtraTargetNum(const Player *from, const Card *) const
    {
        if (from->hasSkill(this) && from->getWeapon() == NULL)
            return 2;
        else
            return 0;
    }
};

class Shenqu : public TriggerSkill
{
public:
    Shenqu() : TriggerSkill("shenqu")
    {
        events << EventPhaseStart << Damaged;
    }

    virtual TriggerList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        TriggerList skill_list;
        if (triggerEvent == EventPhaseStart && player->getPhase() == Player::RoundStart) {
            QList<ServerPlayer *> lvbus = room->findPlayersBySkillName(objectName());
            foreach (ServerPlayer *lvbu, lvbus) {
                if (lvbu->getHandcardNum() <= lvbu->getMaxHp())
                    skill_list.insert(lvbu, QStringList(objectName()));
            }
        } else if (triggerEvent == Damaged && TriggerSkill::triggerable(player))
            skill_list.insert(player, QStringList("shenqu!"));
        return skill_list;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *, QVariant &, ServerPlayer *shenlvbu) const
    {
        if (triggerEvent == EventPhaseStart) {
            if (shenlvbu->askForSkillInvoke(this)) {
                shenlvbu->broadcastSkillInvoke(objectName());
                shenlvbu->drawCards(2, objectName());
            }
        } else if (triggerEvent == Damaged) {
            room->sendCompulsoryTriggerLog(shenlvbu, objectName());
            shenlvbu->broadcastSkillInvoke(objectName());
            room->askForUseCard(shenlvbu, "peach", "@shenqu-peach");
        }
        return false;
    }
};

JiwuCard::JiwuCard()
{
    target_fixed = true;
    m_skillName = "jiwu";
}

void JiwuCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const
{
    room->acquireSkill(source, user_string, true, true);

}

class JiwuViewAsSkill : public OneCardViewAsSkill
{
public:
    JiwuViewAsSkill() : OneCardViewAsSkill("jiwu")
    {
        filter_pattern = ".|.|.|hand!";
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasSkill("qiangxi", true) || !player->hasSkill("lieren", true) ||
                 !player->hasSkill("xuanfeng", true) || !player->hasSkill("wansha", true);
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        JiwuCard *jiwu_card = new JiwuCard;
        jiwu_card->addSubcard(originalCard->getId());
        return jiwu_card;
    }
};

class Jiwu : public TriggerSkill
{
public:
    Jiwu() : TriggerSkill("jiwu")
    {
        view_as_skill = new JiwuViewAsSkill;
    }

    bool trigger(TriggerEvent, Room *, ServerPlayer *, QVariant &) const
    {
        return false;
    }

    QString getSelectBox() const
    {
        return "qiangxi+lieren+xuanfeng+wansha";
    }

    bool buttonEnabled(const QString &button_name, const QList<const Card *> &selected, const QList<const Player *> &) const
    {
        if (button_name.isEmpty()) return true;
        if (selected.isEmpty()) return false;
        return !Self->hasSkill(button_name, true);
    }

};

BifaCard::BifaCard()
{
    will_throw = false;
    handling_method = Card::MethodNone;
}

bool BifaCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select->getPile("bifa").isEmpty() && to_select != Self;
}

void BifaCard::use(Room *, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    ServerPlayer *target = targets.first();
    target->tag["BifaSource" + QString::number(getEffectiveId())] = QVariant::fromValue(source);
    target->addToPile("bifa", this, false);
}

class BifaViewAsSkill : public OneCardViewAsSkill
{
public:
    BifaViewAsSkill() : OneCardViewAsSkill("bifa")
    {
        filter_pattern = ".|.|.|hand";
        response_pattern = "@@bifa";
    }

    const Card *viewAs(const Card *originalcard) const
    {
        Card *card = new BifaCard;
        card->addSubcard(originalcard);
        return card;
    }
};

class Bifa : public TriggerSkill
{
public:
    Bifa() : TriggerSkill("bifa")
    {
        events << EventPhaseStart;
        view_as_skill = new BifaViewAsSkill;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (TriggerSkill::triggerable(player) && player->getPhase() == Player::Finish && !player->isKongcheng()) {
            room->askForUseCard(player, "@@bifa", "@bifa-remove", QVariant(), Card::MethodNone);
        } else if (player->getPhase() == Player::RoundStart && player->getPile("bifa").length() > 0) {
            int card_id = player->getPile("bifa").first();
            ServerPlayer *chenlin = player->tag["BifaSource" + QString::number(card_id)].value<ServerPlayer *>();
            QList<int> ids;
            ids << card_id;

            LogMessage log;
            log.type = "$BifaView";
            log.from = player;
            log.card_str = QString::number(card_id);
            log.arg = "bifa";
            room->sendLog(log, player);

            room->fillAG(ids, player);
            const Card *cd = Sanguosha->getCard(card_id);
            QString pattern;
            if (cd->isKindOf("BasicCard"))
                pattern = "BasicCard";
            else if (cd->isKindOf("TrickCard"))
                pattern = "TrickCard";
            else if (cd->isKindOf("EquipCard"))
                pattern = "EquipCard";
            QVariant data_for_ai = QVariant::fromValue(pattern);
            pattern.append("|.|.|hand");
            const Card *to_give = NULL;
            if (!player->isKongcheng() && chenlin && chenlin->isAlive())
                to_give = room->askForCard(player, pattern, "@bifa-give", data_for_ai, Card::MethodNone, chenlin);
            if (chenlin && to_give) {
                CardMoveReason reasonG(CardMoveReason::S_REASON_GIVE, player->objectName(), chenlin->objectName(), "bifa", QString());
                room->obtainCard(chenlin, to_give, reasonG, false);
                CardMoveReason reason(CardMoveReason::S_REASON_EXCHANGE_FROM_PILE, player->objectName(), "bifa", QString());
                room->obtainCard(player, cd, reason, false);
            } else {
                CardMoveReason reason(CardMoveReason::S_REASON_REMOVE_FROM_PILE, QString(), objectName(), QString());
                room->throwCard(cd, reason, NULL);
                room->loseHp(player);
            }
            room->clearAG(player);
            player->tag.remove("BifaSource" + QString::number(card_id));
        }
        return false;
    }
};

SongciCard::SongciCard()
{
    mute = true;
}

bool SongciCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select->getMark("songci" + Self->objectName()) == 0 && to_select->getHandcardNum() != to_select->getHp();
}

void SongciCard::onUse(Room *room, const CardUseStruct &card_use) const
{
    CardUseStruct use = card_use;

    QVariant data = QVariant::fromValue(use);
    RoomThread *thread = room->getThread();

    thread->trigger(PreCardUsed, room, card_use.from, data);
    use = data.value<CardUseStruct>();

    LogMessage log;
    log.from = card_use.from;
    log.to = card_use.to;
    log.type = "#UseCard";
    log.card_str = card_use.card->toString();
    room->sendLog(log);

    room->notifySkillInvoked(card_use.from, "songci");

    if (card_use.to.first()->getHandcardNum() > card_use.to.first()->getHp())
        card_use.from->broadcastSkillInvoke("songci", 2);
    else if (card_use.to.first()->getHandcardNum() < card_use.to.first()->getHp())
        card_use.from->broadcastSkillInvoke("songci", 1);

    thread->trigger(CardUsed, room, card_use.from, data);
    use = data.value<CardUseStruct>();
    thread->trigger(CardFinished, room, card_use.from, data);
}

void SongciCard::onEffect(const CardEffectStruct &effect) const
{
    int handcard_num = effect.to->getHandcardNum();
    int hp = effect.to->getHp();
    Room *room = effect.from->getRoom();
    room->addPlayerMark(effect.to, "songci" + effect.from->objectName());
    if (handcard_num > hp)
        room->askForDiscard(effect.to, "songci", 2, 2, false, true);
    else if (handcard_num < hp)
        effect.to->drawCards(2, "songci");
}

class SongciViewAsSkill : public ZeroCardViewAsSkill
{
public:
    SongciViewAsSkill() : ZeroCardViewAsSkill("songci")
    {
    }

    const Card *viewAs() const
    {
        return new SongciCard;
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        if (player->getMark("songci" + player->objectName()) == 0 && player->getHandcardNum() != player->getHp()) return true;
        foreach(const Player *sib, player->getAliveSiblings())
            if (sib->getMark("songci" + player->objectName()) == 0 && sib->getHandcardNum() != sib->getHp())
                return true;
        return false;
    }
};

class Songci : public TriggerSkill
{
public:
    Songci() : TriggerSkill("songci")
    {
        events << Death;
        view_as_skill = new SongciViewAsSkill;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target && target->hasSkill(this);
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        DeathStruct death = data.value<DeathStruct>();
        if (death.who != player) return false;
        foreach (ServerPlayer *p, room->getAllPlayers()) {
            if (p->getMark("songci" + player->objectName()) > 0)
                room->setPlayerMark(p, "songci" + player->objectName(), 0);
        }
        return false;
    }
};

class XingwuDiscard : public ViewAsSkill
{
public:
    XingwuDiscard() : ViewAsSkill("xingwu_discard")
    {
        response_pattern = "@@xingwu_discard!";
        expand_pile = "dance";
    }


    bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
        if (selected.length() > 2 || !Self->getPile(expand_pile).contains(to_select->getId())) return false;
        foreach (const Card *card, selected) {
            if (card->getSuit() == to_select->getSuit())
                return false;
        }
        return true;
    }

    const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.length() == 3) {
            DummyCard *discard = new DummyCard;
            discard->addSubcards(cards);
            return discard;
        }

        return NULL;
    }
};

class Xingwu : public PhaseChangeSkill
{
public:
    Xingwu() : PhaseChangeSkill("xingwu")
    {
        
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Discard && !target->isKongcheng();
    }

    virtual bool onPhaseChange(ServerPlayer *erqiao) const
    {
        Room *room = erqiao->getRoom();
        const Card *card = room->askForCard(erqiao, ".|.|.|hand", "@xingwu", QVariant(), Card::MethodNone);
        if (card == NULL) return false;
        room->notifySkillInvoked(erqiao, objectName());
        erqiao->broadcastSkillInvoke(objectName());
        LogMessage log;
        log.type = "#InvokeSkill";
        log.from = erqiao;
        log.arg = objectName();
        room->sendLog(log);
        erqiao->addToPile("dance", card);

        QList<int> dances = erqiao->getPile("dance"), removes;

        foreach (int id1, dances) {
            bool cheak = true;
            foreach (int id2, removes) {
                if (Sanguosha->getCard(id1)->getSuit() == Sanguosha->getCard(id2)->getSuit()) {
                    cheak = false;
                    break;
                }
            }
            if (cheak)
                removes << id1;
        }

        if (removes.length() < 3) return false;

        const Card *card2 = room->askForCard(erqiao, "@@xingwu_discard!", "@xingwu-discard", QVariant(), Card::MethodNone);
        if (card2 != NULL && card2->subcardsLength() == 3) {
            removes = card2->getSubcards();
        }

        DummyCard *dummy_card = new DummyCard(removes);
        dummy_card->deleteLater();
        CardMoveReason reason(CardMoveReason::S_REASON_REMOVE_FROM_PILE, QString(), erqiao->objectName(), objectName(), QString());
        room->throwCard(dummy_card, reason, NULL);

        if (erqiao->isDead()) return false;
        ServerPlayer *vic = room->askForPlayerChosen(erqiao, room->getOtherPlayers(erqiao), objectName(), "@xingwu-choose");
        vic->throwAllEquips();
        if (vic->isAlive())
            room->damage(DamageStruct(objectName(), erqiao, vic, vic->isMale()?2:1));
        return false;
    }
};

class Luoyan : public TriggerSkill
{
public:
    Luoyan() : TriggerSkill("luoyan")
    {
        events << CardsMoveOneTime << EventAcquireSkill << EventLoseSkill;
        frequency = Compulsory;
    }

    bool triggerable(const ServerPlayer *) const
    {
        return false;
    }

};

class Xiaoguo : public TriggerSkill
{
public:
    Xiaoguo() : TriggerSkill("xiaoguo")
    {
        events << EventPhaseStart;
		view_as_skill = new dummyVS;
    }

    virtual TriggerList triggerable(TriggerEvent , Room *room, ServerPlayer *player, QVariant &) const
    {
        TriggerList skill_list;
        if (player->getPhase() == Player::Finish) {
            QList<ServerPlayer *> yuejins = room->findPlayersBySkillName(objectName());
            foreach (ServerPlayer *yuejin, yuejins) {
                if (!yuejin->isKongcheng() && yuejin != player)
                    skill_list.insert(yuejin, QStringList(objectName()));
            }

        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *ask_who) const
    {
        if (room->askForCard(ask_who, ".Basic", "@xiaoguo", data, objectName(), player)) {
            if (!room->askForCard(player, ".Equip", "@xiaoguo-discard", QVariant()))
                room->damage(DamageStruct("xiaoguo", ask_who, player));
            else {
                if (ask_who->isAlive())
                    ask_who->drawCards(1, objectName());
            }
        }
        return false;
    }
};

ZhoufuCard::ZhoufuCard()
{
    will_throw = false;
    handling_method = Card::MethodNone;
}

bool ZhoufuCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select != Self && to_select->getPile("incantation").isEmpty();
}

void ZhoufuCard::use(Room *, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    ServerPlayer *target = targets.first();
    const Card *card = Sanguosha->getCard(getEffectiveId());
    card->setTag("ZhoufuSource", QVariant::fromValue(source));
    target->addToPile("incantation", this);
}

class ZhoufuViewAsSkill : public OneCardViewAsSkill
{
public:
    ZhoufuViewAsSkill() : OneCardViewAsSkill("zhoufu")
    {
        filter_pattern = ".|.|.|hand";
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("ZhoufuCard");
    }

    const Card *viewAs(const Card *originalcard) const
    {
        Card *card = new ZhoufuCard;
        card->addSubcard(originalcard);
        return card;
    }
};

class Zhoufu : public TriggerSkill
{
public:
    Zhoufu() : TriggerSkill("zhoufu")
    {
        events << EventPhaseChanging;
        view_as_skill = new ZhoufuViewAsSkill;
    }

    int getPriority(TriggerEvent) const
    {
        return 5;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == StartJudge && !player->getPile("incantation").isEmpty()) {
            int card_id = player->getPile("incantation").first();
            const Card *incantation = Sanguosha->getCard(card_id);

            JudgeStruct *judge = data.value<JudgeStruct *>();
            judge->card = incantation;

            LogMessage log;
            log.type = "$ZhoufuJudge";
            log.from = player;
            log.arg = objectName();
            log.card_str = QString::number(judge->card->getEffectiveId());
            room->sendLog(log);

            room->moveCardTo(judge->card, NULL, judge->who, Player::PlaceJudge,
                CardMoveReason(CardMoveReason::S_REASON_JUDGE,
                player->objectName(),
                QString(), QString(), judge->reason), true);
            judge->updateResult();
            ServerPlayer *zhangbao = incantation->tag["ZhoufuSource"].value<ServerPlayer *>();
            if (zhangbao && zhangbao->isAlive())
                zhangbao->setFlags("ZhoufuTrigger");
            player->setFlags("ZhoufuTarget");
            room->setTag("SkipGameRule", true);
        } else if (triggerEvent == EventPhaseChanging) {
            if (data.value<PhaseChangeStruct>().to != Player::NotActive) return false;
            foreach (ServerPlayer *zhangbao, room->getAlivePlayers()) {
                if (zhangbao && zhangbao->isAlive() && zhangbao->hasFlag("ZhoufuTrigger")) {
                    LogMessage log;
                    log.type = "#SkillForce";
                    log.from = zhangbao;
                    log.arg = objectName();
                    room->sendLog(log);
                    room->notifySkillInvoked(zhangbao, objectName());
                    zhangbao->broadcastSkillInvoke(objectName());
                    QList<ServerPlayer *> targets;
                    foreach (ServerPlayer *target, room->getAlivePlayers()) {
                        if (target->hasFlag("ZhoufuTarget"))
                            targets << target;
                    }
                    foreach (ServerPlayer *p, targets) {
                        if (p->isAlive())
                            room->loseHp(p);
                    }
                }
            }
        } else if (triggerEvent == EventPhaseStart) {
            if (player->getPhase() == Player::NotActive) {
                foreach (ServerPlayer *p, room->getAlivePlayers()) {
                    p->setFlags("-ZhoufuTrigger");
                    p->setFlags("-ZhoufuTarget");
                }
            }
        }
        return false;
    }
};

class Yingbing : public TriggerSkill
{
public:
    Yingbing() : TriggerSkill("yingbing")
    {
        events << CardUsed << CardResponded << PreCardsMoveOneTime;
        frequency = Compulsory;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == PreCardsMoveOneTime) {
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            if (move.from == player && move.from_places.contains(Player::PlaceSpecial)
                    && move.from_pile_names.contains("incantation") && player->getPile("incantation").isEmpty()) {
                player->tag.remove("yingbing_record");
            }
            return false;
        }
        const Card *usecard = NULL;
        if (triggerEvent == CardUsed) {
            CardUseStruct use = data.value<CardUseStruct>();
            usecard = use.card;
        } else {
            CardResponseStruct resp = data.value<CardResponseStruct>();
            if (resp.m_isUse)
                usecard = resp.m_card;
        }
        if (usecard && usecard->getTypeId() != Card::TypeSkill && usecard->getHandlingMethod() == Card::MethodUse) {
            foreach (ServerPlayer *zhangbao, room->getAlivePlayers()) {
                if (player->getPile("incantation").isEmpty()) break;
                int id = player->getPile("incantation").first();
                if (Sanguosha->getCard(id)->getSuit() != usecard->getSuit()) break;
                if (!TriggerSkill::triggerable(zhangbao)) continue;
                room->sendCompulsoryTriggerLog(zhangbao, objectName());
                zhangbao->broadcastSkillInvoke(objectName());
                zhangbao->drawCards(1, objectName());
                QStringList list = player->tag["yingbing_record"].toStringList();
                if (list.contains(zhangbao->objectName()))
                    player->clearOnePrivatePile("incantation");
                else {
                    list << zhangbao->objectName();
                    player->tag["yingbing_record"] = list;
                }
            }
        }
        return false;
    }
};

class Kuangfu : public TriggerSkill
{
public:
    Kuangfu() : TriggerSkill("kuangfu")
    {
        events << Damage;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *panfeng, QVariant &data) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        ServerPlayer *target = damage.to;
        if (damage.card && damage.card->isKindOf("Slash") && target->hasEquip()
            && !target->hasFlag("Global_DebutFlag") && !damage.chain && !damage.transfer) {
            QStringList equiplist;
            for (int i = 0; i < S_EQUIP_AREA_LENGTH; i++) {
                if (!target->getEquip(i)) continue;
                if (panfeng->canDiscard(target, target->getEquip(i)->getEffectiveId()) || panfeng->getEquip(i) == NULL)
                    equiplist << QString::number(i);
            }
            if (equiplist.isEmpty() || !panfeng->askForSkillInvoke(this, QVariant::fromValue(target)))
                return false;
			room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, panfeng->objectName(), target->objectName());
            panfeng->broadcastSkillInvoke(objectName());
            int card_id = room->askForCardChosen(panfeng, target, "e", objectName());
            const Card *card = Sanguosha->getCard(card_id);

            QStringList choicelist;
            if (panfeng->canSetEquip(card))
                choicelist << "move";
            if (panfeng->canDiscard(target, card_id))
                choicelist << "throw";

            QString choice = room->askForChoice(panfeng, "kuangfu", choicelist.join("+"), QVariant(), QString(), "move+throw");

            if (choice == "move") {
                room->moveCardTo(card, panfeng, Player::PlaceEquip);
            } else {
                room->throwCard(card, target, panfeng);
            }
        }

        return false;
    }
};

class Meibu : public TriggerSkill
{
public:
    Meibu() : TriggerSkill("meibu")
    {
        events << EventPhaseStart << EventPhaseChanging;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseStart && player->getPhase() == Player::Play) {
            foreach (ServerPlayer *sunluyu, room->getOtherPlayers(player)) {
                if (!player->inMyAttackRange(sunluyu) && TriggerSkill::triggerable(sunluyu)
                    && room->askForSkillInvoke(sunluyu, objectName(), QVariant::fromValue(player))) {
                    sunluyu->broadcastSkillInvoke(objectName());
					room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, sunluyu->objectName(), player->objectName());
                    room->acquireSkill(player, "zhixi", true, true);
					if (sunluyu->getMark("mumu") == 0) {
                        QVariantList sunluyus = player->tag[objectName()].toList();
                        sunluyus << QVariant::fromValue(sunluyu);
                        player->tag[objectName()] = QVariant::fromValue(sunluyus);
                        room->insertAttackRangePair(player, sunluyu);
					}
                }
            }
        } else if (triggerEvent == EventPhaseChanging) {
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (change.to != Player::NotActive) return false;

            QVariantList sunluyus = player->tag[objectName()].toList();
            foreach (QVariant sunluyu, sunluyus) {
                ServerPlayer *s = sunluyu.value<ServerPlayer *>();
                room->removeAttackRangePair(player, s);
            }
        }
        return false;
    }
};

class ZhixiFilter : public FilterSkill
{
public:
    ZhixiFilter() : FilterSkill("#zhixi-filter")
    {
	}

    bool viewFilter(const Card *to_select) const
    {
        return to_select->getTypeId() == Card::TypeTrick;
    }

    const Card *viewAs(const Card *originalCard) const
    {
        Slash *slash = new Slash(originalCard->getSuit(), originalCard->getNumber());
        slash->setSkillName("zhixi");
        WrappedCard *card = Sanguosha->getWrappedCard(originalCard->getId());
        card->takeOver(slash);
        return card;
    }
};

MumuCard::MumuCard()
{
    target_fixed = true;
}

void MumuCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const
{
	QList<ServerPlayer *> targets1;
	QList<ServerPlayer *> targets2;
    foreach (ServerPlayer *p, room->getAllPlayers()) {
	    if (source->canDiscard(p, "e"))
			targets1 << p;
	    if (p->getArmor())
			targets2 << p;
	}
	QStringList choices;
	if (!targets1.isEmpty())
		choices << "discard_equip";
	if (!targets2.isEmpty())
		choices << "obtain_armor";
	
	if (!choices.isEmpty()){
		QList<ServerPlayer *> targets = targets1;
        QString prompt = "@mumu-equip";
        QString choice = room->askForChoice(source, "mumu", choices.join("+"), QVariant(), QString(), "discard_equip+obtain_armor");
	    if (choice == "obtain_armor"){
		    targets = targets2;
            prompt = "@mumu-armor";
	    }
        source->tag["MumuChoice"] = choice;//for AI
	    ServerPlayer *target = room->askForPlayerChosen(source, targets, "mumu", prompt);
		source->tag.remove("MumuChoice");//for AI
	    if (choice == "obtain_armor"){
	        WrappedCard *armor = target->getArmor();
            if (armor){
				CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, source->objectName());
                room->obtainCard(source, armor, reason, true);
			}
		} else {
		    if (source->canDiscard(target, "e"))
				room->throwCard(room->askForCardChosen(source, target, "e", "mumu", false, Card::MethodDiscard), target, source);
			source->drawCards(1);
		}
	}
	int used_id = subcards.first();
    const Card *c = Sanguosha->getCard(used_id);
    if (c->isKindOf("Slash") || (c->isBlack() && c->isKindOf("TrickCard"))) {
        source->addMark("mumu");
        QString translation = Sanguosha->translate(":meibu");
        QString in_attack = Sanguosha->translate("meibu_in_attack");
        if (translation.endsWith(in_attack)) {
            translation.remove(in_attack);
            Sanguosha->addTranslationEntry(":meibu", translation.toStdString().c_str());
            JsonArray args;
            args << QSanProtocol::S_GAME_EVENT_UPDATE_SKILL;
            room->doBroadcastNotify(QSanProtocol::S_COMMAND_LOG_EVENT, args);
        }
    }
}

class MumuVS : public OneCardViewAsSkill
{
public:
    MumuVS() : OneCardViewAsSkill("mumu")
    {
        filter_pattern = ".!";
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        if (player->hasUsed("MumuCard"))
			return false;
		if (player->hasEquip())
			return true;
		foreach(const Player *p, player->getAliveSiblings())
			if (p->hasEquip())
				return true;
	    return false;
    }

    const Card *viewAs(const Card *originalCard) const
    {
        MumuCard *mm = new MumuCard;
        mm->addSubcard(originalCard);
        return mm;
    }
};

class Mumu : public PhaseChangeSkill
{
public:
    Mumu() : PhaseChangeSkill("mumu")
    {
        view_as_skill = new MumuVS;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->getPhase() == Player::RoundStart;
    }

    bool onPhaseChange(ServerPlayer *target) const
    {
        if (target->getMark("mumu") > 0) {
            target->setMark("mumu", 0);
            QString translation = Sanguosha->translate(":meibu");
            QString in_attack = Sanguosha->translate("meibu_in_attack");
            if (!translation.endsWith(in_attack)) {
                translation.append(in_attack);
                Sanguosha->addTranslationEntry(":meibu", translation.toStdString().c_str());
                JsonArray args;
                args << QSanProtocol::S_GAME_EVENT_UPDATE_SKILL;
                target->getRoom()->doBroadcastNotify(QSanProtocol::S_COMMAND_LOG_EVENT, args);
            }
        }
        return false;
    }
};

class Zhixi : public TriggerSkill
{
public:
    Zhixi() : TriggerSkill("zhixi")
    {
        events << CardUsed << EventLoseSkill;
		frequency = Compulsory;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    int getPriority(TriggerEvent triggerEvent) const
    {
        if (triggerEvent == CardUsed)
            return 6;

        return TriggerSkill::getPriority(triggerEvent);
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == CardUsed) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card != NULL && use.card->isKindOf("TrickCard") && TriggerSkill::triggerable(player) && player->getMark("#zhixi") < 1) {
                room->sendCompulsoryTriggerLog(player, objectName());
                room->addPlayerTip(player, "#zhixi");
				if (!player->hasSkill("#zhixi-filter", true)) {
                    room->acquireSkill(player, "#zhixi-filter", false);
                    room->filterCards(player, player->getCards("he"), true);
                }
            }
        } else if (triggerEvent == EventLoseSkill) {
            if (data.toString() == objectName()) {
                room->removePlayerTip(player, "#zhixi");
				if (player->hasSkill("#zhixi-filter", true)) {
                    room->detachSkillFromPlayer(player, "#zhixi-filter");
                    room->filterCards(player, player->getCards("he"), true);
                }
            }
        }
        return false;
    }
};

FenxunCard::FenxunCard()
{
}

bool FenxunCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select != Self;
}

void FenxunCard::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.from->getRoom();
    room->setFixedDistance(effect.from, effect.to, 1);
}

class Fenxun : public OneCardViewAsSkill
{
public:
    Fenxun() : OneCardViewAsSkill("fenxun")
    {
        filter_pattern = ".!";
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return player->canDiscard(player, "he") && !player->hasUsed("FenxunCard");
    }

    virtual const Card *viewAs(const Card *originalcard) const
    {
        FenxunCard *first = new FenxunCard;
        first->addSubcard(originalcard->getId());
        first->setSkillName(objectName());
        return first;
    }
};

class Duanbing : public TriggerSkill
{
public:
    Duanbing() : TriggerSkill("duanbing")
    {
        events << TargetChosen;
        view_as_skill = new dummyVS;
    }

    virtual bool trigger(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data) const
    {
		CardUseStruct use = data.value<CardUseStruct>();
		if (use.card->isKindOf("Slash") && !use.card->hasFlag("slashDisableExtraTarget")) {
			QList<ServerPlayer *> available_targets;
			bool no_distance_limit = false;
			if (use.card->hasFlag("slashNoDistanceLimit")){
				no_distance_limit = true;
				room->setPlayerFlag(player, "slashNoDistanceLimit");
			}
			foreach (ServerPlayer *p, room->getAlivePlayers()) {
                if (use.to.contains(p) || room->isProhibited(player, p, use.card)) continue;
                if (use.card->targetFilter(QList<const Player *>(), p, player) && player->distanceTo(p) == 1)
                    available_targets << p;
            }
			if (no_distance_limit)
				room->setPlayerFlag(player, "-slashNoDistanceLimit");
			if (available_targets.isEmpty())
				return false;
			ServerPlayer *extra = room->askForPlayerChosen(player, available_targets, objectName(), "@duanbing-add", true);
			if (extra) {
				LogMessage log;
                log.type = "#QiaoshuiAdd";
                log.from = player;
                log.to << extra;
                log.card_str = use.card->toString();
                log.arg = objectName();
                room->sendLog(log);
                room->notifySkillInvoked(player, objectName());
                player->broadcastSkillInvoke(objectName());
                room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), extra->objectName());
			    use.to.append(extra);
                room->sortByActionOrder(use.to);
		        data = QVariant::fromValue(use);
			}
		}
        return false;
    }
};

class Zhendu : public TriggerSkill
{
public:
    Zhendu() : TriggerSkill("zhendu")
    {
        events << EventPhaseStart;
		view_as_skill = new dummyVS;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (player->getPhase() != Player::Play)
            return false;

        foreach (ServerPlayer *hetaihou, room->getOtherPlayers(player)) {
            if (player->isDead() || !TriggerSkill::triggerable(hetaihou) || !hetaihou->canDiscard(hetaihou, "h"))
                continue;

            if (room->askForCard(hetaihou, ".", "@zhendu-discard:" + player->objectName(), QVariant(), objectName(), player)) {
                Analeptic *analeptic = new Analeptic(Card::NoSuit, 0);
                analeptic->setSkillName("_zhendu");
                room->useCard(CardUseStruct(analeptic, player, QList<ServerPlayer *>()), true);
                if (player->isAlive())
                    room->damage(DamageStruct(objectName(), hetaihou, player));
            }
        }
        return false;
    }
	
	virtual int getEffectIndex(const ServerPlayer *, const Card *card) const
    {
        if (card->isKindOf("Analeptic"))
            return 0;
        else
            return -1;
    }
};

class Qiluan : public TriggerSkill
{
public:
    Qiluan() : TriggerSkill("qiluan")
    {
        events << EventPhaseChanging;
        frequency = Frequent;
    }

    virtual TriggerList triggerable(TriggerEvent , Room *room, ServerPlayer *, QVariant &data) const
    {
        TriggerList skill_list;

        if (data.value<PhaseChangeStruct>().to == Player::NotActive) {
            foreach (ServerPlayer *p, room->getAllPlayers()) {
                if (p->getMark("GlobalKilledCount") > 0 && TriggerSkill::triggerable(p)) {
                    skill_list.insert(p, QStringList(objectName()));
                }
            }
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent, Room *, ServerPlayer *, QVariant &, ServerPlayer *hetaihou) const
    {
        int n = hetaihou->getMark("GlobalKilledCount")*3;
        if (n > 0 && hetaihou->askForSkillInvoke(objectName(), "prompt:::" + QString::number(n))) {
            hetaihou->broadcastSkillInvoke(objectName());
            hetaihou->drawCards(n);
        }
        return false;
    }
};

class Zishu : public TriggerSkill
{
public:
    Zishu() : TriggerSkill("zishu")
    {
        events << CardsMoveOneTime << EventPhaseChanging;
        frequency = Compulsory;
    }

    virtual TriggerList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        TriggerList skill_list;
        if (triggerEvent == CardsMoveOneTime && !room->getTag("FirstRound").toBool() && TriggerSkill::triggerable(player)
                && player->getPhase() != Player::NotActive) {
            QVariantList move_datas = data.toList();
            foreach(QVariant move_data, move_datas) {
                CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
                if (move.to == player && move.to_place == Player::PlaceHand && move.reason.m_skillName != objectName()) {
                    skill_list.insert(player, QStringList(objectName()));
                    return skill_list;
                }
            }
        } else if (triggerEvent == EventPhaseChanging) {
            if (data.value<PhaseChangeStruct>().to != Player::NotActive) return skill_list;
            QList<ServerPlayer *> maliangs = room->findPlayersBySkillName(objectName());
            foreach (ServerPlayer *maliang, maliangs) {
                if (maliang == player || maliang->property("GlobalGetCards").toString() == "") continue;
                QStringList card_list = maliang->property("GlobalGetCards").toString().split("+");
                foreach (QString card_data, card_list) {
                    int id = card_data.toInt();
                    if (room->getCardOwner(id) == maliang && room->getCardPlace(id) == Player::PlaceHand) {
                        skill_list.insert(maliang, QStringList(objectName()));
                        break;
                    }
                }

            }
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *, QVariant &, ServerPlayer *maliang) const
    {
        if (triggerEvent == CardsMoveOneTime) {
            room->sendCompulsoryTriggerLog(maliang, objectName());
            maliang->broadcastSkillInvoke(objectName(), 1);
            maliang->drawCards(1, objectName());
        } else {
            QStringList card_list = maliang->property("GlobalGetCards").toString().split("+");
            DummyCard *dummy = new DummyCard;
            foreach (QString card_data, card_list) {
                int id = card_data.toInt();
                if (room->getCardOwner(id) == maliang && room->getCardPlace(id) == Player::PlaceHand) {
                    dummy->addSubcard(id);
                }
            }
            if (dummy->subcardsLength() > 0) {
                room->sendCompulsoryTriggerLog(maliang, objectName());
                maliang->broadcastSkillInvoke(objectName(), 2);
                CardMoveReason reason(CardMoveReason::S_REASON_PUT, maliang->objectName(), objectName(), QString());
                room->throwCard(dummy, reason, NULL);
            }
            delete dummy;
        }
        return false;
    }
};

class Yingyuan : public TriggerSkill
{
public:
    Yingyuan() : TriggerSkill("yingyuan")
    {
        events << CardFinished;
        view_as_skill = new dummyVS;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        if (player->getPhase() == Player::NotActive) return false;
        if ((use.card->getTypeId() == Card::TypeBasic || use.card->isNDTrick())) {
            QString classname = use.card->getClassName();
            if (use.card->isKindOf("Slash")) classname = "Slash";
            if (player->hasFlag("yingyuanUsed" + classname)) return false;
            if (!room->isAllOnPlace(use.card, Player::PlaceTable)) return false;
            ServerPlayer *target = room->askForPlayerChosen(player, room->getOtherPlayers(player), objectName(), "@yingyuan-invoke:::"+use.card->objectName(), true, true);
            if (target) {
                player->broadcastSkillInvoke(objectName());
                player->setFlags("yingyuanUsed" + classname);
                target->obtainCard(use.card);
            }
        }
        return false;
    }
};

class Shushen : public TriggerSkill
{
public:
    Shushen() : TriggerSkill("shushen")
    {
        events << HpRecover;
		view_as_skill = new dummyVS;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        RecoverStruct recover_struct = data.value<RecoverStruct>();
        int recover = recover_struct.recover;
        for (int i = 0; i < recover; i++) {
            ServerPlayer *target = room->askForPlayerChosen(player, room->getOtherPlayers(player), objectName(), "shushen-invoke", true, true);
            if (target) {
                player->broadcastSkillInvoke(objectName());
                if (target->isWounded() && room->askForChoice(target, objectName(), "recover+draw", data) == "recover")
                    room->recover(target, RecoverStruct(player));
                else
                    target->drawCards(2, objectName());
            } else {
                break;
            }
        }
        return false;
    }
};

class Shenzhi : public PhaseChangeSkill
{
public:
    Shenzhi() : PhaseChangeSkill("shenzhi")
    {
    }

    virtual bool onPhaseChange(ServerPlayer *ganfuren) const
    {
        Room *room = ganfuren->getRoom();
        if (ganfuren->getPhase() != Player::Start || ganfuren->isKongcheng())
            return false;
        if (room->askForSkillInvoke(ganfuren, objectName())) {
            // As the cost, if one of her handcards cannot be throwed, the skill is unable to invoke
            foreach (const Card *card, ganfuren->getHandcards()) {
                if (ganfuren->isJilei(card))
                    return false;
            }
            //==================================
            int handcard_num = ganfuren->getHandcardNum();
            ganfuren->broadcastSkillInvoke(objectName());
            ganfuren->throwAllHandCards();
            if (handcard_num >= ganfuren->getHp())
                room->recover(ganfuren, RecoverStruct(ganfuren));
        }
        return false;
    }
};

class Fulu : public OneCardViewAsSkill
{
public:
    Fulu() : OneCardViewAsSkill("fulu")
    {
        filter_pattern = "Slash";
        response_or_use = true;
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return Slash::IsAvailable(player);
    }

    bool isEnabledAtResponse(const Player *, const QString &pattern) const
    {
        return Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE_USE && pattern == "slash";
    }

    const Card *viewAs(const Card *originalCard) const
    {
        ThunderSlash *acard = new ThunderSlash(originalCard->getSuit(), originalCard->getNumber());
        acard->addSubcard(originalCard->getId());
        acard->setSkillName(objectName());
        return acard;
    }
};

class Zhuji : public TriggerSkill
{
public:
    Zhuji() : TriggerSkill("zhuji")
    {
        events << DamageCaused << FinishJudge;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == DamageCaused) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.nature != DamageStruct::Thunder || !damage.from)
                return false;
            foreach (ServerPlayer *p, room->getAllPlayers()) {
                if (TriggerSkill::triggerable(p) && room->askForSkillInvoke(p, objectName(), data)) {
                    p->broadcastSkillInvoke(objectName());
                    JudgeStruct judge;
                    judge.good = true;
                    judge.reason = objectName();
                    judge.pattern = ".";
                    judge.who = damage.from;
                    judge.play_animation = false;

                    room->judge(judge);
                    if (judge.pattern == "black") {
                        LogMessage log;
                        log.type = "#ZhujiBuff";
                        log.from = p;
                        log.to << damage.to;
                        log.arg = QString::number(damage.damage);
                        log.arg2 = QString::number(++damage.damage);
                        room->sendLog(log);

                        data = QVariant::fromValue(damage);
                    }
                }
            }
        } else if (triggerEvent == FinishJudge) {
            JudgeStruct *judge = data.value<JudgeStruct *>();
            if (judge->reason == objectName()) {
                judge->pattern = (judge->card->isRed() ? "red" : "black");
                if (room->getCardPlace(judge->card->getEffectiveId()) == Player::PlaceJudge && judge->card->isRed())
                    player->obtainCard(judge->card);
            }
        }
        return false;
    }
};

class Fentian : public PhaseChangeSkill
{
public:
    Fentian() : PhaseChangeSkill("fentian")
    {
        frequency = Compulsory;
    }

    bool onPhaseChange(ServerPlayer *hanba) const
    {
        if (hanba->getPhase() != Player::Finish)
            return false;

        if (hanba->getHandcardNum() >= hanba->getHp())
            return false;

        QList<ServerPlayer*> targets;
        Room* room = hanba->getRoom();

        foreach (ServerPlayer *p, room->getAlivePlayers()) {
            if (hanba->inMyAttackRange(p) && !p->isNude())
                targets << p;
        }

        if (targets.isEmpty())
            return false;

        room->sendCompulsoryTriggerLog(hanba, objectName());
        hanba->broadcastSkillInvoke(objectName());
        ServerPlayer *target = room->askForPlayerChosen(hanba, targets, objectName(), "@fentian-choose");
		if (target){
			room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, hanba->objectName(), target->objectName());
            int id = room->askForCardChosen(hanba, target, "he", objectName());
            hanba->addToPile("burn", id);
		}
        return false;
    }
};

class FentianRange : public AttackRangeSkill
{
public:
    FentianRange() : AttackRangeSkill("#fentian")
    {

    }

    int getExtra(const Player *target, bool) const
    {
        if (target->hasSkill(this))
            return target->getPile("burn").length();

        return 0;
    }
};

class Zhiri : public PhaseChangeSkill
{
public:
    Zhiri() : PhaseChangeSkill("zhiri")
    {
        frequency = Wake;
    }

    bool onPhaseChange(ServerPlayer *hanba) const
    {
        if (hanba->getMark(objectName()) > 0 || hanba->getPhase() != Player::Start)
            return false;

        if (hanba->getPile("burn").length() < 3)
            return false;

        Room *room = hanba->getRoom();
        room->sendCompulsoryTriggerLog(hanba, objectName());
        hanba->broadcastSkillInvoke(objectName());

        room->setPlayerMark(hanba, objectName(), 1);
        if (room->changeMaxHpForAwakenSkill(hanba) && hanba->getMark("zhiri") > 0)
            room->acquireSkill(hanba, "xintan");

        return false;
    };

};

XintanCard::XintanCard()
{
}

bool XintanCard::targetFilter(const QList<const Player *> &targets, const Player *, const Player *) const
{
    return targets.isEmpty();
}

void XintanCard::onEffect(const CardEffectStruct &effect) const
{
    effect.from->getRoom()->loseHp(effect.to);
}

class Xintan : public ViewAsSkill
{
public:
    Xintan() : ViewAsSkill("xintan")
    {
        expand_pile = "burn";
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return player->getPile("burn").length() >= 2 && !player->hasUsed("XintanCard");
    }

    bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
        return selected.length() < 2 && Self->getPile("burn").contains(to_select->getId());
    }

    const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.length() == 2) {
            XintanCard *xt = new XintanCard;
            xt->addSubcards(cards);
            return xt;
        }
        return NULL;
    }
};

class Chongzhen : public TriggerSkill
{
public:
    Chongzhen() : TriggerSkill("chongzhen")
    {
        events << CardResponded << TargetSpecified;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == CardResponded) {
            CardResponseStruct resp = data.value<CardResponseStruct>();
            if (resp.m_card->getSkillName() == "longdan"
                && resp.m_who != NULL && !resp.m_who->isKongcheng()) {
                QVariant data = QVariant::fromValue(resp.m_who);
                if (player->askForSkillInvoke(this, data)) {
                    player->broadcastSkillInvoke("chongzhen");
					room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), resp.m_who->objectName());
                    int card_id = room->askForCardChosen(player, resp.m_who, "h", objectName());
                    CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, player->objectName());
                    room->obtainCard(player, Sanguosha->getCard(card_id), reason, false);
                }
            }
        } else {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card->getSkillName() == "longdan") {
                foreach (ServerPlayer *p, use.to) {
                    if (p->isKongcheng()) continue;
                    QVariant data = QVariant::fromValue(p);
                    p->setFlags("ChongzhenTarget");
                    bool invoke = player->askForSkillInvoke(this, data);
                    p->setFlags("-ChongzhenTarget");
                    if (invoke) {
                        player->broadcastSkillInvoke("chongzhen");
						room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), p->objectName());
                        int card_id = room->askForCardChosen(player, p, "h", objectName());
                        CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, player->objectName());
                        room->obtainCard(player, Sanguosha->getCard(card_id), reason, false);
                    }
                }
            }
        }
        return false;
    }
};

LihunCard::LihunCard()
{

}

bool LihunCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select->isMale() && to_select != Self;
}

void LihunCard::extraCost(Room *room, const CardUseStruct &card_use) const
{
    CardMoveReason reason(CardMoveReason::S_REASON_THROW, card_use.from->objectName(), QString(), card_use.card->getSkillName(), QString());
    room->moveCardTo(this, NULL, Player::DiscardPile, reason, true);
	card_use.from->turnOver();
}

void LihunCard::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.from->getRoom();
    effect.to->setFlags("LihunTarget");
    DummyCard *dummy_card = new DummyCard(effect.to->handCards());
    dummy_card->deleteLater();
    if (!effect.to->isKongcheng()) {
        CardMoveReason reason(CardMoveReason::S_REASON_TRANSFER, effect.from->objectName(),
            effect.to->objectName(), "lihun", QString());
        room->moveCardTo(dummy_card, effect.to, effect.from, Player::PlaceHand, reason, false);
    }
}

class LihunSelect : public OneCardViewAsSkill
{
public:
    LihunSelect() : OneCardViewAsSkill("lihun")
    {
        filter_pattern = ".!";
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("LihunCard");
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        LihunCard *card = new LihunCard;
        card->addSubcard(originalCard);
        return card;
    }
};

class Lihun : public TriggerSkill
{
public:
    Lihun() : TriggerSkill("lihun")
    {
        events << EventPhaseStart << EventPhaseEnd;
        view_as_skill = new LihunSelect;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->hasUsed("LihunCard");
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *diaochan, QVariant &) const
    {
        if (triggerEvent == EventPhaseEnd && diaochan->getPhase() == Player::Play) {
            ServerPlayer *target = NULL;
            foreach (ServerPlayer *other, room->getOtherPlayers(diaochan)) {
                if (other->hasFlag("LihunTarget")) {
                    other->setFlags("-LihunTarget");
                    target = other;
                    break;
                }
            }

            if (!target || target->getHp() < 1 || diaochan->isNude())
                return false;

            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, diaochan->objectName(), target->objectName());
            diaochan->broadcastSkillInvoke(objectName(), 2);
            DummyCard *to_goback;
            if (diaochan->getCardCount() <= target->getHp()) {
                to_goback = diaochan->isKongcheng() ? new DummyCard : diaochan->wholeHandCards();
                for (int i = 0; i < 4; i++)
                    if (diaochan->getEquip(i))
                        to_goback->addSubcard(diaochan->getEquip(i)->getEffectiveId());
            } else
                to_goback = (DummyCard *)room->askForExchange(diaochan, objectName(), target->getHp(), target->getHp(), true, "LihunGoBack");

            CardMoveReason reason(CardMoveReason::S_REASON_GIVE, diaochan->objectName(),
                target->objectName(), objectName(), QString());
            room->moveCardTo(to_goback, diaochan, target, Player::PlaceHand, reason);
            to_goback->deleteLater();
        } else if (triggerEvent == EventPhaseStart && diaochan->getPhase() == Player::NotActive) {
            foreach (ServerPlayer *p, room->getAlivePlayers()) {
                if (p->hasFlag("LihunTarget"))
                    p->setFlags("-LihunTarget");
            }
        }

        return false;
    }

    int getEffectIndex(const ServerPlayer *, const Card *card) const
    {
        if (card->isKindOf("LihunCard"))
            return 1;
        return -1;
    }
};

WeikuiCard::WeikuiCard()
{
}

bool WeikuiCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && !to_select->isKongcheng() && to_select != Self;
}

void WeikuiCard::extraCost(Room *room, const CardUseStruct &card_use) const
{
    room->loseHp(card_use.from);
}

void WeikuiCard::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *source = effect.from;
    Room *room = source->getRoom();
    ServerPlayer *target = effect.to;
    if (source->isAlive() && target->isAlive() && !target->isKongcheng()) {
        bool has_jink = false;
        foreach (const Card *c, target->getHandcards()) {
            if (c->isKindOf("Jink")) {
                has_jink = true;
                break;
            }
        }
        if (has_jink) {
            room->doGongxin(source, target, QList<int>(), "weikui");
            if (source->canSlash(target, false)) {
                Slash *slash = new Slash(Card::NoSuit, 0);
                slash->setSkillName("_weikui");
                room->useCard(CardUseStruct(slash, source, target));
            }
        } else {
            int id = room->askForCardChosen(source, target, "h", "weikui", true, Card::MethodDiscard);
            room->throwCard(id, target, source);
        }
    }
}

class Weikui : public ZeroCardViewAsSkill
{
public:
    Weikui() : ZeroCardViewAsSkill("weikui")
    {

    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("WeikuiCard") && player->getHp() > 0;
    }

    virtual const Card *viewAs() const
    {
        return new WeikuiCard;
    }
};

LizhanCard::LizhanCard()
{
}

bool LizhanCard::targetFilter(const QList<const Player *> &, const Player *to_select, const Player *) const
{
    return to_select->isWounded();
}

void LizhanCard::onEffect(const CardEffectStruct &effect) const
{
    effect.to->drawCards(1, "lizhan");
}

class LizhanViewAsSkill : public ZeroCardViewAsSkill
{
public:
    LizhanViewAsSkill() : ZeroCardViewAsSkill("lizhan")
    {
        response_pattern = "@@lizhan";
    }

    virtual const Card *viewAs() const
    {
        return new LizhanCard;
    }
};

class Lizhan : public PhaseChangeSkill
{
public:
    Lizhan() : PhaseChangeSkill("lizhan")
    {
        view_as_skill = new LizhanViewAsSkill;
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
        Room *room = target->getRoom();
        if (target->getPhase() != Player::Finish) return false;
        foreach (ServerPlayer *player, room->getAlivePlayers()) {
            if (player->isWounded()) {
                room->askForUseCard(target, "@@lizhan", "@lizhan-card");
                break;
            }
        }
        return false;
    }
};

SPPackage::SPPackage()
: Package("sp")
{
    General *shenlvbu1 = new General(this, "shenlvbu1", "god", 8, true, true); // SP 008 (2-1)
    shenlvbu1->addSkill("mashu");
    shenlvbu1->addSkill("wushuang");

    General *shenlvbu2 = new General(this, "shenlvbu2", "god", 4, true, true); // SP 008 (2-2)
    shenlvbu2->addSkill("mashu");
    shenlvbu2->addSkill("wushuang");
    shenlvbu2->addSkill(new Xiuluo);
    shenlvbu2->addSkill(new ShenweiKeep);
    shenlvbu2->addSkill(new Shenwei);
    shenlvbu2->addSkill(new Shenji);
    related_skills.insertMulti("shenwei", "#shenwei-keep");

    General *shenlvbu3 = new General(this, "shenlvbu3", "god", 4, true, true);
    shenlvbu3->addSkill("wushuang");
    shenlvbu3->addSkill(new Shenqu);
    shenlvbu3->addSkill(new Jiwu);
    shenlvbu3->addRelateSkill("qiangxi");
    shenlvbu3->addRelateSkill("lieren");
    shenlvbu3->addRelateSkill("xuanfeng");
    shenlvbu3->addRelateSkill("wansha");

    General *yangxiu = new General(this, "yangxiu", "wei", 3); // SP 001
    yangxiu->addSkill(new Jilei);
    yangxiu->addSkill(new JileiClear);
    yangxiu->addSkill(new Danlao);
    related_skills.insertMulti("jilei", "#jilei-clear");

    General *sp_diaochan = new General(this, "sp_diaochan", "qun", 3, false); // *SP 002
    sp_diaochan->addSkill(new Lihun);
    sp_diaochan->addSkill("biyue");

    General *sp_guanyu = new General(this, "sp_guanyu", "wei"); // sp 003
    sp_guanyu->addSkill("wusheng");
    sp_guanyu->addSkill(new Danji);
    sp_guanyu->addRelateSkill("mashu");
    sp_guanyu->addRelateSkill("nuzhan");

    General *sp_machao = new General(this, "sp_machao", "qun"); // SP 011
    sp_machao->addSkill(new Skill("zhuiji", Skill::Compulsory));
    sp_machao->addSkill(new Shichou);

    General *sp_zhaoyun = new General(this, "sp_zhaoyun", "qun", 3); // *SP 001
    sp_zhaoyun->addSkill("longdan");
    sp_zhaoyun->addSkill(new Chongzhen);

    General *sp_jiaxu = new General(this, "sp_jiaxu", "wei", 3); // SP 012
    sp_jiaxu->addSkill(new Zhenlve);
    sp_jiaxu->addSkill(new ZhenlveTrick);
    related_skills.insertMulti("zhenlve", "#zhenlve-trick");
    sp_jiaxu->addSkill(new Jianshu);
    sp_jiaxu->addSkill(new Yongdi);

    General *sp_caoren = new General(this, "sp_caoren", "wei"); // *SP 003
    sp_caoren->addSkill(new Weikui);
    sp_caoren->addSkill(new Lizhan);

    General *caohong = new General(this, "caohong", "wei"); // SP 013
    caohong->addSkill(new Yuanhu);

    General *lingju = new General(this, "lingju", "qun", 3, false);
    lingju->addSkill(new Jieyuan);
    lingju->addSkill(new Fenxin);

    General *fuwan = new General(this, "fuwan", "qun", 4);
    fuwan->addSkill(new Moukui);

    General *xiahouba = new General(this, "xiahouba", "shu"); // SP 019
    xiahouba->addSkill(new Baobian);
	xiahouba->addRelateSkill("tiaoxin");
	xiahouba->addRelateSkill("paoxiao");
	xiahouba->addRelateSkill("shensu");

    General *chenlin = new General(this, "chenlin", "wei", 3); // SP 020
    chenlin->addSkill(new Bifa);
    chenlin->addSkill(new Songci);

    General *erqiao = new General(this, "erqiao", "wu", 3, false); // SP 021
    erqiao->addSkill(new Xingwu);
    erqiao->addSkill(new DetachEffectSkill("xingwu", "dance"));
	related_skills.insertMulti("xingwu", "#xingwu-clear");
    erqiao->addSkill(new Luoyan);
	erqiao->addRelateSkill("liuli");
	erqiao->addRelateSkill("tianxiang");

    General *yuejin = new General(this, "yuejin", "wei", 4, true); // SP 024
    yuejin->addSkill(new Xiaoguo);

    General *zhangbao = new General(this, "zhangbao", "qun", 3); // SP 025
    zhangbao->addSkill(new Zhoufu);
    zhangbao->addSkill(new Yingbing);

    General *panfeng = new General(this, "panfeng", "qun", 4, true); // SP 029
    panfeng->addSkill(new Kuangfu);

    General *dingfeng = new General(this, "dingfeng", "wu", 4, true); // SP 031
    dingfeng->addSkill(new Duanbing);
    dingfeng->addSkill(new Fenxun);

    General *hetaihou = new General(this, "hetaihou", "qun", 3, false); // SP 033
    hetaihou->addSkill(new Zhendu);
    hetaihou->addSkill(new Qiluan);

    General *sunluyu = new General(this, "sunluyu", "wu", 3, false); // SP 034
    sunluyu->addSkill(new Meibu);
    sunluyu->addSkill(new Mumu);

    General *maliang = new General(this, "maliang", "shu", 3); // SP 035
    maliang->addSkill(new Zishu);
    maliang->addSkill(new Yingyuan);

    General *ganfuren = new General(this, "ganfuren", "shu", 3, false); // SP 037
    ganfuren->addSkill(new Shushen);
    ganfuren->addSkill(new Shenzhi);

    General *huangjinleishi = new General(this, "huangjinleishi", "qun", 3, false); // SP 038
    huangjinleishi->addSkill(new Fulu);
    huangjinleishi->addSkill(new Zhuji);

    addMetaObject<LihunCard>();
    addMetaObject<WeikuiCard>();
    addMetaObject<LizhanCard>();
    addMetaObject<JiwuCard>();
	addMetaObject<ShichouCard>();
	addMetaObject<JianshuCard>();
    addMetaObject<YuanhuCard>();
    addMetaObject<BifaCard>();
    addMetaObject<SongciCard>();
    addMetaObject<ZhoufuCard>();
    addMetaObject<FenxunCard>();
    addMetaObject<MumuCard>();

    skills << new Nuzhan << new XingwuDiscard << new Zhixi << new ZhixiFilter;
}

ADD_PACKAGE(SP)

MiscellaneousPackage::MiscellaneousPackage()
: Package("miscellaneous")
{
    General *hanba = new General(this, "hanba", "qun", 4, false);
    hanba->addSkill(new Fentian);
    hanba->addSkill(new FentianRange);
	hanba->addSkill(new DetachEffectSkill("fentian", "burn"));
    related_skills.insertMulti("fentian", "#fentian");
	related_skills.insertMulti("fentian", "#fentian-clear");
    hanba->addSkill(new Zhiri);
    hanba->addRelateSkill("xintan");

    skills << new Xintan;
    addMetaObject<XintanCard>();
}

ADD_PACKAGE(Miscellaneous)
