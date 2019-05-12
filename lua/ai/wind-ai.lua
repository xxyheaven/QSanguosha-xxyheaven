sgs.ai_skill_use["@@shensu1"]=function(self,prompt)
	self:updatePlayers()
	self:sort(self.enemies,"defense")
	if self.player:containsTrick("lightning") and self.player:getCards("j"):length()==1
	  and self:hasWizard(self.friends) and not self:hasWizard(self.enemies, true) then
		return "."
	end

	if self:needBear() then return "." end

	local selfSub = self.player:getHp() - self.player:getHandcardNum()
	local selfDef = sgs.getDefense(self.player)

	for _,enemy in ipairs(self.enemies) do
		local def = sgs.getDefenseSlash(enemy, self)
		local slash = sgs.Sanguosha:cloneCard("slash")
		local eff = self:slashIsEffective(slash, enemy) and sgs.isGoodTarget(enemy, self.enemies, self)

		if not self.player:canSlash(enemy, slash, false) then
		elseif self:slashProhibit(nil, enemy) then
		elseif def < 6 and eff then return "@ShensuCard=.->"..enemy:objectName()

		elseif selfSub >= 2 then return "."
		elseif selfDef < 6 then return "." end
	end

	for _,enemy in ipairs(self.enemies) do
		local def=sgs.getDefense(enemy)
		local slash = sgs.Sanguosha:cloneCard("slash")
		local eff = self:slashIsEffective(slash, enemy) and sgs.isGoodTarget(enemy, self.enemies, self)

		if not self.player:canSlash(enemy, slash, false) then
		elseif self:slashProhibit(nil, enemy) then
		elseif eff and def < 8 then return "@ShensuCard=.->"..enemy:objectName()
		else return "." end
	end
	return "."
end

sgs.ai_get_cardType = function(card)
	if card:isKindOf("Weapon") then return 1 end
	if card:isKindOf("Armor") then return 2 end
	if card:isKindOf("DefensiveHorse") then return 3 end
	if card:isKindOf("OffensiveHorse")then return 4 end
end

sgs.ai_skill_use["@@shensu2"] = function(self, prompt, method)
	self:updatePlayers()
	self:sort(self.enemies, "defenseSlash")

	local selfSub = self.player:getHp() - self.player:getHandcardNum()
	local selfDef = sgs.getDefense(self.player)

	local cards = self.player:getCards("he")
	cards = sgs.QList2Table(cards)

	local eCard
	local hasCard = { 0, 0, 0, 0 }

	if self:needToThrowArmor() and not self.player:isCardLimited(self.player:getArmor(), method) then
		eCard = self.player:getArmor()
	end

	if not eCard then
		for _, card in ipairs(cards) do
			if card:isKindOf("EquipCard") then
				hasCard[sgs.ai_get_cardType(card)] = hasCard[sgs.ai_get_cardType(card)] + 1
			end
		end

		for _, card in ipairs(cards) do
			if card:isKindOf("EquipCard") and hasCard[sgs.ai_get_cardType(card)] > 1 then
				eCard = card
				break
			end
		end

		if not eCard then
			for _, card in ipairs(cards) do
				if card:isKindOf("EquipCard") and sgs.ai_get_cardType(card) > 3 and not self.player:isCardLimited(card, method) then
					eCard = card
					break
				end
			end
		end
		if not eCard then
			for _, card in ipairs(cards) do
				if card:isKindOf("EquipCard") and not card:isKindOf("Armor") and not self.player:isCardLimited(card, method) then
					eCard = card
					break
				end
			end
		end
	end

	if not eCard then return "." end

	local effectslash, best_target, target, throw_weapon
	local defense = 6
	local weapon = self.player:getWeapon()
	if weapon and eCard:getId() == weapon:getId() and (eCard:isKindOf("fan") or eCard:isKindOf("QinggangSword")) then throw_weapon = true end

	for _, enemy in ipairs(self.enemies) do
		local def = sgs.getDefense(enemy)
		local slash = sgs.Sanguosha:cloneCard("slash")
		local eff = self:slashIsEffective(slash, enemy) and sgs.isGoodTarget(enemy, self.enemies, self)

		if not self.player:canSlash(enemy, slash, false) then
		elseif throw_weapon and enemy:hasArmorEffect("vine") and not self.player:hasSkill("zonghuo") then
		elseif self:slashProhibit(nil, enemy) then
		elseif eff then
			if enemy:getHp() == 1 and getCardsNum("Jink", enemy) == 0 then best_target = enemy break end
			if def < defense then
				best_target = enemy
				defense = def
			end
			target = enemy
		end
		if selfSub < 0 then return "." end
	end

	if best_target then return "@ShensuCard="..eCard:getEffectiveId().."->"..best_target:objectName() end
	if target then return "@ShensuCard="..eCard:getEffectiveId().."->"..target:objectName() end

	return "."
end

sgs.ai_cardneed.shensu = function(to, card, self)
	return card:getTypeId() == sgs.Card_TypeEquip and getKnownCard(to, self.player, "EquipCard", false) < 2
end

sgs.ai_card_intention.ShensuCard = sgs.ai_card_intention.Slash

sgs.shensu_keep_value = sgs.xiaoji_keep_value

function sgs.ai_skill_invoke.jushou(self, data)
	--无脑据守大丈夫？233333
	return true
end

