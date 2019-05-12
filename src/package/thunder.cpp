#include "thunder.h"
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

class Wanglie : public TriggerSkill
{
public:
    Wanglie() : TriggerSkill("wanglie")
    {
        events << CardUsed;
    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card && (use.card->isKindOf("Slash") || use.card->isNDTrick()))
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        if (player->askForSkillInvoke(objectName(), data)) {
            player->broadcastSkillInvoke(objectName());
            QStringList fuji_tag = use.card->tag["Fuji_tag"].toStringList();
            fuji_tag << "_ALL_PLAYERS";
            use.card->setTag("Fuji_tag", fuji_tag);
            room->setPlayerCardLimitation(player, "use", ".", true);
        }
        return false;
    }
};

class WanglieTarget : public TargetModSkill
{
public:
    WanglieTarget() : TargetModSkill("#wanglie-target")
    {
        frequency = NotFrequent;
        pattern = "^SkillCard";
    }

    virtual int getDistanceLimit(const Player *from, const Card *, const Player *) const
    {
        if (from->hasSkill("wanglie") && !from->hasFlag("wanglieHadUseCard"))
            return 1000;
        else
            return 0;
    }
};

class Zuilun : public PhaseChangeSkill
{
public:
    Zuilun() : PhaseChangeSkill("zuilun")
    {

    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Finish;
    }

    virtual bool onPhaseChange(ServerPlayer *zhugezhan) const
    {
        Room *room = zhugezhan->getRoom();
        if (room->askForSkillInvoke(zhugezhan, objectName())) {
            zhugezhan->broadcastSkillInvoke(objectName());
            int x = 0;
            if (zhugezhan->getMark("damage_point_round") > 0) x++;
            if (zhugezhan->getMark("GlobalDiscardCount") == 0) x++;
            bool increase = true;
            QList<ServerPlayer *> players = room->getAlivePlayers();
            foreach(ServerPlayer *p, players) {
                if (p->getHandcardNum() < zhugezhan->getHandcardNum()) {
                    increase = false;
                    break;
                }
            }
            if (increase) x++;
            QList<int> card_ids = room->getNCards(3);
            LogMessage log;
            log.type = "$ViewDrawPile";
            log.from = zhugezhan;
            log.card_str = IntList2StringList(card_ids).join("+");
            room->sendLog(log, zhugezhan);

            AskForMoveCardsStruct result = room->askForMoveCards(zhugezhan, card_ids, QList<int>(), true, objectName(), "", x, x, false, false, QList<int>() << -1);
            for (int i = result.top.length() - 1; i >= 0; i--)
                room->getDrawPile().prepend(result.top.at(i));

            room->doBroadcastNotify(QSanProtocol::S_COMMAND_UPDATE_PILE, QVariant(room->getDrawPile().length()));

            LogMessage b;
            b.type = "$GuanxingTop";
            b.from = zhugezhan;
            b.card_str = IntList2StringList(result.top).join("+");
            room->doNotify(zhugezhan, QSanProtocol::S_COMMAND_LOG_SKILL, b.toVariant());


            if (!result.bottom.isEmpty()) {
                DummyCard *dummy = new DummyCard(result.bottom);
                room->obtainCard(zhugezhan, dummy);
                delete dummy;
            }
            if (x == 0) {
                QList<ServerPlayer *> targets = room->getOtherPlayers(zhugezhan);
                if (!targets.isEmpty()) {
                    ServerPlayer *target = room->askForPlayerChosen(zhugezhan, targets, objectName(), "@zuilun-choose");
                    if (target) {
                        room->loseHp(zhugezhan);
                        room->loseHp(target);
                    }
                }
            }
        }
        return false;
    }
};

class Fuyin : public TriggerSkill
{
public:
    Fuyin() : TriggerSkill("fuyin")
    {
        events << TargetConfirmed;
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        CardUseStruct use = data.value<CardUseStruct>();
        if ((use.card->isKindOf("Slash") || use.card->isKindOf("Duel")) &&
                use.from && use.from->isAlive() && use.from->getHandcardNum() >= player->getHandcardNum()) {
            return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());
        CardUseStruct use = data.value<CardUseStruct>();
        int index = use.index;
        QVariantList nullified_list = use.card->tag["Nullified_List"].toList();
        nullified_list[index] = true;
        use.card->setTag("Nullified_List", nullified_list);
        return false;
    }
};


