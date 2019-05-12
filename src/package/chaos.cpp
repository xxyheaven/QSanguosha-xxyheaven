#include "chaos.h"
#include "client.h"
#include "general.h"
#include "skill.h"
#include "yjcm2013.h"
#include "standard-skillcards.h"
#include "engine.h"
#include "maneuvering.h"

#include "settings.h"
#include "json.h"





class Langxi : public PhaseChangeSkill
{
public:
    Langxi() : PhaseChangeSkill("langxi")
    {

    }

    virtual QStringList triggerable(TriggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer* &) const
    {
        if (!PhaseChangeSkill::triggerable(player)) return QStringList();
        if (player->getPhase() == Player::Start) {
            QList<ServerPlayer *> players = room->getOtherPlayers(player);
            foreach(ServerPlayer *p, players) {
                if (player->getHp() >= p->getHp())
                    return QStringList(objectName());
            }
        }

        return QStringList();
    }

    virtual bool onPhaseChange(ServerPlayer *player) const
    {
        Room *room = player->getRoom();
        QList<ServerPlayer *> targets, players = room->getOtherPlayers(player);
        foreach(ServerPlayer *p, players) {
            if (player->getHp() >= p->getHp())
                targets << p;
        }
        if (targets.isEmpty()) return false;
        ServerPlayer *target = room->askForPlayerChosen(player, targets, objectName(), "@langxi-target", true, true);
        if (target){
            player->broadcastSkillInvoke(objectName());
            QList<int> pionts;
            pionts << 0 << 1 << 2;
            qShuffle(pionts);

            int x = pionts.at(qrand() % pionts.length());
            if (x > 0)
                room->damage(DamageStruct(objectName(), player, target, x));
        }
        return false;
    }
};

class Yisuan : public TriggerSkill
{
public:
    Yisuan() : TriggerSkill("yisuan")
    {
        events << CardFinished << EventPhaseChanging;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseChanging && data.value<PhaseChangeStruct>().from == Player::Play)
            room->setPlayerMark(player, "YisuanUsed", 0);
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (triggerEvent != CardFinished || !TriggerSkill::triggerable(player) || player->getMark("YisuanUsed") > 0
                || player->getPhase() != Player::Play) return QStringList();
        CardUseStruct use = data.value<CardUseStruct>();
        const Card *card = use.card;
        if (card && card->isNDTrick() && card->getHandlingMethod() == Card::MethodUse && room->isAllOnPlace(card, Player::PlaceTable)) {
            return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        if (player->askForSkillInvoke(objectName())) {
            player->broadcastSkillInvoke(objectName());
            room->addPlayerMark(player, "YisuanUsed");
            room->loseMaxHp(player);
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card && room->isAllOnPlace(use.card, Player::PlaceTable))
                player->obtainCard(use.card);
        }
        return false;
    }
};

TanbeiCard::TanbeiCard()
{

}

bool TanbeiCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select != Self;
}

void TanbeiCard::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *source = effect.from;
    ServerPlayer *target = effect.to;
    Room *room = source->getRoom();

    QStringList choicelist;
    choicelist << "refuse";
    if (source->canGetCard(target, "hej"))
        choicelist << "rob";
    QString choice = room->askForChoice(target, "tanbei", choicelist.join("+"), QVariant(), "@tanbei-choose:"+source->objectName(), "rob+refuse");

    if (choice == "rob") {
        if (source->canGetCard(target, "hej")) {
            QList<int> ids;
            QList<const Card *> allcards = target->getCards("hej");
            foreach (const Card *card, allcards) {
                if (source->canGetCard(target, card->getEffectiveId()))
                    ids << card->getEffectiveId();
            }
            if (!ids.isEmpty()) {
                int card_id = ids.at(qrand() % ids.length());
                CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, source->objectName());
                room->obtainCard(source, Sanguosha->getCard(card_id), reason, false);
                QStringList assignee_list = source->property("TanbeiProhibit").toString().split("+");
                assignee_list << target->objectName();
                room->setPlayerProperty(source, "TanbeiProhibit", assignee_list.join("+"));
            }
        }
    } else {
        QStringList assignee_list = source->property("TanbeiAssault").toString().split("+");
        assignee_list << target->objectName();
        room->setPlayerProperty(source, "TanbeiAssault", assignee_list.join("+"));
    }
}

class TanbeiViewAsSkill : public ZeroCardViewAsSkill
{
public:
    TanbeiViewAsSkill() : ZeroCardViewAsSkill("tanbei")
    {

    }

