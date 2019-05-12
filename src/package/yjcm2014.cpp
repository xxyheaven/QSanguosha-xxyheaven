#include "yjcm2014.h"
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

DingpinCard::DingpinCard()
{
}

bool DingpinCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *) const
{
    return targets.isEmpty() && to_select != Self && !to_select->hasFlag("dingpin");
}

void DingpinCard::onUse(Room *room, const CardUseStruct &card_use) const
{
    room->setPlayerFlag(card_use.from, "PindiUsed"+Sanguosha->getCard(getEffectiveId())->getType());
    Card::onUse(room, card_use);
}

void DingpinCard::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.from->getRoom();

    room->setPlayerFlag(effect.to, "dingpin");
    int x = effect.from->usedTimes("DingpinCard");
    if (x > 0) {
        QString choice = room->askForChoice(effect.from, "dingpin", "draw+discard", QVariant(), "@dingpin-choice:"+effect.to->objectName() + "::"+QString::number(x));
        if (choice == "draw")
            effect.to->drawCards(x, "dingpin");
        else
            room->askForDiscard(effect.to, "dingpin", x, x, false, true);
    }
    if (effect.to->isWounded() && !effect.from->isChained())
        room->setPlayerProperty(effect.from, "chained", true);
}

class Dingpin : public OneCardViewAsSkill
{
public:
    Dingpin() : OneCardViewAsSkill("dingpin")
    {
    }

    virtual bool isEnabledAtPlay(const Player *) const
    {
        return true;
    }

    virtual bool viewFilter(const Card *to_select) const
    {
        return !Self->isJilei(to_select) && !Self->hasFlag("PindiUsed"+to_select->getType());
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        DingpinCard *card = new DingpinCard;
        card->addSubcard(originalCard);
        return card;
    }
};

class Faen : public TriggerSkill
{
public:
    Faen() : TriggerSkill("faen")
    {
        events << TurnedOver << ChainStateChanged;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (triggerEvent == ChainStateChanged && !player->isChained()) return false;
        if (triggerEvent == TurnedOver && !player->faceUp()) return false;
        foreach (ServerPlayer *p, room->getAllPlayers()) {
            if (!player->isAlive()) return false;
            if (TriggerSkill::triggerable(p)
                && room->askForSkillInvoke(p, objectName(), QVariant::fromValue(player))) {
                p->broadcastSkillInvoke(objectName());
				room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, p->objectName(), player->objectName());
                player->drawCards(1, objectName());
            }
        }
        return false;
    }
};

class Sidi : public TriggerSkill
{
public:
    Sidi() : TriggerSkill("sidi")
    {
        events << EventPhaseStart << PreCardUsed << CardResponded << EventPhaseEnd;
        view_as_skill = new dummyVS;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->getPhase() == Player::Play;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *target, QVariant &data) const
    {
        if (triggerEvent == EventPhaseStart) {
            foreach (ServerPlayer *p, room->getOtherPlayers(target)) {
                if (TriggerSkill::triggerable(p) && p->hasEquip()) {
                    QString pattern;
                    const Card *e_card = p->getEquips().first();
                    if (e_card->isBlack())
                        pattern = "black";
                    else
                        pattern = "red";
                    foreach (const Card *c, p->getEquips()) {
                        if (c->getColor() != e_card->getColor()){
                            pattern = ".";
                            break;
                        }
                    }
                    const Card *card = room->askForCard(p, "^BasicCard|"+pattern, "@sidi-invoke:"+target->objectName(), data, objectName());
					if (card) {
                        room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, p->objectName(), target->objectName());
                        QString color;
                        if (card->isBlack())
                            color = "black";
                        else
                            color = "red";
                        target->tag[objectName()] = QVariant::fromValue(color);
                        target->tag["SidiUser"] = QVariant::fromValue(p);
                        room->addPlayerTip(target, QString("#sidi_%1").arg(color));
                        room->setPlayerCardLimitation(target, "use,response", QString(".|%1|.|hand$0").arg(color), false);
					}
				}
            }
        } else if (triggerEvent == EventPhaseEnd) {
            ServerPlayer *caozhen = target->tag["SidiUser"].value<ServerPlayer *>();
            target->tag.remove("SidiUser");
            if (!target->hasFlag("SidiSlashInPlayPhase") && caozhen && caozhen->canSlash(target, NULL, false)) {
                Slash *slash = new Slash(Card::NoSuit, 0);
                slash->setSkillName("_sidi");
                room->useCard(CardUseStruct(slash, caozhen, target));
            }
            target->setFlags("-SidiSlashInPlayPhase");
        } else {
            if (target->tag[objectName()].isNull()) return false;
            const Card *card = NULL;
            if (triggerEvent == PreCardUsed)
                card = data.value<CardUseStruct>().card;
            else {
                CardResponseStruct response = data.value<CardResponseStruct>();
                if (response.m_isUse)
                    card = response.m_card;
            }
            if (card && card->getHandlingMethod() == Card::MethodUse && card->isKindOf("Slash"))
                target->setFlags("SidiSlashInPlayPhase");
        }
        return false;
    }
};