class Liangyin : public TriggerSkill
{
public:
    Liangyin() : TriggerSkill("liangyin")
    {
        events << CardsMoveOneTime;
    }

    static bool canLiangyin(QVariant &data, bool event)
    {
        QVariantList move_datas = data.toList();
        foreach(QVariant move_data, move_datas) {
            CardsMoveOneTimeStruct move = move_data.value<CardsMoveOneTimeStruct>();
            if (move.to_place == Player::PlaceSpecial && event) {
                foreach (Player::Place place, move.from_places) {
                    if (place != Player::PlaceSpecial)
                        return true;
                }
            }
            if (move.to_place == Player::PlaceHand && !event && move.from_places.contains(Player::PlaceSpecial)) {
                return true;
            }
        }
        return false;
    }

    static QList<ServerPlayer *> getLiangyinTargets(ServerPlayer *player, QVariant &data, bool event)
    {
        QList<ServerPlayer *> targets;
        if (!canLiangyin(data, event)) return targets;
        QList<ServerPlayer *> all = player->getRoom()->getAlivePlayers();
        foreach (ServerPlayer *p, all) {
            if ((p->getHandcardNum() > player->getHandcardNum() && event) ||
                    (p->getHandcardNum() < player->getHandcardNum() && !event && !p->isNude()))
                targets << p;
        }
        return targets;
    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        if (TriggerSkill::triggerable(player) &&
                (!getLiangyinTargets(player, data, true).isEmpty() || !getLiangyinTargets(player, data, false).isEmpty()))
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        QList<ServerPlayer *> targets = getLiangyinTargets(player, data, true);
        if (!targets.isEmpty()) {
            ServerPlayer *target = room->askForPlayerChosen(player, targets, objectName(), "@liangyin1-invoke", true, true);
            if (target) {
                player->broadcastSkillInvoke(objectName());
                target->drawCards(1, objectName());
            }
        }
        targets = getLiangyinTargets(player, data, false);
        if (!targets.isEmpty()) {
            ServerPlayer *target = room->askForPlayerChosen(player, targets, objectName(), "@liangyin2-invoke", true, true);
            if (target) {
                player->broadcastSkillInvoke(objectName());
                room->askForDiscard(target, objectName(), 1, 1, false, true, "@liangyin-discard");
            }
        }
        return false;
    }
};

class KongshengUse : public OneCardViewAsSkill
{
public:
    KongshengUse() : OneCardViewAsSkill("kongsheng_use")
    {
        response_pattern = "@@kongsheng_use!";
        expand_pile = "#harp";
    }

    bool viewFilter(const Card *to_select) const
    {
        return Self->getPile(expand_pile).contains(to_select->getEffectiveId());
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        return originalCard;
    }
};

class Kongsheng : public PhaseChangeSkill
{
public:
    Kongsheng() : PhaseChangeSkill("kongsheng")
    {

    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *player, QVariant &, ServerPlayer* &) const
    {
        switch (player->getPhase()) {
        case Player::Start: {
            if (PhaseChangeSkill::triggerable(player) && ! player->isNude())
                return QStringList(objectName());
            break;
        }
        case Player::Finish: {
            if (!player->getPile("harp").isEmpty())
                return QStringList("kongsheng!");
            break;
        }
        default:
            break;
        }

        return QStringList();
    }

