#include "guandu.h"
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





class Xiying : public PhaseChangeSkill
{
public:
    Xiying() : PhaseChangeSkill("xiying")
    {
        view_as_skill = new dummyVS;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    virtual bool onPhaseChange(ServerPlayer *gaolan) const
    {
        Room *room = gaolan->getRoom();
        if (gaolan->getPhase() == Player::Play && TriggerSkill::triggerable(gaolan) && !gaolan->isNude()) {
            if (room->askForCard(gaolan, "..", "@xiying", QVariant(), objectName())) {
                QList<ServerPlayer *> targets = room->getOtherPlayers(gaolan);
                foreach (ServerPlayer *p, targets)
                    room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, gaolan->objectName(), p->objectName());
                foreach (ServerPlayer *p, targets) {
                    if (p->isAlive()) {
                        if (p->isNude() || !room->askForDiscard(p, objectName(), 1, 1, true, true, "@xiying-discard") && p->getMark("#xiying") == 0) {
                            room->addPlayerTip(p, "#xiying");
                            room->setPlayerCardLimitation(p, "use,response", ".|.|.|.", true);
                            foreach(ServerPlayer *p, room->getAllPlayers())
                                room->filterCards(p, p->getCards("he"), true);
                        }
                    }
                }
            }
        } else if (gaolan->getPhase() == Player::NotActive) {
            QList<ServerPlayer *> players = room->getAllPlayers();
            foreach (ServerPlayer *p, players) {
                if (p->getMark("#xiying") > 0) {
                    room->removePlayerTip(p, "#xiying");
                    room->removePlayerCardLimitation(p, "use,response", ".|.|.|.");
                }
            }
            foreach(ServerPlayer *p, players)
                room->filterCards(p, p->getCards("he"), false);
        }
        return false;
    }
};





YuanlveCard::YuanlveCard()
{
    will_throw = false;
    handling_method = Card::MethodNone;
}

void YuanlveCard::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *source = effect.from;
    Room *room = source->getRoom();
    ServerPlayer *target = effect.to;

    CardMoveReason reason(CardMoveReason::S_REASON_GIVE, source->objectName(), target->objectName(), "yuanlve", QString());
    room->obtainCard(target, this, reason, false);
    int give_id = getEffectiveId();
    if (room->getCardOwner(give_id) != target || room->getCardPlace(give_id) != Player::PlaceHand) return;
    if (Sanguosha->getCard(give_id)->isAvailable(target) &&
        room->askForUseCard(target, QString::number(give_id), "@yuanlve-use:::"+Sanguosha->getCard(give_id)->objectName()) && source->isAlive())
        source->drawCards(1, "yuanlve");
}

class Yuanlve : public OneCardViewAsSkill
{
public:
    Yuanlve() : OneCardViewAsSkill("yuanlve")
    {
        filter_pattern = "^EquipCard";
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("YuanlveCard");
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        YuanlveCard *yuanlveCard = new YuanlveCard;
        yuanlveCard->addSubcard(originalCard);
        return yuanlveCard;
    }
};

class Gangzhi : public TriggerSkill
{
public:
    Gangzhi() : TriggerSkill("gangzhi")
    {
        frequency = Compulsory;
        events << Predamage;
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());
        DamageStruct damage = data.value<DamageStruct>();
        room->preventDamage(damage);
        room->loseHp(player, damage.damage);
        return true;
    }
};

