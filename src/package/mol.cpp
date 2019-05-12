#include "mol.h"
#include "client.h"
#include "general.h"
#include "skill.h"
#include "standard-skillcards.h"
#include "engine.h"
#include "maneuvering.h"
#include "json.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCommandLinkButton>
#include "settings.h"

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

class Dujin : public DrawCardsSkill
{
public:
    Dujin() : DrawCardsSkill("dujin")
    {
        frequency = Frequent;
    }

    int getDrawNum(ServerPlayer *player, int n) const
    {
        if (player->askForSkillInvoke(this)) {
            player->broadcastSkillInvoke(objectName());
            return n + player->getEquips().length() / 2 + 1;
        } else
            return n;
    }
};

class YingjianViewAsSkill : public ZeroCardViewAsSkill
{
public:
    YingjianViewAsSkill() : ZeroCardViewAsSkill("yingjian")
    {
        response_pattern = "@@yingjian";
    }

    const Card *viewAs() const
    {
        Slash *slash = new Slash(Card::NoSuit, 0);
        slash->setSkillName("yingjian");
        slash->setFlags("Global_NoDistanceChecking");
        return slash;
    }
};

class Yingjian : public TriggerSkill
{
public:
    Yingjian() : TriggerSkill("yingjian")
    {
        events << EventPhaseStart;
        view_as_skill = new YingjianViewAsSkill;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (player->getPhase() == Player::Start && Slash::IsAvailable(player))
            room->askForUseCard(player, "@@yingjian", "@yingjian-slash");
        return false;
    }
};

class Shixin : public TriggerSkill
{
public:
    Shixin() : TriggerSkill("shixin")
    {
        events << DamageInflicted;
        frequency = Compulsory;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (damage.nature == DamageStruct::Fire) {
            room->notifySkillInvoked(player, objectName());
            player->broadcastSkillInvoke(objectName());

            LogMessage log;
            log.type = "#ShixinProtect";
            log.from = player;
            log.arg = QString::number(damage.damage);
            log.arg2 = "fire_nature";
            room->sendLog(log);
            return true;
        }
        return false;
    }
};