class SidiClear : public TriggerSkill
{
public:
    SidiClear() : TriggerSkill("#sidi-clear")
    {
        events << EventPhaseEnd;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->getPhase() == Player::Play && !target->tag["sidi"].isNull();
    }

    int getPriority(TriggerEvent) const
    {
        return 1;
    }

    virtual bool trigger(TriggerEvent , Room *room, ServerPlayer *player, QVariant &) const
    {
        QString color = player->tag["sidi"].toString();
        player->tag.remove("sidi");
        room->removePlayerCardLimitation(player, "use,response", QString(".|%1|.|hand$0").arg(color));
        room->removePlayerTip(player, QString("#sidi_%1").arg(color));
        return false;
    }
};

class ShenduanViewAsSkill : public OneCardViewAsSkill
{
public:
    ShenduanViewAsSkill() : OneCardViewAsSkill("shenduan")
    {
        response_pattern = "@@shenduan";
		expand_pile = "#shenduan";
    }

    bool viewFilter(const Card *to_select) const
    {
        return Self->getPile("#shenduan").contains(to_select->getEffectiveId());
    }

    const Card *viewAs(const Card *originalCard) const
    {
        SupplyShortage *ss = new SupplyShortage(originalCard->getSuit(), originalCard->getNumber());
        ss->addSubcard(originalCard);
        ss->setSkillName("shenduan");
        ss->setFlags("Global_NoDistanceChecking");
        return ss;
    }
};

class Shenduan : public TriggerSkill
{
public:
    Shenduan() : TriggerSkill("shenduan")
    {
        events << CardsMoveOneTime;
        view_as_skill = new ShenduanViewAsSkill;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
        if (move.from != player)
            return false;
        if (move.to_place == Player::DiscardPile
            && ((move.reason.m_reason & CardMoveReason::S_MASK_BASIC_REASON) == CardMoveReason::S_REASON_DISCARD)) {

            int i = 0;
            QList<int> shenduan_card;
            foreach (int card_id, move.card_ids) {
                const Card *c = Sanguosha->getCard(card_id);
                if ((move.from_places[i] == Player::PlaceHand || move.from_places[i] == Player::PlaceEquip)
                    && c->isBlack() && c->getTypeId() == Card::TypeBasic && room->getCardPlace(card_id) == Player::DiscardPile) {
                    shenduan_card << card_id;
                }
                i++;
            }
            if (shenduan_card.isEmpty())
                return false;

            do {
				room->notifyMoveToPile(player, shenduan_card, "shenduan", Player::PlaceTable, true, true);
                const Card *use = room->askForUseCard(player, "@@shenduan", "@shenduan-use");
				if (use == NULL){
				    room->notifyMoveToPile(player, shenduan_card, "shenduan", Player::PlaceTable, false, false);
				    break;
				}
                int card_id = use->getSubcards().first();
                shenduan_card.removeOne(card_id);
				QList<int> shenduan_card2;
				foreach (int id, shenduan_card) {
                    if (room->getCardPlace(id) == Player::DiscardPile)
                        shenduan_card2 << id;
                }
                shenduan_card = shenduan_card2;
            } while (!shenduan_card.isEmpty());
        }
        return false;
    }
};

class ShenduanUse : public TriggerSkill
{
public:
    ShenduanUse() : TriggerSkill("#shenduan")
    {
        events << PreCardUsed;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card->isKindOf("SupplyShortage") && use.card->getSkillName() == "shenduan") {
            QList<int> ids = StringList2IntList(player->tag["shenduan_forAI"].toString().split("+"));
			room->notifyMoveToPile(player, ids, "shenduan", Player::PlaceTable, false, false);
        }
        return false;
    }
};

class YonglveViewAsSkill : public OneCardViewAsSkill
{
public:
    YonglveViewAsSkill() : OneCardViewAsSkill("yonglve")
    {
        response_pattern = "@@yonglve";
		expand_pile = "#yonglve";
    }

    bool viewFilter(const Card *to_select) const
    {
        return Self->getPile("#yonglve").contains(to_select->getEffectiveId());
    }

    const Card *viewAs(const Card *originalCard) const
    {
        return originalCard;
    }
};