--据守选择一张手牌中的能弃置的非装备牌或能使用的装备牌
sgs.ai_skill_cardask["@jushou"] = function(self, data)
	local equips, to_discard = {}, {}
	for _,to_select in sgs.qlist(self.player:getHandcards())do
        if to_select:getTypeId() == sgs.Card_TypeEquip then
			if to_select:isAvailable(self.player) then
				table.insert(equips, to_select)
			end
		else
			if not self.player:isJilei(to_select) then
				table.insert(to_discard, to_select)
			end
		end
    end
	--优先使用缺少的装备
	self:sortByUsePriority(equips)
	for _, card in ipairs(equips) do
		local equip = card:getRealCard():toEquipCard()
		local equip_index = equip:location()
		if not self.player:getEquip(equip_index) then
			return "$" .. card:getId()
		end
	end
	--换成价值更高的装备（考虑武器和防具），同时记录不该扔的牌
	local _cards = {}
	for _, card in ipairs(equips) do
		if card:isKindOf("Weapon") then
			local weapon = self.player:getWeapon()
			if weapon then
				if self:evaluateWeapon(card) > self:evaluateWeapon(weapon) then
					return "$" .. card:getId()
				elseif self:evaluateWeapon(card) < self:evaluateWeapon(weapon) then
					table.insert(_cards, card)
				end
			end
		elseif card:isKindOf("Armor") then
			local armor = self.player:getArmor()
			if armor then
				if self:evaluateArmor(card) > self:evaluateArmor(armor) then
					return "$" .. card:getId()
				elseif self:evaluateArmor(card) < self:evaluateArmor(armor) then
					table.insert(_cards, card)
				end
			end
		end
	end
	
	local all_cards = to_discard
	for _, card in ipairs(equips) do
		if not table.contains(_cards, card) then
			table.insert(all_cards, card)
		end
	end
	self:sortByKeepValue(all_cards)
	if #all_cards > 0 then
		return "$" .. all_cards[1]:getId()
	end
	if #_cards > 0 then
		return "$" .. _cards[1]:getId()
	end
end

--解围转化无懈部分
sgs.ai_view_as.jiewei = function(card, player, card_place)
    local suit = card:getSuitString()
    local number = card:getNumberString()
    local card_id = card:getEffectiveId()
    if card_place == sgs.Player_PlaceEquip then
        return ("nullification:jiewei[%s:%s]=%d"):format(suit, number, card_id)
    end
end

--解围（抄巧变）
local function chosen_for_jiewei(self, return_prompt)
	local c
	local p
	local f
	for _, friend in ipairs(self.friends) do
		if friend:getJudgingArea():length() > 0 and not friend:containsTrick("YanxiaoCard") then
			local tricks = friend:getCards("j")
            for _, trick in sgs.qlist(tricks) do
                if trick:isKindOf("Indulgence") then
                    c = trick
					for _, enemy in ipairs(self.enemies) do
						if not enemy:containsTrick(trick:objectName()) and not self.room:isProhibited(self.player, enemy, trick)
							and not friend:containsTrick("YanxiaoCard") then
							f = friend
							p = enemy
							if return_prompt == "c" then return c:getId()
							elseif return_prompt == "p" then return p
							elseif return_prompt == "f" then return f
							else
								return (c and p)
							end
						end
					end	
                elseif trick:isKindOf("SupplyShortage") then
                    c = trick
					for _, enemy in ipairs(self.enemies) do
						if not enemy:containsTrick(trick:objectName()) and not self.room:isProhibited(self.player, enemy, trick)
							and not friend:containsTrick("YanxiaoCard") then
							f = friend
							p = enemy
							if return_prompt == "c" then return c:getId()
							elseif return_prompt == "p" then return p
							elseif return_prompt == "f" then return f
							else
								return (c and p)
							end
						end
					end
                end
            end
		end
	end
	self:sort(self.enemies, "defense")
	self:sort(self.friends, "defense")
	for _, enemy in ipairs(self.enemies) do
		if not self:doNotDiscard(enemy, "e") then
			local equips = enemy:getCards("e")
			for _, equip in sgs.qlist(equips) do
				if equip:isKindOf("WoodenOx") then
					c = equip
					for _, friend in ipairs(self.friends) do
						if not friend:getTreasure() then
							p = friend
							f = enemy
							if return_prompt == "c" then return c:getId()
							elseif return_prompt == "p" then return p
							elseif return_prompt == "f" then return f
							else
								return (c and p)
							end
						end
					end
				elseif equip:isKindOf("DefensiveHorse") then
					c = equip
					for _, friend in ipairs(self.friends) do
						if not friend:getDefensiveHorse() then
							p = friend
							f = enemy
							if return_prompt == "c" then return c:getId()
							elseif return_prompt == "p" then return p
							elseif return_prompt == "f" then return f
							else
								return (c and p)
							end
						end
					end
				elseif equip:isKindOf("Armor") and not self:needToThrowArmor(enemy) then
					c = equip
					for _, friend in ipairs(self.friends) do
						if not friend:getArmor() then
							p = friend
							f = enemy
							if return_prompt == "c" then return c:getId()
							elseif return_prompt == "p" then return p
							elseif return_prompt == "f" then return f
							else
								return (c and p)
							end
						end
					end
				elseif equip:isKindOf("Weapon") then
					c = equip
					for _, friend in ipairs(self.friends) do
						if not friend:getWeapon() then
							p = friend
							f = enemy
							if return_prompt == "c" then return c:getId()
							elseif return_prompt == "p" then return p
							elseif return_prompt == "f" then return f
							else
								return (c and p)
							end
						end
					end
				elseif equip:isKindOf("OffensiveHorse") then
					c = equip
					for _, friend in ipairs(self.friends) do
						if not friend:getOffensiveHorse() then
							p = friend
							f = enemy
							if return_prompt == "c" then return c:getId()
							elseif return_prompt == "p" then return p
							elseif return_prompt == "f" then return f
							else
								return (c and p)
							end
						end
					end
				end
			end
		end
	end
	
	if #self.friends > 1 then
		for _, friend1 in ipairs(sgs.reverse(self.friends)) do
			if self:needToThrowCard(friend1, "e") then
				local equips = friend1:getCards("e")
				for _, equip in sgs.qlist(equips) do
					if equip:isKindOf("Armor") and self:needToThrowArmor(friend1) then
						c = equip
						for _, friend in ipairs(self.friends) do
							if not friend:getArmor() and self:evaluateArmor(equip, friend) > 0 then
								p = friend
								f = friend1
								if return_prompt == "c" then return c:getId()
								elseif return_prompt == "p" then return p
								elseif return_prompt == "f" then return f
								else
									return (c and p)
								end
							end
						end
					elseif equip:isKindOf("Weapon") then
						c = equip
						for _, friend in ipairs(self.friends) do
							if not friend:getWeapon() then
								p = friend
								f = friend1
								if return_prompt == "c" then return c:getId()
								elseif return_prompt == "p" then return p
								elseif return_prompt == "f" then return f
								else
									return (c and p)
								end
							end
						end
					elseif equip:isKindOf("OffensiveHorse") then
						c = equip
						for _, friend in ipairs(self.friends) do
							if not friend:getOffensiveHorse() then
								p = friend
								f = friend1
								if return_prompt == "c" then return c:getId()
								elseif return_prompt == "p" then return p
								elseif return_prompt == "f" then return f
								else
									return (c and p)
								end
							end
						end
					end
				end
			end
		end
	end
	
	if return_prompt == "c" then return c:getId()
	elseif return_prompt == "p" then return p
	elseif return_prompt == "f" then return f
	else
		return (c and p)
	end
