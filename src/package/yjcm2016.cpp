#include "yjcm2016.h"
#include "general.h"
#include "skill.h"
#include "standard.h"
#include "engine.h"
#include "clientplayer.h"
#include "settings.h"
#include "json.h"

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

ZhigeCard::ZhigeCard()
{
}

bool ZhigeCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select->inMyAttackRange(Self);
}

void ZhigeCard::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.from->getRoom();
	bool use_slash = room->askForUseSlashTo(effect.to, room->getAlivePlayers(), "@zhige-slash:" + effect.from->objectName());
    if (!use_slash && effect.to->hasEquip()) {
		QList<int> equips;
        foreach (const Card *card, effect.to->getEquips())
            equips << card->getEffectiveId();
		room->fillAG(equips, effect.to);
        int to_give = room->askForAG(effect.to, equips, false, "zhige");
        room->clearAG(effect.to);
		CardMoveReason reason(CardMoveReason::S_REASON_GIVE, effect.to->objectName(), effect.from->objectName(), "zhige", QString());
		reason.m_playerId = effect.from->objectName();
        room->moveCardTo(Sanguosha->getCard(to_give), effect.to, effect.from, Player::PlaceHand, reason);
	}
}

class Zhige : public ZeroCardViewAsSkill
{
public:
    Zhige() : ZeroCardViewAsSkill("zhige")
    {
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("ZhigeCard") && player->getHandcardNum() > player->getHp();
    }

    virtual const Card *viewAs() const
    {
        return new ZhigeCard;
    }
};

class Zongzuo : public TriggerSkill
{
public:
    Zongzuo() : TriggerSkill("zongzuo")
    {
        events << TurnStart << DeathAfter;
		frequency = Compulsory;
	}

    virtual TriggerList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        TriggerList skill_list;
        if (triggerEvent == TurnStart) {
            if (!room->getTag("FirstRound").toBool()) return skill_list;
            QList<ServerPlayer *> liuyus = room->findPlayersBySkillName(objectName());
            foreach (ServerPlayer *liuyu, liuyus)
                skill_list.insert(liuyu, QStringList(objectName()));

        } else if (triggerEvent == DeathAfter) {
            foreach(ServerPlayer *p, room->getAlivePlayers()) {
                if (p->getKingdom() == player->getKingdom())
                    return skill_list;
            }
            QList<ServerPlayer *> liuyus = room->findPlayersBySkillName(objectName());
            foreach (ServerPlayer *liuyu, liuyus)
                skill_list.insert(liuyu, QStringList(objectName()));
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *, QVariant &, ServerPlayer *liuyu) const
    {
        if (triggerEvent == TurnStart) {
            QSet<QString> kingdom_set;
            foreach(ServerPlayer *p, room->getAlivePlayers())
                kingdom_set << p->getKingdom();

            int n = kingdom_set.size();
            if (n > 0) {
                room->sendCompulsoryTriggerLog(liuyu, objectName());
                liuyu->broadcastSkillInvoke(objectName());

                LogMessage log;
                log.type = "#GainMaxHp";
                log.from = liuyu;
                log.arg = QString::number(n);
                log.arg2 = QString::number(liuyu->getMaxHp() + n);
                room->sendLog(log);

                room->setPlayerProperty(liuyu, "maxhp", liuyu->getMaxHp() + n);
                room->recover(liuyu, RecoverStruct(liuyu, NULL, n));

            }
        } else if (triggerEvent == DeathAfter) {
            room->sendCompulsoryTriggerLog(liuyu, objectName());
            liuyu->broadcastSkillInvoke(objectName());
            room->loseMaxHp(liuyu);
		}
        return false;
    }
};

JisheCard::JisheCard()
{
    target_fixed = true;
}

void JisheCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const
{
    source->drawCards(1, "jishe");
    room->addPlayerMark(source, "Global_MaxcardsDecrease");
}

JisheChainCard::JisheChainCard()
{
    handling_method = Card::MethodNone;
    m_skillName = "jishe";
}

bool JisheChainCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.length() < Self->getHp() && !to_select->isChained();
}