class Yonglve : public PhaseChangeSkill
{
public:
    Yonglve() : PhaseChangeSkill("yonglve")
    {
		view_as_skill = new YonglveViewAsSkill;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
        if (target->getPhase() != Player::Judge) return false;
        Room *room = target->getRoom();
        foreach (ServerPlayer *hs, room->getOtherPlayers(target)) {
            if (target->isDead() || target->getJudgingArea().isEmpty()) break;
            if (!TriggerSkill::triggerable(hs) || !hs->inMyAttackRange(target)) continue;
			QList<int> judge_card;
			foreach(const Card *card, target->getJudgingArea()){
			    judge_card << card->getEffectiveId();
			}
			if (judge_card.isEmpty())
                return false;
			room->notifyMoveToPile(hs, judge_card, "yonglve", Player::PlaceTable, true, true);
			const Card *card = room->askForCard(hs, "@@yonglve", "@yonglve-use:" + target->objectName(), QVariant(), Card::MethodNone);
            room->notifyMoveToPile(hs, judge_card, "yonglve", Player::PlaceTable, false, false);
			if (card) {
                hs->broadcastSkillInvoke(objectName());
                room->notifySkillInvoked(hs, objectName());
                LogMessage log;
                log.from = hs;
                log.type = "#InvokeSkill";
                log.arg = objectName();
                room->sendLog(log);
				room->throwCard(card, NULL, hs);
				if (hs->isAlive() && target->isAlive() && hs->canSlash(target, false)) {
                    room->setTag("YonglveUser", QVariant::fromValue(hs));
                    Slash *slash = new Slash(Card::NoSuit, 0);
                    slash->setSkillName("_yonglve");
                    room->useCard(CardUseStruct(slash, hs, target));
                }
			}
        }
        return false;
    }
	
	virtual int getEffectIndex(const ServerPlayer *, const Card *card) const
    {
        if (card->isKindOf("Slash"))
            return 0;
        else
            return -1;
    }
};

class YonglveSlash : public TriggerSkill
{
public:
    YonglveSlash() : TriggerSkill("#yonglve")
    {
        events << PreDamageDone << CardFinished;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == PreDamageDone) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.card && damage.card->isKindOf("Slash") && damage.card->getSkillName() == "yonglve")
                damage.card->setFlags("YonglveDamage");
        } else if (!player->hasFlag("Global_ProcessBroken")) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card->isKindOf("Slash") && use.card->getSkillName() == "yonglve" && !use.card->hasFlag("YonglveDamage")) {
                ServerPlayer *hs = room->getTag("YonglveUser").value<ServerPlayer *>();
                if (hs)
                    hs->drawCards(1, "yonglve");
            }
        }
        return false;
    }
};

