#include "compete-world.h"
#include "client.h"
#include "engine.h"
#include "general.h"
#include "room.h"



Demobilizing::Demobilizing(Suit suit, int number)
    : SingleTargetTrick(suit, number)
{
    setObjectName("demobilizing");
}

bool Demobilizing::targetRated(const QList<const Player *> &targets, const Player *to_select, const Player *) const
{
    return targets.isEmpty() && to_select->hasEquip();
}

void Demobilizing::onEffect(const CardEffectStruct &effect) const
{
    QList<const Card *> equips = effect.to->getEquips();
    if (equips.isEmpty()) return;
    DummyCard *card = new DummyCard;
    foreach (const Card *equip, equips) {
        card->addSubcard(equip);
    }
    if (card->subcardsLength() > 0)
        effect.to->obtainCard(card);
}

class FloweringTreeDiscard : public ViewAsSkill
{
public:
    FloweringTreeDiscard() : ViewAsSkill("floweringtree_discard")
    {
        response_pattern = "@@floweringtree_discard";
    }


    bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
        return selected.length() < 2 && !Self->isJilei(to_select);
    }

    const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (!cards.isEmpty()) {
            DummyCard *discard = new DummyCard;
            discard->addSubcards(cards);
            return discard;
        }

        return NULL;
    }
};

FloweringTree::FloweringTree(Suit suit, int number)
    : SingleTargetTrick(suit, number)
{
    setObjectName("flowering_tree");
    target_fixed = true;
}

bool FloweringTree::targetRated(const QList<const Player *> &targets, const Player *, const Player *) const
{
    return targets.isEmpty();
}

QList<ServerPlayer *> FloweringTree::defaultTargets(Room *, ServerPlayer *source) const
{
    return QList<ServerPlayer *>() << source;
}

bool FloweringTree::isAvailable(const Player *player) const
{
    return !player->isProhibited(player, this) && TrickCard::isAvailable(player);
}

void FloweringTree::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.to->getRoom();
    const Card *card = room->askForCard(effect.to, "@@floweringtree_discard", "@floweringtree-discard", QVariant(), Card::MethodNone);
    if (card->subcardsLength() > 0) {
        int x = card->subcardsLength();
        QList<int> to_discard = card->getSubcards();
        foreach (int id, to_discard) {
            if (Sanguosha->getCard(id)->getTypeId() == Card::TypeEquip) {
                x++;
                break;
            }
        }
        CardMoveReason mreason(CardMoveReason::S_REASON_THROW, effect.to->objectName(), QString(), objectName(), QString());
        room->throwCard(card, mreason, effect.to);
        effect.to->drawCards(x, objectName());
    }
}




BrokenHalberd::BrokenHalberd(Suit suit, int number)
    : Weapon(suit, number, 0)
{
    setObjectName("broken_halberd");
}