end

--解围弃牌
sgs.ai_skill_cardask["@jiewei"] = function(self, data)
	if not chosen_for_jiewei(self, ".") then return "." end
	local to_discard = self:askForDiscard("jiewei", 1, 1, false, true)
	if #to_discard > 0 then return "$" .. to_discard[1] else return "." end
end

--解围选择移动牌的角色
sgs.ai_skill_use["@@jiewei_move"] = function(self, prompt)
	local from = chosen_for_jiewei(self, "f")
	local to = chosen_for_jiewei(self, "p")
	if from and to then
		return ("@JieweiMoveCard=.->%s+%s"):format(from:objectName(), to:objectName())
	end
end

--解围选择要移动的卡牌
sgs.ai_skill_askforag.jiewei = function(self, card_ids)
	local id = chosen_for_jiewei(self, "c")
	if table.contains(card_ids, id) then
		return id
	end
	return -1
end

sgs.ai_skill_invoke.liegong = function(self, data)
	local target = data:toPlayer()
	return not self:isFriend(target)
end

function SmartAI:canLiegong(to, from, card)
	from = from or self.room:getCurrent()
	to = to or self.player
	card = card or sgs.Sanguosha:cloneCard("slash")
	if not from then return false end
	local handcardnum = from:getHandcardNum()
	for _, id in sgs.qlist(card:getSubcards()) do
		if from:handCards():contains(id) then
			handcardnum = handcardnum - 1
		end
	end
	if from:hasSkill("liegong") and to:getHandcardNum() <= handcardnum then return true end
	if from:hasSkill("kofliegong") and from:getPhase() == sgs.Player_Play and to:getHandcardNum() >= from:getHp() then return true end
	if from:getMark("zhaxiang") > 0 and card:isRed() then return true end
	if from:hasSkill("jianchu") and to:hasEquip() then return true end
	if from:hasSkill("tieji") and to:getCardCount(true) < 4 then return true end
	if from:hasSkill("pojun") and to:getHandcardNum() <= to:getHp() then return true end
	if from:hasSkill("fuji") and to:distanceTo(from) == 1 then return true end
	return false
end

sgs.ai_skill_invoke.kuanggu = true

sgs.ai_skill_choice.kuanggu = function(self, choices, data)
    return "recover"
end

sgs.ai_skill_cardask["@guidao-card"]=function(self, data)
	local judge = data:toJudge()
	local all_cards = self.player:getCards("he")
	if all_cards:isEmpty() then return "." end

	local needTokeep = judge.card:getSuit() ~= sgs.Card_Spade and (not self.player:hasSkill("leiji") or judge.card:getSuit() ~= sgs.Card_Club)
						and sgs.ai_AOE_data and self:playerGetRound(judge.who) < self:playerGetRound(self.player) and self:findLeijiTarget(self.player, 50)
						and (self:getCardsNum("Jink") > 0 or self:hasEightDiagramEffect()) and self:getFinalRetrial() == 1

	if not needTokeep then
		local who = judge.who
		if who:getPhase() == sgs.Player_Judge and not who:getJudgingArea():isEmpty() and who:containsTrick("lightning") and judge.reason ~= "lightning" then
			needTokeep = true
		end
	end
	local keptspade, keptblack = 0, 0
	if needTokeep then
		if self.player:hasSkill("nosleiji") then keptspade = 2 end
		if self.player:hasSkill("leiji") then keptblack = 2 end
	end
	local cards = {}
	for _, card in sgs.qlist(all_cards) do
		if card:isBlack() and not card:hasFlag("using") then
			if card:getSuit() == sgs.Card_Spade then keptspade = keptspade - 1 end
			keptblack = keptblack - 1
			table.insert(cards, card)
		end
	end

	if #cards == 0 then return "." end
	if keptblack >= 1 then return "." end
	if keptspade >= 1 and not self.player:hasSkill("leiji") then return "." end

	local card_id = self:getRetrialCardId(cards, judge)
	if card_id == -1 then
		if self:needRetrial(judge) and judge.reason ~= "beige" then
			if self:needToThrowArmor() then return "$" .. self.player:getArmor():getEffectiveId() end
			self:sortByUseValue(cards, true)
			if self:getUseValue(judge.card) > self:getUseValue(cards[1]) then
				return "$" .. cards[1]:getId()
			end
		end
	else
		local card = sgs.Sanguosha:getCard(card_id)
		local value1 = self:getCardJudgeValue(judge.reason, judge.card, judge.who)
		local value2 = self:getCardJudgeValue(judge.reason, card, judge.who)
		local bad_change = (judge.who and judge.who:isAlive() and (self:isFriend(judge.who) and value1 > value2) or (self:isEnemy(judge.who) and value1 < value2))
		if self:needRetrial(judge) or (self:getUseValue(judge.card) > self:getUseValue(card) and not bad_change) then
			return "$" .. card_id
		end
	end

	return "."
