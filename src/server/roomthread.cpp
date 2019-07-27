#include "roomthread.h"
#include "room.h"
#include "engine.h"
#include "gamerule.h"
#include "scenario.h"
#include "ai.h"
#include "settings.h"
#include "standard.h"
#include "json.h"
#include "structs.h"

#include <QTime>

#ifdef QSAN_UI_LIBRARY_AVAILABLE
#pragma message WARN("UI elements detected in server side!!!")
#endif

using namespace QSanProtocol;
LogMessage::LogMessage()
    : from(NULL)
{
}

QVariant LogMessage::toVariant() const
{
    QStringList tos;
    foreach(ServerPlayer *player, to)
        if (player != NULL) tos << player->objectName();

    QStringList log;
    log << type << (from ? from->objectName() : "") << tos.join("+") << card_str << arg << arg2;
    QVariant json_log = JsonUtils::toJsonArray(log);
    return json_log;
}

DamageStruct::DamageStruct()
    : from(NULL), to(NULL), card(NULL), damage(1), nature(Normal), chain(false),
    transfer(false), by_user(true), reason(QString())
{
}

DamageStruct::DamageStruct(const Card *card, ServerPlayer *from, ServerPlayer *to, int damage, DamageStruct::Nature nature)
    : chain(false), transfer(false), by_user(true), reason(QString()), prevented(false)
{
    this->card = card;
    this->from = from;
    this->to = to;
    this->damage = damage;
    this->nature = nature;
}

DamageStruct::DamageStruct(const QString &reason, ServerPlayer *from, ServerPlayer *to, int damage, DamageStruct::Nature nature)
    : card(NULL), chain(false), transfer(false), by_user(true), prevented(false)
{
    this->from = from;
    this->to = to;
    this->damage = damage;
    this->nature = nature;
    this->reason = reason;
}

QString DamageStruct::getReason() const
{
    if (reason != QString())
        return reason;
    else if (card)
        return card->objectName();
    return QString();
}

CardEffectStruct::CardEffectStruct()
    : card(NULL), to_card(NULL), from(NULL), to(NULL), multiple(false), nullified(false)
{
}

SlashEffectStruct::SlashEffectStruct()
    : jink_num(1), slash(NULL), jink(NULL), from(NULL), to(NULL), drank(0), cardinality(1), nature(DamageStruct::Normal), nullified(false), flags(QStringList())
{
}

DyingStruct::DyingStruct()
    : who(NULL), damage(NULL)
{
}

DeathStruct::DeathStruct()
    : who(NULL), damage(NULL)
{
}

RecoverStruct::RecoverStruct(ServerPlayer *who, const Card *card, int recover)
    : recover(recover), who(who), card(card)
{
}

PindianStruct::PindianStruct()
    : from(NULL), to(NULL), from_card(NULL), to_card(NULL), success(false)
{
}

bool PindianStruct::isSuccess() const
{
    return success;
}

JudgeStruct::JudgeStruct()
    : who(NULL), card(NULL), pattern("."), good(true), time_consuming(false),
    negative(false), play_animation(true), retrial_by_response(NULL),
    _m_result(TRIAL_RESULT_UNKNOWN)
{
}

bool JudgeStruct::isEffected() const
{
    return negative ? isBad() : isGood();
}

void JudgeStruct::updateResult()
{
    bool effected = (good == ExpPattern(pattern).match(who, card));
    if (effected)
        _m_result = TRIAL_RESULT_GOOD;
    else
        _m_result = TRIAL_RESULT_BAD;
}

bool JudgeStruct::isGood() const
{
    Q_ASSERT(_m_result != TRIAL_RESULT_UNKNOWN);
    return _m_result == TRIAL_RESULT_GOOD;
}

bool JudgeStruct::isBad() const
{
    return !isGood();
}

bool JudgeStruct::isGood(const Card *card) const
{
    Q_ASSERT(card);
    return (good == ExpPattern(pattern).match(who, card));
}

PhaseChangeStruct::PhaseChangeStruct()
    : from(Player::NotActive), to(Player::NotActive)
{
}

CardUseStruct::CardUseStruct()
    : card(NULL), to_card(NULL), from(NULL), m_isOwnerUse(true), m_addHistory(true), nullified_list(QStringList())
{
}

CardUseStruct::CardUseStruct(const Card *card, ServerPlayer *from, QList<ServerPlayer *> to, bool isOwnerUse)
{
    this->card = card;
    this->from = from;
    this->to = to;
    this->m_isOwnerUse = isOwnerUse;
    this->m_addHistory = true;
}

CardUseStruct::CardUseStruct(const Card *card, ServerPlayer *from, ServerPlayer *target, bool isOwnerUse)
{
    this->card = card;
    this->from = from;
    Q_ASSERT(target != NULL);
    this->to << target;
    this->m_isOwnerUse = isOwnerUse;
    this->m_addHistory = true;
}