class Fenyin : public TriggerSkill
{
public:
    Fenyin() : TriggerSkill("fenyin")
    {
        events << CardUsed << CardResponded;
        frequency = Frequent;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (TriggerSkill::triggerable(player) && player->getPhase() != Player::NotActive) {
            const Card *card = NULL;
            if (triggerEvent == CardUsed)
                card = data.value<CardUseStruct>().card;
            else if (triggerEvent == CardResponded) {
                CardResponseStruct resp = data.value<CardResponseStruct>();
                if (resp.m_isUse)
                    card = resp.m_card;
            }
            if (card && card->getTypeId() != Card::TypeSkill && card->getHandlingMethod() == Card::MethodUse) {
                QVariantList card_list = player->tag["RoundUsedCards"].toList();
                if (card_list.length() > 1) {
                    QVariant card_data = card_list.at(card_list.length()-2);
                    const Card *last_card = card_data.value<const Card *>();
                    if (last_card && (!card->sameColorWith(last_card)))
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

class TunchuDraw : public DrawCardsSkill
{
public:
    TunchuDraw() : DrawCardsSkill("tunchu")
    {
        view_as_skill = new dummyVS;
    }

    int getDrawNum(ServerPlayer *player, int n) const
    {
        if (player->askForSkillInvoke("tunchu")) {
            player->setFlags("tunchu");
            player->broadcastSkillInvoke("tunchu");
            return n + 2;
        }

        return n;
    }
};

class TunchuEffect : public TriggerSkill
{
public:
    TunchuEffect() : TriggerSkill("#tunchu-effect")
    {
        events << AfterDrawNCards;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (player->hasFlag("tunchu") && !player->isKongcheng()) {
            const Card *c = room->askForExchange(player, "tunchu", 1000, 1, false, "@tunchu-put", true);
            if (c != NULL)
                player->addToPile("food", c);
        }

        return false;
    }
};

class Tunchu : public CardLimitedSkill
{
public:
    Tunchu() : CardLimitedSkill("#tunchu-disable")
    {
    }
    virtual bool isCardLimited(const Player *player, const Card *card, Card::HandlingMethod method) const
    {
        if (player->hasSkill("tunchu") && !player->getPile("food").isEmpty() && card->isKindOf("Slash") && method == Card::MethodUse) return true;
        return false;
    }
};

class ShuliangVS : public OneCardViewAsSkill
{
public:
    ShuliangVS() : OneCardViewAsSkill("shuliang")
    {
        response_pattern = "@@shuliang";
        filter_pattern = ".|.|.|food";
        expand_pile = "food";
    }

    const Card *viewAs(const Card *originalCard) const
    {
        return originalCard;
    }
};

class Shuliang : public TriggerSkill
{
public:
    Shuliang() : TriggerSkill("shuliang")
    {
        events << EventPhaseStart;
        view_as_skill = new ShuliangVS;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->isAlive() && target->getPhase() == Player::Finish;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        foreach (ServerPlayer *const &p, room->getAllPlayers()) {
            if (player->isDead() || player->getHandcardNum() >= player->getHp()) break;
			if (!TriggerSkill::triggerable(p) || p->getPile("food").isEmpty()) continue;

            const Card *card = room->askForCard(p, "@@shuliang", "@shuliang:" + player->objectName(), data, Card::MethodNone);
            if (card) {
                room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, p->objectName(), player->objectName());
                p->broadcastSkillInvoke(objectName());
				CardMoveReason r(CardMoveReason::S_REASON_REMOVE_FROM_PILE, p->objectName(), objectName(), QString());
				room->moveCardTo(card, NULL, Player::DiscardPile, r, true);
				player->drawCards(2, objectName());
			}
        }
		return false;
    }
};

ZhanyiViewAsBasicCard::ZhanyiViewAsBasicCard()
{
    m_skillName = "_zhanyi";
    will_throw = false;
}

bool ZhanyiViewAsBasicCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    if (Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE_USE) {
        const Card *card = NULL;
        if (!user_string.isEmpty())
            card = Sanguosha->cloneCard(user_string.split("+").first());
        return card && card->targetFilter(targets, to_select, Self) && !Self->isProhibited(to_select, card, targets);
    } else if (Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE) {
        return false;
    }

    Card *card = Sanguosha->cloneCard(user_string);
    if (card == NULL)
        return false;
    card->addSubcard(this);
    card->setSkillName(getSkillName());
    return card->targetFilter(targets, to_select, Self) && !Self->isProhibited(to_select, card, targets);
}

bool ZhanyiViewAsBasicCard::targetFixed() const
{
    if (Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE_USE) {
        const Card *card = NULL;
        if (!user_string.isEmpty())
            card = Sanguosha->cloneCard(user_string.split("+").first());
        return card && card->targetFixed();
    } else if (Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE) {
        return true;
    }

    Card *card = Sanguosha->cloneCard(user_string);
    if (card == NULL)
        return false;
    card->addSubcard(this);
    card->setSkillName(getSkillName());
    return card->targetFixed();
}

bool ZhanyiViewAsBasicCard::targetsFeasible(const QList<const Player *> &targets, const Player *Self) const
{
    if (Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE_USE) {
        const Card *card = NULL;
        if (!user_string.isEmpty())
            card = Sanguosha->cloneCard(user_string.split("+").first());
        return card && card->targetsFeasible(targets, Self);
    } else if (Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE) {
        return true;
    }

    Card *card = Sanguosha->cloneCard(user_string);
    if (card == NULL)
        return false;
    card->addSubcard(this);
    card->setSkillName(getSkillName());
    return card->targetsFeasible(targets, Self);
}

const Card *ZhanyiViewAsBasicCard::validate(CardUseStruct &) const
{
    const Card *card = Sanguosha->getCard(subcards.first());
    QString user_str = user_string;
    Card *use_card = Sanguosha->cloneCard(user_str, card->getSuit(), card->getNumber());
    use_card->setSkillName("_zhanyi");
    use_card->addSubcard(subcards.first());
    use_card->deleteLater();
    return use_card;
}

const Card *ZhanyiViewAsBasicCard::validateInResponse(ServerPlayer *) const
{
    const Card *card = Sanguosha->getCard(subcards.first());
    QString user_str = user_string;
    Card *use_card = Sanguosha->cloneCard(user_str, card->getSuit(), card->getNumber());
    use_card->setSkillName("_zhanyi");
    use_card->addSubcard(subcards.first());
    use_card->deleteLater();
    return use_card;
}

ZhanyiCard::ZhanyiCard()
{
    target_fixed = true;
}

void ZhanyiCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const
{
    room->loseHp(source);
    if (source->isAlive()) {
        const Card *c = Sanguosha->getCard(subcards.first());
        if (c->getTypeId() == Card::TypeBasic) {
            room->setPlayerMark(source, "ViewAsSkill_zhanyiEffect", 1);
        } else if (c->getTypeId() == Card::TypeEquip)
            source->setFlags("zhanyiEquip");
        else if (c->getTypeId() == Card::TypeTrick) {
            source->drawCards(2, "zhanyi");
            room->setPlayerFlag(source, "zhanyiTrick");
        }
    }
}

class ZhanyiNoDistanceLimit : public TargetModSkill
{
public:
    ZhanyiNoDistanceLimit() : TargetModSkill("#zhanyi-trick")
    {
        pattern = "^SkillCard";
    }

    int getDistanceLimit(const Player *from, const Card *, const Player *) const
    {
        return from->hasFlag("zhanyiTrick") ? 1000 : 0;
    }
};

class ZhanyiDiscard2 : public TriggerSkill
{
public:
    ZhanyiDiscard2() : TriggerSkill("#zhanyi-equip")
    {
        events << TargetSpecified;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->isAlive() && target->hasFlag("zhanyiEquip");
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *, QVariant &data) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card == NULL || !use.card->isKindOf("Slash"))
            return false;


        foreach (ServerPlayer *p, use.to) {
            room->askForDiscard(p, "zhanyi", 2, 2, false, true, "@zhanyiequip_discard");
        }
        return false;
    }
};

class Zhanyi : public OneCardViewAsSkill
{
public:
    Zhanyi() : OneCardViewAsSkill("zhanyi")
    {

    }

    bool isResponseOrUse() const
    {
        return Self->getMark("ViewAsSkill_zhanyiEffect") > 0;
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        if (!player->hasUsed("ZhanyiCard"))
            return true;

        if (player->getMark("ViewAsSkill_zhanyiEffect") > 0)
            return true;

        return false;
    }

    bool isEnabledAtResponse(const Player *player, const QString &pattern) const
    {
        if (player->getMark("ViewAsSkill_zhanyiEffect") == 0) return false;
        if (pattern.startsWith(".") || pattern.startsWith("@")) return false;
        for (int i = 0; i < pattern.length(); i++) {
            QChar ch = pattern[i];
            if (ch.isUpper() || ch.isDigit()) return false; // This is an extremely dirty hack!! For we need to prevent patterns like 'BasicCard'
        }
        return !(pattern == "nullification");
    }