    virtual bool onPhaseChange(ServerPlayer *player) const
    {
        Room *room = player->getRoom();
        switch (player->getPhase()) {
        case Player::Start: {
            const Card *card = room->askForExchange(player, objectName(), 998, 1, true, "@kongsheng-put", true);
            if (card) {
                LogMessage log;
                log.type = "#InvokeSkill";
                log.from = player;
                log.arg = objectName();
                room->sendLog(log);
                room->notifySkillInvoked(player, objectName());
                player->broadcastSkillInvoke(objectName());
                player->addToPile("harp", card, true);
            }
            break;
        }
        case Player::Finish: {
            while (true) {
                QList<int> harps = player->getPile("harp"), equips;
                foreach (int id, harps) {
                    const Card *card = Sanguosha->getCard(id);
                    if (card->getTypeId() == Card::TypeEquip && card->isAvailable(player)) {
                        equips << id;
                    }
                }
                if (equips.isEmpty()) break;
                room->notifyMoveToPile(player, equips, "harp", Player::PlaceTable, true, true);
                const Card *to_use = room->askForCard(player, "@@kongsheng_use!", "@kongsheng-use", QVariant(), Card::MethodNone);
                room->notifyMoveToPile(player, equips, "harp", Player::PlaceTable, false, false);
                if (to_use == NULL)
                    to_use = Sanguosha->getCard(equips.first());

                room->useCard(CardUseStruct(to_use, player, player));
            }

            if (!player->getPile("harp").isEmpty()) {
                DummyCard *dummy = new DummyCard(player->getPile("harp"));
                CardMoveReason reason(CardMoveReason::S_REASON_EXCHANGE_FROM_PILE, player->objectName(), objectName(), QString());
                room->obtainCard(player, dummy, reason, true);
                delete dummy;
            }
            break;
        }
        default:
            break;
        }
        return false;
    }
};







class Qianjie : public ProhibitSkill
{
public:
    Qianjie() : ProhibitSkill("qianjie")
    {
    }

    virtual bool isProhibited(const Player *, const Player *to, const Card *card, const QList<const Player *> &) const
    {
        return to->hasSkill(this) && card->isKindOf("DelayedTrick");
    }
};

JueyanCard::JueyanCard()
{
    target_fixed = true;
    m_skillName = "jueyan";
}

void JueyanCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const
{
    QString area_name = user_string;
    if (area_name == "weapon") {
        source->sealAreas("Weapon");
        room->setPlayerFlag(source, "JueyanWeapon");
    } else if (area_name == "armor") {
        source->sealAreas("Armor");
        source->drawCards(3, "jueyan");
        room->addPlayerMark(source, "Global_MaxcardsIncrease", 3);
    } else if (area_name == "horse") {
        source->sealAreas("DefensiveHorse+OffensiveHorse");
        room->setPlayerFlag(source, "JueyanHorse");
    } else if (area_name == "treasure") {
        source->sealAreas("Treasure");
        room->setPlayerFlag(source, "JueyanTreasureSealed");
        room->acquireSkill(source, "jizhi");
    }
}

class JueyanViewAsSkill : public ZeroCardViewAsSkill
{
public:
    JueyanViewAsSkill() : ZeroCardViewAsSkill("jueyan")
    {

    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("JueyanCard");
    }

    virtual const Card *viewAs() const
    {
        return new JueyanCard;
    }
};

class Jueyan : public TriggerSkill
{
public:
    Jueyan() : TriggerSkill("jueyan")
    {
        events << EventPhaseChanging;
        view_as_skill = new JueyanViewAsSkill;
    }

    bool triggerable(const ServerPlayer *) const
    {
        return false;
    }

    virtual void record(TriggerEvent , Room *room, ServerPlayer *source, QVariant &data) const
    {
        if (data.value<PhaseChangeStruct>().from == Player::Play) {
            if (source->hasFlag("JueyanTreasureSealed"))
                room->detachSkillFromPlayer(source, "jizhi");

            room->setPlayerFlag(source, "-JueyanWeapon");
            room->setPlayerFlag(source, "-JueyanHorse");
            room->setPlayerFlag(source, "-JueyanTreasure");
        }
    }

    QString getSelectBox() const
    {
        return "weapon+armor+horse+treasure";
    }

    bool buttonEnabled(const QString &button_name, const QList<const Card *> &, const QList<const Player *> &) const
    {
        if (button_name.isEmpty())
            return true;
        if (button_name == "weapon")
            return Self->getMark("WeaponSealed") == 0;
        else if (button_name == "armor")
            return Self->getMark("ArmorSealed") == 0;
        else if (button_name == "horse")
            return Self->getMark("DefensiveHorseSealed") == 0 && Self->getMark("OffensiveHorseSealed") == 0;
        else if (button_name == "treasure")
            return Self->getMark("TreasureSealed") == 0;
        return false;
    }
};