    const Card *viewAs() const
    {
        return new TanbeiCard;
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("TanbeiCard");
    }
};

class Tanbei : public TriggerSkill
{
public:
    Tanbei() : TriggerSkill("tanbei")
    {
        events << EventPhaseChanging;
        view_as_skill = new TanbeiViewAsSkill;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseChanging && data.value<PhaseChangeStruct>().from == Player::Play) {
            room->setPlayerProperty(player, "TanbeiAssault", QVariant());
            room->setPlayerProperty(player, "TanbeiProhibit", QVariant());
        }
    }

    bool triggerable(const ServerPlayer *) const
    {
        return false;
    }

};

class TanbeiTargetMod : public TargetModSkill
{
public:
    TanbeiTargetMod() : TargetModSkill("#tanbei-target")
    {
        pattern = "^SkillCard";
    }

    int getResidueNum(const Player *from, const Card *, const Player *to) const
    {
        QStringList assignee_list = from->property("TanbeiAssault").toString().split("+");
        if (to && assignee_list.contains(to->objectName()))
            return 1000;
        return 0;
    }

    int getDistanceLimit(const Player *from, const Card *, const Player *to) const
    {
        QStringList assignee_list = from->property("TanbeiAssault").toString().split("+");
        if (to && assignee_list.contains(to->objectName()))
            return 1000;
        return 0;
    }
};

class TanbeiProhibit : public ProhibitSkill
{
public:
    TanbeiProhibit() : ProhibitSkill("#tanbei-prohibit")
    {
    }

    virtual bool isProhibited(const Player *from, const Player *to, const Card *card, const QList<const Player *> &) const
    {
        if (from != NULL && card->getTypeId() != Card::TypeSkill) {
            QStringList assignee_list = from->property("TanbeiProhibit").toString().split("+");
            return assignee_list.contains(to->objectName());
        }
        return false;
    }
};

SidaoCard::SidaoCard()
{
    will_throw = false;
    handling_method = Card::MethodNone;
}

bool SidaoCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    QStringList tos = Self->property("sidao_targets").toString().split("+");
    if (!tos.contains(to_select->objectName())) return false;
    Snatch *snatch = new Snatch(getSuit(), getNumber());
    if (snatch) {
        snatch->addSubcards(this->subcards);
        snatch->deleteLater();
        snatch->setFlags("Global_NoDistanceChecking");
    }
    return snatch && snatch->targetFilter(targets, to_select, Self) && !Self->isProhibited(to_select, snatch, targets);
}

bool SidaoCard::targetsFeasible(const QList<const Player *> &targets, const Player *Self) const
{
    Snatch *snatch = new Snatch(getSuit(), getNumber());
    if (snatch) {
        snatch->addSubcards(this->subcards);
        snatch->deleteLater();
    }
    return snatch && snatch->targetsFeasible(targets, Self);
}

const Card *SidaoCard::validate(CardUseStruct &card_use) const
{
    ServerPlayer *source = card_use.from;
    Room *room = source->getRoom();
    room->addPlayerMark(source, "SidaoUsed");

    Snatch *snatch = new Snatch(getSuit(), getNumber());
    snatch->setSkillName("sidao");
    snatch->addSubcards(this->subcards);
    return snatch;
}


class SidaoViewAsSkill : public OneCardViewAsSkill
{
public:
    SidaoViewAsSkill() : OneCardViewAsSkill("sidao")
    {
        response_pattern = "@@sidao";
        response_or_use = true;
    }

    bool viewFilter(const Card *to_select) const
    {
        if (to_select->isEquipped()) return false;
        Snatch *snatch = new Snatch(to_select->getSuit(), to_select->getNumber());
        snatch->addSubcard(to_select);
        return snatch->isAvailable(Self);
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        SidaoCard *snatch = new SidaoCard;
        snatch->addSubcard(originalCard);
        return snatch;
    }
};