bool CardUseStruct::isValid(const QString &pattern) const
{
    Q_UNUSED(pattern)
        return card != NULL;
    /*if (card == NULL) return false;
    if (!card->getSkillName().isEmpty()) {
    bool validSkill = false;
    QString skillName = card->getSkillName();
    QSet<const Skill *> skills = from->getVisibleSkills();
    for (int i = 0; i < 4; i++) {
    const EquipCard *equip = from->getEquip(i);
    if (equip == NULL) continue;
    const Skill *skill = Sanguosha->getSkill(equip);
    if (skill)
    skills.insert(skill);
    }
    foreach (const Skill *skill, skills) {
    if (skill->objectName() != skillName) continue;
    const ViewAsSkill *vsSkill = ViewAsSkill::parseViewAsSkill(skill);
    if (vsSkill) {
    if (!vsSkill->isAvailable(from, m_reason, pattern))
    return false;
    else {
    validSkill = true;
    break;
    }
    } else if (skill->getFrequency() == Skill::Wake) {
    bool valid = (from->getMark(skill->objectName()) > 0);
    if (!valid)
    return false;
    else
    validSkill = true;
    } else
    return false;
    }
    if (!validSkill) return false;
    }
    if (card->targetFixed())
    return true;
    else {
    QList<const Player *> targets;
    foreach (const ServerPlayer *player, to)
    targets.push_back(player);
    return card->targetsFeasible(targets, from);
    }*/
}

bool CardUseStruct::tryParse(const QVariant &usage, Room *room)
{
    JsonArray use = usage.value<JsonArray>();
    if (use.size() < 2 || !JsonUtils::isString(use[0]) || !use[1].canConvert<JsonArray>())
        return false;

    card = Card::Parse(use[0].toString());
    JsonArray targets = use[1].value<JsonArray>();

    foreach(const QVariant &target, targets)
    {
        if (!JsonUtils::isString(target)) return false;
        this->to << room->findChild<ServerPlayer *>(target.toString());
    }
    return true;
}

void CardUseStruct::parse(const QString &str, Room *room)
{
    QStringList words = str.split("->", QString::KeepEmptyParts);
    Q_ASSERT(words.length() == 1 || words.length() == 2);

    QString card_str = words.at(0);
    QString target_str = ".";

    if (words.length() == 2 && !words.at(1).isEmpty())
        target_str = words.at(1);

    card = Card::Parse(card_str);

    if (target_str != ".") {
        QStringList target_names = target_str.split("+");
        foreach(const QString &target_name, target_names)
            to << room->findChild<ServerPlayer *>(target_name);
    }
}

MarkStruct::MarkStruct()
    : who(NULL), name(QString()), count(NULL), gain(NULL)
{

}

TurnStruct::TurnStruct()
    : who(NULL), name(QString())
{

}

QString EventTriplet::toString() const
{
    return QString("event[%1], room[%2], target = %3[%4]\n")
        .arg(_m_event)
        .arg(_m_room->getId())
        .arg(_m_target ? _m_target->objectName() : "NULL")
        .arg(_m_target ? _m_target->getGeneralName() : QString());
}

RoomThread::RoomThread(Room *room)
    : room(room)
{

}

void RoomThread::addPlayerSkills(ServerPlayer *player, bool invoke_game_start)
{
    QVariant void_data;
    bool invoke_verify = false;

    foreach (const TriggerSkill *skill, player->getTriggerSkills()) {
        addTriggerSkill(skill);

        if (invoke_game_start && skill->getTriggerEvents().contains(GameStart))
            invoke_verify = true;
    }

    //We should make someone trigger a whole GameStart event instead of trigger a skill only.
    if (invoke_verify)
        trigger(GameStart, room, player, void_data);
}

void RoomThread::constructTriggerTable()
{
    foreach(ServerPlayer *player, room->getPlayers())
        addPlayerSkills(player, true);
}

ServerPlayer *RoomThread::find3v3Next(QList<ServerPlayer *> &first, QList<ServerPlayer *> &second)
{
    bool all_actioned = true;
    foreach (ServerPlayer *player, room->m_alivePlayers) {
        if (!player->getMark("actionedM")) {
            all_actioned = false;
            break;
        }
    }

    if (all_actioned) {
        foreach (ServerPlayer *player, room->m_alivePlayers) {
            room->setPlayerFlag(player, "-actioned");
            room->setPlayerMark(player, "actionedM", 0);
            trigger(ActionedReset, room, player);
        }

        qSwap(first, second);
        QList<ServerPlayer *> first_alive;
        foreach (ServerPlayer *p, first) {
            if (p->isAlive())
                first_alive << p;
        }
        return room->askForPlayerChosen(first.first(), first_alive, "3v3-action", "@3v3-action");
    }

    ServerPlayer *current = room->getCurrent();
    if (current != first.first()) {
        ServerPlayer *another = NULL;
        if (current == first.last())
            another = first.at(1);
        else
            another = first.last();
        if (!another->getMark("actionedM") && another->isAlive())
            return another;
    }

    QList<ServerPlayer *> targets;
    do {
        targets.clear();
        qSwap(first, second);
        foreach (ServerPlayer *player, first) {
            if (!player->getMark("actionedM") && player->isAlive())
                targets << player;
        }
    } while (targets.isEmpty());

    return room->askForPlayerChosen(first.first(), targets, "3v3-action", "@3v3-action");
}