class JueyanTargetMod : public TargetModSkill
{
public:
    JueyanTargetMod() : TargetModSkill("#jueyan-target")
    {
        frequency = NotFrequent;
        pattern = "^SkillCard";
    }

    virtual int getResidueNum(const Player *from, const Card *card, const Player *) const
    {
        if (card->isKindOf("Slash") && from->hasFlag("JueyanWeapon"))
            return 3;
        else
            return 0;
    }

    virtual int getDistanceLimit(const Player *from, const Card *, const Player *) const
    {
        if (from->hasFlag("JueyanHorse"))
            return 1000;
        else
            return 0;
    }
};

class Poshi : public PhaseChangeSkill
{
public:
    Poshi() : PhaseChangeSkill("poshi")
    {
        frequency = Wake;
    }

    virtual bool triggerable(const ServerPlayer *target) const
    {
        if (PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Start && target->getMark(objectName()) == 0) {
            if (target->getHp()==1) return true;
            QStringList area_names;
            area_names << "WeaponSealed" << "ArmorSealed" << "DefensiveHorseSealed" << "OffensiveHorseSealed" << "TreasureSealed";
            foreach (QString area_name, area_names) {
                if (target->getMark(area_name) == 0)
                    return false;
            }
            return true;
        }
        return false;
    }

    virtual bool onPhaseChange(ServerPlayer *sunce) const
    {
        Room *room = sunce->getRoom();
        room->sendCompulsoryTriggerLog(sunce, objectName());
        sunce->broadcastSkillInvoke(objectName());

        room->setPlayerMark(sunce, objectName(), 1);
        if (room->changeMaxHpForAwakenSkill(sunce) && sunce->getMark(objectName()) == 1) {
            int x = sunce->getMaxHp() - sunce->getHandcardNum();
            if (x > 0)
                sunce->drawCards(x, objectName());
            room->handleAcquireDetachSkills(sunce, "-jueyan|huairou");
        }
        return false;
    }
};

HuairouCard::HuairouCard()
{
    will_throw = false;
    can_recast = true;
    handling_method = Card::MethodRecast;
    target_fixed = true;
}

void HuairouCard::onUse(Room *room, const CardUseStruct &card_use) const
{
    ServerPlayer *xiahou = card_use.from;

    LogMessage log;
    log.type = "$RecastCardWithSkill";
    log.from = xiahou;
    log.arg = getSkillName();
    log.card_str = QString::number(card_use.card->getSubcards().first());
    room->sendLog(log);
    xiahou->broadcastSkillInvoke(getSkillName());
    room->notifySkillInvoked(xiahou, getSkillName());

    CardMoveReason reason(CardMoveReason::S_REASON_RECAST, xiahou->objectName());
    reason.m_skillName = getSkillName();
    room->moveCardTo(this, xiahou, NULL, Player::DiscardPile, reason);

    xiahou->drawCards(1, "recast");
}

class Huairou : public OneCardViewAsSkill
{
public:
    Huairou() : OneCardViewAsSkill("huairou")
    {
        filter_pattern = "EquipCard";
    }

    const Card *viewAs(const Card *originalCard) const
    {
        if (Self->isCardLimited(originalCard, Card::MethodRecast))
            return NULL;

        HuairouCard *recast = new HuairouCard;
        recast->addSubcard(originalCard);
        return recast;
    }

    bool isEnabledAtPlay(const Player *) const
    {
        return true;
    }
};

class Zhengu : public TriggerSkill
{
public:
    Zhengu() : TriggerSkill("zhengu")
    {
        events << EventPhaseStart << EventPhaseChanging;
    }

    virtual void record(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &) const
    {
        if (triggerEvent == EventPhaseStart && player->getPhase() == Player::NotActive)
            player->tag.remove("ZhenguSource");
    }

