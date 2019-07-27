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
























QianxiniCard::QianxiniCard()
{
    target_fixed = true;
}

void QianxiniCard::use(Room *, ServerPlayer *, QList<ServerPlayer *> &) const
{





}

class Qianxini : public ViewAsSkill
{
public:
    Qianxini() : ViewAsSkill("qianxini")
    {
    }

    virtual bool viewFilter(const QList<const Card *> &, const Card *to_select) const
    {
        return !to_select->isEquipped();
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.isEmpty())
            return NULL;

        QianxiniCard *qianxin_card = new QianxiniCard;
        qianxin_card->addSubcards(cards);
        qianxin_card->setSkillName(objectName());
        return qianxin_card;
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("QianxiniCard");
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

            int x = room->askForChoice(player, objectName(), "1+2+3", QVariant(), "@zhenxing-choose").toInt();

            QList<int> ids = room->getNCards(x, false);
            room->notifyMoveToPile(player, ids, "zhenxing", Player::PlaceTable, true, true);
            const Card *card = room->askForCard(player, "@@zhenxing_select", "@zhenxing-select", QVariant(), Card::MethodNone);
            room->notifyMoveToPile(player, ids, "zhenxing", Player::PlaceTable, false, false);

            if (card) {
                ids.removeOne(card->getId());
                player->obtainCard(card);
            }

            if (!ids.isEmpty()) {
                DummyCard *dummy = new DummyCard(ids);
                CardMoveReason reason(CardMoveReason::S_REASON_NATURAL_ENTER, player->objectName(), objectName(), QString());
                room->throwCard(dummy, reason, NULL);
                delete dummy;
            }
        }
        return false;
    }
};

FuhaiCard::FuhaiCard()
{
    will_throw = false;
    handling_method = Card::MethodNone;
}

bool FuhaiCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    QList<const Player *> players = Self->getAliveSiblings();
    players.append(Self);

    int x = Self->getSeat();
    int x1 = x+1,x2 = x-1;
    if (x == 1)
        x2 = players.length();
    if (x == players.length())
        x1 = 1;

    return targets.isEmpty() && (to_select->getSeat() == x1 || to_select->getSeat() == x2) && !to_select->isKongcheng() && !to_select->hasFlag("fuhaiTargeted");
}

void FuhaiCard::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *weizhu = effect.from;
    ServerPlayer *first = effect.to;
    Room *room = weizhu->getRoom();

    QList<ServerPlayer *> players = room->getAlivePlayers();

    bool start = false;
    QList<ServerPlayer *> fuhai_targets;
    foreach (ServerPlayer *p, players) {
        if (p == weizhu || start) {
            start = true;
            fuhai_targets << p;
        }
    }
    foreach (ServerPlayer *p, players) {
        if (p == weizhu) break;
        fuhai_targets << p;
    }
    fuhai_targets.removeOne(weizhu);

    if (first != fuhai_targets.first()) {
        QList<ServerPlayer *> _targets;
        for (int i = fuhai_targets.length()-1; i >= 0; i--) {
            _targets << fuhai_targets[i];
        }
        fuhai_targets = _targets;
    }

    foreach (ServerPlayer *target, fuhai_targets) {
        if (target->isDead()) continue;
        if (weizhu->isDead() || weizhu->isKongcheng() || target->isKongcheng() || target->hasFlag("fuhaiTargeted")) break;

        room->addPlayerMark(weizhu, "fuhaiTimes");
        room->setPlayerFlag(target, "fuhaiTargeted");

        const Card *card1 = NULL;
        if (start) {
            start = false;
            card1 = this;
        } else {
            card1 = room->askForCard(weizhu, ".!", "@fuhai-show1::"+target->objectName(), QVariant(), Card::MethodNone);
            if (card1 == NULL)
                card1 = weizhu->getHandcards().first();

            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, weizhu->objectName(), target->objectName());
        }

        room->showCard(weizhu, card1->getEffectiveId());

        const Card *card2 = room->askForCard(target, ".!", "@fuhai-show2:"+weizhu->objectName(), QVariant(), Card::MethodNone);

        if (card2 == NULL)
            card2 = target->getHandcards().first();

        room->showCard(target, card2->getEffectiveId());
        room->getThread()->delay(3000);

        if (card1->getNumber() < card2->getNumber()) {
            room->throwCard(card2, target);
            int x = weizhu->getMark("fuhaiTimes");
            weizhu->drawCards(x, objectName());
            target->drawCards(x, objectName());
            room->setPlayerFlag(weizhu, "fuhaiStop");
            break;
        } else
            room->throwCard(card1, weizhu);
    }
}