class WomenDressSkill : public ArmorSkill
{
public:
    WomenDressSkill() : ArmorSkill("women_dress")
    {
        events << CardsMoveOneTime;
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (triggerEvent == CardsMoveOneTime && player->getMark("Armor_Nullified") == 0) {
            QVariantList move_datas = data.toList();
            foreach(QVariant move_data, move_datas) {
                CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
                QString source_name = move.reason.m_playerId;
                ServerPlayer *source = room->findPlayer(source_name);
                if (source && source->ingoreArmor(player)) continue;

                if (move.from == player && move.from_places.contains(Player::PlaceEquip)) {
                    for (int i = 0; i < move.card_ids.size(); i++) {
                        if (move.from_places[i] != Player::PlaceEquip) continue;
                        const Card *card = Sanguosha->getEngineCard(move.card_ids[i]);
                        if (card->objectName() == objectName()) {
                            return QStringList(objectName());
                        }
                    }
                }
                if (move.to == player && move.to_place == Player::PlaceEquip) {
                    for (int i = 0; i < move.card_ids.size(); i++) {
                        const Card *card = Sanguosha->getEngineCard(move.card_ids[i]);
                        if (card->objectName() == objectName()) {
                            return QStringList(objectName());
                        }
                    }
                }

            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        room->sendCompulsoryTriggerLog(player, objectName(), false);
        room->setEmotion(player, "armor/women_dress");

        room->askForDiscard(player, "women_dress", 1, 1, false, true);
        return false;
    }
};

WomenDress::WomenDress(Suit suit, int number)
    : Armor(suit, number)
{
    setObjectName("women_dress");
    gift = true;
}














class Falu : public TriggerSkill
{
public:
    Falu() : TriggerSkill("falu")
    {
        events << TurnStart << CardsMoveOneTime;
        frequency = Compulsory;
    }

    virtual TriggerList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        TriggerList skill_list;
        if ((triggerEvent == TurnStart && room->getTag("FirstRound").toBool())) {
            QList<ServerPlayer *> zhangqiyings = room->findPlayersBySkillName(objectName());
            foreach (ServerPlayer *zhangqiying, zhangqiyings)
                skill_list.insert(zhangqiying, QStringList(objectName()));

        } else if (triggerEvent == CardsMoveOneTime && TriggerSkill::triggerable(player)) {
            QVariantList move_datas = data.toList();
            foreach(QVariant move_data, move_datas) {
                CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
                if (move.from == player && move.to_place == Player::DiscardPile
                        && (move.reason.m_reason & CardMoveReason::S_MASK_BASIC_REASON) == CardMoveReason::S_REASON_DISCARD) {
                    QStringList names;
                    names << "@ziwei" << "@houtu" << "@yuqing" << "@gouchen";
                    for (int i = 0; i < move.card_ids.length(); i++) {
                        int id = move.card_ids.at(i);
                        if (move.from_places[i] == Player::PlaceHand || move.from_places[i] == Player::PlaceEquip) {
                            int suit = (int) Sanguosha->getCard(id)->getSuit();
                            if (player->getMark(names.at(suit)) == 0) {
                                skill_list.insert(player, QStringList(objectName()));
                                return skill_list;
                            }
                        }
                    }
                }
            }
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *, QVariant &data, ServerPlayer *player) const
    {
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());

        QStringList names, addmarks;
        names << "@ziwei" << "@houtu" << "@yuqing" << "@gouchen";

        if (triggerEvent == TurnStart)
            addmarks = names;
        else {
            QVariantList move_datas = data.toList();
            foreach(QVariant move_data, move_datas) {
                CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
                if (move.from == player && move.to_place == Player::DiscardPile
                        && (move.reason.m_reason & CardMoveReason::S_MASK_BASIC_REASON) == CardMoveReason::S_REASON_DISCARD) {
                    for (int i = 0; i < move.card_ids.length(); i++) {
                        int id = move.card_ids.at(i);
                        if (move.from_places[i] == Player::PlaceHand || move.from_places[i] == Player::PlaceEquip) {
                            int suit = (int) Sanguosha->getCard(id)->getSuit();
                            QString mark_name = names.at(suit);
                            if (player->getMark(mark_name) == 0 && !addmarks.contains(mark_name))
                                addmarks << mark_name;
                        }
                    }
                }
            }
        }

        foreach (QString mark_name, names) {
            if (addmarks.contains(mark_name))
                player->gainMark(mark_name);
        }
        return false;
    }
};

class ZhenyiViewAsSkill : public OneCardViewAsSkill
{
public:
    ZhenyiViewAsSkill() : OneCardViewAsSkill("zhenyi")
    {
        filter_pattern = ".|.|.|hand";
        response_or_use = true;
    }

    virtual bool isEnabledAtPlay(const Player *) const
    {
        return false;
    }

    virtual bool isEnabledAtResponse(const Player *player, const QString &pattern) const
    {
        return pattern.contains("peach") && player->getHp() < 1 && player->getMark("@houtu") > 0;
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        Peach *peach = new Peach(originalCard->getSuit(), originalCard->getNumber());
        peach->addSubcard(originalCard->getId());
        peach->setSkillName(objectName());
        return peach;
    }
};

class Zhenyi : public TriggerSkill
{
public:
    Zhenyi() : TriggerSkill("zhenyi")
    {
        events << PreCardUsed << AskForRetrial << DamageCaused << Damaged;
        view_as_skill = new ZhenyiViewAsSkill;
    }

    virtual void record(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == PreCardUsed) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card->isKindOf("Peach") && use.card->getSkillName() == objectName())
                player->loseMark("@houtu");
        }
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        if (triggerEvent == AskForRetrial) {
            if (player->getMark("@ziwei") > 0) return QStringList(objectName());
        } else if (triggerEvent == DamageCaused) {
            if (player->getMark("@yuqing") > 0) return QStringList(objectName());
        } else if (triggerEvent == Damaged) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.nature != DamageStruct::Normal && player->getMark("@gouchen") > 0) return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        if (triggerEvent == AskForRetrial) {
            JudgeStruct *judge = data.value<JudgeStruct *>();
            if (player->askForSkillInvoke(objectName(), data)) {
                player->broadcastSkillInvoke(objectName());
                player->loseMark("@ziwei");
                QString choice = room->askForChoice(player, objectName(), "spade+heart", data, "@zhenyi-choose:"+judge->who->objectName()+"::"+judge->reason);
                if (choice == "heart") {

                } else if (choice == "spade") {

                }
            }
        } else if (triggerEvent == DamageCaused) {
            if (player->askForSkillInvoke(objectName(), data)) {
                player->broadcastSkillInvoke(objectName());
                player->loseMark("@yuqing");
                JudgeStruct judge;
                judge.reason = objectName();
                judge.pattern = ".|black";
                judge.who = player;
                room->judge(judge);
                if (judge.isGood()) {
                    DamageStruct damage = data.value<DamageStruct>();
                    damage.damage++;
                    data = QVariant::fromValue(damage);
                }
            }
        } else if (triggerEvent == Damaged) {
            if (player->askForSkillInvoke(objectName(), data)) {
                player->broadcastSkillInvoke(objectName());
                player->loseMark("@gouchen");

                QList<Card::CardType> types;
                types << Card::TypeBasic << Card::TypeTrick << Card::TypeEquip;

                foreach (Card::CardType type, types) {
                    if (player->isDead()) break;
                    QList<int> cards;
                    foreach (int card_id, room->getDrawPile()) {
                        if (type == Sanguosha->getCard(card_id)->getTypeId())
                            cards << card_id;
                    }
                    if (!cards.isEmpty()){
                        int index = qrand() % cards.length();
                        int id = cards.at(index);
                        player->obtainCard(Sanguosha->getCard(id), false);
                    }
                }
            }
        }
        return false;
    }
};