    virtual TriggerList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        TriggerList skill_list;
        if (triggerEvent == EventPhaseStart && player->getPhase() == Player::Finish && TriggerSkill::triggerable(player))
            skill_list.insert(player, QStringList(objectName()));
        else if (triggerEvent == EventPhaseChanging && data.value<PhaseChangeStruct>().to == Player::NotActive) {
            QStringList haozhao_list = player->tag["ZhenguSource"].toStringList();
            QList<ServerPlayer *> haozhaos = room->getAlivePlayers();
            foreach (ServerPlayer *haozhao, haozhaos) {
                if (haozhao_list.contains(haozhao->objectName()) && haozhao->getHandcardNum() != player->getHandcardNum())
                    skill_list.insert(haozhao, QStringList("zhengu!"));
            }
        }
        return skill_list;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer *ask_who) const
    {
        ServerPlayer *target = NULL;
        if (triggerEvent == EventPhaseStart) {
            target = room->askForPlayerChosen(player, room->getOtherPlayers(player), objectName(), "@zhengu-invoke", true, true);
            if (target) {
                player->broadcastSkillInvoke(objectName());
                QStringList target_list = target->tag["ZhenguSource"].toStringList();
                target_list.append(player->objectName());
                target->tag["ZhenguSource"] = target_list;
            }
        } else
            target = player;
        if (target) {
            int x = ask_who->getHandcardNum() - target->getHandcardNum();
            if (x > 0)
                target->drawCards(x, objectName());
            else if (x < 0)
                room->askForDiscard(target, objectName(), -x,-x, false, false, "@zhengu-discard");
        }
        return false;
    }
};

class Zhengrong : public TriggerSkill
{
public:
    Zhengrong() : TriggerSkill("zhengrong")
    {
        events << Damage;
    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        DamageStruct damage = data.value<DamageStruct>();
        if (damage.to && !damage.to->isNude() && damage.to->getHandcardNum() > player->getHandcardNum() && !damage.to->hasFlag("Global_DebutFlag"))
            return QStringList(objectName());
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        ServerPlayer *target = damage.to;
        if (room->askForSkillInvoke(player, objectName(), QVariant::fromValue(target))) {
            player->broadcastSkillInvoke(objectName());
            room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), target->objectName());

            int card_id = room->askForCardChosen(player, target, "he", objectName(), false, Card::MethodNone);
            QList<ServerPlayer *> p_list;
            p_list << player;
            player->addToPile("honour", card_id, false, p_list);

        }
        return false;
    }
};


HongjuCard::HongjuCard()
{
    will_throw = false;
    handling_method = Card::MethodNone;
    target_fixed = true;
}

void HongjuCard::onUse(Room *room, const CardUseStruct &card_use) const
{
    QList<int> pile = card_use.from->getPile("honour");
    QList<int> subCards = card_use.card->getSubcards();
    QList<int> to_handcard;
    QList<int> to_pile;
    foreach (int id, (subCards + pile).toSet()) {
        if (!subCards.contains(id))
            to_handcard << id;
        else if (!pile.contains(id))
            to_pile << id;
    }

    Q_ASSERT(to_handcard.length() == to_pile.length());

    if (to_pile.length() == 0 || to_handcard.length() != to_pile.length()) return;

    card_use.from->addToPile("honour", to_pile, false);

    DummyCard *to_handcard_x = new DummyCard(to_handcard);
    CardMoveReason reason(CardMoveReason::S_REASON_EXCHANGE_FROM_PILE, card_use.from->objectName());
    room->obtainCard(card_use.from, to_handcard_x, reason, false);
    to_handcard_x->deleteLater();
}

class HongjuChoose : public ViewAsSkill
{
public:
    HongjuChoose() : ViewAsSkill("hongju_choose")
    {
        response_pattern = "@@hongju_choose";
        expand_pile = "honour";
    }

    virtual bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
        if (selected.length() < Self->getPile("honour").length())
            return !to_select->isEquipped();

        return false;
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.length() == Self->getPile("honour").length()) {
            HongjuCard *c = new HongjuCard;
            c->addSubcards(cards);
            return c;
        }

        return NULL;
    }
};

class Hongju : public PhaseChangeSkill
{
public:
    Hongju() : PhaseChangeSkill("hongju")
    {
        frequency = Wake;
    }