class Beizhan : public TriggerSkill
{
public:
    Beizhan() : TriggerSkill("beizhan")
    {
        events << EventPhaseStart << EventPhaseChanging;
        view_as_skill = new dummyVS;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseStart) {
            if (player->getPhase() == Player::RoundStart && player->getMark("beizhan_target") > 0) {
                room->setPlayerMark(player, "beizhan_target", 0);
                QList<ServerPlayer *> players = room->getOtherPlayers(player);
                foreach (ServerPlayer *p, players) {
                    if (player->getHandcardNum() < p->getHandcardNum())
                        return false;
                }
                room->addPlayerTip(player, "#beizhan");
                room->setPlayerFlag(player, "DisabledOtherTargets");
            } else if (player->getPhase() == Player::NotActive)
                room->removePlayerTip(player, "#beizhan");
        } else if (triggerEvent == EventPhaseChanging && TriggerSkill::triggerable(player)) {
            if (data.value<PhaseChangeStruct>().to == Player::NotActive) {
                ServerPlayer *to = room->askForPlayerChosen(player, room->getAlivePlayers(), objectName(), "@beizhan-invoke", true, true);
                if (!to) return false;
                player->broadcastSkillInvoke(objectName());
                int x = to->getMaxHp() - to->getHandcardNum();
                if (x > 0)
                    to->drawCards(x, objectName());
                room->setPlayerMark(to, "beizhan_target", 1);
            }
        }
        return false;
    }
};







class Fenglve : public TriggerSkill
{
public:
    Fenglve() : TriggerSkill("fenglve")
    {
        events << EventPhaseStart << Pindian;
        view_as_skill = new dummyVS;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseStart && TriggerSkill::triggerable(player)) {
            if (player->getPhase() == Player::Play && !player->isKongcheng()) {
                QList<ServerPlayer *> targets;
                QList<ServerPlayer *> other_players = room->getAlivePlayers();
                foreach (ServerPlayer *p, other_players) {
                    if (Self->canPindian(p)) {
                        targets << p;
                    }
                }
                if (targets.isEmpty()) return false;
                ServerPlayer *target = room->askForPlayerChosen(player, targets, objectName(), "@fenglve-invoke", true, true);
                if (target) {
                    player->broadcastSkillInvoke(objectName());
                    if (player->pindian(target, objectName())) {
                        if (!target->isAllNude() && player->isAlive()) {
                            int n = 0;
                            if (!target->isKongcheng()) n++;
                            if (!target->getEquips().isEmpty()) n++;
                            if (!target->getJudgingArea().isEmpty()) n++;
                            QList<int> ids = room->askForCardsChosen(target, target, "hej", "fenglve1", n, n);
                            DummyCard *dummy = new DummyCard(ids);
                            CardMoveReason reason(CardMoveReason::S_REASON_GIVE, target->objectName(), player->objectName(), objectName(), QString());
                            room->obtainCard(player, dummy, reason, false);
                        }

                    } else {
                        if (!player->isNude() && target->isAlive()) {
                            int card_id = room->askForCardChosen(player, player, "he", "fenglve2");
                            CardMoveReason reason(CardMoveReason::S_REASON_GIVE, player->objectName(), target->objectName(), objectName(), QString());
                            room->obtainCard(target, Sanguosha->getCard(card_id), reason, false);
                        }
                    }
                }
            }
        } if (triggerEvent == Pindian) {
            PindianStruct *pindian = data.value<PindianStruct *>();
            if (pindian->reason != objectName() || pindian->from != player) return false;
            if (player->isAlive() && pindian->to->isAlive()) {
                const Card *to_obtain = pindian->from_card;
                if (to_obtain && room->getCardPlace(to_obtain->getEffectiveId()) == Player::PlaceTable
                    && room->askForSkillInvoke(player, "_fenglve", "prompt::"+pindian->to->objectName()+":"+to_obtain->objectName())) {
                    player->broadcastSkillInvoke(objectName());
                    pindian->to->obtainCard(to_obtain);
                }
            }
        }
        return false;
    }
};

MoushiCard::MoushiCard()
{
    will_throw = false;
    handling_method = Card::MethodNone;
}

