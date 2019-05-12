#include "yjcm2017.h"
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

ZhongjianCard::ZhongjianCard()
{
    will_throw = false;
    handling_method = Card::MethodNone;
}

bool ZhongjianCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select->getHp() < to_select->getHandcardNum() && to_select != Self;
}

void ZhongjianCard::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *source = effect.from;
    Room *room = source->getRoom();
    ServerPlayer *target = effect.to;
    room->showCard(source, getEffectiveId());
    int x = target->getHandcardNum() - target->getHp();
    if (x < 1) return;
    QList<int> cards = room->askForCardsChosen(source, target, "h", "zhongjian", x, x);
    room->showCard(target, cards);
    bool color = false, number = false;
    foreach (int id, cards) {
        const Card *card = Sanguosha->getCard(id);
        if (sameColorWith(card))
            color = true;
        if (card->getNumber() == getNumber())
            number = true;
    }
    if (color) {
        QStringList choices;
        choices << "draw";
        if (source->canDiscard(target, "he"))
            choices << "discard";
        if (room->askForChoice(source, "zhongjian", choices.join("+"), QVariant(), "@zhongjian-choose:" + target->objectName(), "draw+discard") == "draw")
            source->drawCards(1, "zhongjian");
        else {
            int to_throw = room->askForCardChosen(source, target, "he", "zhongjian", false, Card::MethodDiscard);
            room->throwCard(Sanguosha->getCard(to_throw), target, source);
        }
    }
    if (number)
        room->setPlayerFlag(source, "ZhongjianSamePoint");
    if (!color && !number && source->getMaxCards() > 0)
        room->setPlayerMark(source, "#xinxianying_maxcards", source->getMark("#xinxianying_maxcards")-1);
}

class Zhongjian : public OneCardViewAsSkill
{
public:
    Zhongjian() : OneCardViewAsSkill("zhongjian")
    {
        filter_pattern = ".|.|.|hand";
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return player->usedTimes("ZhongjianCard") < (player->hasFlag("ZhongjianSamePoint") ? 2 : 1);
    }

    virtual const Card *viewAs(const Card *originalCard) const
    {
        ZhongjianCard *zhongjian = new ZhongjianCard;
        zhongjian->addSubcard(originalCard->getId());
        return zhongjian;
    }
};

class Caishi : public PhaseChangeSkill
{
public:
    Caishi() : PhaseChangeSkill("caishi")
    {
        view_as_skill = new dummyVS;
    }

    virtual bool onPhaseChange(ServerPlayer *player) const
    {
        Room *room = player->getRoom();
        if (player->getPhase() == Player::Draw) {
            if (room->askForSkillInvoke(player, "skill_ask", "prompt:::"+objectName())) {
                QStringList choices;
                choices << "maxcards" << "cancel";
                if (player->isWounded())
                    choices << "recover";
                QString choice = room->askForChoice(player, objectName(), choices.join("+"), QVariant(), "@caishi-choose", "maxcards+recover+cancel");
                if (choice != "cancel") {
                    LogMessage log;
                    log.type = "#InvokeSkill";
                    log.from = player;
                    log.arg = objectName();
                    room->sendLog(log);

                    room->notifySkillInvoked(player, objectName());
                    player->broadcastSkillInvoke(objectName());
                    if (choice == "maxcards") {
                        room->addPlayerMark(player, "#xinxianying_maxcards");
                        room->setPlayerFlag(player, "CaishiOthers");
                    } else {
                        room->recover(player, RecoverStruct(player));
                        room->setPlayerFlag(player, "CaishiSelf");
                    }
                }

            }
        }
        return false;
    }
};

class CaishiProhibit : public ProhibitSkill
{
public:
    CaishiProhibit() : ProhibitSkill("#caishi-prohibit")
    {
    }

    bool isProhibited(const Player *from, const Player *to, const Card *card, const QList<const Player *> & /* = QList<const Player *>() */) const
    {
        if (card->isKindOf("SkillCard")) return false;
        return (from && from->hasFlag("CaishiOthers") && from != to) || (from && from->hasFlag("CaishiSelf") && from == to);
    }
};

class XinxianyingMaxCards : public MaxCardsSkill
{
public:
    XinxianyingMaxCards() : MaxCardsSkill("#xinxianying-maxcards")
    {
    }

    virtual int getExtra(const Player *target) const
    {
        return target->getMark("#xinxianying_maxcards");
    }
};





QingxianCard::QingxianCard()
{
}

bool QingxianCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return to_select != Self && targets.length() < subcardsLength();
}

bool QingxianCard::targetsFeasible(const QList<const Player *> &targets, const Player *) const
{
    return targets.length() == subcardsLength();
}

void QingxianCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    if (targets.length() == source->getHp())
        source->drawCards(1, "qingxian");

    foreach (ServerPlayer *p, targets) {
        if (source->isDead()) break;
        if (p->isAlive()) {
            int x1 = source->getEquips().length();
            int x2 = p->getEquips().length();
            if (x1 == x2)
                p->drawCards(1, "qingxian");
            else if (x1 < x2)
                room->loseHp(p);
            else if (x1 > x2)
                room->recover(p, RecoverStruct(source));
        }
    }
}