    virtual QStringList triggerable(TriggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer* &) const
    {
        if (TriggerSkill::triggerable(player) && player->getPhase() == Player::Start && player->getPile("honour").length() > 2 &&
                player->getMark(objectName()) == 0 && Sanguosha->getPlayerCount(room->getMode()) > room->alivePlayerCount()) {
            return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool onPhaseChange(ServerPlayer *player) const
    {
        Room *room = player->getRoom();
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());

        room->setPlayerMark(player, objectName(), 1);

        QList<int> stars = player->getPile("honour");
        //QList<int> handcards = player->handCards();


        room->askForUseCard(player, "@@hongju_choose", "@hongju-exchange:::"+QString::number(stars.length()));

        if (room->changeMaxHpForAwakenSkill(player) && player->getMark(objectName()) == 1)
            room->acquireSkill(player, "qingce");
        return false;
    }
};

QingceCard::QingceCard()
{
}

bool QingceCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && Self->canDiscard(to_select, "ej");
}

void QingceCard::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.from->getRoom();
    if (effect.from->canDiscard(effect.to, "ej")) {
        int to_throw = room->askForCardChosen(effect.from, effect.to, "ej", "qingce", false, Card::MethodDiscard);
        room->throwCard(to_throw, effect.to, effect.from);
    }
}

class Qingce : public OneCardViewAsSkill
{
public:
    Qingce() : OneCardViewAsSkill("qingce")
    {
        filter_pattern = ".|.|.|honour";
        expand_pile = "honour";
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->getPile("honour").isEmpty();
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        QingceCard *qc = new QingceCard;
        qc->addSubcard(originalCard);
        return qc;
    }
};

class Yongsi : public TriggerSkill
{
public:
    Yongsi() : TriggerSkill("yongsi")
    {
        events << DrawNCards << EventPhaseEnd;
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &, ServerPlayer * &) const
    {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        if (triggerEvent == DrawNCards)
            return QStringList(objectName());
        else if (triggerEvent == EventPhaseEnd && player->getPhase() == Player::Play) {
            int x = player->getMark("damage_point_play_phase");
            if ((x == 0 && player->getHandcardNum() < player->getHp()) || x > 1)
                return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *yuanshu, QVariant &data, ServerPlayer *) const
    {
        if (triggerEvent == DrawNCards) {
            QSet<QString> kingdom_set;
            foreach(ServerPlayer *p, room->getAlivePlayers())
                kingdom_set << p->getKingdom();

            room->sendCompulsoryTriggerLog(yuanshu, objectName());
            yuanshu->broadcastSkillInvoke(objectName());
            data = kingdom_set.size();
        } else if (triggerEvent == EventPhaseEnd) {
            int x = yuanshu->getMark("damage_point_play_phase");
            if (x==0 && yuanshu->getHandcardNum() < yuanshu->getHp()) {
                room->sendCompulsoryTriggerLog(yuanshu, objectName());
                yuanshu->broadcastSkillInvoke(objectName());
                yuanshu->drawCards(yuanshu->getHp()- yuanshu->getHandcardNum(), objectName());
            }else if (x>1) {
                room->sendCompulsoryTriggerLog(yuanshu, objectName());
                yuanshu->broadcastSkillInvoke(objectName());
                room->setPlayerFlag(yuanshu, "yongsiMaxcards");
            }
        }
        return false;
    }
};

class YongsiMaxCards : public MaxCardsSkill
{
public:
    YongsiMaxCards() : MaxCardsSkill("#yongsi-maxcards")
    {

    }

    int getFixed(const Player *target) const
    {
        if (target->hasFlag("yongsiMaxcards"))
            return target->getLostHp();

        return -1;
    }
};

class Weidi : public PhaseChangeSkill
{
public:
    Weidi() : PhaseChangeSkill("weidi$")
    {

    }