void MoushiCard::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *source = effect.from;
    Room *room = source->getRoom();
    ServerPlayer *target = effect.to;

    CardMoveReason reason(CardMoveReason::S_REASON_GIVE, source->objectName(), target->objectName(), "moushi", QString());
    room->obtainCard(target, this, reason);

    room->addPlayerTip(target, "#moushi");
    QVariantList moushi_list = target->tag["MoushiSource"].toList();
    moushi_list << source->objectName();
    target->tag["MoushiSource"] = QVariant::fromValue(moushi_list);
}

class MoushiViewAsSkill : public OneCardViewAsSkill
{
public:
    MoushiViewAsSkill() : OneCardViewAsSkill("moushi")
    {
        filter_pattern = ".|.|.|hand";
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("MoushiCard");
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        MoushiCard *moushiCard = new MoushiCard;
        moushiCard->addSubcard(originalCard);
        return moushiCard;
    }
};

class Moushi : public TriggerSkill
{
public:
    Moushi() : TriggerSkill("moushi")
    {
        events << Damage << PreDamageDone << DamageComplete << EventPhaseChanging;
        view_as_skill = new MoushiViewAsSkill;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == Damage) {
            QVariantList moushi_list = player->tag["MoushiDamage"].toList();
            if (moushi_list.isEmpty()) return false;
            int moushi_index = moushi_list.last().toInt();
            if (moushi_index == 1) {
                QVariantList moushi_list = player->tag["MoushiSource"].toList();
                foreach (ServerPlayer *p, room->getAlivePlayers()) {
                    if (p->isAlive() && moushi_list.contains(p->objectName())) {
                        LogMessage log;
                        log.type = "#SkillForce";
                        log.from = p;
                        log.arg = objectName();
                        room->sendLog(log);
                        room->notifySkillInvoked(p, objectName());
                        p->broadcastSkillInvoke(objectName());
                        p->drawCards(1, objectName());
                    }
                }
            }
        } else if (triggerEvent == PreDamageDone) {
            DamageStruct damage = data.value<DamageStruct>();
            if (!damage.from || damage.from->getPhase() != Player::Play) return false;
            QVariantList moushi_list = damage.from->tag["MoushiDamage"].toList();
            QVariantList damaged_targets = damage.from->tag["MoushiTarget"].toList();
            if (damaged_targets.contains(player->objectName()))
                moushi_list << 0;
            else {
                damaged_targets << player->objectName();
                moushi_list << 1;
            }
            damage.from->tag["MoushiDamage"] = QVariant::fromValue(moushi_list);
            damage.from->tag["MoushiTarget"] = QVariant::fromValue(damaged_targets);
        } else if (triggerEvent == DamageComplete) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.prevented || !damage.from || damage.from->getPhase() != Player::Play) return false;
            QVariantList moushi_list = damage.from->tag["MoushiDamage"].toList();
            moushi_list.takeLast();
            damage.from->tag["MoushiDamage"] = QVariant::fromValue(moushi_list);
        } else if (triggerEvent == EventPhaseChanging) {
            if (data.value<PhaseChangeStruct>().from != Player::Play) return false;
            room->removePlayerTip(player, "#moushi");
            player->tag.remove("MoushiDamage");
            player->tag.remove("MoushiTarget");
        }
        return false;
    }
};

GuanduWarPackage::GuanduWarPackage()
    : Package("guandu_war")
{
    General *gaolan = new General(this, "gaolan", "qun");
    gaolan->addSkill(new Xiying);

    General *zhanghe = new General(this, "sp_zhanghe", "qun");
    zhanghe->addSkill(new Yuanlve);

    General *shenpei = new General(this, "shenpei", "qun", 3);
    shenpei->addSkill(new Gangzhi);
    shenpei->addSkill(new Beizhan);

    General *xunchen = new General(this, "xunchen", "qun", 3);
    xunchen->addSkill(new Fenglve);
    xunchen->addSkill(new Moushi);

    addMetaObject<YuanlveCard>();
    addMetaObject<MoushiCard>();

    skills;
}

ADD_PACKAGE(GuanduWar)