    QString getSelectBox() const
    {
        return "guhuo_b";
    }

    bool buttonEnabled(const QString &button_name, const QList<const Card *> &, const QList<const Player *> &) const
    {
        if (Self->getMark("ViewAsSkill_zhanyiEffect") == 0)
            return false;
        return Skill::buttonEnabled(button_name);
    }

    bool viewFilter(const Card *to_select) const
    {
        if (Self->getMark("ViewAsSkill_zhanyiEffect") > 0)
            return to_select->isKindOf("BasicCard");
        else
            return true;
    }

    const Card *viewAs(const Card *originalCard) const
    {
        if (Self->getMark("ViewAsSkill_zhanyiEffect") == 0) {
            ZhanyiCard *zy = new ZhanyiCard;
            zy->addSubcard(originalCard);
            return zy;
        }

        if (Sanguosha->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE
            || Sanguosha->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE_USE) {
            ZhanyiViewAsBasicCard *card = new ZhanyiViewAsBasicCard;
            card->setUserString(Sanguosha->getCurrentCardUsePattern());
            card->addSubcard(originalCard);
            return card;
        }

        ZhanyiViewAsBasicCard *card = new ZhanyiViewAsBasicCard;
        card->addSubcard(originalCard);
        return card;
    }
};

class ZhanyiRemove : public TriggerSkill
{
public:
    ZhanyiRemove() : TriggerSkill("#zhanyi-basic")
    {
        events << EventPhaseStart;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->isAlive() && target->getMark("ViewAsSkill_zhanyiEffect") > 0;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (player->getPhase() == Player::NotActive)
            room->setPlayerMark(player, "ViewAsSkill_zhanyiEffect", 0);
        return false;
    }
};

class Yuhua : public HideCardSkill
{
public:
    Yuhua() : HideCardSkill("yuhua")
    {
    }
    virtual bool isCardHided(const Player *player, const Card *card) const
    {
        return (player->hasSkill(objectName()) && card->getTypeId() != Card::TypeBasic);
    }
};

class Qirang : public TriggerSkill
{
public:
    Qirang() : TriggerSkill("qirang")
    {
        events << CardsMoveOneTime;
    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *zhugeguo, QVariant &data, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(zhugeguo)) return QStringList();
        QVariantList move_datas = data.toList();
        foreach(QVariant move_data, move_datas) {
            CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
            if (move.to == zhugeguo && move.to_place == Player::PlaceEquip) {
                return QStringList(objectName());
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *zhugeguo, QVariant &, ServerPlayer *) const
    {
        if (zhugeguo->askForSkillInvoke(this)) {
            zhugeguo->broadcastSkillInvoke(objectName());
            QList<int> tricks;
            foreach (int card_id, room->getDrawPile())
                if (Sanguosha->getCard(card_id)->getTypeId() == Card::TypeTrick)
                    tricks << card_id;
            if (tricks.isEmpty()){
                LogMessage log;
                log.type = "$SearchFailed";
                log.from = zhugeguo;
                log.arg = "trick";
                room->sendLog(log);
                return false;
            }
            int index = qrand() % tricks.length();
            int id = tricks.at(index);
            zhugeguo->obtainCard(Sanguosha->getCard(id), false);
        }
        return false;
    }
};

class ShanjiaSlash : public ZeroCardViewAsSkill
{
public:
    ShanjiaSlash() : ZeroCardViewAsSkill("shanjia_slash")
    {
        response_pattern = "@@shanjia_slash!";
    }

    const Card *viewAs() const
    {
        Slash *slash = new Slash(Card::NoSuit, 0);
        slash->setSkillName("_shanjia");
        slash->setFlags("Global_NoDistanceChecking");
        return slash;
    }
};

class ShanjiaDiscard : public ViewAsSkill
{
public:
    ShanjiaDiscard() : ViewAsSkill("shanjia_discard")
    {
        response_pattern = "@@shanjia_discard!";
    }

    bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
        return selected.length() < Self->getMark("shanjia_disnum") && !Self->isJilei(to_select);
    }

    const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.length() == Self->getMark("shanjia_disnum")) {
            DummyCard *xt = new DummyCard;
            xt->addSubcards(cards);
            return xt;
        }
        return NULL;
    }
};

class Shanjia : public PhaseChangeSkill
{
public:
    Shanjia() : PhaseChangeSkill("shanjia")
    {
    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *player, QVariant &, ServerPlayer* &) const
    {
        if (!PhaseChangeSkill::triggerable(player)) return QStringList();
        if (player->getPhase() != Player::Play) return QStringList();
        if (player->getCardUsedTimes("EquipCard|all") > 0)
            return QStringList(objectName());

        return QStringList();
    }