    virtual QStringList triggerable(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer * &) const
    {
        if (player != NULL && player->isAlive() && player->hasLordSkill("weidi") && player->getPhase() == Player::Discard
                && player->getHandcardNum() > player->getMaxCards() && !room->getLieges("qun", player).isEmpty()) {
            return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool onPhaseChange(ServerPlayer *player) const
    {
        Room *room = player->getRoom();
        int x = player->getHandcardNum() - player->getMaxCards();
        QList<int> handcards = player->handCards();
        QList<ServerPlayer *> lieges = room->getLieges("qun", player);
        CardMoveReason reason(CardMoveReason::S_REASON_GIVE, player->objectName(), objectName(), QString());
        room->askForRende(player, handcards, objectName(), false, false, true, x, 0, lieges, reason, "@weidi-give", QString(), true);
        return false;
    }
};

XiongluanCard::XiongluanCard()
{
}

void XiongluanCard::extraCost(Room *room, const CardUseStruct &card_use) const
{
    room->removePlayerMark(card_use.from, "@disorder");
    card_use.from->sealAreas("Weapon+Armor+DefensiveHorse+OffensiveHorse+Treasure+Judge");

}

void XiongluanCard::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *source = effect.from;
    Room *room = source->getRoom();
    ServerPlayer *target = effect.to;

    room->addPlayerTip(source, "#xiongluan");
    room->addPlayerTip(target, "#xiongluan");

    room->setPlayerCardLimitation(target, "use,response", ".|.|.|hand", true);

    QStringList assignee_list = source->property("xiongluan_targets").toString().split("+");
    assignee_list << target->objectName();
    room->setPlayerProperty(source, "xiongluan_targets", assignee_list.join("+"));
}

class XiongluanViewAsSkill : public ZeroCardViewAsSkill
{
public:
    XiongluanViewAsSkill() : ZeroCardViewAsSkill("xiongluan")
    {
        frequency = Limited;
        limit_mark = "@disorder";
    }

    virtual const Card *viewAs() const
    {
        return new XiongluanCard;
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return player->getMark("@disorder") > 0;
    }

};

class Xiongluan : public TriggerSkill
{
public:
    Xiongluan() : TriggerSkill("xiongluan")
    {
        events << Damage << EventPhaseChanging;
        frequency = Limited;
        limit_mark = "@disorder";
        view_as_skill = new XiongluanViewAsSkill;
    }

    virtual void record(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (data.value<PhaseChangeStruct>().from == Player::Play) {
            QStringList assignee_list = player->property("xiongluan_targets").toString().split("+");
            QList<ServerPlayer *> all_players = room->getAlivePlayers();
            foreach (ServerPlayer *p, all_players) {
                if (assignee_list.contains(p->objectName())) {
                    room->removePlayerTip(p, "#xiongluan");
                    room->removePlayerCardLimitation(p, "use,response", ".|.|.|hand");
                }
            }
            room->removePlayerTip(player, "#xiongluan");
            room->setPlayerProperty(player, "xiongluan_targets", QVariant());

        }
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *, QVariant &, ServerPlayer* &) const
    {
        return QStringList();
    }
};



class XiongluanTargetMod : public TargetModSkill
{
public:
    XiongluanTargetMod() : TargetModSkill("#xiongluan-target")
    {
        pattern = "^SkillCard";
    }

    int getResidueNum(const Player *from, const Card *, const Player *to) const
    {
        QStringList assignee_list = from->property("xiongluan_targets").toString().split("+");
        if (to && assignee_list.contains(to->objectName()))
            return 10000;
        return 0;
    }