void RoomThread::run3v3(QList<ServerPlayer *> &first, QList<ServerPlayer *> &second, GameRule *game_rule, ServerPlayer *current)
{
    try {
        forever{
            room->setCurrent(current);
            trigger(TurnStart, room, room->getCurrent());
            room->setPlayerFlag(current, "actioned");
            room->setPlayerMark(current, "actionedM", 1);
            current = find3v3Next(first, second);
        }
    }
    catch (TriggerEvent triggerEvent) {
        if (triggerEvent == TurnBroken)
            _handleTurnBroken3v3(first, second, game_rule);
        else
            throw triggerEvent;
    }
}

void RoomThread::_handleTurnBroken3v3(QList<ServerPlayer *> &first, QList<ServerPlayer *> &second, GameRule *game_rule)
{
    try {
        ServerPlayer *player = room->getCurrent();
        trigger(TurnBroken, room, player);
        if (player->getPhase() != Player::NotActive) {
            QVariant v;
            game_rule->trigger(EventPhaseEnd, room, player, v);
            player->changePhase(player->getPhase(), Player::NotActive);
        }
        if (!player->hasFlag("actioned"))
            room->setPlayerFlag(player, "actioned");
        if (!player->getMark("actionedM"))
            room->setPlayerMark(player, "actionedM", 1);

        ServerPlayer *next = find3v3Next(first, second);
        run3v3(first, second, game_rule, next);
    }
    catch (TriggerEvent triggerEvent) {
        if (triggerEvent == TurnBroken) {
            _handleTurnBroken3v3(first, second, game_rule);
        } else {
            throw triggerEvent;
        }
    }
}

ServerPlayer *RoomThread::findHulaoPassNext(ServerPlayer *shenlvbu, QList<ServerPlayer *> league, int stage)
{
    ServerPlayer *current = room->getCurrent();
    if (stage == 1) {
        if (current == shenlvbu) {
            foreach (ServerPlayer *p, league) {
                if (p->isAlive() && !p->getMark("actionedM"))
                    return p;
            }
            foreach (ServerPlayer *p, league) {
                if (p->isAlive())
                    return p;
            }
            Q_ASSERT(false);
            return league.first();
        } else {
            return shenlvbu;
        }
    } else {
        Q_ASSERT(stage == 2);
        return current->getNextAlive();
    }
}