void JisheChainCard::onEffect(const CardEffectStruct &effect) const
{
    if (!effect.to->isChained())
        effect.to->getRoom()->setPlayerProperty(effect.to, "chained", true);
}

class JisheViewAsSkill : public ZeroCardViewAsSkill
{
public:
    JisheViewAsSkill() : ZeroCardViewAsSkill("jishe")
    {
    }

    virtual const Card *viewAs() const
    {
        if (Sanguosha->currentRoomState()->getCurrentCardUsePattern() == "@@jishe")
            return new JisheChainCard;
        else
            return new JisheCard;
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return player->getMaxCards() > 0;
    }

    bool isEnabledAtResponse(const Player *, const QString &pattern) const
    {
        return pattern == "@@jishe";
    }
};

class Jishe : public TriggerSkill
{
public:
    Jishe() : TriggerSkill("jishe")
    {
        events << EventPhaseChanging << EventPhaseStart << PlayCard;
        view_as_skill = new JisheViewAsSkill;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseChanging && data.value<PhaseChangeStruct>().from == Player::Play) {
            room->setPlayerMark(player, "#jishe", 0);
        } else if (triggerEvent == PlayCard && TriggerSkill::triggerable(player)) {
            room->setPlayerMark(player, "#jishe", player->getMaxCards());
        }
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (triggerEvent == EventPhaseStart && TriggerSkill::triggerable(player) && player->getPhase() == Player::Finish && player->isKongcheng()) {
            QList<ServerPlayer *> all_players = room->getAlivePlayers();
            foreach (ServerPlayer *p, all_players) {
                if (!p->isChained())
                    return QStringList(objectName());
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        room->askForUseCard(player, "@@jishe", "@jishe-chain:::" + QString::number(player->getHp()), QVariant(), Card::MethodNone);
        return false;
    }

    int getEffectIndex(const ServerPlayer *player, const Card *card) const
    {
        Room *room = player->getRoom();
        if (card->isKindOf("JisheCard")) {
            QString tag_name = QString("AudioEffect:cenhun-jishe=1+2");
            int index = room->getTag(tag_name).toInt();
            if (index == 1 || index == 2)
                index = 3 - index;
            else
                index = qrand() % 2 + 1;
            room->setTag(tag_name, index);
            return index;
        } else if (card->isKindOf("JisheChainCard")) {
            QString tag_name = QString("AudioEffect:cenhun-jishe=3+4");
            int index = room->getTag(tag_name).toInt();
            if (index == 3 || index == 4)
                index = 7 - index;
            else
                index = qrand() % 2 + 3;
            room->setTag(tag_name, index);
            return index;
        }
        return -1;
    }
};

class Lianhuo : public TriggerSkill
{
public:
    Lianhuo() : TriggerSkill("lianhuo")
    {
        events << DamageInflicted;
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (damage.nature == DamageStruct::Fire && player->isChained() && !damage.chain)
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());

        DamageStruct damage = data.value<DamageStruct>();
        ++damage.damage;
        data = QVariant::fromValue(damage);
        return false;
    }
};

QinqingCard::QinqingCard()
{
}

bool QinqingCard::targetFilter(const QList<const Player *> &, const Player *to_select, const Player *Self) const
{
    if (Self->isLord())
        return (to_select->inMyAttackRange(Self));
    foreach(const Player *lord, Self->getAliveSiblings()) {
        if (lord->isLord())
            return (to_select->inMyAttackRange(lord));
    }
    return false;
}

void QinqingCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    foreach (ServerPlayer *target, targets) {
        if (source->canDiscard(target, "he")) {
            int id = room->askForCardChosen(source, target, "he", "qinqing", false, Card::MethodDiscard);
            room->throwCard(id, target, source);
        }
        if (target->isAlive())
            target->drawCards(1, "qinqing");
    }
    ServerPlayer *lord = room->getLord();
    if (lord == NULL) return;
    int n = 0;
    foreach(ServerPlayer *p, targets)
        if (p->getHandcardNum() > lord->getHandcardNum())
            n++;
    if (n > 0)
        source->drawCards(n, "qinqing");
}