    int getDistanceLimit(const Player *from, const Card *, const Player *to) const
    {
        QStringList assignee_list = from->property("xiongluan_targets").toString().split("+");
        if (to && assignee_list.contains(to->objectName()))
            return 10000;
        return 0;
    }
};

CongjianCard::CongjianCard()
{
    will_throw = false;
}

bool CongjianCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    QStringList tos = Self->property("congjian_targets").toString().split("+");
    return targets.isEmpty() && to_select != Self && tos.contains(to_select->objectName());
}

void CongjianCard::onEffect(const CardEffectStruct &effect) const
{
    effect.to->setFlags("CongjianTarget");
}

class CongjianViewAsSkill : public OneCardViewAsSkill
{
public:
    CongjianViewAsSkill() : OneCardViewAsSkill("congjian")
    {
        filter_pattern = ".";
        response_pattern = "@@congjian";
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        CongjianCard *congjian = new CongjianCard;
        congjian->addSubcard(originalCard);
        return congjian;
    }
};

class Congjian : public TriggerSkill
{
public:
    Congjian() : TriggerSkill("congjian")
    {
        events << TargetConfirmed;
        view_as_skill = new CongjianViewAsSkill;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer * &) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        if (TriggerSkill::triggerable(player) && use.card->isNDTrick() && !player->isNude()) {
            foreach (ServerPlayer *p, use.to) {
                if (p != player)
                    return QStringList(objectName());
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        QStringList tos;
        foreach(ServerPlayer *t, use.to)
            tos.append(t->objectName());
        room->setPlayerProperty(player, "congjian_targets", tos.join("+"));
        const Card *to_give = room->askForUseCard(player, "@@congjian", "@congjian-give");
        room->setPlayerProperty(player, "congjian_targets", QString());
        ServerPlayer *target = NULL;
        foreach (ServerPlayer *p, room->getAlivePlayers()) {
            if (p->hasFlag("CongjianTarget")) {
                p->setFlags("-CongjianTarget");
                target = p;
                break;
            }
        }

        if (to_give && target) {
            bool is_equip = Sanguosha->getCard(to_give->getEffectiveId())->getTypeId() == Card::TypeEquip;

            CardMoveReason reason(CardMoveReason::S_REASON_GIVE, player->objectName(), target->objectName(), objectName(), QString());
            room->obtainCard(target, to_give, reason);

            player->drawCards(is_equip?2:1, objectName());
        }
    }
};

ThunderPackage::ThunderPackage()
    : Package("thunder")
{
    General *chendao = new General(this, "chendao", "shu");
    chendao->addSkill(new Wanglie);
    chendao->addSkill(new WanglieTarget);
    related_skills.insertMulti("wanglie", "#wanglie-target");

    General *zhugezhan = new General(this, "zhugezhan", "shu", 3);
    zhugezhan->addSkill(new Zuilun);
    zhugezhan->addSkill(new Fuyin);

    General *zhoufei = new General(this, "zhoufei", "wu", 3, false);
    zhoufei->addSkill(new Liangyin);
    zhoufei->addSkill(new Kongsheng);

    General *lukang = new General(this, "lukang", "wu");
    lukang->addSkill(new Qianjie); //only qianjie2; qianjie1 in Room::setPlayerProperty(); qianjie3 in Player::canPindian()
    lukang->addSkill(new Jueyan);
    lukang->addSkill(new JueyanTargetMod);
    lukang->addSkill(new Poshi);
    lukang->addRelateSkill("huairou");
    lukang->addRelateSkill("jizhi");
    related_skills.insertMulti("jueyan", "#jueyan-target");

    General *haozhao = new General(this, "haozhao", "wei");
    haozhao->addSkill(new Zhengu);

    General *guanqiujian = new General(this, "guanqiujian", "wei");
    guanqiujian->addSkill(new Zhengrong);
    guanqiujian->addSkill(new Hongju);
    guanqiujian->addRelateSkill("qingce");

    General *yuanshu = new General(this, "yuanshu$", "qun");
    yuanshu->addSkill(new Yongsi);
    yuanshu->addSkill(new YongsiMaxCards);
    yuanshu->addSkill(new Weidi);
    related_skills.insertMulti("yongsi", "#yongsi-maxcards");

    General *zhangxiu = new General(this, "zhangxiu", "qun");
    zhangxiu->addSkill(new Xiongluan);
    zhangxiu->addSkill(new XiongluanTargetMod);
    zhangxiu->addSkill(new Congjian);
    related_skills.insertMulti("xiongluan", "#xiongluan-target");

    addMetaObject<JueyanCard>();
    addMetaObject<HuairouCard>();
    addMetaObject<HongjuCard>();
    addMetaObject<QingceCard>();
    addMetaObject<XiongluanCard>();
    addMetaObject<CongjianCard>();

    skills << new KongshengUse << new Huairou << new HongjuChoose << new Qingce;
}

ADD_PACKAGE(Thunder)