void RoomThread::actionHulaoPass(ServerPlayer *shenlvbu, QList<ServerPlayer *> league, GameRule *game_rule, int stage)
{
    try {
        if (stage == 1) {
            forever{
                ServerPlayer *current = room->getCurrent();
                trigger(TurnStart, room, current);

                ServerPlayer *next = findHulaoPassNext(shenlvbu, league, 1);
                if (current != shenlvbu) {
                    if (current->isAlive() && !current->hasFlag("actioned"))
                        room->setPlayerFlag(current, "actioned");
                    if (current->isAlive() && !current->getMark("actionedM"))
                        room->setPlayerMark(current, "actionedM", 1);
                } else {
                    bool all_actioned = true;
                    foreach (ServerPlayer *player, league) {
                        if (player->isAlive() && !player->getMark("actionedM")) {
                            all_actioned = false;
                            break;
                        }
                    }
                    if (all_actioned) {
                        foreach (ServerPlayer *player, league) {
                            if (player->hasFlag("actioned"))
                                room->setPlayerFlag(player, "-actioned");
                            if (player->getMark("actionedM"))
                                room->setPlayerMark(player, "actionedM", 0);
                        }
                        foreach (ServerPlayer *player, league) {
                            if (player->isDead()) {
                                JsonArray arg;
                                arg << QSanProtocol::S_GAME_EVENT_PLAYER_REFORM << player->objectName();
                                room->doBroadcastNotify(QSanProtocol::S_COMMAND_LOG_EVENT, arg);
                                QString tag_name = "HulaoReforming"+player->objectName();
                                int x = room->getTag(tag_name).toInt();
                                if (x > 2) {
                                    room->removeTag(tag_name);
                                    LogMessage log;
                                    log.type = "#ReformingRevive";
                                    log.from = player;
                                    room->sendLog(log);

                                    room->revivePlayer(player);
                                    room->setPlayerProperty(player, "hp", qMin(player->getMaxHp(), 3));
                                    player->drawCards(3, "revive");
                                    room->setPlayerFlag(player, "actioned");
                                    room->setPlayerMark(player, "actionedM", 1);
                                } else
                                    room->setTag(tag_name, x+1);
                            }
                        }
                    }
                }

                room->setCurrent(next);
            }
        } else {
            Q_ASSERT(stage == 2);
            forever{
                ServerPlayer *current = room->getCurrent();
                trigger(TurnStart, room, current);

                ServerPlayer *next = findHulaoPassNext(shenlvbu, league, 2);

                if (current == shenlvbu) {
                    foreach (ServerPlayer *player, league) {
                        if (player->isDead()) {
                            JsonArray arg;
                            arg << QSanProtocol::S_GAME_EVENT_PLAYER_REFORM << player->objectName();
                            room->doBroadcastNotify(QSanProtocol::S_COMMAND_LOG_EVENT, arg);
                            QString tag_name = "HulaoReforming"+player->objectName();
                            int x = room->getTag(tag_name).toInt();
                            if (x > 2) {
                                room->removeTag(tag_name);
                                LogMessage log;
                                log.type = "#ReformingRevive";
                                log.from = player;
                                room->sendLog(log);

                                room->revivePlayer(player);
                                room->setPlayerProperty(player, "hp", qMin(player->getMaxHp(), 3));
                                player->drawCards(3, "revive");
                                room->setPlayerFlag(player, "actioned");
                                room->setPlayerMark(player, "actionedM", 1);
                            } else
                                room->setTag(tag_name, x+1);
                        }
                    }
                }
                room->setCurrent(next);
            }
        }
    }
    catch (TriggerEvent triggerEvent) {
        if (triggerEvent == StageChange) {
            stage = 2;
            trigger(triggerEvent, room, NULL);
            foreach (ServerPlayer *player, room->getPlayers()) {
                if (player != shenlvbu) {
                    if (player->hasFlag("actioned"))
                        room->setPlayerFlag(player, "-actioned");
                    if (player->getMark("actionedM"))
                        room->setPlayerMark(player, "actionedM", 0);

                    if (player->getPhase() != Player::NotActive) {
                        QVariant v;
                        game_rule->trigger(EventPhaseEnd, room, player, v);
                        player->changePhase(player->getPhase(), Player::NotActive);
                    }
                }
            }

            room->setCurrent(shenlvbu);
            actionHulaoPass(shenlvbu, league, game_rule, 2);
        } else if (triggerEvent == TurnBroken) {
            _handleTurnBrokenHulaoPass(shenlvbu, league, game_rule, stage);
        } else {
            throw triggerEvent;
        }
    }
}

void RoomThread::_handleTurnBrokenHulaoPass(ServerPlayer *shenlvbu, QList<ServerPlayer *> league, GameRule *game_rule, int stage)
{
    try {
        ServerPlayer *player = room->getCurrent();
        trigger(TurnBroken, room, player);
        ServerPlayer *next = findHulaoPassNext(shenlvbu, league, stage);
        if (player->getPhase() != Player::NotActive) {
            QVariant v;
            game_rule->trigger(EventPhaseEnd, room, player, v);
            player->changePhase(player->getPhase(), Player::NotActive);
            if (player != shenlvbu && stage == 1)
            {
                room->setPlayerFlag(player, "actioned");
                room->setPlayerMark(player, "actionedM", 1);
            }
        }

        room->setCurrent(next);
        actionHulaoPass(shenlvbu, league, game_rule, stage);
    }
    catch (TriggerEvent triggerEvent) {
        if (triggerEvent == TurnBroken)
            _handleTurnBrokenHulaoPass(shenlvbu, league, game_rule, stage);
        else
            throw triggerEvent;
    }
}