class QinqingViewAsSkill : public ZeroCardViewAsSkill
{
public:
    QinqingViewAsSkill() : ZeroCardViewAsSkill("qinqing")
    {
        response_pattern = "@@qinqing";
    }

    virtual const Card *viewAs() const
    {
        return new QinqingCard;
    }
};

class Qinqing : public PhaseChangeSkill
{
public:
    Qinqing() : PhaseChangeSkill("qinqing")
    {
        view_as_skill = new QinqingViewAsSkill;
    }

    virtual bool onPhaseChange(ServerPlayer *huanghao) const
    {
        Room *room = huanghao->getRoom();
		if (!isNormalGameMode(room->getMode()) && room->getMode() != "08_zdyj") return false;
		if (huanghao->getPhase() != Player::Finish) return false;
		ServerPlayer *lord = room->getLord();
        if (lord == NULL) return false;
        room->askForUseCard(huanghao, "@@qinqing", "@qinqing", QVariant(), Card::MethodNone);
        return false;
    }
};

class HuishengViewAsSkill : public ViewAsSkill
{
public:
    HuishengViewAsSkill() : ViewAsSkill("huisheng")
    {
        response_pattern = "@@huisheng";
    }

    bool viewFilter(const QList<const Card *> &, const Card *) const
    {
        return true;
    }

    const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.length() > 0) {
            DummyCard *xt = new DummyCard;
            xt->addSubcards(cards);
            return xt;
        }

        return NULL;
    }
};

class HuishengObtain : public OneCardViewAsSkill
{
public:
    HuishengObtain() : OneCardViewAsSkill("huisheng_obtain")
    {
        expand_pile = "#huisheng";
    }

    virtual bool isEnabledAtPlay(const Player *) const
    {
        return false;
    }

    virtual bool isEnabledAtResponse(const Player *, const QString &pattern) const
    {
        return pattern.startsWith("@@huisheng_obtain");
    }

    bool viewFilter(const Card *to_select) const
    {
        return Self->getPile("#huisheng").contains(to_select->getEffectiveId());
    }

    const Card *viewAs(const Card *originalCard) const
    {
        return originalCard;
    }
};

class Huisheng : public TriggerSkill
{
public:
    Huisheng() : TriggerSkill("huisheng")
    {
        events << DamageInflicted;
        view_as_skill = new HuishengViewAsSkill;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *huanghao, QVariant &data) const
    {
		ServerPlayer *target = data.value<DamageStruct>().from;
		if (huanghao->isNude() || !target || target->isDead() || target == huanghao || target->getMark("huisheng" + huanghao->objectName()) > 0) return false;
		const Card *card = room->askForCard(huanghao, "@@huisheng", "@huisheng-show::" + target->objectName(), data, Card::MethodNone);
		if (card){
			LogMessage log;
            log.type = "#InvokeSkill";
            log.arg = objectName();
            log.from = huanghao;
            room->sendLog(log);
            room->notifySkillInvoked(huanghao, objectName());
            huanghao->broadcastSkillInvoke(objectName());
			room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, huanghao->objectName(), target->objectName());
            QList<int> to_show = card->getSubcards();
			int n = card->subcardsLength();
			room->notifyMoveToPile(target, to_show, "huisheng", Player::PlaceTable, true, true);
			QString prompt = "@huisheng-obtain:" + huanghao->objectName() + "::" + QString::number(n);
			QString pattern = "@@huisheng_obtain";
			bool optional = true;
			if (target->forceToDiscard(n, true).length() < n) {
				optional = false;
                pattern = "@@huisheng_obtain!";
			}
			const Card *to_obtain = room->askForCard(target, pattern, prompt, data, Card::MethodNone);
			room->notifyMoveToPile(target, to_show, "huisheng", Player::PlaceTable, false, false);
			if (to_obtain) {
				target->obtainCard(to_obtain, false);
                room->addPlayerMark(target, "huisheng" + huanghao->objectName());
                return true;
			} else if (optional)
				room->askForDiscard(target, "huisheng", n, n, false, true);
			else {
                room->addPlayerMark(target, "huisheng" + huanghao->objectName());
                target->obtainCard(Sanguosha->getCard(to_show.first()), false);
                return true;
			}
		}
        return false;
    }
};

