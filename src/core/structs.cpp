#include "structs.h"
#include "protocol.h"
#include "json.h"


bool CardsMoveStruct::tryParse(const QVariant &arg)
{
    JsonArray args = arg.value<JsonArray>();
    if (args.size() != 9) return false;

    if ((!JsonUtils::isNumber(args[0]) && !args[0].canConvert<JsonArray>()) ||
        !JsonUtils::isNumberArray(args, 1, 2) || !JsonUtils::isStringArray(args, 3, 6) || !JsonUtils::isBool(args[7])) return false;

    if (JsonUtils::isNumber(args[0])) {
        int size = args[0].toInt();
        for (int i = 0; i < size; i++)
            card_ids.append(Card::S_UNKNOWN_CARD_ID);
    } else if (!JsonUtils::tryParse(args[0], card_ids)) {
        return false;
    }

    from_place = (Player::Place)args[1].toInt();
    to_place = (Player::Place)args[2].toInt();
    from_player_name = args[3].toString();
    to_player_name = args[4].toString();
    from_pile_name = args[5].toString();
    to_pile_name = args[6].toString();
    is_open_pile = args[7].toBool();
    reason.tryParse(args[8]);
    return true;
}

QVariant CardsMoveStruct::toVariant() const
{
    JsonArray arg;
    if (open) {
        arg << JsonUtils::toJsonArray(card_ids);
    } else {
        arg << card_ids.size();
    }

    arg << (int)from_place;
    arg << (int)to_place;
    arg << from_player_name;
    arg << to_player_name;
    arg << from_pile_name;
    arg << to_pile_name;
    arg << is_open_pile;
    arg << reason.toVariant();
    return arg;
}

bool CardMoveReason::tryParse(const QVariant &arg)
{
    JsonArray args = arg.value<JsonArray>();
    if (args.size() != 6 || !args[0].canConvert<int>() || !JsonUtils::isStringArray(args, 1, 5))
        return false;

    m_reason = args[0].toInt();
    m_playerId = args[1].toString();
    m_skillName = args[2].toString();
    m_eventName = args[3].toString();
    m_targetId = args[4].toString();
    m_extraData = QVariant::fromValue(Card::Parse(args[5].toString()));

    return true;
}

QVariant CardMoveReason::toVariant() const
{
    JsonArray result;
    result << m_reason;
    result << m_playerId;
    result << m_skillName;
    result << m_eventName;
    result << m_targetId;

    QVariant card_str = QString();
    if ((m_reason & CardMoveReason::S_MASK_BASIC_REASON) == CardMoveReason::S_REASON_USE && m_extraData.canConvert<CardUseStruct>()) {
        CardUseStruct use = m_extraData.value<CardUseStruct>();
        if (use.card)
            card_str = use.card->toString();
    } else if (m_reason == CardMoveReason::S_REASON_RESPONSE && m_extraData.canConvert<CardResponseStruct>()) {
        CardResponseStruct resp = m_extraData.value<CardResponseStruct>();
        if (resp.m_card)
            card_str = resp.m_card->toString();
    }
    result << card_str;
    return result;
}

AskForMoveCardsStruct::AskForMoveCardsStruct()
{
    is_success = false;
    top.clear();
    bottom.clear();
}