void RoomThread::actionNormal(GameRule *game_rule)
{
    try {
        forever{
            LogMessage log;
            log.type = "$AppendSeparator";
            room->sendLog(log);
            if (!room->getTag("RoundStart").isNull() && room->getTag("RoundStart").value<ServerPlayer *>() &&
                    room->getCurrent() == room->getTag("RoundStart").value<ServerPlayer *>()){
                room->incTurn();
                QVariant data = room->getTurn();
                foreach(ServerPlayer *p, room->getAllPlayers())
                    trigger(RoundStart, room, p, data);
                room->setTag("TurnFirstRound", true);
                room->removeTag("RoundStart");
            }
			trigger(TurnStart, room, room->getCurrent());
            if (room->isFinished()) break;
			ServerPlayer *last_player = room->getCurrent();
			while (!room->getTag("ExtraTurnList").isNull()) {
                QStringList extraTurnList = room->getTag("ExtraTurnList").toStringList();
                if (!extraTurnList.isEmpty()) {
			        QString extraTurnPlayer = extraTurnList.takeFirst();
                    room->setTag("ExtraTurnList", QVariant::fromValue(extraTurnList));
                    ServerPlayer *next = room->findPlayer(extraTurnPlayer);
					if (next){
						room->setTag("ExtraTurn", true);
						room->setCurrent(next);
                        room->setNormalCurrent(last_player);
						LogMessage log;
                        log.type = "$AppendSeparator";
                        room->sendLog(log);
                        trigger(TurnStart, room, next);
						room->removeTag("ExtraTurn");
					}
                    if (room->isFinished()) break;
                } else
                    room->removeTag("ExtraTurnList");
            }
            if (room->isFinished()) break;
            ServerPlayer *nextp = last_player->getNextAlive();
            if (last_player->getRealSeat() > nextp->getRealSeat())
                room->setTag("RoundStart", QVariant::fromValue(nextp));
            room->setCurrent(nextp);
        }
    }
    catch (TriggerEvent triggerEvent) {
        if (triggerEvent == TurnBroken)
            _handleTurnBrokenNormal(game_rule);
        else
            throw triggerEvent;
    }
}

void RoomThread::_handleTurnBrokenNormal(GameRule *game_rule)
{
    try {
        ServerPlayer *player = room->getCurrent(), *nplayer = room->getNormalCurrent();
        trigger(TurnBroken, room, player);
        ServerPlayer *next = nplayer->getNextAlive();
        if (player->getPhase() != Player::NotActive) {
            QVariant data = QVariant();
            game_rule->trigger(EventPhaseEnd, room, player, data);
            player->changePhase(player->getPhase(), Player::NotActive);
        }

        while (!room->getTag("ExtraTurnList").isNull()) {
            QStringList extraTurnList = room->getTag("ExtraTurnList").toStringList();
            if (!extraTurnList.isEmpty()) {
                QString extraTurnPlayer = extraTurnList.takeFirst();
                room->setTag("ExtraTurnList", QVariant::fromValue(extraTurnList));
                ServerPlayer *next = room->findPlayer(extraTurnPlayer);
                if (next){
                    room->setTag("ExtraTurn", true);
                    room->setCurrent(next);
                    room->setNormalCurrent(nplayer);
                    LogMessage log;
                    log.type = "$AppendSeparator";
                    room->sendLog(log);
                    trigger(TurnStart, room, next);
                    room->removeTag("ExtraTurn");
                }
                if (room->isFinished()) break;
            } else
                room->removeTag("ExtraTurnList");
        }

        if (!room->getTag("break&NewTurn").isNull() && room->getTag("break&NewTurn").toBool())
        {
            room->setTag("RoundStart", QVariant::fromValue(nplayer));
            room->removeTag("break&NewTurn");
            actionNormal(game_rule);
        }
        else
        {
            if (nplayer->getRealSeat() > next->getRealSeat())
                room->setTag("RoundStart", QVariant::fromValue(next));
            room->setCurrent(next);
            actionNormal(game_rule);
        }
    }
    catch (TriggerEvent triggerEvent) {
        if (triggerEvent == TurnBroken)
            _handleTurnBrokenNormal(game_rule);
        else
            throw triggerEvent;
    }
}