class Guizao : public TriggerSkill
{
public:
    Guizao() : TriggerSkill("guizao")
    {
        events << CardsMoveOneTime << EventPhaseEnd << EventPhaseChanging;
        view_as_skill = new dummyVS;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == CardsMoveOneTime && player->getPhase() == Player::Discard) {
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            QVariantList guizaoRecord = player->tag["GuizaoRecord"].toList();
            if (move.from == player && (move.reason.m_reason & CardMoveReason::S_MASK_BASIC_REASON) == CardMoveReason::S_REASON_DISCARD) {
                foreach (int card_id, move.card_ids) {
                    guizaoRecord << card_id;
                }
            }
            player->tag["GuizaoRecord"] = guizaoRecord;
        } else if (triggerEvent == EventPhaseEnd && player->getPhase() == Player::Discard && TriggerSkill::triggerable(player)) {
            QVariantList guizaoRecord = player->tag["GuizaoRecord"].toList();
			if (guizaoRecord.length() < 2) return false;
			QStringList suitlist;
			foreach (QVariant card_data, guizaoRecord) {
                int card_id = card_data.toInt();
                const Card *card = Sanguosha->getCard(card_id);
                QString suit = card->getSuitString();
                if (!suitlist.contains(suit))
                    suitlist << suit;
                else{
					return false;
				}
            }
            QStringList choices;
            if (player->isWounded())
                choices << "recover";
            choices << "draw" << "cancel";
            QString choice = room->askForChoice(player, objectName(), choices.join("+"), data, QString(), "recover+draw+cancel");
            if (choice != "cancel") {
                LogMessage log;
                log.type = "#InvokeSkill";
                log.from = player;
                 log.arg = objectName();
                room->sendLog(log);

                room->notifySkillInvoked(player, objectName());
                player->broadcastSkillInvoke(objectName());
                if (choice == "recover")
                    room->recover(player, RecoverStruct(player));
                else
                    player->drawCards(1, objectName());
            }
        } else if (triggerEvent == EventPhaseChanging) {
            if (data.value<PhaseChangeStruct>().to == Player::Discard) {
                player->tag.remove("GuizaoRecord");
            }
        }

        return false;
    }
};

JiyuCard::JiyuCard()
{
    
}

bool JiyuCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && !Self->hasFlag("jiyu" + to_select->objectName()) && !to_select->isKongcheng();
}

void JiyuCard::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *source = effect.from;
	ServerPlayer *target = effect.to;
	Room *room = source->getRoom();
	room->setPlayerFlag(source, "jiyu" + target->objectName());
	if (target->canDiscard(target, "h")) {
        const Card *c = room->askForCard(target, ".!", "@jiyu-discard:" + source->objectName());
        if (c == NULL) {
            const Card *d = target->getHandcards().first();
            //c=Card::Parse(d->toString());
            CardMoveReason reason(CardMoveReason::S_REASON_THROW, target->objectName(), QString(), "jiyu", QString());
            room->moveCardTo(d, target, NULL, Player::DiscardPile, reason);
            c = d;

        }
		room->setPlayerCardLimitation(source, "use", QString(".|%1|.|.").arg(c->getSuitString()), true);
		if (c->getSuit() == Card::Spade) {
			source->turnOver();
        	room->loseHp(target);
		}
	}
}

class Jiyu : public ZeroCardViewAsSkill
{
public:
    Jiyu() : ZeroCardViewAsSkill("jiyu")
    {
    }

    virtual const Card *viewAs() const
    {
        return new JiyuCard;
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
		foreach (const Card *card, player->getHandcards()) {
            if (card->isAvailable(player))
                return true;
        }
        return false;
    }
};

TaoluanCard::TaoluanCard()
{
    will_throw = false;
    handling_method = Card::MethodNone;
    m_skillName = "taoluan";
}