class Dianhua : public PhaseChangeSkill
{
public:
    Dianhua() : PhaseChangeSkill("dianhua")
    {
        frequency = Frequent;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *target, QVariant &, ServerPlayer* &) const
    {
        if (PhaseChangeSkill::triggerable(target) && getGuanxingNum(target) > 0
                && (target->getPhase() == Player::Start || target->getPhase() == Player::Finish))
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool onPhaseChange(ServerPlayer *zhuge) const
    {
        if (zhuge->askForSkillInvoke(this)) {
            zhuge->broadcastSkillInvoke(objectName());
            Room *room = zhuge->getRoom();
            QList<int> guanxing = room->getNCards(getGuanxingNum(zhuge));

            LogMessage log;
            log.type = "$ViewDrawPile";
            log.from = zhuge;
            log.card_str = IntList2StringList(guanxing).join("+");
            room->sendLog(log, zhuge);

            room->askForGuanxing(zhuge, guanxing, Room::GuanxingUpOnly);
        }

        return false;
    }

    virtual int getGuanxingNum(ServerPlayer *target) const
    {
        return target->getMark("@ziwei") + target->getMark("@houtu") + target->getMark("@yuqing") + target->getMark("@gouchen");
    }
};



class ZhenxingSelect : public OneCardViewAsSkill
{
public:
    ZhenxingSelect() : OneCardViewAsSkill("zhenxing_select")
    {
        expand_pile = "#zhenxing",
        response_pattern = "@@zhenxing_select";
    }

    bool viewFilter(const Card *to_select) const
    {
        QList<int> cards = Self->getPile(expand_pile);

        bool cheak = false;

        foreach (int id, cards) {
            const Card *card = Sanguosha->getCard(id);
            if (card->sameSuitWith(to_select)) {
                cheak = !cheak;
                if (!cheak) break;
            }
        }

        return cheak && cards.contains(to_select->getEffectiveId());

    }

    const Card *viewAs(const Card *originalCard) const
    {
        return originalCard;
    }
};

class Zhenxing : public TriggerSkill
{
public:
    Zhenxing() : TriggerSkill("zhenxing")
    {
        events << EventPhaseStart << Damaged;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *target, QVariant &, ServerPlayer* &) const
    {
        if (TriggerSkill::triggerable(target) && (triggerEvent == Damaged || target->getPhase() == Player::Finish))
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const
    {
        if (player->askForSkillInvoke(this)) {
            player->broadcastSkillInvoke(objectName());
            QList<int> ids = room->getNCards(3, false);
            room->notifyMoveToPile(player, ids, "zhenxing", Player::PlaceTable, true, true);
            const Card *card = room->askForCard(player, "@@zhenxing_select", "@zhenxing-select", QVariant(), Card::MethodNone);
            room->notifyMoveToPile(player, ids, "zhenxing", Player::PlaceTable, false, false);
            room->returnToTopDrawPile(ids);
            if (card)
                player->obtainCard(card);
        }
        return false;
    }
};



CompeteWorldCardPackage::CompeteWorldCardPackage()
    : Package("CompeteWorldCard", Package::CardPack)
{
    QList<Card *> cards;


    cards << new Demobilizing(Card::Club, 3)
          << new Demobilizing(Card::Diamond, 3)
          << new FloweringTree(Card::Heart, 9)
          << new FloweringTree(Card::Heart, 11)
          << new FloweringTree(Card::Diamond, 9)
          << new BrokenHalberd << new WomenDress;

    foreach(Card *card, cards)
        card->setParent(this);

    skills << new FloweringTreeDiscard << new WomenDressSkill;
}

ADD_PACKAGE(CompeteWorldCard)


CompeteWorldPackage::CompeteWorldPackage()
    :Package("CompeteWorld")
{

    General *zhangqiying = new General(this, "zhangqiying", "qun", 3, false);
    zhangqiying->addSkill(new Falu);
    zhangqiying->addSkill(new Zhenyi);
    zhangqiying->addSkill(new Dianhua);

    General *zhanggong = new General(this, "zhanggong", "wei", 3);
    zhanggong->addSkill(new Skill("qianxini"));
    zhanggong->addSkill(new Zhenxing);

    General *lvkai = new General(this, "lvkai", "shu", 3);
    lvkai->addSkill(new Skill("tunan"));
    lvkai->addSkill(new Skill("bijing"));

    General *weiwenzhugezhi = new General(this, "weiwenzhugezhi", "wu", 4);
    weiwenzhugezhi->addSkill(new Skill("fuhai"));

    skills << new ZhenxingSelect;

}

ADD_PACKAGE(CompeteWorld)