class Sidao : public TriggerSkill
{
public:
    Sidao() : TriggerSkill("sidao")
    {
        events << CardFinished << EventPhaseChanging;
        view_as_skill = new SidaoViewAsSkill;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseChanging && data.value<PhaseChangeStruct>().from == Player::Play)
            room->setPlayerMark(player, "SidaoUsed", 0);
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {

        if (triggerEvent != CardFinished || !TriggerSkill::triggerable(player) ||
                player->getMark("SidaoUsed") > 0 || player->getPhase() != Player::Play) return QStringList();
        CardUseStruct use = data.value<CardUseStruct>();
        const Card *card = use.card;
        if (card && card->getTypeId() != Card::TypeSkill && card->getHandlingMethod() == Card::MethodUse) {

            Snatch *snatch = new Snatch(Card::NoSuit, 0);
            room->setCardFlag(snatch, "Global_NoDistanceChecking");

            foreach (ServerPlayer *p, use.to) {
                if (snatch->targetFilter(QList<const Player *>(), p, player) && !player->isProhibited(p, snatch))
                    return QStringList(objectName());
            }


            /*
            int n = card->tag["PlayCardUseNum"].toInt();
            if (n > 1) {
                QVariantList targets_list = player->tag["PlayCardUseTargets"].toList();
                if (targets_list.length() >= n-1) {
                    QStringList targets = targets_list.at(n-1).toStringList();
                    foreach (ServerPlayer *p, use.to) {
                        if (targets.contains(p->objectName()))
                            return QStringList(objectName());
                    }
                }
            }
            */

        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        QStringList tos;
        foreach (ServerPlayer *p, use.to) {
            tos << p->objectName();
        }
        room->setPlayerProperty(player, "sidao_targets", tos.join("+"));
        room->askForUseCard(player, "@@sidao", "@sidao-snatch", data, Card::MethodUse, true);
        room->setPlayerProperty(player, "sidao_targets", QString());
        return false;
    }
};

class Xingluan : public TriggerSkill
{
public:
    Xingluan() : TriggerSkill("xingluan")
    {
        events << CardFinished << EventPhaseChanging;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseChanging && data.value<PhaseChangeStruct>().from == Player::Play)
            room->setPlayerMark(player, "XingluanUsed", 0);
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (triggerEvent != CardFinished || !TriggerSkill::triggerable(player) || player->getMark("XingluanUsed") > 0
                || player->getPhase() != Player::Play) return QStringList();
        CardUseStruct use = data.value<CardUseStruct>();
        const Card *card = use.card;
        if (card && card->hasFlag("XingluanCanInvoke") && card->getTypeId() != Card::TypeSkill
                && card->getHandlingMethod() == Card::MethodUse) {
            QList<int> all_ids = room->getDrawPile();
            foreach (int card_id, all_ids) {
                if (Sanguosha->getCard(card_id)->getNumber() == 6)
                    return QStringList(objectName());
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        if (player->askForSkillInvoke(objectName())) {
            player->broadcastSkillInvoke(objectName());
            room->addPlayerMark(player, "XingluanUsed");
            QList<int> card_ids, all_ids = room->getDrawPile();
            foreach (int card_id, all_ids)
                if (Sanguosha->getCard(card_id)->getNumber() == 6)
                    card_ids << card_id;
            if (!card_ids.isEmpty()){
                int id = card_ids.at(qrand() % card_ids.length());
                player->obtainCard(Sanguosha->getCard(id), true);
            }
        }
        return false;
    }
};

LvemingCard::LvemingCard()
{
}

bool LvemingCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select->getEquips().length() < Self->getEquips().length();
}

void LvemingCard::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *source = effect.from;
    Room *room = source->getRoom();
    ServerPlayer *target = effect.to;
    room->addPlayerMark(source, "#lveming");

    QStringList nums;
    for (int i = 1; i <= 13; i++)
        nums << QString::number(i);

    int x = room->askForChoice(target, "lveming", nums.join("+"), QVariant(), "@lveming-choice:"+source->objectName()).toInt();

    JudgeStruct judge;
    judge.pattern = QString(".|.|%1|.").arg(QString::number(x));
    judge.who = source;
    judge.play_animation = false;
    judge.reason = "lveming";
    room->judge(judge);

    if (judge.isGood())
        room->damage(DamageStruct("lveming", source, target, 2));
    else {
        QList<int> ids;
        QList<const Card *> allcards = target->getCards("hej");
        foreach (const Card *card, allcards) {
            if (source->canGetCard(target, card->getEffectiveId()))
                ids << card->getEffectiveId();
        }
        if (!ids.isEmpty()) {
            int card_id = ids.at(qrand() % ids.length());
            CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, source->objectName());
            room->obtainCard(source, Sanguosha->getCard(card_id), reason, false);
        }
    }
}

class Lveming : public ZeroCardViewAsSkill
{
public:
    Lveming() : ZeroCardViewAsSkill("lveming")
    {
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("LvemingCard") && player->hasEquip();
    }