class Benxi : public TriggerSkill
{
public:
    Benxi() : TriggerSkill("benxi")
    {
        events << EventPhaseChanging << CardUsed << TargetChosed;
        frequency = Compulsory;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseChanging) {
            if (data.value<PhaseChangeStruct>().to == Player::NotActive)
                room->setPlayerMark(player, "#benxi", 0);
        }
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (TriggerSkill::triggerable(player) && player->getPhase() != Player::NotActive) {
            if (triggerEvent == CardUsed) {
                CardUseStruct use = data.value<CardUseStruct>();
                if (use.card->getTypeId() != Card::TypeSkill)
                    return QStringList(objectName());
            } else if (triggerEvent == TargetChosed) {
                QList<ServerPlayer *> targets = room->getOtherPlayers(player);
                foreach (ServerPlayer *p, targets) {
                    if (player->distanceTo(p) != 1)
                        return QStringList();
                }
                CardUseStruct use = data.value<CardUseStruct>();
                if (use.card->isKindOf("Slash") && !player->getUseExtraTargets(use, true).isEmpty())
                    return QStringList(objectName());
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        if (triggerEvent == CardUsed) {
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            room->addPlayerMark(player, "#benxi");
        } else if (triggerEvent == TargetChosed) {
            CardUseStruct use = data.value<CardUseStruct>();
            QList<ServerPlayer *> targets = player->getUseExtraTargets(use, true);
            ServerPlayer *extra = room->askForPlayerChosen(player, targets, objectName(), "@benxi-target", true);
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

// the part of Armor ignorance is coupled in Player::hasArmorEffect

class BenxiDistance : public DistanceSkill
{
public:
    BenxiDistance() : DistanceSkill("#benxi-dist")
    {
    }

    virtual int getCorrect(const Player *from, const Player *) const
    {
        return -from->getMark("#benxi");
    }
};

class Qiangzhi : public TriggerSkill
{
public:
    Qiangzhi() : TriggerSkill("qiangzhi")
    {
        events << EventPhaseStart << CardUsed << CardResponded << EventPhaseChanging;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseChanging) {
            if (data.value<PhaseChangeStruct>().from == Player::Play)
                room->setPlayerMark(player, "QiangzhiTypeId", 0);
        }
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (triggerEvent == EventPhaseStart) {
            if (TriggerSkill::triggerable(player) && player->getPhase() == Player::Play) {
                QList<ServerPlayer *> players = room->getOtherPlayers(player);
                foreach (ServerPlayer *p, players) {
                    if (!p->isKongcheng())
                        return QStringList(objectName());
                }
            }
        } else if (triggerEvent == CardUsed || triggerEvent == CardResponded) {
            if (player->getMark("QiangzhiTypeId") > 0) {
                const Card *cardstar = NULL;
                if (triggerEvent == CardUsed) {
                    CardUseStruct use = data.value<CardUseStruct>();
                    cardstar = use.card;
                } else {
                    CardResponseStruct resp = data.value<CardResponseStruct>();
                    if (resp.m_isUse)
                        cardstar = resp.m_card;
                }
                if (cardstar && int(cardstar->getTypeId()) == player->getMark("QiangzhiTypeId"))
                    return QStringList(objectName());
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        if (triggerEvent == EventPhaseStart) {
            QList<ServerPlayer *> targets;
            foreach (ServerPlayer *p, room->getOtherPlayers(player)) {
                if (!p->isKongcheng())
                    targets << p;
            }
            if (targets.isEmpty()) return false;
            ServerPlayer *target = room->askForPlayerChosen(player, targets, objectName(), "@qiangzhi-invoke", true, true);
            if (target) {
                player->broadcastSkillInvoke(objectName(), 1);
                int id = room->askForCardChosen(player, target, "h", objectName());
                room->showCard(target, id);
                player->setMark("QiangzhiTypeId", int(Sanguosha->getCard(id)->getTypeId()));
            }
        } else {
            if (room->askForChoice(player, objectName(), "yes+no", data, "@qiangzhi-draw") == "yes") {
                LogMessage log;
                log.type = "#SkillForce";
                log.from = player;
                log.arg = objectName();
                room->sendLog(log);
                player->broadcastSkillInvoke(objectName(), 2);
                player->drawCards(1, objectName());
            }

        }
        return false;
    }
};

class Xiantu : public TriggerSkill
{
public:
    Xiantu() : TriggerSkill("xiantu")
    {
        events << EventPhaseStart << EventPhaseEnd << EventPhaseChanging;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *, QVariant &data) const
    {
        if (triggerEvent == EventPhaseChanging) {
            if (data.value<PhaseChangeStruct>().from == Player::Play) {
                QList<ServerPlayer *> players = room->getAlivePlayers();
                foreach(ServerPlayer *p, players) {
                    room->setPlayerMark(p, "XiantuInvoked", 0);
                }
            }
        }
    }

    virtual TriggerList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        TriggerList skill_list;
        if (triggerEvent == EventPhaseStart && player->getPhase() == Player::Play && player->isAlive()) {
            QList<ServerPlayer *> zhangsongs = room->findPlayersBySkillName(objectName());
            foreach(ServerPlayer *zhangsong, zhangsongs) {
                if (zhangsong != player)
                    skill_list.insert(zhangsong, QStringList(objectName()));
            }
        } else if (triggerEvent == EventPhaseEnd && player->getPhase() == Player::Play && player->getMark("GlobalKilledCountinPlay") == 0) {
            QList<ServerPlayer *> players = room->getAlivePlayers();
            foreach(ServerPlayer *p, players) {
                if (p->getMark("XiantuInvoked") > 0) {
                    skill_list.insert(p, QStringList(objectName()));
                }
            }
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer *zhangsong) const
    {
        if (triggerEvent == EventPhaseStart) {
            if (room->askForSkillInvoke(zhangsong, objectName())) {
                zhangsong->broadcastSkillInvoke(objectName(), 1);
                room->addPlayerMark(zhangsong, "XiantuInvoked");
                zhangsong->drawCards(2, objectName());
                if (zhangsong->isAlive() && player->isAlive() && !zhangsong->isNude()) {
                    int num = qMin(2, zhangsong->getCardCount(true));
                    const Card *to_give = room->askForExchange(zhangsong, objectName(), num, num, true,
                        QString("@xiantu-give::%1:%2").arg(player->objectName()).arg(num));
                    player->obtainCard(to_give, false);
                    delete to_give;
                }
            }

        } else if (triggerEvent == EventPhaseEnd) {
            room->sendCompulsoryTriggerLog(zhangsong, objectName());
            zhangsong->broadcastSkillInvoke(objectName(), 2);
            room->loseHp(zhangsong);
        }
        return false;
    }
};

class Zhongyong : public TriggerSkill
{
public:
    Zhongyong() : TriggerSkill("zhongyong")
    {
        events << SlashMissed << CardFinished;
		view_as_skill = new dummyVS;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == SlashMissed) {
            SlashEffectStruct effect = data.value<SlashEffectStruct>();
            const Card *slash = effect.slash;
            const Card *jink = effect.jink;
            if (!slash || !jink) return false;
            QVariantList zhongyongJink = slash->tag["ZhongyongJink"].toList();
            if (!jink->isVirtualCard())
                zhongyongJink << jink->getEffectiveId();
            else {
                foreach (int id, jink->getSubcards()) {
                    zhongyongJink << id;
                }
            }
            slash->tag["ZhongyongJink"] = zhongyongJink;
        } else if (triggerEvent == CardFinished) {
            CardUseStruct use = data.value<CardUseStruct>();
            const Card *slash = use.card;
            if (!slash->isKindOf("Slash")) return false;
            QVariantList zhongyongJink = slash->tag["ZhongyongJink"].toList();
            slash->tag.remove("ZhongyongJink");
            QList<int> jinks;
            foreach (QVariant card_data, zhongyongJink) {
                int card_id = card_data.toInt();
                jinks << card_id;
            }
            QStringList choices;
            if (room->isAllOnPlace(slash, Player::PlaceTable))
                choices << "giveslash";
            if (room->isAllOnPlace(jinks, Player::DiscardPile))
                choices << "givejink";
            QList<ServerPlayer *> targets;
            foreach (ServerPlayer *p, room->getOtherPlayers(player)) {
                if (!use.to.contains(p))
                    targets << p;
            }
            if (choices.isEmpty() || targets.isEmpty()) return false;
            choices << "cancel";
            QString choice = room->askForChoice(player, objectName(), choices.join("+"), data, "@zhongyong-give", "giveslash+givejink+cancel");
            if (choice == "cancel") return false;
            ServerPlayer *target = room->askForPlayerChosen(player, targets, objectName(), "@zhongyong-give", true, true);
            if (target) {
                player->broadcastSkillInvoke(objectName());
                bool has_red = false;
                if (choice == "giveslash") {
                    target->obtainCard(slash);
                    has_red = (!slash->isBlack());
                } else {
                    DummyCard *dummy = new DummyCard(jinks);
                    room->obtainCard(target, dummy);
                    delete dummy;
                    foreach (int id, jinks) {
                        if (Sanguosha->getCard(id)->isRed()) {
                            has_red = true;
                            break;
                        }
                    }
                }
                if (has_red) {
                    QList<ServerPlayer *> zhongyong_targets;
                    foreach (ServerPlayer *p, room->getAlivePlayers()) {
                        if (player->inMyAttackRange(p))
                            zhongyong_targets << p;
                    }
                    room->askForUseSlashTo(target, zhongyong_targets, "@zhongyong-slash:"+player->objectName(), false);
                }
            }
        }
        return false;
    }
};

ShenxingCard::ShenxingCard()
{
    target_fixed = true;
}

void ShenxingCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const
{
    if (source->isAlive())
        room->drawCards(source, 1, "shenxing");
}

class Shenxing : public ViewAsSkill
{
public:
    Shenxing() : ViewAsSkill("shenxing")
    {
    }

    virtual bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
        return selected.length() < 2 && !Self->isJilei(to_select);
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.length() != 2)
            return NULL;

        ShenxingCard *card = new ShenxingCard;
        card->addSubcards(cards);
        return card;
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return player->getCardCount(true) >= 2 && player->canDiscard(player, "he");
    }
};

class Bingyi : public PhaseChangeSkill
{
public:
    Bingyi() : PhaseChangeSkill("bingyi")
    {
    }

    virtual bool onPhaseChange(ServerPlayer *target) const
    {
		Room *room = target->getRoom();
        if (target->getPhase() != Player::Finish || target->isKongcheng()) return false;
        if (room->askForSkillInvoke(target, objectName())){
            target->broadcastSkillInvoke(objectName());
			room->showAllCards(target);
			bool trigger_this = true;
			Card::Color color = Card::Colorless;
            foreach (const Card *c, target->getHandcards()) {
                if (color == Card::Colorless)
                    color = c->getColor();
                else if (c->getColor() != color){
                    trigger_this = false;
                    break;
				}
            }
            if (trigger_this) {
                int x = target->getHandcardNum();
                QList<ServerPlayer *> choosees = room->askForPlayersChosen(target, room->getAlivePlayers(), objectName(), 1, x,
                        "@bingyi-target:::"+QString::number(x));
                foreach(ServerPlayer *p, choosees)
                    p->drawCards(1, objectName());
            }
		}
        return false;
    }
};

class Zenhui : public TriggerSkill
{
public:
    Zenhui() : TriggerSkill("zenhui")
    {
        events << TargetSpecifying << CardFinished;
		view_as_skill = new dummyVS;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        if (triggerEvent == CardFinished && (use.card->isKindOf("Slash") || (use.card->isNDTrick() && use.card->isBlack()))) {
            use.from->setFlags("-ZenhuiUser_" + use.card->toString());
            return false;
        }
        if (!TriggerSkill::triggerable(player) || player->getPhase() != Player::Play || player->hasFlag(objectName()))
            return false;

        if (use.to.length() == 1 && !use.card->targetFixed()
            && (use.card->isKindOf("Slash") || (use.card->isNDTrick() && use.card->isBlack()))) {
            QList<ServerPlayer *> targets;
            foreach (ServerPlayer *p, room->getAlivePlayers()) {
                if (p != player && p != use.to.first() && !room->isProhibited(player, p, use.card) && use.card->targetFilter(QList<const Player *>(), p, player))
                    targets << p;
            }
            if (targets.isEmpty()) return false;
            use.from->tag["zenhui"] = data;
            ServerPlayer *target = room->askForPlayerChosen(player, targets, objectName(), "zenhui-invoke:" + use.to.first()->objectName(), true, true);
            use.from->tag.remove("zenhui");
            if (target) {
                player->setFlags(objectName());

                // Collateral
                ServerPlayer *collateral_victim = NULL;
                if (use.card->isKindOf("Collateral")) {
                    QList<ServerPlayer *> victims;
                    foreach (ServerPlayer *p, room->getOtherPlayers(target)) {
                        if (target->canSlash(p))
                            victims << p;
                    }
                    Q_ASSERT(!victims.isEmpty());
                    collateral_victim = room->askForPlayerChosen(player, victims, "zenhui_collateral", "@zenhui-collateral:" + target->objectName());
                    target->tag["collateralVictim"] = QVariant::fromValue((collateral_victim));

                    LogMessage log;
                    log.type = "#CollateralSlash";
                    log.from = player;
                    log.to << collateral_victim;
                    room->sendLog(log);
                }

                player->broadcastSkillInvoke(objectName());

                bool extra_target = true;
                if (!target->isNude()) {
                    const Card *card = room->askForCard(target, "..", "@zenhui-give:" + player->objectName(), data, Card::MethodNone);
                    if (card) {
                        extra_target = false;
                        CardMoveReason reason(CardMoveReason::S_REASON_GIVE, target->objectName(),
                            player->objectName(), objectName(), QString());
                        reason.m_playerId = player->objectName();
                        room->moveCardTo(card, target, player, Player::PlaceHand, reason);

                        if (target->isAlive()) {
                            LogMessage log;
                            log.type = "#BecomeUser";
                            log.from = target;
                            log.card_str = use.card->toString();
                            room->sendLog(log);

                            target->setFlags("ZenhuiUser_" + use.card->toString()); // For AI
                            use.from = target;
                            data = QVariant::fromValue(use);
                        }
                    }
                }
                if (extra_target) {
                    LogMessage log;
                    log.type = "#BecomeTarget";
                    log.from = target;
                    log.card_str = use.card->toString();
                    room->sendLog(log);

                    if (use.card->isKindOf("Collateral") && collateral_victim)
                        room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, target->objectName(), collateral_victim->objectName());

                    use.to.append(target);
                    room->sortByActionOrder(use.to);
                    data = QVariant::fromValue(use);
                }
            }
        }
        return false;
    }
};

class Jiaojin : public TriggerSkill
{
public:
    Jiaojin() : TriggerSkill("jiaojin")
    {
        events << DamageInflicted;
		view_as_skill = new dummyVS;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (damage.from && damage.from->isMale() && player->canDiscard(player, "he")) {
            if (room->askForCard(player, ".Equip", "@jiaojin", data, objectName())) {

                LogMessage log;
                log.type = "#Jiaojin";
                log.from = player;
                log.arg = QString::number(damage.damage);
                log.arg2 = QString::number(--damage.damage);
                room->sendLog(log);

                if (damage.damage < 1)
                    return true;
                data = QVariant::fromValue(damage);
            }
        }
        return false;
    }
};

class Fenli : public TriggerSkill
{
public:
    Fenli() : TriggerSkill("fenli")
    {
        events << EventPhaseChanging;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        PhaseChangeStruct change = data.value<PhaseChangeStruct>();
        if (player->isSkipped(change.to)) return false;
        QString phase_string;
        switch (change.to) {
        case Player::Draw: {
            QList<ServerPlayer *> players = room->getOtherPlayers(player);
            foreach (ServerPlayer *p, players) {
                if (p->getHandcardNum() > player->getHandcardNum()) {
                    return false;
                }
            }
            phase_string = "draw";
            break;
        }
        case Player::Play: {
            QList<ServerPlayer *> players = room->getOtherPlayers(player);
            foreach (ServerPlayer *p, players) {
                if (p->getHp() > player->getHp()) {
                    return false;
                }
            }
            phase_string = "play";
            break;
        }
        case Player::Discard: {
            if (!player->hasEquip()) return false;
            QList<ServerPlayer *> players = room->getOtherPlayers(player);
            foreach (ServerPlayer *p, players) {
                if (p->getEquips().length() > player->getEquips().length()) {
                    return false;
                }
            }
            phase_string = "discard";
            break;
        }
        default: return false;
        }
        if (player->askForSkillInvoke(objectName(), "prompt:::"+phase_string)) {
            player->broadcastSkillInvoke(objectName());
            player->skip(change.to);
        }
        return false;
    }
};

PingkouCard::PingkouCard()
{
}

bool PingkouCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.length() < Self->getMark("PingkouNum") && to_select != Self;
}

