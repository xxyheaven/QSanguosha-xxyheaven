sgs.ai_skill_invoke.qirang = true
sgs.ai_skill_invoke.shanjia = true



sgs.ai_skill_discard.choulve = function(self, discard_num, min_num, optional, include_equip)
	local to_discard = {}
	local cards = self.player:getHandcards()
	local liuye = self.room:getCurrent()
	cards = sgs.QList2Table(cards)
	if self:isFriend(liuye) then
		for _, card in ipairs(cards) do
			if isCard("Peach", card, liuye) and ((not self:isWeak() and self:getCardsNum("Peach") > 0) or self:getCardsNum("Peach") > 1) then
				table.insert(to_discard, card:getEffectiveId())
				return to_discard
			end
			if isCard("Analeptic", card, liuye) and self:getCardsNum("Analeptic") > 1 then
				table.insert(to_discard, card:getEffectiveId())
				return to_discard
			end
			if isCard("Jink", card, liuye) and self:getCardsNum("Jink") > 1 then
				table.insert(to_discard, card:getEffectiveId())
				return to_discard
			end
		end
		self:sortByKeepValue(cards)
		for _, card in ipairs(cards) do
			if not isCard("Peach", card, self.player) and not isCard("ExNihilo", card, self.player) then
				table.insert(to_discard, card:getEffectiveId())
				return to_discard
			end
		end
		return cards[1]
	end
	return {}
end




