end

function sgs.ai_cardneed.guidao(to, card, self)
	for _, player in sgs.qlist(self.room:getAllPlayers()) do
		if self:getFinalRetrial(to) == 1 then
			if player:containsTrick("lightning") and not player:containsTrick("YanxiaoCard") then
				return card:getSuit() == sgs.Card_Spade and card:getNumber() >= 2 and card:getNumber() <= 9 and not self:hasSkills("hongyan|wuyan")
			end
			if self:isFriend(player) and self:willSkipDrawPhase(player) then
				return card:getSuit() == sgs.Card_Club and self:hasSuit("club", true, to)
			end
		end
	end
	if self:getFinalRetrial(to) == 1 then
		if to:hasSkill("nosleiji") then
			return card:getSuit() == sgs.Card_Spade
		end
		if to:hasSkill("leiji") then
			return card:isBlack()
		end
	end
end

function SmartAI:findLeijiTarget(player, leiji_value, slasher, latest_version)
	if not latest_version then
		return self:findLeijiTarget(player, leiji_value, slasher, 1) or self:findLeijiTarget(player, leiji_value, slasher, -1)
	end
	if not player:hasSkill(latest_version == 1 and "leiji" or "nosleiji") then return nil end
	if slasher then
		if not self:slashIsEffective(sgs.Sanguosha:cloneCard("slash"), player, slasher, slasher:hasWeapon("qinggang_sword")) then return nil end
		if slasher:hasSkill("liegong") and slasher:getPhase() == sgs.Player_Play and self:isEnemy(player, slasher)
			and (player:getHandcardNum() >= slasher:getHp() or player:getHandcardNum() <= slasher:getAttackRange()) then
			return nil
		end
		if slasher:hasSkill("kofliegong") and slasher:getPhase() == sgs.Player_Play
			and self:isEnemy(player, slasher) and player:getHandcardNum() >= slasher:getHp() then
			return nil
		end
		if not latest_version then
			if not self:hasSuit("spade", true, player) and player:getHandcardNum() < 3 then return nil end
		else
			if not self:hasSuit("black", true, player) and player:getHandcardNum() < 2 then return nil end
		end
		if not (getKnownCard(player, self.player, "Jink", true) > 0
				or (getCardsNum("Jink", player, self.player) >= 1 and sgs.card_lack[player:objectName()]["Jink"] ~= 1 and player:getHandcardNum() >= 4)
				or (not self:isWeak(player) and self:hasEightDiagramEffect(player) and not slasher:hasWeapon("qinggang_sword") and sgs.card_lack[player:objectName()]["Jink"] ~= 1)) then
			return nil
		end
	end
	local getCmpValue = function(enemy)
		local value = 0
		if not self:damageIsEffective(enemy, sgs.DamageStruct_Thunder, player) then return 99 end
		if enemy:hasSkill("hongyan") then
			if latest_version == -1 then return 99
			elseif not self:hasSuit("club", true, player) and player:getHandcardNum() < 3 then value = value + 80
			else value = value + 70 end
		end
		if self:cantbeHurt(enemy, player, latest_version == 1 and 1 or 2) or self:objectiveLevel(enemy) < 3
			or (enemy:isChained() and not self:isGoodChainTarget(enemy, player, sgs.DamageStruct_Thunder, latest_version == 1 and 1 or 2)) then return 100 end
		if not sgs.isGoodTarget(enemy, self.enemies, self) then value = value + 50 end
		if not latest_version and enemy:hasArmorEffect("silver_lion") then value = value + 20 end
		if enemy:hasSkills(sgs.exclusive_skill) then value = value + 10 end
		if enemy:hasSkills(sgs.masochism_skill) then value = value + 5 end
		if enemy:isChained() and self:isGoodChainTarget(enemy, player, sgs.DamageStruct_Thunder, latest_version == 1 and 1 or 2) and #(self:getChainedEnemies(player)) > 1 then value = value - 25 end
		if enemy:isLord() then value = value - 5 end
		value = value + enemy:getHp() + sgs.getDefenseSlash(enemy, self) * 0.01
		if latest_version and player:isWounded() and not self:needToLoseHp(player) then value = value + 15 end
		return value
	end

	local cmp = function(a, b)
		return getCmpValue(a) < getCmpValue(b)
	end

	local enemies = self:getEnemies(player)
	table.sort(enemies, cmp)
	for _,enemy in ipairs(enemies) do
		if getCmpValue(enemy) < leiji_value then return enemy end
	end
	return nil
end

sgs.ai_skill_playerchosen.leiji = function(self, targets)
	local mode = self.room:getMode()
	if mode:find("_mini_17") or mode:find("_mini_19") or mode:find("_mini_20") or mode:find("_mini_26") then
		local players = self.room:getAllPlayers();
		for _, aplayer in sgs.qlist(players) do
			if aplayer:getState() ~= "robot" then
				return aplayer
			end
		end
	end

	self:updatePlayers()
	return self:findLeijiTarget(self.player, 100, nil, 1)
end

sgs.ai_judge_value.leiji = function(self, card, target)
    if card:getSuit() == sgs.Card_Spade then
        if target:hasSkill("hongyan") then return 0 end
        return -2
    elseif card:getSuit() == sgs.Card_Club then
        if self.player:getHp() == 1 then return -2 end
        return -1
    end
    return 0
end

function SmartAI:needLeiji(to, from)
	return self:findLeijiTarget(to, 50, from)
end

sgs.ai_playerchosen_intention.leiji = 80