void PingkouCard::onEffect(const CardEffectStruct &effect) const
{
    effect.to->getRoom()->damage(DamageStruct("pingkou", effect.from, effect.to));
}

class PingkouViewAsSkill : public ZeroCardViewAsSkill
{
public:
    PingkouViewAsSkill() : ZeroCardViewAsSkill("pingkou")
    {
        response_pattern = "@@pingkou";
    }

    virtual const Card *viewAs() const
    {
        return new PingkouCard;
    }
};

class Pingkou : public TriggerSkill
{
public:
    Pingkou() : TriggerSkill("pingkou")
    {
        events << EventPhaseChanging;
        view_as_skill = new PingkouViewAsSkill;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (data.value<PhaseChangeStruct>().to != Player::NotActive) return false;
        int n = 0;
        for (int i = 1; i < 7; i++) {
            Player::Phase phase = (Player::Phase)i;
            if (player->hasSkipped(phase)) n++;
        }
        if (n < 1) return false;
        room->setPlayerMark(player, "PingkouNum", n);
        room->askForUseCard(player, "@@pingkou", "@pingkou-invoke:::" + QString::number(n));
        room->setPlayerMark(player, "PingkouNum", 0);
        return false;
    }
};

class Qieting : public PhaseChangeSkill
{
public:
    Qieting() : PhaseChangeSkill("qieting")
    {
        view_as_skill = new dummyVS;
    }