    virtual bool onPhaseChange(ServerPlayer *caochun) const
    {
        Room *room = caochun->getRoom();
        int n = qMin(caochun->getCardUsedTimes("EquipCard|all"), 7);
		if (n > 0 && caochun->askForSkillInvoke(this)){
            caochun->broadcastSkillInvoke(objectName());
			caochun->drawCards(n);
            QList<int> all_cards = caochun->forceToDiscard(10086, true);
            QList<int> to_discard = caochun->forceToDiscard(n, true);
			if (all_cards.length() > n){
                room->setPlayerMark(caochun, "shanjia_disnum", n);
				const Card *card = room->askForCard(caochun, "@@shanjia_discard!", "@shanjia-discard:::" + QString::number(n), QVariant(), Card::MethodNone);
                room->setPlayerMark(caochun, "shanjia_disnum", 0);
				if (card != NULL && card->subcardsLength() == n) {
					to_discard = card->getSubcards();
				}
			}
			bool selected_equipped = false;
			foreach (int card_id, to_discard) {
			    if (room->getCardPlace(card_id) == Player::PlaceEquip) {
					selected_equipped = true;
				}
			}
			DummyCard *dummy_card = new DummyCard(to_discard);
            CardMoveReason mreason(CardMoveReason::S_REASON_THROW, caochun->objectName(), QString(), objectName(), QString());
            room->throwCard(dummy_card, mreason, caochun);
			delete dummy_card;
			if (selected_equipped && Slash::IsAvailable(caochun)){
				ServerPlayer *slash_target = NULL;
                foreach (ServerPlayer *p, room->getAlivePlayers()) {
                    if (caochun->canSlash(p, NULL, false)) {
						slash_target = p;
						break;
					}
                }
                if (slash_target == NULL) return false;
                const Card *use = room->askForUseCard(caochun, "@@shanjia_slash!", "@shanjia-slash", QVariant(), Card::MethodUse, false);
				if (!use){
					Slash *slash = new Slash(Card::NoSuit, 0);
					slash->setSkillName("_shanjia");
                    room->useCard(CardUseStruct(slash, caochun, slash_target), false);
				}
			}
		}
		return false;
    }
};

class Kuangcai : public TriggerSkill
{
public:
    Kuangcai() : TriggerSkill("kuangcai")
    {
        events << EventPhaseStart << EventPhaseEnd << CardUsed << CardResponded;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->isAlive();
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseStart) {
            if (TriggerSkill::triggerable(player) && player->getPhase() == Player::Play) {
                if (player->askForSkillInvoke(this)) {
                    player->broadcastSkillInvoke(objectName());
                    room->setPlayerFlag(player, "kuangcai");
                    room->setPlayerMark(player, "#kuangcai", 5);
                }
            }
        } else if (triggerEvent == EventPhaseEnd) {
            if (player->hasFlag("kuangcai")) {
                room->setPlayerFlag(player, "-kuangcai");
                room->setPlayerMark(player, "#kuangcai", 0);
            }
        } else {
            if (player->hasFlag("kuangcai")) {
                const Card *card = NULL;
                if (triggerEvent == CardUsed)
                    card = data.value<CardUseStruct>().card;
                else {
                    CardResponseStruct resp = data.value<CardResponseStruct>();
                    if (resp.m_isUse)
                        card = resp.m_card;
                }
                if (card != NULL && card->getTypeId() != Card::TypeSkill) {
                    player->drawCards(1, objectName());
                    room->removePlayerMark(player, "#kuangcai");
                }
            }
        }
        return false;
    }
};

class KuangcaiTargetMod : public TargetModSkill
{
public:
    KuangcaiTargetMod() : TargetModSkill("#kuangcai-target")
    {
        frequency = NotFrequent;
        pattern = "^SkillCard";
    }

    virtual int getResidueNum(const Player *from, const Card *, const Player *) const
    {
        if (from->hasFlag("kuangcai"))
            return 1000;
        else
            return 0;
    }

    virtual int getDistanceLimit(const Player *from, const Card *, const Player *) const
    {
        if (from->hasFlag("kuangcai"))
            return 1000;
        else
            return 0;
    }
};

class Shejian : public TriggerSkill
{
public:
    Shejian() : TriggerSkill("shejian")
    {
        events << EventPhaseEnd << EventPhaseChanging;
        view_as_skill = new dummyVS;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseEnd && player->getPhase() == Player::Discard && TriggerSkill::triggerable(player)) {
            QVariantList shejianRecord = player->tag["ShejianRecord"].toList();
            if (shejianRecord.isEmpty()) return false;
            QStringList suitlist;
            foreach (QVariant card_data, shejianRecord) {
                int card_id = card_data.toInt();
                const Card *card = Sanguosha->getCard(card_id);
                QString suit = card->getSuitString();
                if (!suitlist.contains(suit))
                    suitlist << suit;
                else{
                    return false;
                }
            }
            QList<ServerPlayer *> targets;
            foreach (ServerPlayer *target, room->getOtherPlayers(player)) {
                if (player->canDiscard(target, "he"))
                    targets << target;
            }
            if (targets.isEmpty())
                return false;
            ServerPlayer *target = room->askForPlayerChosen(player, targets, objectName(), "@shejian-invoke", true, true);
            if (target != NULL) {
                player->broadcastSkillInvoke(objectName());
                int to_throw = room->askForCardChosen(player, target, "he", objectName(), false, Card::MethodDiscard);
                room->throwCard(to_throw, target, player);
            }
        } else if (triggerEvent == EventPhaseChanging) {
            if (data.value<PhaseChangeStruct>().to == Player::Discard) {
                player->tag.remove("ShejianRecord");
            }
        }
        return false;
    }
};