class FuhaiViewAsSkill : public OneCardViewAsSkill
{
public:
    FuhaiViewAsSkill() : OneCardViewAsSkill("fuhai")
    {
        filter_pattern = ".|.|.|hand";
    }

    virtual bool isEnabledAtPlay(const Player *weizhu) const
    {
        return !weizhu->hasFlag("fuhaiStop");
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        FuhaiCard *card = new FuhaiCard;
        card->addSubcard(originalCard);
        card->setSkillName(objectName());
        return card;
    }
};

class Fuhai : public TriggerSkill
{
public:
    Fuhai() : TriggerSkill("fuhai")
    {
        events << EventPhaseChanging;
        view_as_skill = new FuhaiViewAsSkill;
    }

    bool triggerable(const ServerPlayer *) const
    {
        return false;
    }

    virtual void record(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (data.value<PhaseChangeStruct>().from == Player::Play) {
            room->setPlayerMark(player, "fuhaiTimes", 0);
            room->setPlayerFlag(player, "-fuhaiStop");
            QList<ServerPlayer *> players = room->getAlivePlayers();
            foreach (ServerPlayer *p, players) {
                room->setPlayerFlag(p, "-fuhaiTargeted");
            }
        }
    }
};










TunanCard::TunanCard()
{
}

void TunanCard::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *lvkai = effect.from;
    ServerPlayer *target = effect.to;
    Room *room = lvkai->getRoom();


}

class Tunan : public ZeroCardViewAsSkill
{
public:
    Tunan() : ZeroCardViewAsSkill("tunan")
    {
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("TunanCard");
    }

    virtual const Card *viewAs() const
    {
        return new TunanCard;
    }
};




class Bijing : public TriggerSkill
{
public:
    Bijing() : TriggerSkill("bijing")
    {
        events << EventPhaseStart << PreCardsMoveOneTime;
        frequency = Compulsory;
    }

    virtual void record(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == PreCardsMoveOneTime) {
            const Card *card = player->tag["bijingCard"].value<const Card *>();
            if (card == NULL) return;
            QVariantList move_datas = data.toList();
            foreach(QVariant move_data, move_datas) {
                CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
                if (player != move.from) return;
                int id = card->getEffectiveId();
                if (move.card_ids.contains(id) && move.from_places[move.card_ids.indexOf(id)] == Player::PlaceHand) {
                    player->tag.remove("bijingCard");
                    if (room->getCurrent() && room->getCurrent()->getPhase() != Player::NotActive)
                        room->setPlayerFlag(player, "bijingLostCard");
                    return;
                }
            }
        }
    }
    virtual TriggerList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        TriggerList skill_list;
        if (triggerEvent == EventPhaseStart) {
            switch (player->getPhase()) {
            case Player::Start: {
                const Card *card = player->tag["bijingCard"].value<const Card *>();
                if (card) {
                    int id = card->getEffectiveId();
                    if (room->getCardOwner(id) == player && room->getCardPlace(id) == Player::PlaceHand)
                        skill_list.insert(player, QStringList(objectName()));
                }
                break;
            }
            case Player::Discard: {
                QList<ServerPlayer *> players = room->getOtherPlayers(player);
                foreach (ServerPlayer *p, players) {
                    if (p->hasFlag("bijingLostCard"))
                        skill_list.insert(p, QStringList(objectName()));
                }
                break;
            }
            case Player::Finish: {
                if (TriggerSkill::triggerable(player) && !player->isKongcheng())
                    skill_list.insert(player, QStringList(objectName()));
                break;
            }
            default: break;
            }
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        switch (player->getPhase()) {
        case Player::Start: {
            const Card *card = player->tag["bijingCard"].value<const Card *>();
            if (card)
                room->throwCard(card->getEffectiveId(), player);
            break;
        }
        case Player::Discard: {
            room->askForDiscard(player, objectName(), 2, 2, false, true);
            break;
        }
        case Player::Finish: {

            const Card *card = room->askForCard(player, ".", "@bijing-invoke", data, Card::MethodNone);
            if (card) {
                player->broadcastSkillInvoke(objectName());
                room->notifySkillInvoked(player, objectName());
                LogMessage log;
                log.from = player;
                log.type = "#InvokeSkill";
                log.arg = objectName();
                room->sendLog(log);
                player->tag["bijingCard"] = QVariant::fromValue(card);
            }

            break;
        }
        default: break;
        }
        return false;
    }
};