    virtual int getPriority(TriggerEvent) const
    {
        return 1;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->getPhase() == Player::NotActive && target->getMark("qieting") == 0;
    }

    virtual bool onPhaseChange(ServerPlayer *player) const
    {
        Room *room = player->getRoom();
        foreach (ServerPlayer *caifuren, room->getAllPlayers()) {
            if (!TriggerSkill::triggerable(caifuren) || caifuren == player) continue;
            QStringList choices;
            QList<int> disables;
            foreach (const Card *card, player->getEquips()) {
                if (caifuren->canSetEquip(card)) {
                    if (!choices.contains("move"))
                        choices << "move";
                } else
                    disables << card->getEffectiveId();
            }
            choices << "draw" << "cancel";
            QString choice = room->askForChoice(caifuren, objectName(), choices.join("+"), QVariant::fromValue(player), QString(), "move+draw+cancel");
            if (choice == "cancel") {
                continue;
            } else {
                LogMessage log;
                log.type = "#InvokeSkill";
                log.arg = objectName();
                log.from = caifuren;
                room->sendLog(log);
                room->notifySkillInvoked(caifuren, objectName());
                caifuren->broadcastSkillInvoke(objectName());
                if (choice == "draw") {
                    caifuren->drawCards(1, objectName());
                } else {
                    int card_id = room->askForCardChosen(caifuren, player, "e", objectName(), false, Card::MethodNone, disables);
                    if (card_id < 0) return false;
                    const Card *card = Sanguosha->getCard(card_id);
                    room->moveCardTo(card, caifuren, Player::PlaceEquip);
                }
            }
        }
        return false;
    }
};