function sgs.ai_slash_prohibit.leiji(self, from, to, card) -- @todo: Qianxi flag name
	if self:isFriend(to) then return false end
	if to:hasFlag("QianxiTarget") and (not self:hasEightDiagramEffect(to) or self.player:hasWeapon("qinggang_sword")) then return false end
	local hcard = to:getHandcardNum()
	if self:canLiegong(to, from) then return false end
	if from:getRole() == "rebel" and to:isLord() then
		local other_rebel
		for _, player in sgs.qlist(self.room:getOtherPlayers(from)) do
			if sgs.evaluatePlayerRole(player) == "rebel" or sgs.compareRoleEvaluation(player, "rebel", "loyalist") == "rebel" then
				other_rebel = player
				break
			end
		end
		if not other_rebel and (self:hasSkills("hongyan") or self.player:getHp() >= 4) and (self:getCardsNum("Peach") > 0  or self.player:hasSkills("hongyan|ganglie|neoganglie")) then
			return false
		end
	end

	if sgs.card_lack[to:objectName()]["Jink"] == 2 then return true end
	if getKnownCard(to, self.player, "Jink", true) >= 1 or (self:hasSuit("spade", true, to) and hcard >= 2) or hcard >= 4 then return true end
	if not from then
		from = self.room:getCurrent()
	end
	if self:hasEightDiagramEffect(to) and not IgnoreArmor(from, to) then return true end
end

local huangtianv_skill = {}
huangtianv_skill.name = "huangtian_attach"
table.insert(sgs.ai_skills, huangtianv_skill)

huangtianv_skill.getTurnUseCard = function(self)
	if self.player:getKingdom() ~= "qun" then return nil end

	local cards = self.player:getCards("h")
	cards = sgs.QList2Table(cards)
	local card
	self:sortByUseValue(cards,true)
	for _,acard in ipairs(cards)  do
		if acard:isKindOf("Jink") then
			card = acard
			break
		end
	end
	if not card then return nil end

	local card_id = card:getEffectiveId()
	local card_str = "@HuangtianCard="..card_id
	local skillcard = sgs.Card_Parse(card_str)

	assert(skillcard)
	return skillcard
end

sgs.ai_skill_use_func.HuangtianCard = function(card, use, self)
	if self:needBear() or self:getCardsNum("Jink", "h") <= 1 then
		return "."
	end
	local targets = {}
	for _,friend in ipairs(self.friends_noself) do
		if friend:hasLordSkill("huangtian") then
			if not friend:hasFlag("HuangtianInvoked") then
				if not friend:hasSkill("manjuan") then
					table.insert(targets, friend)
				end
			end
		end
	end
	if #targets > 0 then --黄天己方
		use.card = card
		self:sort(targets, "defense")
		if use.to then
			use.to:append(targets[1])
		end
	elseif self:getCardsNum("Slash", "he") >= 2 then --黄天对方
		for _,enemy in ipairs(self.enemies) do
			if enemy:hasLordSkill("huangtian") then
				if not enemy:hasFlag("HuangtianInvoked") then
					if not enemy:hasSkill("manjuan") then
						if enemy:isKongcheng() and not enemy:hasSkill("kongcheng") and not enemy:hasSkills("tuntian+zaoxian") then --必须保证对方空城，以保证天义/陷阵的拼点成功
							table.insert(targets, enemy)
						end
					end
				end
			end
		end
		if #targets > 0 then
			local flag = false
			if self.player:hasSkill("tianyi") and not self.player:hasUsed("TianyiCard") then
				flag = true
			elseif self.player:hasSkill("xianzhen") and not self.player:hasUsed("XianzhenCard") then
				flag = true
			end
			if flag then
				local maxCard = self:getMaxCard(self.player) --最大点数的手牌
				if maxCard:getNumber() > card:getNumber() then --可以保证拼点成功
					self:sort(targets, "defense", true)
					for _,enemy in ipairs(targets) do
						if self.player:canSlash(enemy, nil, false, 0) then --可以发动天义或陷阵
								use.card = card
								enemy:setFlags("AI_HuangtianPindian")
								if use.to then
									use.to:append(enemy)
								end
								break
						end
					end
				end
			end
		end
	end
end

sgs.ai_card_intention.HuangtianCard = function(self, card, from, tos)
	if tos[1]:isKongcheng() and ((from:hasSkill("tianyi") and not from:hasUsed("TianyiCard"))
								or (from:hasSkill("xianzhen") and not from:hasUsed("XianzhenCard"))) then
	else
		sgs.updateIntention(from, tos[1], -80)
	end
end

sgs.ai_use_priority.HuangtianCard = 10
sgs.ai_use_value.HuangtianCard = 8.5

sgs.guidao_suit_value = {
	spade = 3.9,
	club = 2.7
}



sgs.ai_skill_invoke.fenji = function(self, data)
	local target = data:toPlayer()
	if self:isWeak() or not target or not self:isFriend(target)
		or hasManjuanEffect(target)
		or self:needKongcheng(target, true) then return false end
	return target:getHandcardNum() < (self.player:getHp() <= 1 and 3 or 5)
end

sgs.ai_choicemade_filter.skillInvoke.fenji = function(self, player, promptlist)
	if sgs.ai_fenji_target then
		if promptlist[3] == "yes" then
			sgs.updateIntention(player, sgs.ai_fenji_target, -10)
		end
		sgs.ai_fenji_target = nil
	end
end