class Zongkui : public TriggerSkill
{
public:
    Zongkui() : TriggerSkill("zongkui")
    {
        events << TurnStart;
        frequency = Compulsory;
    }

    virtual TriggerList triggerable(TriggerEvent , Room *room, ServerPlayer *player, QVariant &) const
    {
        TriggerList skill_list;
        if (room->getTag("TurnFirstRound").toBool()) {
            QList<ServerPlayer *> beimihus = room->findPlayersBySkillName(objectName());
            foreach (ServerPlayer *beimihu, beimihus) {
                QList<ServerPlayer *> targets, allplayers = room->getOtherPlayers(beimihu);
                int min_hp = allplayers.first()->getHp();
                foreach (ServerPlayer *p, allplayers) {
                    if (p->getHp() < min_hp) {
                        targets.clear();
                        min_hp = p->getHp();
                    }
                    if (p->getHp() == min_hp && p->getMark("@puppet") == 0)
                        targets << p;
                }
                if (!targets.isEmpty())
                    skill_list.insert(beimihu, QStringList("zongkui!"));
            }
        }
        if (TriggerSkill::triggerable(player) && !skill_list.contains(player)) {
            QList<ServerPlayer *> allplayers = room->getOtherPlayers(player);
            foreach (ServerPlayer *p, allplayers) {
                if (p->getMark("@puppet") == 0) {
                    skill_list.insert(player, QStringList(objectName()));
                }
            }
        }

        return skill_list;
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *beimihu) const
    {
        if (room->getTag("TurnFirstRound").toBool()) {
            QList<ServerPlayer *> targets, allplayers = room->getOtherPlayers(beimihu);
            int min_hp = allplayers.first()->getHp();
            foreach (ServerPlayer *p, allplayers) {
                if (p->getHp() < min_hp) {
                    targets.clear();
                    min_hp = p->getHp();
                }
                if (p->getHp() == min_hp && p->getMark("@puppet") == 0)
                    targets << p;
            }
            if (!targets.isEmpty()) {
                room->sendCompulsoryTriggerLog(beimihu, objectName());
                beimihu->broadcastSkillInvoke(objectName());
                ServerPlayer *target = room->askForPlayerChosen(beimihu, targets, objectName(), "@zongkui2-invoke");
                room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, beimihu->objectName(), target->objectName());
                target->gainMark("@puppet");
            }
        }
        if (beimihu == player && TriggerSkill::triggerable(beimihu)) {
            QList<ServerPlayer *> targets, allplayers = room->getOtherPlayers(beimihu);
            foreach (ServerPlayer *p, allplayers) {
                if (p->getMark("@puppet") == 0)
                    targets << p;
            }
            if (targets.isEmpty()) return false;

            ServerPlayer *target = room->askForPlayerChosen(beimihu, targets, objectName(), "@zongkui-invoke", true, true);
            if (target) {
                beimihu->broadcastSkillInvoke(objectName());
                target->gainMark("@puppet");
            }
        }

        return false;
    }
};

class Guju : public TriggerSkill
{
public:
    Guju() : TriggerSkill("guju")
    {
        events << Damaged;
        frequency = Compulsory;
    }

    virtual TriggerList triggerable(TriggerEvent , Room *room, ServerPlayer *player, QVariant &) const
    {
        TriggerList skill_list;
        if (player->getMark("@puppet") > 0) {
            QList<ServerPlayer *> beimihus = room->findPlayersBySkillName(objectName());
            foreach (ServerPlayer *beimihu, beimihus)
                skill_list.insert(beimihu, QStringList(objectName()));
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *, QVariant &, ServerPlayer *beimihu) const
    {
        room->sendCompulsoryTriggerLog(beimihu, objectName());
        beimihu->broadcastSkillInvoke(objectName());

        beimihu->drawCards(1, objectName());
        room->addPlayerMark(beimihu, "#guju");
        return false;
    }
};

class Baijia : public PhaseChangeSkill
{
public:
    Baijia() : PhaseChangeSkill("baijia")
    {
        frequency = Wake;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target)
            && target->getMark("baijia") == 0
            && target->getPhase() == Player::Start
            && target->getMark("#guju") > 6;
    }