bool TaoluanCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    Card *card = Sanguosha->cloneCard(user_string);
    if (card == NULL)
        return false;
    card->addSubcard(this);
    card->setSkillName("taoluan");
    card->setCanRecast(false);
    card->deleteLater();
    return card && card->targetFilter(targets, to_select, Self) && !Self->isProhibited(to_select, card, targets);
}

bool TaoluanCard::targetFixed() const
{
    Card *card = Sanguosha->cloneCard(user_string);
    if (card == NULL)
        return false;
    card->addSubcard(this);
    card->setSkillName("taoluan");
    card->setCanRecast(false);
    card->deleteLater();
    return card && card->targetFixed();
}

bool TaoluanCard::targetsFeasible(const QList<const Player *> &targets, const Player *Self) const
{
    Card *card = Sanguosha->cloneCard(user_string);
    if (card == NULL)
        return false;
    card->addSubcard(this);
    card->setSkillName("taoluan");
    card->setCanRecast(false);
    card->deleteLater();
    return card && card->targetsFeasible(targets, Self);
}

const Card *TaoluanCard::validate(CardUseStruct &card_use) const
{
    ServerPlayer *zhangrang = card_use.from;
    Room *room = zhangrang->getRoom();

    Card *c = Sanguosha->cloneCard(user_string, Card::NoSuit, 0);

    QString classname;
    if (c->isKindOf("Slash"))
        classname = "Slash";
    else
        classname = c->getClassName();

    room->setPlayerMark(zhangrang, "Taoluan_" + classname, 1);

    QStringList taoluanList = zhangrang->tag.value("taoluanClassName").toStringList();
    taoluanList << classname;
    zhangrang->tag["taoluanClassName"] = taoluanList;

    c->addSubcard(this);
    c->setSkillName("taoluan");
    c->deleteLater();
    return c;
}

const Card *TaoluanCard::validateInResponse(ServerPlayer *zhangrang) const
{
    Room *room = zhangrang->getRoom();

    Card *c = Sanguosha->cloneCard(user_string, Card::NoSuit, 0);

    QString classname;
    if (c->isKindOf("Slash"))
        classname = "Slash";
    else
        classname = c->getClassName();

    room->setPlayerMark(zhangrang, "Taoluan_" + classname, 1);

    QStringList taoluanList = zhangrang->tag.value("taoluanClassName").toStringList();
    taoluanList << classname;
    zhangrang->tag["taoluanClassName"] = taoluanList;

    c->addSubcard(this);
    c->setSkillName("taoluan");
    c->deleteLater();
    return c;

}

class TaoluanVS : public OneCardViewAsSkill
{
public:
    TaoluanVS() : OneCardViewAsSkill("taoluan")
    {
        filter_pattern = ".";
        response_or_use = true;
    }

    const Card *viewAs(const Card *originalCard) const
    {
        TaoluanCard *skill_card = new TaoluanCard;
        skill_card->addSubcard(originalCard);
        return skill_card;
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasFlag("TaoluanInvalid");
    }

    bool isEnabledAtResponse(const Player *player, const QString &pattern) const
    {
        if (Sanguosha->currentRoomState()->getCurrentCardUseReason() != CardUseStruct::CARD_USE_REASON_RESPONSE_USE)
            return false;

        if (player->hasFlag("Global_Dying") || player->hasFlag("TaoluanInvalid"))
            return false;
        foreach(const Player *sib, player->getAliveSiblings()) {
            if (sib->hasFlag("Global_Dying"))
                return false;
        }

#define TAOLUAN_CAN_USE(x) (player->getMark("Taoluan_" #x) == 0)

        if (pattern == "slash")
            return TAOLUAN_CAN_USE(Slash);
        else if (pattern == "peach")
            return TAOLUAN_CAN_USE(Peach);
        else if (pattern.contains("analeptic"))
            return TAOLUAN_CAN_USE(Peach) || TAOLUAN_CAN_USE(Analeptic);
        else if (pattern == "jink")
            return TAOLUAN_CAN_USE(Jink);
		else if (pattern == "nullification")
            return TAOLUAN_CAN_USE(Nullification);

        return false;
    }