sgs.ai_skill_use["@@tianxiang"] = function(self, data, method)
	if not method then method = sgs.Card_MethodDiscard end
	local friend_lost_hp = 10
	local friend_hp = 0
	local card_id
	local target
	local cant_use_skill
	local dmg

	if data == "@tianxiang-card" then
		dmg = self.player:getTag("TianxiangDamage"):toDamage()
	else
		dmg = data
	end

	if not dmg then self.room:writeToConsole(debug.traceback()) return "." end

	local cards = self.player:getCards("h")
	cards = sgs.QList2Table(cards)
	self:sortByUseValue(cards, true)
	for _, card in ipairs(cards) do
		if not self.player:isCardLimited(card, method) and card:getSuit() == sgs.Card_Heart and not card:isKindOf("Peach") then
			card_id = card:getId()
			break
		end
	end
	if not card_id then return "." end

	self:sort(self.enemies, "hp")

	for _, enemy in ipairs(self.enemies) do
		if (enemy:getHp() <= dmg.damage and enemy:isAlive() and enemy:getLostHp() + dmg.damage < 3) then
			if (enemy:getHandcardNum() <= 2 or enemy:hasSkills("guose|leiji|ganglie|enyuan|qingguo|wuyan|kongcheng") or enemy:containsTrick("indulgence"))
				and self:canAttack(enemy, dmg.from or self.room:getCurrent(), dmg.nature)
				and not (dmg.card and dmg.card:getTypeId() == sgs.Card_TypeTrick and enemy:hasSkill("wuyan")) then
				return "@TianxiangCard=" .. card_id .. "->" .. enemy:objectName()
			end
		end
	end

	for _, friend in ipairs(self.friends_noself) do
		if (friend:getLostHp() + dmg.damage > 1 and friend:isAlive()) then
			if friend:isChained() and dmg.nature ~= sgs.DamageStruct_Normal and not self:isGoodChainTarget(friend, dmg.from, dmg.nature, dmg.damage, dmg.card) then
			elseif friend:getHp() >= 2 and dmg.damage < 2
					and (friend:hasSkills("yiji|buqu|nosbuqu|shuangxiong|zaiqi|yinghun|jianxiong|fangzhu")
						or self:getDamagedEffects(friend, dmg.from or self.room:getCurrent())
						or self:needToLoseHp(friend)
						or (friend:getHandcardNum() < 3 and (friend:hasSkill("nosrende") or (friend:hasSkill("rende") and not friend:hasUsed("RendeCard"))))) then
				return "@TianxiangCard=" .. card_id .. "->" .. friend:objectName()
				elseif dmg.card and dmg.card:getTypeId() == sgs.Card_TypeTrick and friend:hasSkill("wuyan") and friend:getLostHp() > 1 then
					return "@TianxiangCard=" .. card_id .. "->" .. friend:objectName()
			elseif hasBuquEffect(friend) then return "@TianxiangCard=" .. card_id .. "->" .. friend:objectName() end
		end
	end

	for _, enemy in ipairs(self.enemies) do
		if (enemy:getLostHp() <= 1 or dmg.damage > 1) and enemy:isAlive() and enemy:getLostHp() + dmg.damage < 4 then
			if (enemy:getHandcardNum() <= 2)
				or enemy:containsTrick("indulgence") or enemy:hasSkills("guose|leiji|vsganglie|ganglie|enyuan|qingguo|wuyan|kongcheng")
				and self:canAttack(enemy, (dmg.from or self.room:getCurrent()), dmg.nature)
				and not (dmg.card and dmg.card:getTypeId() == sgs.Card_TypeTrick and enemy:hasSkill("wuyan")) then
				return "@TianxiangCard=" .. card_id .. "->" .. enemy:objectName() end
		end
	end

	for i = #self.enemies, 1, -1 do
		local enemy = self.enemies[i]
		if not enemy:isWounded() and not self:hasSkills(sgs.masochism_skill, enemy) and enemy:isAlive()
			and self:canAttack(enemy, dmg.from or self.room:getCurrent(), dmg.nature)
			and (not (dmg.card and dmg.card:getTypeId() == sgs.Card_TypeTrick and enemy:hasSkill("wuyan") and enemy:getLostHp() > 0) or self:isWeak()) then
			return "@TianxiangCard=" .. card_id .. "->" .. enemy:objectName()
		end
	end

	return "."
end

sgs.ai_card_intention.TianxiangCard = function(self, card, from, tos)
	local to = tos[1]
	if self:getDamagedEffects(to) or self:needToLoseHp(to) then return end
	local intention = 10
	if hasBuquEffect(to) then intention = 0
	elseif (to:getHp() >= 2 and to:hasSkills("yiji|shuangxiong|zaiqi|yinghun|jianxiong|fangzhu"))
		or (to:getHandcardNum() < 3 and (to:hasSkill("nosrende") or (to:hasSkill("rende") and not to:hasUsed("RendeCard")))) then
		intention = 0
	end
	sgs.updateIntention(from, to, intention)
end

function sgs.ai_slash_prohibit.tianxiang(self, from, to)
	if from:hasSkill("jueqing") or (from:hasSkill("nosqianxi") and from:distanceTo(to) == 1) then return false end
	if from:hasFlag("NosJiefanUsed") then return false end
	if self:isFriend(to, from) then return false end
	return self:cantbeHurt(to, from)
end

sgs.tianxiang_suit_value = {
	heart = 4.9
}

function sgs.ai_cardneed.tianxiang(to, card, self)
	return (card:getSuit() == sgs.Card_Heart or (to:hasSkill("hongyan") and card:getSuit() == sgs.Card_Spade))
		and (getKnownCard(to, self.player, "heart", false) + getKnownCard(to, self.player, "spade", false)) < 2
end

table.insert(sgs.ai_global_flags, "questioner")

sgs.ai_skill_choice.guhuo = function(self, choices)
	local yuji = self.room:findPlayerBySkillName("guhuo")
	if not self:isEnemy(yuji) then return "noquestion" end
	local guhuoname = self.room:getTag("GuhuoType"):toString()
	if guhuoname == "peach+analeptic" then guhuoname = "peach" end
	if guhuoname == "normal_slash" then guhuoname = "slash" end
	local guhuocard = sgs.Sanguosha:cloneCard(guhuoname)
	local guhuotype = guhuocard:getClassName()
	if guhuotype and self:getRestCardsNum(guhuotype, yuji) == 0 and self.player:getHp() > 0 then return "question" end
	if guhuotype and guhuotype == "AmazingGrace" then return "noquestion" end
	if self.player:hasSkill("hunzi") and self.player:getMark("hunzi") == 0 and math.random(1, 15) ~= 1 then return "noquestion" end
	if guhuotype:match("Slash") then
		if yuji:getState() ~= "robot" and math.random(1, 8) == 1 then return "question" end
		if not self:hasCrossbowEffect(yuji) then return "noquestion" end
	end
	local x = 5
	if guhuoname == "peach" or guhuoname == "ex_nihilo" then
		x = 2
		if getKnownCard(yuji, self.player, guhuotype, false) > 0 then x = x * 3 end
	end
	return math.random(1, x) == 1 and "question" or "noquestion"