class Zhaohuo : public TriggerSkill
{
public:
    Zhaohuo() : TriggerSkill("zhaohuo")
    {
        events << Dying;
        frequency = Compulsory;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *taoqian, QVariant &data) const
    {
        DyingStruct dying = data.value<DyingStruct>();
        if (dying.who != taoqian && taoqian->getMaxHp() > 1) {
            room->sendCompulsoryTriggerLog(taoqian, objectName());
            taoqian->broadcastSkillInvoke(objectName());
            int x = taoqian->getMaxHp()-1;
            room->loseMaxHp(taoqian, x);
            taoqian->drawCards(x, objectName());
        }
        return false;
    }
};

class Yixiang : public TriggerSkill
{
public:
    Yixiang() : TriggerSkill("yixiang")
    {
        events << TargetConfirmed << EventPhaseChanging;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == TargetConfirmed && TriggerSkill::triggerable(player)) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card->getTypeId() == Card::TypeSkill) return false;
            if (!use.to.contains(player)) return false;
            if (use.from && use.from->getHp() > player->getHp() && !player->hasFlag("YibingUsed")) {
                room->setPlayerFlag(player, "YibingUsed");
                if (player->askForSkillInvoke(this)) {
                    player->broadcastSkillInvoke(objectName());
                    QList<int> basics;
                    foreach (int card_id, room->getDrawPile()) {
                        const Card *card = Sanguosha->getCard(card_id);
                        if (card->getTypeId() == Card::TypeBasic) {
                            bool will_append = true;
                            foreach (const Card *c, player->getHandcards()) {
                                if ((c->isKindOf("Slash") && card->isKindOf("Slash")) || c->objectName() == card->objectName()) {
                                    will_append = false;
                                    break;
                                }
                            }
                            if (will_append)
                                basics << card_id;
                        }
                    }

                    if (basics.isEmpty()){
                        LogMessage log;
                        log.type = "$YixiangSearchFailed";
                        log.from = player;
                        log.arg = "basic";
                        room->sendLog(log);
                    } else {
                        int index = qrand() % basics.length();
                        int id = basics.at(index);
                        player->obtainCard(Sanguosha->getCard(id), false);
                    }
                }
            }
        } else if (triggerEvent == EventPhaseChanging) {
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (change.to == Player::NotActive) {
                foreach (ServerPlayer *p, room->getAlivePlayers()) {
                    if (p->hasFlag("YibingUsed"))
                        room->setPlayerFlag(p, "-YibingUsed");
                }
            }
        }
        return false;
    }
};

class Yirang : public PhaseChangeSkill
{
public:
    Yirang() : PhaseChangeSkill("yirang")
    {
        view_as_skill = new dummyVS;
    }

    virtual bool onPhaseChange(ServerPlayer *taoqian) const
    {
        Room *room = taoqian->getRoom();
        if (taoqian->getPhase() != Player::Play) return false;
        DummyCard *dummy = new DummyCard;
        QSet<Card::CardType> types;
        foreach (const Card *card, taoqian->getCards("he")) {
            if (card->getTypeId() != Card::TypeBasic) {
                dummy->addSubcard(card);
                types << card->getTypeId();
            }
        }
        if (dummy->subcardsLength() > 0) {
            QList<ServerPlayer *> targets;
            foreach (ServerPlayer *player, room->getOtherPlayers(taoqian)) {
                if (player->getMaxHp() > taoqian->getMaxHp()) {
                    targets << player;
                }
            }
            if (!targets.isEmpty()) {
                ServerPlayer *target = room->askForPlayerChosen(taoqian, targets, objectName(), "@yirang-target", true, true);
                if (target != NULL) {
                    taoqian->broadcastSkillInvoke(objectName());
                    CardMoveReason reason(CardMoveReason::S_REASON_GIVE, taoqian->objectName(), target->objectName(), objectName(), QString());
                    room->obtainCard(target, dummy, reason, false);

                    LogMessage log;
                    log.type = "#GainMaxHp";
                    log.from = taoqian;
                    log.arg = QString::number(target->getMaxHp() - taoqian->getMaxHp());
                    log.arg2 = QString::number(target->getMaxHp());
                    room->sendLog(log);

                    room->setPlayerProperty(taoqian, "maxhp", target->getMaxHp());

                    if (taoqian->isWounded() && types.size() > 0)
                        room->recover(taoqian, RecoverStruct(taoqian, NULL, types.size()));

                }
            }
        }
        delete dummy;
        return false;
    }
};

class Polu : public TriggerSkill
{
public:
    Polu() : TriggerSkill("polu")
    {
        events << EventPhaseStart << Damaged;
        frequency = Compulsory;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseStart && player->getPhase() == Player::RoundStart && !player->hasWeapon("catapult")) {
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            int card_id = -1;
            Package *package = PackageAdder::packages()["DerivativeCard"];
            if (package) {
                QList<Card *> all_cards = package->findChildren<Card *>();
                foreach (Card *card, all_cards) {
                    if (card->objectName() == "catapult") {
                        card_id = card->getEffectiveId();
                        break;
                    }
                }
            }
            if (card_id < 0) return false;
            const Card *catapult = Sanguosha->getCard(card_id);
            LogMessage log;
            log.type = "#AddCard";
            log.card_str = catapult->toString();
            room->sendLog(log);
            room->setCardMapping(card_id, NULL, Player::DrawPile);
            room->useCard(CardUseStruct(catapult, player, QList<ServerPlayer *>()));
        } else if (triggerEvent == Damaged) {
            DamageStruct damage = data.value<DamageStruct>();
            for (int i = 0; i < damage.damage; i++) {
                if (player->hasWeapon("catapult")) break;
                room->sendCompulsoryTriggerLog(player, objectName());
                player->broadcastSkillInvoke(objectName());
                player->drawCards(1, objectName());
            }
        }
        return false;
    }
};