class Qingxian : public ViewAsSkill
{
public:
    Qingxian() : ViewAsSkill("qingxian")
    {
    }

    virtual bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
        return !Self->isJilei(to_select) && selected.length() < Self->getHp();
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.isEmpty()) return NULL;
        QingxianCard *card = new QingxianCard;
        foreach(const Card *c, cards)
            card->addSubcard(c);
        return card;
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("QingxianCard");
    }
};

CanyunCard::CanyunCard()
{
}

bool CanyunCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    QStringList canyun_targets = Self->property("canyun_targets").toString().split("+");
    return to_select != Self && targets.length() < subcardsLength() && !canyun_targets.contains(to_select->objectName());
}

bool CanyunCard::targetsFeasible(const QList<const Player *> &targets, const Player *) const
{
    return targets.length() == subcardsLength();
}

void CanyunCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    QSet<QString> canyun_targets = source->property("canyun_targets").toString().split("+").toSet();
    foreach (ServerPlayer *p, targets)
        canyun_targets.insert(p->objectName());
    room->setPlayerProperty(source, "canyun_targets", QStringList(canyun_targets.toList()).join("+"));

    if (targets.length() == source->getHp())
        source->drawCards(1, "canyun");

    foreach (ServerPlayer *p, targets) {
        if (source->isDead()) break;
        if (p->isAlive()) {
            int x1 = source->getEquips().length();
            int x2 = p->getEquips().length();
            if (x1 == x2)
                p->drawCards(1, "canyun");
            else if (x1 < x2)
                room->loseHp(p);
            else if (x1 > x2)
                room->recover(p, RecoverStruct(source));
        }
    }
}

class Canyun : public ViewAsSkill
{
public:
    Canyun() : ViewAsSkill("canyun")
    {
    }

    virtual bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
        return !Self->isJilei(to_select) && selected.length() < Self->getHp();
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.isEmpty()) return NULL;
        CanyunCard *card = new CanyunCard;
        foreach(const Card *c, cards)
            card->addSubcard(c);
        return card;
    }

    virtual bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("CanyunCard");
    }
};

class JuexiangDiscard : public OneCardViewAsSkill
{
public:
    JuexiangDiscard() : OneCardViewAsSkill("juexiang_discard")
    {
        response_pattern = "@@juexiang_discard";
        expand_pile = "#judgecard";
    }

    bool viewFilter(const Card *to_select) const
    {
        return to_select->getSuit() == Card::Club && !Self->isJilei(to_select);
    }

    const Card *viewAs(const Card *originalCard) const
    {
        return originalCard;
    }
};


class Juexiang : public TriggerSkill
{
public:
    Juexiang() : TriggerSkill("juexiang")
    {
        events << Death << EventPhaseStart;
        view_as_skill = new dummyVS;
    }

    virtual QStringList triggerable(TriggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const
    {
        if (player == NULL || !player->hasSkill(objectName())) return QStringList();
        DeathStruct death = data.value<DeathStruct>();
        if (death.who == player) {
            //if (death.damage && death.damage->from && death.damage->from->isAlive()) return QStringList("juexiang!");
            return QStringList("juexiang!");
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const
    {
        room->sendCompulsoryTriggerLog(player, objectName());
        player->broadcastSkillInvoke(objectName());

        DeathStruct death = data.value<DeathStruct>();

        if (death.damage && death.damage->from && death.damage->from->isAlive()) {
            death.damage->from->throwAllEquips();
            room->loseHp(death.damage->from);
        }

        ServerPlayer *target = room->askForPlayerChosen(player, room->getAlivePlayers(), objectName(), "juexiang-invoke", true);

        if (!target) return false;

        room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, player->objectName(), target->objectName());

        room->acquireSkill(target, "canyun");

        QList<int> judge_card;
        foreach(const Card *card, target->getJudgingArea()){
            judge_card << card->getEffectiveId();
        }

        if (!judge_card.isEmpty())
            room->notifyMoveToPile(target, judge_card, "judgecard", Player::PlaceTable, true, true);
        const Card *card = room->askForCard(target, "@@juexiang_discard", "@juexiang-discard", data, Card::MethodNone);
        if (!judge_card.isEmpty())
            room->notifyMoveToPile(target, judge_card, "judgecard", Player::PlaceTable, false, false);

        if (card) {
            room->throwCard(card, target);
            room->acquireSkill(target, "juexiang");
        }
        return false;
    }
};

YJCM2017Package::YJCM2017Package()
: Package("YJCM2017")
{
    General *xinxianying = new General(this, "xinxianying", "wei", 3, false);
    xinxianying->addSkill(new Zhongjian);
    xinxianying->addSkill(new Caishi);
    xinxianying->addSkill(new CaishiProhibit);
    related_skills.insertMulti("caishi", "#caishi-prohibit");

    General *jikang = new General(this, "jikang", "wei", 3);
    jikang->addSkill(new Qingxian);
    jikang->addSkill(new Juexiang);

    addMetaObject<ZhongjianCard>();
    addMetaObject<QingxianCard>();
    addMetaObject<CanyunCard>();

    skills << new XinxianyingMaxCards << new Canyun << new JuexiangDiscard;
}

ADD_PACKAGE(YJCM2017)