end

local guhuo_skill = {}
guhuo_skill.name = "guhuo"
table.insert(sgs.ai_skills, guhuo_skill)
guhuo_skill.getTurnUseCard = function(self)
	if self.player:isKongcheng() or self.player:hasFlag("GuhuoUsed") then return end
	local current = self.room:getCurrent()
	if not current or current:isDead() or current:getPhase() == sgs.Player_NotActive then return end

	local cards = sgs.QList2Table(self.player:getHandcards())
	local GuhuoCard_str = {}

	for _, card in ipairs(cards) do
		if card:isNDTrick() then
			local dummy_use = { isDummy = true }
			self:useTrickCard(card, dummy_use)
			if dummy_use.card and dummy_use.to then table.insert(GuhuoCard_str, "@GuhuoCard=" .. card:getId() .. ":" .. card:objectName()) end
		end
	end

	local peach_str = self:getGuhuoCard("Peach", true, 1)
	if peach_str then table.insert(GuhuoCard_str, peach_str) end

	local fakeCards = {}

	for _, card in sgs.qlist(self.player:getHandcards()) do
		if (card:isKindOf("Slash") and self:getCardsNum("Slash", "h") >= 2 and not self:hasCrossbowEffect())
			or (card:isKindOf("Jink") and self:getCardsNum("Jink", "h") >= 3)
			or (card:isKindOf("EquipCard") and self:getSameEquip(card))
			or card:isKindOf("Disaster") then
			table.insert(fakeCards, card)
		end
	end
	self:sortByUseValue(fakeCards, true)

	local function fake_guhuo(objectName)
		if #fakeCards == 0 then return end

		local fakeCard
		local guhuo = "peach|ex_nihilo|snatch|dismantlement|amazing_grace|archery_attack|savage_assault"
		local ban = table.concat(sgs.Sanguosha:getBanPackages(), "|")
		if not ban:match("maneuvering") then guhuo = guhuo .. "|fire_attack" end
		local guhuos = guhuo:split("|")
		for i = 1, #guhuos do
			local forbidden = guhuos[i]
			local forbid = sgs.Sanguosha:cloneCard(forbidden)
			if self.player:isLocked(forbid) then
				table.remove(guhuos, i)
				i = i - 1
			end
		end
		for i=1, 10 do
			local card = fakeCards[math.random(1, #fakeCards)]
			local newguhuo = objectName or guhuos[math.random(1, #guhuos)]
			local guhuocard = sgs.Sanguosha:cloneCard(newguhuo, card:getSuit(), card:getNumber())
			if self:getRestCardsNum(guhuocard:getClassName()) > 0 then
				local dummyuse = {isDummy = true}
				if newguhuo == "peach" then self:useBasicCard(guhuocard, dummyuse) else self:useTrickCard(guhuocard, dummyuse) end
				if dummyuse.card then
					fakeCard = sgs.Card_Parse("@GuhuoCard=" .. card:getId() .. ":" .. newguhuo)
					break
				end
			end
		end
		return fakeCard
	end

	local enemy_num = #self.enemies
	local can_question = enemy_num
	for _, enemy in ipairs(self.enemies) do
		if enemy:hasSkill("chanyuan") or (enemy:hasSkill("hunzi") and enemy:getMark("hunzi") == 0) then can_question = can_question - 1 end
	end
	local ratio = (can_question == 0) and 100 or (enemy_num / can_question)
	if #GuhuoCard_str > 0 then
		local guhuo_str = GuhuoCard_str[math.random(1, #GuhuoCard_str)]

		local str = guhuo_str:split("=")
		str = str[2]:split(":")
		local cardid, cardname = str[1], str[2]

		if sgs.Sanguosha:getCard(cardid):objectName() == cardname and cardname == "ex_nihilo" then
			if math.random(1, 3) <= ratio then
				local fake_exnihilo = fake_guhuo(cardname)
				if fake_exnihilo then return fake_exnihilo end
			end
			return sgs.Card_Parse(guhuo_str)
		elseif math.random(1, 5) <= ratio then
			local fake_GuhuoCard = fake_guhuo()
			if fake_GuhuoCard then return fake_GuhuoCard end
		else
			return sgs.Card_Parse(guhuo_str)
		end
	elseif math.random(1, 5) <= 3 * ratio then
		local fake_GuhuoCard = fake_guhuo()
		if fake_GuhuoCard then return fake_GuhuoCard end
	end

	if self:isWeak() then
		local peach_str = self:getGuhuoCard("Peach", true, 1)
		if peach_str then
			local card = sgs.Card_Parse(peach_str)
			local peach = sgs.Sanguosha:cloneCard("peach", card:getSuit(), card:getNumber())
			local dummy_use = { isDummy = true }
			self:useBasicCard(peach, dummy_use)
			if dummy_use.card and dummy_use.to then return card end
		end
	end
	local slash_str = self:getGuhuoCard("Slash", true, 1)
	if slash_str and self:slashIsAvailable() then
		local card = sgs.Card_Parse(slash_str)
		local slash = sgs.Sanguosha:cloneCard("slash", card:getSuit(), card:getNumber())
		local dummy_use = { isDummy = true }
		self:useBasicCard(slash, dummy_use)
		if dummy_use.card and dummy_use.to then return card end
	end
end

sgs.ai_skill_use_func.GuhuoCard=function(card,use,self)
	local userstring=card:toString()
	userstring=(userstring:split(":"))[3]
	local guhuocard=sgs.Sanguosha:cloneCard(userstring, card:getSuit(), card:getNumber())
	guhuocard:setSkillName("guhuo")
	if guhuocard:getTypeId() == sgs.Card_TypeBasic then
		self:useBasicCard(guhuocard, use)
		if not use.isDummy and use.card and use.card:isKindOf("Slash") and (not use.to or use.to:isEmpty()) then return end
	else
		assert(guhuocard)
		self:useTrickCard(guhuocard, use)
	end
	if not use.card then return end
	use.card=card
end

sgs.ai_use_priority.GuhuoCard = 10

function SmartAI:getGuhuoViewCard(class_name, latest_version)
	local card_use, fakeCards = {}, {}
	local all_cards = (self.room:getMode() == "_mini_48")
	local can_question = #self.enemies
	if latest_version == 1 then
		for _, enemy in ipairs(self.enemies) do
			if enemy:hasSkill("chanyuan") or (enemy:hasSkill("hunzi") and enemy:getMark("hunzi") == 0) then can_question = can_question - 1 end
		end
		if can_question == 0 then all_cards = true end
	end
	local ratio = (can_question == 0) and 100 or (#self.enemies / can_question)
	if all_cards then
		card_use = sgs.QList2Table(self.player:getHandcards())
		self:sortByKeepValue(card_use)
	else
		if latest_version == -1 then
			for _, card in sgs.qlist(self.player:getHandcards()) do
				if card:isKindOf(class_name) and card:getSuit() == sgs.Card_Heart then
					table.insert(card_use, card)
				end
			end
			for _, card in sgs.qlist(self.player:getHandcards()) do
				if card:isKindOf(class_name) and not table.contains(card_use, card) then
					table.insert(card_use, card)
				end
			end
		else
			for _, card in sgs.qlist(self.player:getHandcards()) do
				if card:isKindOf(class_name) then
					table.insert(card_use, card)
				end
			end
		end
		for _, card in sgs.qlist(self.player:getHandcards()) do
			if not card:isKindOf(class_name) then
				if (card:isKindOf("Slash") and self:getCardsNum("Slash", "h") >= 2 and not self:hasCrossbowEffect())
					or (card:isKindOf("Jink") and self:getCardsNum("Jink", "h") >= 3)
					or (card:isKindOf("EquipCard") and self:getSameEquip(card))
					or card:isKindOf("Disaster") then
					table.insert(fakeCards, card)
				end
			end
		end
		self:sortByKeepValue(fakeCards)
	end

	local classname2objectname = {
		["Slash"] = "slash", ["Jink"] = "jink",
		["Peach"] = "peach", ["Analeptic"] = "analeptic",
		["Nullification"] = "nullification",
		["FireSlash"] = "fire_slash", ["ThunderSlash"] = "thunder_slash"
	}

	if classname2objectname[class_name] then
		local card = sgs.Sanguosha:cloneCard(classname2objectname[class_name])
		if not card or self.player:isCardLimited(card, sgs.Card_MethodUse, true) then return end
		if #card_use > 1 or (#card_use > 0 and (latest_version == 1 or card_use[1]:getSuit() == sgs.Card_Heart or all_cards)) then
			local index = 1
			local ban = table.concat(sgs.Sanguosha:getBanPackages(), "|")
			if not all_cards and (class_name == "Peach" or (class_name == "Analeptic" and not ban:match("maneuvering")) or class_name == "Jink") then
				index = #card_use
			end
			local card_class = latest_version == 1 and "@GuhuoCard=" or "@NosGuhuoCard="
			return card_class .. card_use[index]:getEffectiveId() .. ":" .. classname2objectname[class_name]
		end
		if #fakeCards > 0 and math.random(1, 5) <= ratio then
			local card_class = latest_version == 1 and "@GuhuoCard=" or "@NosGuhuoCard="
			return card_class .. fakeCards[1]:getEffectiveId() .. ":" .. classname2objectname[class_name]
		end
	end
end

function SmartAI:getGuhuoCard(class_name, at_play, latest_version)
	if not latest_version then return self:getGuhuoCard(class_name, at_play, 1) or self:getGuhuoCard(class_name, at_play, -1) end
	local player = self.player
	local current = self.room:getCurrent()
	if not (latest_version == 1 and player:hasSkill("guhuo") and not player:hasFlag("GuhuoUsed")
			and current and current:isAlive() and current:getPhase() ~= sgs.Player_NotActive)
		and not (latest_version == -1 and player:hasSkill("nosguhuo")) then return end
	if at_play then
		if class_name == "Peach" and not player:isWounded() then return
		elseif class_name == "Analeptic" and player:hasUsed("Analeptic") then return
		elseif (class_name == "Slash" or class_name == "ThunderSlash" or class_name == "FireSlash") and not self:slashIsAvailable(player) then return
		elseif class_name == "Jink" or class_name == "Nullification" then return
		end
	else
		if class_name == "Peach" and self.player:getMark("Global_PreventPeach") > 0 then return end
	end
	return self:getGuhuoViewCard(class_name, latest_version)
end

sgs.guhuo_suit_value = {
	heart = 5,
}

sgs.ai_skill_choice.guhuo_saveself = function(self, choices)
	if self:getCard("Peach") or not self:getCard("Analeptic") then return "peach" else return "analeptic" end
end

sgs.ai_suit_priority.guidao= "diamond|heart|club|spade"
sgs.ai_suit_priority.hongyan= "club|diamond|spade|heart"
sgs.ai_suit_priority.guhuo= "club|spade|diamond|heart"
sgs.ai_skill_choice.guhuo_slash = function(self, choices)
	return "slash"
end

function sgs.ai_cardneed.kuanggu(to, card, self)
	return card:isKindOf("OffensiveHorse") and not (to:getOffensiveHorse() or getKnownCard(to, self.player, "OffensiveHorse", false) > 0)
end