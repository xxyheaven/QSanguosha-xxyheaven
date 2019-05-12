sgs.ai_view_as.diy_xianyou = function(card, player, card_place)
	local suit = card:getSuitString()
	local number = card:getNumberString()
	local card_id = card:getEffectiveId()
	if player:getHandcardNum() <= player:getHp() then
		return ("jink:diy_xianyou[%s:%s]=%d"):format(suit, number, card_id)
	end
end

function sgs.ai_cardneed.diy_xianyou(to, card)
	return player:getHandcardNum() <= player:getHp()
end

sgs.diy_xianyou_keep_value = {
	Jink = 3,
}