    virtual bool isEnabledAtNullification(const ServerPlayer *player) const
    {
        if (player->hasFlag("Global_Dying") || player->hasFlag("TaoluanInvalid"))
            return false;
        foreach(const Player *sib, player->getAliveSiblings()) {
            if (sib->hasFlag("Global_Dying"))
                return false;
        }
        if (player->isNude() && player->getHandPile().isEmpty()) return false;
        return TAOLUAN_CAN_USE(Nullification);

#undef TAOLUAN_CAN_USE

    }
};

class Taoluan : public TriggerSkill
{
public:
    Taoluan() : TriggerSkill("taoluan")
    {
        view_as_skill = new TaoluanVS;
        events << CardFinished << EventPhaseChanging;
    }

    QString getSelectBox() const
    {
        return "guhuo_bt";
    }

    bool buttonEnabled(const QString &button_name, const QList<const Card *> &, const QList<const Player *> &) const
    {
        if (button_name.isEmpty())
            return true;
        Card *card = Sanguosha->cloneCard(button_name, Card::NoSuit, 0);
        if (card == NULL)
            return false;
        card->setSkillName("taoluan");
        QString classname = card->getClassName();
        if (card->isKindOf("Slash"))
            classname = "Slash";
        if (Self->getMark("Taoluan_" + classname) > 0)
            return false;
        return Skill::buttonEnabled(button_name);
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *zhangrang, QVariant &data) const
    {
        if (triggerEvent == CardFinished) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card->getSkillName() != objectName()) return false;
            ServerPlayer *target = room->askForPlayerChosen(zhangrang, room->getOtherPlayers(zhangrang), objectName(), "@taoluan-choose");
            QString type_name[4] = { QString(), "BasicCard", "TrickCard", "EquipCard" };
            QStringList types;
            types << "BasicCard" << "TrickCard" << "EquipCard";
            types.removeOne(type_name[use.card->getTypeId()]);
            const Card *card = room->askForCard(target, types.join(",") + "|.|.|.",
                    "@taoluan-give:" + zhangrang->objectName() + "::" + use.card->getType(), data, Card::MethodNone);
            if (card) {
                CardMoveReason reason(CardMoveReason::S_REASON_GIVE, target->objectName(), zhangrang->objectName(), objectName(), QString());
                reason.m_playerId = zhangrang->objectName();
                room->moveCardTo(card, target, zhangrang, Player::PlaceHand, reason);
                delete card;
            } else {
                room->loseHp(zhangrang);
                room->setPlayerFlag(zhangrang, "TaoluanInvalid");
            }
        } else if (triggerEvent == EventPhaseChanging) {
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (change.to == Player::NotActive) {
                foreach (ServerPlayer *p, room->getAlivePlayers()) {
                    if (p->hasFlag("TaoluanInvalid"))
                        room->setPlayerFlag(p, "-TaoluanInvalid");
                }
            }
        }
        return false;
    }
};

YJCM2016Package::YJCM2016Package()
: Package("YJCM2016")
{
    General *cenhun = new General(this, "cenhun", "wu", 3);
    cenhun->addSkill(new Jishe);
	cenhun->addSkill(new Lianhuo);

	General *huanghao = new General(this, "huanghao", "shu", 3);
	huanghao->addSkill(new Qinqing);
	huanghao->addSkill(new Huisheng);

	General *liuyu = new General(this, "liuyu", "qun", 2);
	liuyu->addSkill(new Zhige);
	liuyu->addSkill(new Zongzuo);

	General *sunziliufang = new General(this, "sunziliufang", "wei", 3);
	sunziliufang->addSkill(new Guizao);
	sunziliufang->addSkill(new Jiyu);

	General *zhangrang = new General(this, "zhangrang", "qun", 3);
	zhangrang->addSkill(new Taoluan);

    addMetaObject<ZhigeCard>();
	addMetaObject<JisheCard>();
	addMetaObject<JisheChainCard>();
    addMetaObject<QinqingCard>();
	addMetaObject<JiyuCard>();
	addMetaObject<TaoluanCard>();

    skills << new HuishengObtain;
}

ADD_PACKAGE(YJCM2016)