XianzhouDamageCard::XianzhouDamageCard()
{
    mute = true;
}

void XianzhouDamageCard::onUse(Room *room, const CardUseStruct &card_use) const
{
    foreach (ServerPlayer *p, room->getAlivePlayers()) {
		if (card_use.to.contains(p))
            room->damage(DamageStruct("xianzhou", card_use.from, p));
	}
}

bool XianzhouDamageCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.length() < Self->getMark("xianzhou") && Self->inMyAttackRange(to_select);
}


XianzhouCard::XianzhouCard()
{
}

bool XianzhouCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select != Self;
}

void XianzhouCard::extraCost(Room *room, const CardUseStruct &card_use) const
{
    room->removePlayerMark(card_use.from, "@handover");

    DummyCard *dummy = new DummyCard;
    foreach (const Card *c, card_use.from->getEquips())
        dummy->addSubcard(c);

	CardMoveReason reason(CardMoveReason::S_REASON_GIVE, card_use.from->objectName(), card_use.to.first()->objectName(), "xianzhou", QString());
    room->obtainCard(card_use.to.first(), dummy, reason, false);
	card_use.from->setMark("xianzhou_len", dummy->subcardsLength());
    delete dummy;
}

void XianzhouCard::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.from->getRoom();
    int len = effect.from->getMark("xianzhou_len");
	effect.from->setMark("xianzhou_len", 0);
	room->setPlayerMark(effect.to, "xianzhou", len);

    if (!room->askForUseCard(effect.to, "@xianzhou", "@xianzhou-damage:" + effect.from->objectName() + "::" + QString::number(len)) && effect.from->isWounded())
        room->recover(effect.from, RecoverStruct(effect.to, NULL, len));
}

class Xianzhou : public ZeroCardViewAsSkill
{
public:
    Xianzhou() : ZeroCardViewAsSkill("xianzhou")
    {
        frequency = Skill::Limited;
        limit_mark = "@handover";
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return player->getMark("@handover") > 0 && player->getEquips().length() > 0;
    }

    virtual bool isEnabledAtResponse(const Player *, const QString &pattern) const
    {
        return pattern == "@xianzhou";
    }