void RoomThread::run()
{
    qsrand(QTime(0, 0, 0).secsTo(QTime::currentTime()));
    Sanguosha->registerRoom(room);
    GameRule *game_rule;
    if (room->getMode() == "04_1v3")
        game_rule = new HulaoPassMode(this);
    else
        game_rule = new GameRule(this);

    addTriggerSkill(game_rule);
    foreach(const TriggerSkill *triggerSkill, Sanguosha->getGlobalTriggerSkills())
        addTriggerSkill(triggerSkill);
    if (Config.EnableBasara) addTriggerSkill(new BasaraMode(this));

    if (room->getScenario() != NULL) {
        const ScenarioRule *rule = room->getScenario()->getRule();
        if (rule) addTriggerSkill(rule);
    }

    // start game
    try {
        QString order;
        QList<ServerPlayer *> warm, cool;
        QList<ServerPlayer *> first, second;
        if (room->getMode() == "06_3v3") {
            foreach (ServerPlayer *player, room->m_players) {
                switch (player->getRoleEnum()) {
                case Player::Lord: warm.prepend(player); break;
                case Player::Loyalist: warm.append(player); break;
                case Player::Renegade: cool.prepend(player); break;
                case Player::Rebel: cool.append(player); break;
                }
            }
            order = room->askForOrder(cool.first(), "cool");
            if (order == "warm") {
                first = warm;
                second = cool;
            } else {
                first = cool;
                second = warm;
            }
        }
        constructTriggerTable();
        trigger(GameStart, (Room *)room, NULL);
        if (room->getMode() == "06_3v3") {
            run3v3(first, second, game_rule, first.first());
        } else if (room->getMode() == "04_1v3") {
            ServerPlayer *shenlvbu = room->getLord();
            QList<ServerPlayer *> league = room->getPlayers();
            league.removeOne(shenlvbu);

            room->setCurrent(league.first());
            actionHulaoPass(shenlvbu, league, game_rule, 1);
        } else {
            if (room->getMode() == "02_1v1") {
                ServerPlayer *first = room->getPlayers().first();
                if (first->getRole() != "renegade")
                    first = room->getPlayers().at(1);
                ServerPlayer *second = first->getNext();
                trigger(Debut, (Room *)room, first);
                trigger(Debut, (Room *)room, second);
                room->setCurrent(first);
            }

            room->setTag("RoundStart", QVariant::fromValue(room->getCurrent()));
            actionNormal(game_rule);
        }
    }
    catch (TriggerEvent triggerEvent) {
        if (triggerEvent == GameFinished) {
            Sanguosha->unregisterRoom();
            return;
        } else if (triggerEvent == TurnBroken || triggerEvent == StageChange) { // caused in Debut trigger
            ServerPlayer *first = room->getPlayers().first();
            if (first->getRole() != "renegade")
                first = room->getPlayers().at(1);
            room->setCurrent(first);
            room->setTag("RoundStart", QVariant::fromValue(first));
            actionNormal(game_rule);
        } else {
            Q_ASSERT(false);
        }
    }
}

static bool compareByPriority(const TriggerSkill *a, const TriggerSkill *b)
{
    return a->getCurrentPriority() > b->getCurrentPriority();
}