    virtual bool onPhaseChange(ServerPlayer *beimihu) const
    {
        Room *room = beimihu->getRoom();
        room->sendCompulsoryTriggerLog(beimihu, objectName());

        beimihu->broadcastSkillInvoke(objectName());

        room->setPlayerMark(beimihu, objectName(), 1);
        if (room->changeMaxHpForAwakenSkill(beimihu, 1) && beimihu->getMark(objectName()) == 1) {
            room->recover(beimihu, RecoverStruct(beimihu));
            foreach (ServerPlayer *p, room->getOtherPlayers(beimihu)) {
                if (p->getMark("@puppet") == 0)
                    p->gainMark("@puppet");
            }
            room->handleAcquireDetachSkills(beimihu, "-guju|canshii");
        }
        return false;
    }
};

class Canshii : public TriggerSkill
{
public:
    Canshii() : TriggerSkill("canshii")
    {
        events << TargetChosed << TargetConfirming;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card->getTypeId() != Card::TypeBasic && !use.card->isNDTrick()) return QStringList();
        if (use.card->isKindOf("Collateral") || use.card->isKindOf("BeatAnother")) return QStringList();
        if (triggerEvent == TargetChosed) {
            QList<ServerPlayer *> available_targets = player->getUseExtraTargets(use, false);
            foreach (ServerPlayer *p, available_targets) {
                if (p->getMark("@puppet") > 0)
                    return QStringList(objectName());
            }

        } else if (triggerEvent == TargetConfirming) {
            if (use.from && use.from->isAlive() && use.from->getMark("@puppet") > 0) {
                foreach (ServerPlayer *to, use.to)
                    if (to->isAlive() && to != player) return QStringList();

                return QStringList(objectName());
            }

        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        if (triggerEvent == TargetChosed) {
            QList<ServerPlayer *> available_targets = player->getUseExtraTargets(use, false);
            foreach (ServerPlayer *p, available_targets) {
                if (p->getMark("@puppet") == 0) available_targets.removeOne(p);
            }

            if (available_targets.isEmpty()) return false;

            QList<ServerPlayer *> choosees = room->askForPlayersChosen(player, available_targets, objectName(), 0, 100,
                                                                       "@canshii-add:::" + use.card->objectName(), true);

            if (choosees.length() > 0) {
                player->broadcastSkillInvoke(objectName());
                room->sortByActionOrder(choosees);
                foreach (ServerPlayer *target, choosees) {
                    use.to.append(target);
                    room->removePlayerMark(target, "@puppet");
                }
            }

            room->sortByActionOrder(use.to);
            data = QVariant::fromValue(use);
            return false;


        } else if (triggerEvent == TargetConfirming) {
            if (player->askForSkillInvoke(objectName(), "prompt:::" + use.card->objectName())) {
                player->broadcastSkillInvoke(objectName());
                room->removePlayerMark(use.from, "@puppet");
                use.to.removeOne(player);
                data = QVariant::fromValue(use);
            }
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
    General *zhanggong = new General(this, "zhanggong", "wei", 3);
    zhanggong->addSkill(new Qianxini);
    zhanggong->addSkill(new Zhenxing);

    General *weiwenzhugezhi = new General(this, "weiwenzhugezhi", "wu", 4);
    weiwenzhugezhi->addSkill(new Fuhai);

    General *lvkai = new General(this, "lvkai", "shu", 3);
    lvkai->addSkill(new Tunan);
    lvkai->addSkill(new Bijing);

    General *beimihu = new General(this, "beimihu", "qun", 3, false);
    beimihu->addSkill(new Zongkui);
    beimihu->addSkill(new Guju);
    beimihu->addSkill(new DetachEffectSkill("guju", QString(), "#guju"));
    related_skills.insertMulti("guju", "#guju-clear");
    beimihu->addSkill(new Baijia);
    beimihu->addSkill("canshii");

    General *zhangqiying = new General(this, "zhangqiying", "qun", 3, false);
    zhangqiying->addSkill(new Falu);
    zhangqiying->addSkill(new Zhenyi);
    zhangqiying->addSkill(new Dianhua);

    addMetaObject<QianxiniCard>();
    addMetaObject<TunanCard>();
    addMetaObject<FuhaiCard>();

    skills << new ZhenxingSelect << new Canshii;

}

ADD_PACKAGE(CompeteWorld)