class CatapultSkill : public WeaponSkill
{
public:
    CatapultSkill() : WeaponSkill("catapult")
    {
        events << Damage << PreCardsMoveOneTime;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (triggerEvent == Damage && WeaponSkill::triggerable(player)) {
            DamageStruct damage = data.value<DamageStruct>();
            ServerPlayer *to = damage.to;
            if (!to || to->isDead() || to->getMark("Equips_of_Others_Nullified_to_You") > 0) return QStringList();

            if (to->getArmor() && player->canDiscard(to, to->getArmor()->getEffectiveId()))
                return QStringList(objectName());
            if (to->getDefensiveHorse() && player->canDiscard(to, to->getDefensiveHorse()->getEffectiveId()))
                return QStringList(objectName());

        } else if (triggerEvent == PreCardsMoveOneTime) {
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            if (move.from != player || !move.from_places.contains(Player::PlaceEquip)) return QStringList();
            for (int i = 0; i < move.card_ids.size(); i++) {
                if (move.from_places[i] != Player::PlaceEquip) continue;
                const Card *catapult = Sanguosha->getEngineCard(move.card_ids[i]);
                if (catapult->objectName() == objectName()) {
                    return QStringList("catapult!");
                }
            }
        }
        return QStringList();
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == Damage && WeaponSkill::triggerable(player)) {
            DamageStruct damage = data.value<DamageStruct>();
            ServerPlayer *to = damage.to;
            QVariant data = QVariant::fromValue(to);
            if (!to || to->isDead() || to->getMark("Equips_of_Others_Nullified_to_You") > 0) return false;
            DummyCard *dummy = new DummyCard;
            if (to->getArmor() && player->canDiscard(to, to->getArmor()->getEffectiveId()))
                dummy->addSubcard(to->getArmor());
            if (to->getDefensiveHorse() && player->canDiscard(to, to->getDefensiveHorse()->getEffectiveId()))
                dummy->addSubcard(to->getDefensiveHorse());
            if (dummy->subcardsLength() > 0 && room->askForSkillInvoke(player, objectName(), data)) {
                room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), to->objectName());
                room->setEmotion(player, "weapon/catapult");
                room->throwCard(dummy, to, player);
            }
            delete dummy;
        } else if (triggerEvent == PreCardsMoveOneTime) {
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            if (move.from != player || !move.from_places.contains(Player::PlaceEquip)) return false;
            for (int i = 0; i < move.card_ids.size(); i++) {
                if (move.from_places[i] != Player::PlaceEquip) continue;
                const Card *catapult = Sanguosha->getEngineCard(move.card_ids[i]);
                if (catapult->objectName() == objectName()) {
                    room->sendCompulsoryTriggerLog(player, objectName(), false);
                    room->setEmotion(player, "weapon/catapultdestory");
                    LogMessage log;
                    log.type = "#RemoveCard";
                    log.card_str = catapult->toString();
                    room->sendLog(log);
                    CardMoveReason reason(CardMoveReason::S_REASON_NATURAL_ENTER, player->objectName(), QString(), objectName(), QString());
                    room->moveCardTo(catapult, NULL, Player::PlaceTable, reason, true);
                    room->setCardMapping(catapult->getEffectiveId(), NULL, Player::PlaceSpecial);
                    return false;
                }
            }
        }
        return false;
    }
};

Catapult::Catapult(Suit suit, int number)
    : Weapon(suit, number, 9)
{
    setObjectName("catapult");
}

class ChoulveUse : public ZeroCardViewAsSkill
{
public:
    ChoulveUse() : ZeroCardViewAsSkill("choulve_use")
    {
        response_pattern = "@@choulve_use!";
    }

    const Card *viewAs() const
    {
        QString choulve_card = Self->property("choulve").toString();
        if (choulve_card.isEmpty()) return NULL;
        Card *use_card = Sanguosha->cloneCard(choulve_card);
        use_card->setCanRecast(false);
        use_card->setSkillName("_choulve");
        return use_card;
    }
};

class Choulve : public PhaseChangeSkill
{
public:
    Choulve() : PhaseChangeSkill("choulve")
    {
        view_as_skill = new dummyVS;
    }

    virtual bool onPhaseChange(ServerPlayer *player) const
    {
        Room *room = player->getRoom();
        if (player->getPhase() != Player::Play) return false;
        ServerPlayer *target = room->askForPlayerChosen(player, room->getOtherPlayers(player), objectName(), "@choulve-invoke", true, true);
        if (target) {
            player->broadcastSkillInvoke(objectName());
            const Card *card = room->askForExchange(target, objectName(), 1, 1, true, "@choulve-give:" + player->objectName(), true);
            if (card) {
                CardMoveReason reason(CardMoveReason::S_REASON_GIVE, target->objectName(), player->objectName(), objectName(), QString());
                reason.m_playerId = player->objectName();
                room->moveCardTo(card, target, player, Player::PlaceHand, reason);

                QString choulve_card = player->property("choulve").toString();
                if (choulve_card.isEmpty()) return false;
                Card *use_card = Sanguosha->cloneCard(choulve_card);
                use_card->setCanRecast(false);
                use_card->setSkillName("_choulve");
                if (!use_card->isAvailable(player)) return false;
                if (use_card->targetFixed())
                    room->useCard(CardUseStruct(use_card, player, QList<ServerPlayer *>()));
                else {
                    ServerPlayer *default_target = NULL;
                    foreach (ServerPlayer *p, room->getAlivePlayers()) {
                        if (!room->isProhibited(player, p, use_card) && use_card->targetFilter(QList<const Player *>(), p, player)) {
                            default_target = p;
                            break;
                        }
                    }
                    if (default_target == NULL) return false;
                    if (!room->askForUseCard(player, "@@choulve_use!", "@choulve-use:::" + choulve_card, QVariant(), Card::MethodUse, false))
                        room->useCard(CardUseStruct(use_card, player, default_target));
                }
            }
        }
        return false;
    }
};