    virtual const Card *viewAs() const
    {
        return new LvemingCard;
    }
};

TunjunCard::TunjunCard()
{
}

bool TunjunCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *) const
{
    return targets.isEmpty() && to_select->getEquips().length() < S_EQUIP_AREA_LENGTH;
}

void TunjunCard::extraCost(Room *room, const CardUseStruct &card_use) const
{
    room->removePlayerMark(card_use.from, "@garrison");
}

void TunjunCard::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *source = effect.from;
    Room *room = source->getRoom();
    ServerPlayer *target = effect.to;
    int x = source->getMark("#lveming");

    for (int i = 0; i < x; i++) {
        if (target->isDead()) break;
        QList<int> equips, allcards = room->getDrawPile();
        foreach (int card_id, allcards) {
            const Card *card = Sanguosha->getCard(card_id);
            if (card->getTypeId() == Card::TypeEquip && !target->hasSameEquipKind(card) && card->isAvailable(target)) {
                equips << card_id;
            }
        }
        if (equips.isEmpty()) break;
        int index = qrand() % equips.length();
        int id = equips.at(index);
        const Card *equip = Sanguosha->getCard(id);
        room->useCard(CardUseStruct(equip, target, target));
    }
}

class Tunjun : public ZeroCardViewAsSkill
{
public:
    Tunjun() : ZeroCardViewAsSkill("tunjun")
    {
        frequency = Limited;
        limit_mark = "@garrison";
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return player->getMark(limit_mark) > 0 && player->getMark("#lveming") > 0;
    }

    virtual const Card *viewAs() const
    {
        return new TunjunCard;
    }
};

XionghuoCard::XionghuoCard()
{
}

bool XionghuoCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select != Self && to_select->getMark("@brutal") == 0;
}

void XionghuoCard::extraCost(Room *, const CardUseStruct &card_use) const
{
    card_use.from->loseMark("@brutal");
}

void XionghuoCard::onEffect(const CardEffectStruct &effect) const
{
    effect.to->gainMark("@brutal");
}

class XionghuoViewAsSkill : public ZeroCardViewAsSkill
{
public:
    XionghuoViewAsSkill() : ZeroCardViewAsSkill("xionghuo")
    {
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return player->getMark("@brutal") > 0;
    }

    virtual const Card *viewAs() const
    {
        return new XionghuoCard;
    }
};

class Xionghuo : public TriggerSkill
{
public:
    Xionghuo() : TriggerSkill("xionghuo")
    {
        events << EventPhaseStart << TurnStart << DamageCaused << EventPhaseChanging;
        view_as_skill = new XionghuoViewAsSkill;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseChanging && data.value<PhaseChangeStruct>().from == Player::Play) {
            room->setPlayerProperty(player, "XionghuoProhibit", QVariant());
        }
    }

    virtual TriggerList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        TriggerList skill_list;
        if ((triggerEvent == TurnStart && room->getTag("FirstRound").toBool())) {
            QList<ServerPlayer *> xurongs = room->findPlayersBySkillName(objectName());
            foreach (ServerPlayer *xurong, xurongs)
                skill_list.insert(xurong, QStringList("xionghuo!"));

        } else if (triggerEvent == DamageCaused && TriggerSkill::triggerable(player)) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.to && damage.to != player && damage.to->getMark("@brutal") > 0)
                skill_list.insert(player, QStringList("xionghuo!"));
        } else if (triggerEvent == EventPhaseStart && player->isAlive() && player->getPhase() == Player::Play && player->getMark("@brutal") > 0) {
            QList<ServerPlayer *> xurongs = room->findPlayersBySkillName(objectName());
            foreach (ServerPlayer *xurong, xurongs) {
                if (xurong != player)
                    skill_list.insert(xurong, QStringList("xionghuo!"));
            }
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *xurong) const
    {
        if (triggerEvent == TurnStart) {
            room->sendCompulsoryTriggerLog(xurong, objectName());
            xurong->broadcastSkillInvoke(objectName());
            xurong->gainMark("@brutal", 3);
        } else if (triggerEvent == DamageCaused) {
            room->sendCompulsoryTriggerLog(xurong, objectName());
            xurong->broadcastSkillInvoke(objectName());
            DamageStruct damage = data.value<DamageStruct>();
            damage.damage++;
            data = QVariant::fromValue(damage);
        } else if (triggerEvent == EventPhaseStart) {
            room->sendCompulsoryTriggerLog(xurong, objectName());
            xurong->broadcastSkillInvoke(objectName());
            player->loseMark("@brutal");
            int x = qrand() % 3;
            switch (x) {
            case 0:{
                room->damage(DamageStruct(objectName(), xurong, player, 1, DamageStruct::Fire));
                QStringList assignee_list = player->property("XionghuoProhibit").toString().split("+");
                assignee_list << xurong->objectName();
                room->setPlayerProperty(player, "XionghuoProhibit", assignee_list.join("+"));
                break;
            }
            case 1:{
                room->loseHp(player);
                if (player->getMaxCards() > 0)
                    room->addPlayerMark(player, "Global_MaxcardsDecrease");
                break;
            }
            case 2:{
                if (xurong->canGetCard(player, "e")) {
                    QList<int> ids;
                    QList<const Card *> equips = player->getEquips();
                    foreach (const Card *card, equips) {
                        if (xurong->canGetCard(player, card->getEffectiveId()))
                            ids << card->getEffectiveId();
                    }
                    if (!ids.isEmpty()) {
                        int id = ids.at(qrand() % ids.length());
                        CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, xurong->objectName());
                        room->obtainCard(xurong, Sanguosha->getCard(id), reason, false);
                    }
                }
                if (xurong->canGetCard(player, "h")) {
                    QList<int> ids, handcards = player->handCards();
                    foreach (int id, handcards) {
                        if (xurong->canGetCard(player, id))
                            ids << id;
                    }
                    if (!ids.isEmpty()) {
                        int id = ids.at(qrand() % ids.length());
                        CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, xurong->objectName());
                        room->obtainCard(xurong, Sanguosha->getCard(id), reason, false);
                    }
                }
                break;
            }
            default:
                break;
            }
        }
        return false;
    }
};