bool RoomThread::trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *target, QVariant &data)
{
    // push it to event stack
    EventTriplet triplet(triggerEvent, room, target);
    event_stack.push_back(triplet);

    bool broken = false;
    QList<const TriggerSkill *> will_trigger;
    QSet<const TriggerSkill *> triggerable_tested;
    QList<const TriggerSkill *> rules; // we can't get a GameRule with Engine::getTriggerSkill() :(
    TriggerList trigger_who;

    try {
        QList<const TriggerSkill *> triggered;
        QList<const TriggerSkill *> &skills = skill_table[triggerEvent];
        foreach (const TriggerSkill *skill, skills) {
             skill->setCurrentPriority(skill->getDynamicPriority(triggerEvent));
        }

        qStableSort(skills.begin(), skills.end(), compareByPriority);

        //qStableSort(skills.begin(), skills.end(), [triggerEvent](const TriggerSkill *a, const TriggerSkill *b) {return a->getDynamicPriority(triggerEvent) > b->getDynamicPriority(triggerEvent); });


        do {
            trigger_who.clear();
            foreach (const TriggerSkill *skill, skills) {
                if (!triggered.contains(skill)) {
                    if (skill->objectName() == "game_rule" || (room->getScenario()
                        && room->getScenario()->objectName() == skill->objectName())) {
                        room->tryPause();
                        if (will_trigger.isEmpty()
                            || skill->getDynamicPriority(triggerEvent) == will_trigger.last()->getDynamicPriority(triggerEvent)) {
                            will_trigger.append(skill);
                            trigger_who[NULL].append(skill->objectName());// Don't assign game rule to some player.
                            rules.append(skill);
                        } else if (skill->getDynamicPriority(triggerEvent) != will_trigger.last()->getDynamicPriority(triggerEvent))
                            break;
                        triggered.prepend(skill);
                    } else {
                        room->tryPause();
                        if (will_trigger.isEmpty()
                            || skill->getDynamicPriority(triggerEvent) == will_trigger.last()->getDynamicPriority(triggerEvent)) {
                            skill->record(triggerEvent, room, target, data); //to record something for next.

                            TriggerList triggerSkillList = skill->triggerable(triggerEvent, room, target, data);

                            foreach (ServerPlayer *p, room->getPlayers()) {
                                if (triggerSkillList.contains(p) && !triggerSkillList.value(p).isEmpty()) {
                                    foreach (const QString &skill_name, triggerSkillList.value(p)) {
                                        QString _skillname = skill_name;
                                        if (_skillname.endsWith("!"))
                                            _skillname.chop(1);
                                        const TriggerSkill *trskill = Sanguosha->getTriggerSkill(_skillname);
                                        if (trskill) {
                                            will_trigger.append(trskill);
                                            trigger_who[p].append(skill_name);
                                        }
                                    }
                                }
                            }
                        } else if (skill->getDynamicPriority(triggerEvent) != will_trigger.last()->getDynamicPriority(triggerEvent))
                            break;

                        triggered << skill;
                    }
                }
                triggerable_tested << skill;
            }

            if (!will_trigger.isEmpty()) {
                will_trigger.clear();

                foreach (ServerPlayer *p, room->getAllPlayers(true)) {
                    if (!trigger_who.contains(p)) continue;
                    QStringList already_triggered;
                    forever {
                        QStringList who_skills = trigger_who.value(p);
                        if (who_skills.isEmpty()) break;
                        bool has_compulsory = false;
                        foreach (const QString &skill_name, who_skills) {
                            QString _skillname = skill_name;
                            if (_skillname.endsWith("!"))
                                _skillname.chop(1);
                            const TriggerSkill *trskill = Sanguosha->getTriggerSkill(_skillname); // "yiji"
                            if (trskill && (trskill->getFrequency() == Skill::Compulsory || trskill->getFrequency() == Skill::Wake || skill_name.endsWith("!"))) {
                                has_compulsory = true;
                                break;
                            }
                        }
                        will_trigger.clear();
                        QStringList names, back_up;
                        foreach (const QString &skill_name, who_skills) {
                            if (names.contains(skill_name))
                                back_up << skill_name;
                            else
                                names << skill_name;

                        }

                        if (names.isEmpty()) break;

                        QString name;

                        if (name.isEmpty()) {
                            if (p != NULL) {
                                QString reason = "GameRule_TriggerOrder";
                                QStringList skill_names;
                                foreach (const QString &skillName, names) {
                                    // codes below for UI shows the rest times of some skill
                                    int n = back_up.count(skillName) + 1;
                                    QString skillName_times = skillName;
                                    if (skillName_times.endsWith("!"))
                                        skillName_times.chop(1);
                                    if (n > 1) // default means one time.
                                        skillName_times = QString("%1*%2").arg(skillName).arg(n);
                                    skill_names << skillName_times;
                                }
                                if (skill_names.length() == 1)
                                    name = names.first();
                                else {
                                    QStringList all_names = skill_names;
                                    if (!has_compulsory) skill_names << "cancel";
                                    all_names << "cancel";
                                    int ai_delay = Config.AIDelay;
                                    Config.AIDelay = 0;
                                    name = room->askForChoice(p, reason, skill_names.join("+"), data, "@askfortriggerorder", all_names.join("+"));
                                    Config.AIDelay = ai_delay;
                                    if (name != "cancel")
                                        name = names.at(skill_names.indexOf(name));

                                }
                            } else
                                name = names.last();

                        }
                        if (name == "cancel") break;
                        QString skill_owner = name.split(":").first();
                        QString skill_position;
                        if (skill_owner.contains("?")) skill_position = skill_owner.split("?").last();

                        // "xxxx*yyy", "sgs1:xxxx" or "sgs1:xxxx*yyy", now it may be "sgs1?left:xxxx" or "sgs1?right:xxxx*yyy" or "sgs1?right:tieqi->sgs2&1"
                        int split = -1;
                        if ((split = name.indexOf('*')) != -1)
                            name = name.left(split);
                        if ((split = name.indexOf(':')) != -1)
                            name = name.mid(split + 1); // "xxxx"

                        ServerPlayer *skill_target = target;

                        QString _name = name;
                        if (_name.endsWith("!"))
                            _name.chop(1);
                        const TriggerSkill *result_skill = Sanguosha->getTriggerSkill(_name);

                        const Skill *mainskill = NULL;
                        if (p && result_skill && !skill_position.isEmpty())
                            mainskill = Sanguosha->getMainSkill(result_skill->objectName());
                        if (mainskill != NULL) {
                            QStringList skill_positions = room->getTag(mainskill->objectName() + p->objectName()).toStringList();   //for player audio & show
                            skill_positions.append(skill_position);
                            room->setTag(mainskill->objectName() + p->objectName(), skill_positions);
                        }

                        Q_ASSERT(skill_target && result_skill);

                        //----------------------------------------------- TriggerSkill::effect
                        already_triggered.append(name);
                        broken = result_skill->effect(triggerEvent, room, skill_target, data, p);
                        if (broken) {
                            if (mainskill != NULL) {
                                QStringList skill_positions = room->getTag(mainskill->objectName() + p->objectName()).toStringList();    //remove this record before broken
                                if (!skill_positions.isEmpty()) {
                                    skill_positions.removeLast();
                                    room->setTag(mainskill->objectName() + p->objectName(), skill_positions);
                                }
                            }
                            break;
                        }

                        //-----------------------------------------------
                        if (mainskill != NULL) {
                            QStringList skill_positions = room->getTag(mainskill->objectName() + p->objectName()).toStringList();           //remove this record
                            if (!skill_positions.isEmpty()) {
                                skill_positions.removeLast();
                                room->setTag(mainskill->objectName() + p->objectName(), skill_positions);
                            }
                        }


                        trigger_who.clear();
                        foreach (const TriggerSkill *skill, triggered) {
                            if (skill->objectName() == "game_rule" || (room->getScenario()
                                && room->getScenario()->objectName() == skill->objectName())) {
                                room->tryPause();
                                continue; // dont assign them to some person.
                            } else {
                                room->tryPause();
                                if (skill->getDynamicPriority(triggerEvent) == triggered.first()->getDynamicPriority(triggerEvent)) {

                                    TriggerList triggerSkillList = skill->triggerable(triggerEvent, room, target, data);

                                    foreach (ServerPlayer *player, room->getAllPlayers(true)) {
                                        if (triggerSkillList.contains(player) && !triggerSkillList.value(player).isEmpty()) {
                                            foreach (const QString &skill_name, triggerSkillList.value(player)) {
                                                QString _skillname = skill_name;
                                                if (_skillname.endsWith("!"))
                                                    _skillname.chop(1);

                                                const TriggerSkill *trskill = Sanguosha->getTriggerSkill(_skillname);
                                                if (trskill) // "yiji"
                                                    trigger_who[player].append(skill_name);
                                            }
                                        }
                                    }
                                } else
                                    break;
                            }
                        }

                        foreach (const QString &s, already_triggered) {
                            if (trigger_who[p].contains(s))
                                trigger_who[p].removeOne(s);
                        }

                        if (has_compulsory) {
                            has_compulsory = false;
                            foreach (const QString &skillName, trigger_who[p]) {
                                QString _skillname = skillName;
                                if (_skillname.endsWith("!"))
                                    _skillname.chop(1);
                                const TriggerSkill *s = Sanguosha->getTriggerSkill(_skillname); // "yiji"
                                if (s && (s->getFrequency() == Skill::Compulsory || s->getFrequency() == Skill::Wake || skillName.endsWith("!"))) {
                                    has_compulsory = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (broken) break;
                }
                // @todo_Slob: for drawing cards when game starts -- stupid design of triggering no player!
                if (!broken) {
                    if (!trigger_who[NULL].isEmpty()) {
                        foreach (QString skill_name, trigger_who[NULL]) {
                            const TriggerSkill *skill = NULL;
                            foreach (const TriggerSkill *rule, rules) { // because we cannot get a GameRule with Engine::getTriggerSkill()
                                if (rule->objectName() == skill_name) {
                                    skill = rule;
                                    break;
                                }
                            }
                            Q_ASSERT(skill != NULL);

                            broken = skill->trigger(triggerEvent, room, target, data);
                            if (broken)
                                break;
                        }
                    }
                }
            }

            if (broken)
                break;
        } while (skills.length() != triggerable_tested.size());

        if (target) {
            foreach(AI *ai, room->ais)
                ai->filterEvent(triggerEvent, target, data);
        }

        // pop event stack
        event_stack.pop_back();
    }
    catch (TriggerEvent throwed_event) {
        if (target) {
            foreach(AI *ai, room->ais)
                ai->filterEvent(triggerEvent, target, data);
        }

        // pop event stack
        event_stack.pop_back();

        throw throwed_event;
    }

    room->tryPause();
    return broken;
}

const QList<EventTriplet> *RoomThread::getEventStack() const
{
    return &event_stack;
}

bool RoomThread::trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *target)
{
    QVariant data;
    return trigger(triggerEvent, room, target, data);
}

void RoomThread::addTriggerSkill(const TriggerSkill *skill)
{
    if (skill == NULL || skillSet.contains(skill->objectName()))
        return;

    skillSet << skill->objectName();

    QList<TriggerEvent> events = skill->getTriggerEvents();
    foreach (const TriggerEvent &triggerEvent, events) {
        QList<const TriggerSkill *> &table = skill_table[triggerEvent];
        table << skill;
        foreach (const TriggerSkill *_skill, table){
             _skill->setCurrentPriority(_skill->getDynamicPriority(triggerEvent));
        }
        qStableSort(table.begin(), table.end(), compareByPriority);
    }

    if (skill->isVisible()) {
        foreach (const Skill *skill, Sanguosha->getRelatedSkills(skill->objectName())) {
            const TriggerSkill *trigger_skill = qobject_cast<const TriggerSkill *>(skill);
            if (trigger_skill)
                addTriggerSkill(trigger_skill);
        }
    }
}

void RoomThread::delay(long secs)
{
    if (secs == -1) secs = Config.AIDelay;
    Q_ASSERT(secs >= 0);
    if (room->property("to_test").toString().isEmpty() && Config.AIDelay > 0)
        msleep(secs);
}