class ChoulveRecord : public TriggerSkill
{
public:
    ChoulveRecord() : TriggerSkill("#choulve-record")
    {
        events << PreDamageDone;
        //global = true;
    }

    virtual bool trigger(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (damage.card && (damage.card->getTypeId() == Card::TypeBasic || damage.card->isNDTrick()))
            room->setPlayerProperty(player, "choulve", damage.card->objectName());
        return false;
    }
};

PingcaiMoveCard::PingcaiMoveCard()
{
    mute = true;
}

bool PingcaiMoveCard::targetsFeasible(const QList<const Player *> &targets, const Player *) const
{
    return targets.length() == 2;
}

bool PingcaiMoveCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{

    if (targets.isEmpty()) {
        if (Self->hasFlag("PingcaiAllEquip"))
            return !to_select->getEquips().isEmpty();
        else
            return to_select->getArmor() != NULL;
    } else if (targets.length() == 1){
        if (Self->hasFlag("PingcaiAllEquip")) {
            for (int i = 0; i < S_EQUIP_AREA_LENGTH; i++) {
                if (targets.first()->getEquip(i) && !to_select->getEquip(i))
                    return true;
            }
        } else
            return to_select->getArmor() == NULL;
    }
    return false;
}

void PingcaiMoveCard::onUse(Room *room, const CardUseStruct &card_use) const
{
    CardUseStruct use = card_use;
    ServerPlayer *caoren = use.from;
    if (use.to.length() != 2) return;

    ServerPlayer *from = use.to.first();
    ServerPlayer *to = use.to.last();

    int card_id = -1;
    if (caoren->hasFlag("PingcaiAllEquip")) {
        QList<int> all, ids, disabled_ids;
        for (int i = 0; i < S_EQUIP_AREA_LENGTH; i++) {
            if (from->getEquip(i)){
                if (!to->getEquip(i))
                    ids << from->getEquip(i)->getEffectiveId();
                else
                    disabled_ids << from->getEquip(i)->getEffectiveId();
                all << from->getEquip(i)->getEffectiveId();
            }
        }

        room->fillAG(all, caoren, disabled_ids);
        from->setFlags("PingcaiTarget");
        card_id = room->askForAG(caoren, ids, true, "pingcai");
        from->setFlags("-PingcaiTarget");
        room->clearAG(caoren);
    } else
        card_id = from->getArmor()->getEffectiveId();

    if (card_id != -1)
        room->moveCardTo(Sanguosha->getCard(card_id), from, to, room->getCardPlace(card_id), CardMoveReason(CardMoveReason::S_REASON_TRANSFER, caoren->objectName(), "pingcai", QString()));
}

class PingcaiMove : public ZeroCardViewAsSkill
{
public:
    PingcaiMove() : ZeroCardViewAsSkill("pingcai_move")
    {
        response_pattern = "@@pingcai_move";
    }

    virtual const Card *viewAs() const
    {
        return new PingcaiMoveCard;
    }
};

PingcaiCard::PingcaiCard()
{
    target_fixed = true;
}

void PingcaiCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const
{
    QString choice = room->askForChoice(source, "pingcai", "wolong+fengchu+shuijing+xuanjian", QVariant(), "@pingcai-choice");
    if (choice == "wolong") {
        int x = (room->findPlayerByGeneralName("wolong") == NULL)?1:2;

        QList<ServerPlayer *> choosees = room->askForPlayersChosen(source, room->getAlivePlayers(), "pingcai", 0, x,
                "@pingcai-wolong:::"+QString::number(x));
        if (choosees.length() > 0) {
            source->broadcastSkillInvoke("pingcai", 2);
            foreach (ServerPlayer *target, choosees) {
                if (target->isAlive())
                    room->damage(DamageStruct("pingcai", source, target, 1, DamageStruct::Fire));
            }
        }
    } else if (choice == "fengchu") {
        int x = (room->findPlayerByGeneralName("pangtong") == NULL)?3:4;


        QList<ServerPlayer *> to_choosees;
        foreach (ServerPlayer *p, room->getAlivePlayers()) {
            if (!p->isChained())
                to_choosees << p;
        }
        if (to_choosees.isEmpty()) return;

        QList<ServerPlayer *> choosees = room->askForPlayersChosen(source, to_choosees, "pingcai", 0, x,
                "@pingcai-fengchu:::"+QString::number(x));
        if (choosees.length() > 0) {
            source->broadcastSkillInvoke("pingcai", 3);
            foreach (ServerPlayer *target, choosees) {
                if (target->isAlive() && !target->isChained())
                    room->setPlayerProperty(target, "chained", true);
            }
        }
    } else if (choice == "shuijing") {
        if (room->findPlayerByGeneralName("simahui") != NULL)
            room->setPlayerFlag(source, "PingcaiAllEquip");
        room->askForUseCard(source, "@@pingcai_move", "@pingcai-shuijing");





    } else if (choice == "xuanjian") {
        bool xushu = (room->findPlayerByGeneralName("xushu") != NULL || room->findPlayerByGeneralName("yj_xushu") != NULL);
        ServerPlayer *target = room->askForPlayerChosen(source, room->getAlivePlayers(), "pingcai", "@pingcai-xuanjian", true);
        if (target) {
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, source->objectName(), target->objectName());
            source->broadcastSkillInvoke("pingcai", 5);
            target->drawCards(1, "pingcai");
            room->recover(target, RecoverStruct(source));
            if(xushu && source->isAlive())
                source->drawCards(1, "pingcai");
        }

    }
}