class XionghuoProhibit : public ProhibitSkill
{
public:
    XionghuoProhibit() : ProhibitSkill("#xionghuo-prohibit")
    {
    }

    virtual bool isProhibited(const Player *from, const Player *to, const Card *card, const QList<const Player *> &) const
    {
        if (from != NULL && card->isKindOf("Slash")) {
            QStringList assignee_list = from->property("XionghuoProhibit").toString().split("+");
            return assignee_list.contains(to->objectName());
        }
        return false;
    }
};

class Shajue : public TriggerSkill
{
public:
    Shajue() : TriggerSkill("shajue")
    {
        events << Dying;
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        DyingStruct dying_data = data.value<DyingStruct>();

        if (dying_data.who && dying_data.who != player && dying_data.who->getHp() < 0)
            return QStringList(objectName());

        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *xurong, QVariant &data, ServerPlayer *) const
    {
        DyingStruct dying_data = data.value<DyingStruct>();
        room->sendCompulsoryTriggerLog(xurong, objectName());
        xurong->broadcastSkillInvoke(objectName());
        xurong->gainMark("@brutal");
        if (dying_data.damage && dying_data.damage->card && room->isAllOnPlace(dying_data.damage->card, Player::PlaceTable)) {
            xurong->obtainCard(dying_data.damage->card);
        }
        return false;
    }

};

ChaosPackage::ChaosPackage()
: Package("chaos")
{
    General *lijue = new General(this, "lijue", "qun", 6);
    lijue->addSkill(new Langxi);
    lijue->addSkill(new Yisuan);

    General *guosi = new General(this, "guosi", "qun");
    guosi->addSkill(new Tanbei);
    guosi->addSkill(new TanbeiTargetMod);
    guosi->addSkill(new TanbeiProhibit);
    related_skills.insertMulti("tanbei", "#tanbei-target");
    related_skills.insertMulti("tanbei", "#tanbei-prohibit");
    guosi->addSkill(new Sidao);

    General *fanchou = new General(this, "fanchou", "qun");
    fanchou->addSkill(new Xingluan);

    General *zhangji = new General(this, "zhangji", "qun");
    zhangji->addSkill(new Lveming);
    zhangji->addSkill(new Tunjun);

    General *xurong = new General(this, "xurong", "qun");
    xurong->addSkill(new Xionghuo);
    xurong->addSkill(new XionghuoProhibit);
    xurong->addSkill(new Shajue);
    related_skills.insertMulti("xionghuo", "#xionghuo-prohibit");



    addMetaObject<TanbeiCard>();
    addMetaObject<SidaoCard>();
    addMetaObject<LvemingCard>();
    addMetaObject<TunjunCard>();
    addMetaObject<XionghuoCard>();
}

ADD_PACKAGE(Chaos)