    virtual const Card *viewAs() const
    {
        QString pattern = Sanguosha->currentRoomState()->getCurrentCardUsePattern();
        if (pattern == "@xianzhou") {
            return new XianzhouDamageCard;
        } else {
            return new XianzhouCard;
        }
    }
};

class Jianying : public TriggerSkill
{
public:
    Jianying() : TriggerSkill("jianying")
    {
        events << CardUsed << CardResponded;
        frequency = Frequent;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (TriggerSkill::triggerable(player) && player->getPhase() == Player::Play) {
            const Card *card = NULL;
            if (triggerEvent == CardUsed)
                card = data.value<CardUseStruct>().card;
            else if (triggerEvent == CardResponded) {
                CardResponseStruct resp = data.value<CardResponseStruct>();
                if (resp.m_isUse)
                    card = resp.m_card;
            }
            if (card && card->getTypeId() != Card::TypeSkill && card->getHandlingMethod() == Card::MethodUse) {
                QVariantList card_list = player->tag["PhaseUsedCards"].toList();
                if (card_list.length() > 1) {
                    QVariant card_data = card_list.at(card_list.length()-2);
                    const Card *last_card = card_data.value<const Card *>();
                    if (last_card && (card->sameSuitWith(last_card) || card->sameNumberWith(last_card)))
                        return QStringList(objectName());
                }
            }

        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        if (player->askForSkillInvoke(this)) {
            player->broadcastSkillInvoke(objectName());
            player->drawCards(1, objectName());
        }
        return false;
    }
};

class Shibei : public MasochismSkill
{
public:
    Shibei() : MasochismSkill("shibei")
    {
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (MasochismSkill::triggerable(player)) {
            DamageStruct damage = data.value<DamageStruct>();
            return (damage.flags.contains("FirstDamge") || damage.flags.contains("NotFirstDamge")) ? QStringList(objectName()) : QStringList();
        }
        return QStringList();
    }

    virtual void onDamaged(ServerPlayer *player, const DamageStruct &damage) const
    {
        Room *room = player->getRoom();
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());
        if (damage.flags.contains("FirstDamge"))
            room->recover(player, RecoverStruct(player));
        if (damage.flags.contains("NotFirstDamge"))
            room->loseHp(player);
    }

};


YJCM2014Package::YJCM2014Package()
    : Package("YJCM2014")
{
    General *caifuren = new General(this, "caifuren", "qun", 3, false); // YJ 301
    caifuren->addSkill(new Qieting);
    caifuren->addSkill(new Xianzhou);

    General *caozhen = new General(this, "caozhen", "wei"); // YJ 302
    caozhen->addSkill(new Sidi);
    caozhen->addSkill(new SidiClear);
    related_skills.insertMulti("sidi", "#sidi-clear");

    General *chenqun = new General(this, "chenqun", "wei", 3); // YJ 303
    chenqun->addSkill(new Dingpin);
    chenqun->addSkill(new Faen);

    General *guyong = new General(this, "guyong", "wu", 3); // YJ 304
    guyong->addSkill(new Shenxing);
    guyong->addSkill(new Bingyi);

    General *hanhaoshihuan = new General(this, "hanhaoshihuan", "wei"); // YJ 305
    hanhaoshihuan->addSkill(new Shenduan);
    hanhaoshihuan->addSkill(new ShenduanUse);
    hanhaoshihuan->addSkill(new Yonglve);
    hanhaoshihuan->addSkill(new YonglveSlash);
    related_skills.insertMulti("shenduan", "#shenduan");
    related_skills.insertMulti("yonglve", "#yonglve");

    General *jvshou = new General(this, "jvshou", "qun", 3); // YJ 306
    jvshou->addSkill(new Jianying);
    jvshou->addSkill(new Shibei);

    General *sunluban = new General(this, "sunluban", "wu", 3, false); // YJ 307
    sunluban->addSkill(new Zenhui);
    sunluban->addSkill(new Jiaojin);

    General *wuyi = new General(this, "wuyi", "shu"); // YJ 308
    wuyi->addSkill(new Benxi);
    wuyi->addSkill(new BenxiDistance);
    related_skills.insertMulti("benxi", "#benxi-dist");

    General *zhangsong = new General(this, "zhangsong", "shu", 3); // YJ 309
    zhangsong->addSkill(new Qiangzhi);
    zhangsong->addSkill(new Xiantu);

    General *zhoucang = new General(this, "zhoucang", "shu"); // YJ 310
    zhoucang->addSkill(new Zhongyong);

    General *zhuhuan = new General(this, "zhuhuan", "wu"); // YJ 311
    zhuhuan->addSkill(new Fenli);
    zhuhuan->addSkill(new Pingkou);

    addMetaObject<DingpinCard>();
    addMetaObject<ShenxingCard>();
    addMetaObject<XianzhouCard>();
    addMetaObject<XianzhouDamageCard>();
    addMetaObject<PingkouCard>();

}

ADD_PACKAGE(YJCM2014)