class Pingcai : public ZeroCardViewAsSkill
{
public:
    Pingcai() : ZeroCardViewAsSkill("pingcai")
    {

    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("PingcaiCard");
    }

    const Card *viewAs() const
    {
        return new PingcaiCard;
    }

    virtual int getEffectIndex(const ServerPlayer *, const Card *card) const
    {
        if (card->isKindOf("PingcaiCard"))
            return 1;
        else
            return -1;
    }
};

class Yinshii : public ProhibitSkill
{
public:
    Yinshii() : ProhibitSkill("yinshii")
    {
    }

    virtual bool isProhibited(const Player *, const Player *to, const Card *card, const QList<const Player *> &) const
    {
        return to->hasSkill(this) && card->isKindOf("DelayedTrick");
    }
};

class YinshiiSkip : public PhaseChangeSkill
{
public:
    YinshiiSkip() : PhaseChangeSkill("#yinshii-skip")
    {

    }

    virtual bool onPhaseChange(ServerPlayer *player) const
    {
        if (player->getPhase() != Player::RoundStart) return false;
        player->getRoom()->sendCompulsoryTriggerLog(player, "yinshii");
        player->broadcastSkillInvoke("yinshii");
        player->skip(Player::Start);
        player->skip(Player::Judge);
        player->skip(Player::Finish);
        return false;
    }
};


MOLPackage::MOLPackage()
: Package("MOL")
{
    General *sunru = new General(this, "sunru", "wu", 3, false);
    sunru->addSkill(new Yingjian);
    sunru->addSkill(new Shixin);

    General *lingcao = new General(this, "lingcao", "wu", 4);
    lingcao->addSkill(new Dujin);

    General *liuzan = new General(this, "liuzan", "wu");
    liuzan->addSkill(new Fenyin);

    General *lifeng = new General(this, "lifeng", "shu", 3);
    lifeng->addSkill(new TunchuDraw);
    lifeng->addSkill(new TunchuEffect);
    lifeng->addSkill(new Tunchu);
	lifeng->addSkill(new DetachEffectSkill("tunchu", "food"));
    related_skills.insertMulti("tunchu", "#tunchu-effect");
    related_skills.insertMulti("tunchu", "#tunchu-disable");
	related_skills.insertMulti("tunchu", "#tunchu-clear");
    lifeng->addSkill(new Shuliang);

    General *zhuling = new General(this, "zhuling", "wei");
    zhuling->addSkill(new Zhanyi);
    zhuling->addSkill(new ZhanyiDiscard2);
    zhuling->addSkill(new ZhanyiNoDistanceLimit);
    zhuling->addSkill(new ZhanyiRemove);
    related_skills.insertMulti("zhanyi", "#zhanyi-basic");
    related_skills.insertMulti("zhanyi", "#zhanyi-equip");
    related_skills.insertMulti("zhanyi", "#zhanyi-trick");

	General *zhugeguo = new General(this, "zhugeguo", "shu", 3, false);
	zhugeguo->addSkill(new Yuhua);
	zhugeguo->addSkill(new Qirang);

    General *caochun = new General(this, "caochun", "wei");
    caochun->addSkill(new Shanjia);

    General *miheng = new General(this, "miheng", "qun", 3);
    miheng->addSkill(new Kuangcai);
    miheng->addSkill(new KuangcaiTargetMod);
    miheng->addSkill(new Shejian);
    related_skills.insertMulti("kuangcai", "#kuangcai-target");

    General *taoqian = new General(this, "taoqian", "qun", 3);
    taoqian->addSkill(new Zhaohuo);
    taoqian->addSkill(new Yixiang);
    taoqian->addSkill(new Yirang);

    General *liuye = new General(this, "liuye", "wei", 3);
    liuye->addSkill(new Polu);
    liuye->addSkill(new Choulve);

    General *pangdegong = new General(this, "pangdegong", "qun", 3);
    pangdegong->addSkill(new Pingcai);
    pangdegong->addSkill(new Yinshii);
    pangdegong->addSkill(new YinshiiSkip);
    related_skills.insertMulti("yinshii", "#yinshii-skip");

    addMetaObject<ZhanyiCard>();
    addMetaObject<ZhanyiViewAsBasicCard>();
    addMetaObject<PingcaiCard>();
    addMetaObject<PingcaiMoveCard>();
	
    skills << new ShanjiaDiscard << new ShanjiaSlash << new ChoulveUse << new ChoulveRecord << new PingcaiMove;
}

ADD_PACKAGE(MOL)

DerivativeCardPackage::DerivativeCardPackage()
    : Package("DerivativeCard", Package::CardPack)
{
    QList<Card *> cards;

    cards << new Catapult;

    foreach(Card *card, cards)
        card->setParent(this);

    skills << new CatapultSkill;
}

ADD_PACKAGE(DerivativeCard)
