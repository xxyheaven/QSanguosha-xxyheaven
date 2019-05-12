sgs.ai_skill_invoke.zhengnan = true

function SmartAI:sortByNumber(cards, inverse)
    local compare_func = function(a, b)
		if not inverse then return a:getNumber() > b:getNumber() end
		return a:getNumber() < b:getNumber()
    end
    table.sort(cards, compare_func)
end


sgs.ai_skill_cardask["@tuifeng-put"] = function(self, data)
	local to_discard = {}
	local cards = self.player:getCards("he")
	cards = sgs.QList2Table(cards)
	self:sortByKeepValue(cards)
	for _,c in ipairs(cards) do
		if not (c:isKindOf("Peach") or c:isKindOf("Analeptic")) then
			return "$" .. cards[1]:getEffectiveId()
		end
	end
	return "."
end

function getTotalNumber(cards, number, excepts)
    local returns = {}
    if excepts == nil then excepts = {} end
    for _,card in ipairs(cards) do
        if not table.contains(excepts, card) then
            if card:getNumber() == number then
                table.insert(returns, card)
                return returns
            elseif card:getNumber() < number then
                table.insert(excepts, card)
                local gets = getTotalNumber(cards, number - card:getNumber(), excepts)
                if #gets > 0 then
                    table.insert(returns, card)
                    for _,get in ipairs(gets) do
                        table.insert(returns, get)
                    end
                else
                    table.removeOne(excepts, card)
                end
                return returns
            end
        end
    end
    return returns
end

local ziyuan_skill = {}
ziyuan_skill.name = "ziyuan"
table.insert(sgs.ai_skills, ziyuan_skill)
ziyuan_skill.getTurnUseCard = function(self)
	if self.player:hasUsed("ZiyuanCard") then return nil end
	if #self.friends_noself < 1 then return nil end
	local cards=self.player:getCards("h")
	cards=sgs.QList2Table(cards)
	self:sortByUseValue(cards,true)
	for _,card in ipairs(cards) do
		if card:getNumber()== 13 then
			self.room:setCardFlag(card,"chosen")
			return sgs.Card_Parse("@ZiyuanCard=.")
		end
	end
	self:sortByNumber(cards, false)
	cards[#cards]:setFlags("least")
    
    local selected = getTotalNumber(cards, 13)
	-- local selected = {}
	-- if canEqual213(cards, selected) then
		for _,p in ipairs(selected) do
			self.room:setCardFlag(p,"chosen")
		end
		return sgs.Card_Parse("@ZiyuanCard=.") 
	-- end
end


sgs.ai_skill_use_func["ZiyuanCard"] = function(card, use, self)
	self:sort(self.friends_noself, "hp")
	local target
	for _,p in ipairs(self.friends_noself) do
		if not hasManjuanEffect(p) then
			target = p
			break
		end
	end
	local to_give = {}
	for _,c in sgs.qlist(self.player:getHandcards()) do
		if c:hasFlag("chosen") then
			table.insert(to_give,c:getEffectiveId())
		end
	end
	if target and #to_give > 0 then
		use.card = sgs.Card_Parse("@ZiyuanCard="..table.concat(to_give,"+"))
		if use.to then use.to:append(target) end
	end
end

sgs.ai_use_value["ZiyuanCard"] = 10
sgs.ai_use_priority["ZiyuanCard"] = 10
sgs.ai_card_intention.ZiyuanCard = -80

sgs.ai_skill_playerchosen.hongde = function(self, targets)
	return self:findPlayerToDraw(false, 1)
end

sgs.ai_playerchosen_intention.hongde = -30

local dingpan_skill = {}
dingpan_skill.name = "dingpan"
table.insert(sgs.ai_skills, dingpan_skill)
dingpan_skill.getTurnUseCard = function(self)
	if self.player:getMark("#dingpan") > 0 then
		return sgs.Card_Parse("@DingpanCard=.")
	end
end

sgs.ai_skill_use_func.DingpanCard = function(card, use, self)
    --白银、良助、枭姬、旋风、卖血、无伤、限制敌方装备、制衡装备
	self.dingpan_choice = nil
	self.dingpan_id = -1
	local target
	local sp_sun = self.room:findPlayerBySkillName("liangzhu")
	local sun = self.room:findPlayerBySkillName("xiaoji")
--多装备的香香，收益是最大的
	if sun and sun:getEquips():length() > 1 and self:isFriend(sun) and (sun:getHp() > 1 or self:getSaveNum(true) >= 1) then
		use.card = card
		if use.to then use.to:append(sun) end
		return
	end
--刷白银（有良助，或者多装备的情况下）
	--使用手牌中的白银，再随便检索一张其他装备挂上
	local SilverLion, OtherEquip
	if self.player:hasArmorEffect("SilverLion") then
		SilverLion = self.player:getArmor()
	end
	for _, c in sgs.qlist(self.player:getHandcards()) do
		if c:isKindOf("SilverLion") and not self.player:isLocked(c) then
			SilverLion = c
		end
		if c:isKindOf("EquipCard") and not c:isKindOf("Armor") and not self.player:isLocked(c) then
			OtherEquip = c
		end
	end

	local hasotherequip = false
	for _, equip in sgs.qlist(self.player:getEquips()) do
		if not equip:isKindOf("Armor") then
			hasotherequip = true
			break
		end
	end
	if SilverLion and (OtherEquip or hasotherequip or (sp_sun and self:isFriend(sp_sun))) then
		if not self.player:hasArmorEffect("SilverLion") then
			use.card = SilverLion
			return
		end
		if not hasotherequip and OtherEquip then
			use.card = OtherEquip
			return
		end
		use.card = card
		if use.to then
			use.to:append(self.player)
			if use.to:length() >= 1 then return end
		end
	end
--桃换牌（桃子溢出、1血有酒、有良助的情况下）
	
	
	
	
	
	
	
	
	
	
	
	


--香香、凌统
	
	
	
	
	
	
	
	
	--换装备、弃掉价值较低的同类装备、制衡装备（策略引用“制衡”ai）
	if self:needToThrowArmor() and self.player:canDiscard(self.player, self.player:getArmor():getEffectiveId()) then
        use.card = card
		if use.to then
			use.to:append(self.player)
			self.dingpan_choice = "disequip"
			self.dingpan_id = self.player:getArmor():getEffectiveId()
			return
		end
    end

	local equips = {}

	for _,to_select in sgs.qlist(self.player:getHandcards())do
        if to_select:getTypeId() == sgs.Card_TypeEquip then
			if to_select:isAvailable(self.player) then
				table.insert(equips, to_select)
			end
		end
    end

	local weapons, armors, dhorses, ohorses = {}, {}, {}, {}
	
	for _, e_card in ipairs(equips) do
		if e_card:isKindOf("Weapon") then
			local weapon = self.player:getWeapon()
			if weapon then
				if self.player:canDiscard(self.player, weapon:getEffectiveId()) then
					use.card = card
					if use.to then
						use.to:append(self.player)
						self.dingpan_choice = "disequip"
						self.dingpan_id = weapon:getEffectiveId()
						return
					end
				end
			end
			table.insert(weapons, e_card)
		elseif e_card:isKindOf("Armor") then
			local armor = self.player:getArmor()
			if armor then
				if self:evaluateArmor(e_card) > self:evaluateArmor(armor) and self.player:canDiscard(self.player, armor:getEffectiveId()) then
					use.card = card
					if use.to then
						use.to:append(self.player)
						self.dingpan_choice = "disequip"
						self.dingpan_id = armor:getEffectiveId()
						return
					end
				end
			end
			table.insert(armors, e_card)
		elseif e_card:isKindOf("DefensiveHorse") then
			local horse = self.player:getDefensiveHorse()
			if horse and self.player:canDiscard(self.player, horse:getEffectiveId()) then
				use.card = card
				if use.to then
					use.to:append(self.player)
					self.dingpan_choice = "disequip"
					self.dingpan_id = horse:getEffectiveId()
					return
				end
			end
			table.insert(dhorses, e_card)
		elseif e_card:isKindOf("OffensiveHorse") then
			local horse = self.player:getOffensiveHorse()
			if horse and self.player:canDiscard(self.player, horse:getEffectiveId()) then
				use.card = card
				if use.to then
					use.to:append(self.player)
					self.dingpan_choice = "disequip"
					self.dingpan_id = horse:getEffectiveId()
					return
				end
			end
			table.insert(ohorses, e_card)
		end
	end
	
	if not self.player:getWeapon() and #weapons > 1 then
		use.card = weapons[1]
		return
	end
	if not self.player:getArmor() and #armors > 1 then
		use.card = armors[1]
		return
	end
	if not self.player:getDefensiveHorse() and #dhorses > 1 then
		use.card = dhorses[1]
		return
	end
	if not self.player:getOffensiveHorse() and #ohorses > 1 then
		use.card = ohorses[1]
		return
	end
	
	local zhiheng = sgs.Card_Parse("@ZhihengCard=.")
	local dummy_use = { isDummy = true }
	self:useSkillCard(zhiheng, dummy_use)
	if dummy_use.card then
		for _, id in sgs.qlist(dummy_use.card:getSubcards()) do
			if self.player:getEquips():contains(sgs.Sanguosha:getCard(id)) then
				use.card = card
				if use.to then
					use.to:append(self.player)
					self.dingpan_choice = "disequip"
					self.dingpan_id = id
					return
				end
			end
		end
	end
end

sgs.ai_use_priority.DingpanCard = 20

sgs.ai_skill_choice.dingpan = function(self, choices, data)
	if self.dingpan_choice then return self.dingpan_choice end
	
	if self.player:hasArmorEffect("SilverLion") or self:needToLoseHp() or not self:damageIsEffective() then return "takeback" end
	
	if self.player:hasSkill("xiaoji") then
		if self.player:getEquips():length() == 1 then
			if not self:isWeak() then
				return "takeback"
			end
		else
			if self.player:getHp() > 1 or self:getSaveNum(true) >= 1 then
				return "takeback"
			end
		end
	end
	if self:hasSkills("qirang|xuanfeng") then
		if self.player:getEquips():length() > 1 and not self:isWeak() then
			return "takeback"
		end
	end
	if self.player:getEquips():length() > 1 and sgs.isGoodHp(self.player) then
		return "takeback"
	end
	return "disequip"
end


sgs.ai_skill_cardchosen.dingpan = function(self, who, flags)
	if self.dingpan_id > -1 then
		local card = sgs.Sanguosha:getCard(self.dingpan_id)
		if who:getEquips():contains(card) and self.player:canDiscard(who, self.dingpan_id) then
			return self.dingpan_id
		end
	end
	return nil
end






local gushe_skill = {}
gushe_skill.name = "gushe"
table.insert(sgs.ai_skills, gushe_skill)
gushe_skill.getTurnUseCard = function(self)
    if self.player:isKongcheng() or self.player:hasUsed("GusheCard") then return end
    return sgs.Card_Parse("@GusheCard=.")
end

sgs.ai_skill_use_func.GusheCard = function(card, use, self)
    local targets = sgs.SPlayerList()
    self:sort(self.enemies)
    for _,enemy in ipairs(self.enemies) do
        if not enemy:isKongcheng() and not (self:needKongcheng(enemy, true) and enemy:getHandcardNum() <= 2) and not (enemy:hasSkills(sgs.lose_equip_skill) and enemy:hasEquip()) then
            targets:append(enemy)
            if targets:length() == 3 then break end
        end
    end
    if targets:length() > 0 then
        use.to = targets
        use.card = card
        
        local max_card
        local max_number = 0
        local cards = sgs.QList2Table(self.player:getCards("h"))
        self:sortByKeepValue(cards)
        
        local rap_num = self.player:getMark("#rap")
        for _,c in ipairs(cards) do
            local number = c:getNumber()
            if number < rap_num then number = number + rap_num end
            if number > max_number then
                max_card = c
                max_number = number
            end
        end
        if rap_num == 6 and max_number < 10 then 
            use.card = nil
            use.to = sgs.SPlayerList()
        end
        
        self.gushe_card = max_card:getEffectiveId()
    end
end

sgs.ai_skill_discard.gushe = function(self, discard_num, min_num, optional, include_equip)
    if self:needKongcheng() then
        return self:askForDiscard("dummy", discard_num, min_num, optional, include_equip)
    end
	local current = self.room:getCurrent()
    if self:isFriend(current) then
        return {}
    end
    return self:askForDiscard("dummy", discard_num, min_num, optional, include_equip)
end

sgs.ai_skill_pindian.gushe = function(minusecard, self, requestor, maxcard, mincard)
    self.room:writeToConsole(self.player:objectName())
end

sgs.ai_use_value["GusheCard"] = 10
sgs.ai_use_priority["GusheCard"] = 10

sgs.ai_skill_invoke.jici = true

--悍勇
sgs.ai_skill_invoke.hanyong = function(self, data)
	local use = data:toCardUse()
	local earnings = 0
	local need
	if use.card:isKindOf("SavageAssault") then need = "Slash"
	elseif use.card:isKindOf("ArcheryAttack") then need = "Jink" end
	for _, enemy in ipairs(self.enemies) do
		if not enemy:hasArmorEffect("Vine") and self:damageIsEffective(enemy, nil, self.player) and getCardsNum(need, enemy, self.player) == 0 then
			earnings = earnings + 1
			if self:isWeak(enemy) then
				earnings = earnings + 1
			end
			if self:hasEightDiagramEffect(enemy) and need == "Jink" then
				earnings = earnings - 1
			end
		end
	end
	for _, friend in ipairs(self.friends_noself) do
		if not friend:hasArmorEffect("Vine") and self:damageIsEffective(friend, nil, self.player) and getCardsNum(need, friend, self.player) == 0 then
			earnings = earnings - 1
			if self:isWeak(friend) then
				earnings = earnings - 1
			end
			if self:hasEightDiagramEffect(friend) and need == "Jink" then
				earnings = earnings + 1
			end
		else
			earnings = earnings + 1
		end
	end
	if earnings >= 0 then return true end
	return false
end

--奇制
sgs.ai_skill_playerchosen.qizhi = function(self, targets)
	self:updatePlayers()
	self.qizhi_id = -1
	local targetlist = sgs.QList2Table(targets)
	
	
	local lingtong = self.room:findPlayerBySkillName("xuanfeng")
	if (lingtong and lingtong:hasEquip() and self:isFriend(lingtong) and targets:contains(lingtong)) then return lingtong end
	
	local xiangxiang = self.room:findPlayerBySkillName("xiaoji")
	if (xiangxiang and xiangxiang:hasEquip() and self:isFriend(xiangxiang) and targets:contains(xiangxiang)) then return xiangxiang end
	
	
	
	
	
	local luxun = self.room:findPlayerBySkillName("lianying")
	if (luxun and luxun:getHandcardNum() == 1 and self:isFriend(luxun) and targets:contains(luxun)) then return luxun end

	if targets:contains(self.player) then
		local zhiheng = sgs.Card_Parse("@ZhihengCard=.")
		local dummy_use = { isDummy = true }
		self:useSkillCard(zhiheng, dummy_use)
		if dummy_use.card then
			self.qizhi_id = dummy_use.card:getSubcards():first()
			return self.player
		end
		
	end
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	targetlist = sgs.shuffle(targetlist)
	
	return targetlist[1]
	
	
	
	
	
	--[[
	for _, target in ipairs(targetlist) do
		if self:isFriend(target) and (target:hasSkills(sgs.lose_equip_skill) or self:doNotDiscard(target) or self:needToThrowArmor(target)) then
			return target
		end
	end

	if table.contains(targetlist, self.player)then return self.player end

	for _, target in ipairs(targetlist) do
		if self:isEnemy(target) and target:hasEquip()
		and not target:hasSkills(sgs.lose_equip_skill) and not self:doNotDiscard(target) and not target:hasSkills(sgs.cardneed_skill) then
			return target
		end
	end
	return "."
	]]
end

sgs.ai_skill_cardchosen["qizhi"] = function(self, who, flags)
	if self.qizhi_id > -1 then
		return self.qizhi_id
	end
	return nil
end

--进趋
sgs.ai_skill_invoke.jinqu = function(self, data)
    return self.player:getMark("#qizhi") >= self.player:getHandcardNum()
end

--芳魂
local fanghun_skill={}
fanghun_skill.name="fanghun"
table.insert(sgs.ai_skills,fanghun_skill)
fanghun_skill.getTurnUseCard=function(self)
	if self.player:getMark("#plumshadow") < 1 then return nil end
    local cards = self.player:getCards("h")
    cards = sgs.QList2Table(cards)

    local jink_card

    self:sortByUseValue(cards,true)

    for _,card in ipairs(cards)  do
        if card:isKindOf("Jink") then
            jink_card = card
            break
        end
    end

    if not jink_card then return nil end
    local suit = jink_card:getSuitString()
    local number = jink_card:getNumberString()
    local card_id = jink_card:getEffectiveId()
    local card_str = ("slash:fanghun[%s:%s]=%d"):format(suit, number, card_id)
    local slash = sgs.Card_Parse(card_str)
    assert(slash)
    return slash
end

sgs.ai_view_as.fanghun = function(card, player, card_place)
	if (player:getMark("#plumshadow")  > 0) then
		local suit = card:getSuitString()
		local number = card:getNumberString()
		local card_id = card:getEffectiveId()
		if card_place == sgs.Player_PlaceHand then
			if card:isKindOf("Jink") then
				return ("slash:fanghun[%s:%s]=%d"):format(suit, number, card_id)
			elseif card:isKindOf("Slash") then
				return ("jink:fanghun[%s:%s]=%d"):format(suit, number, card_id)
			end
		end
	end
end

sgs.ai_skill_invoke.fuhan = function(self, data)
	local count = sgs.Sanguosha:getPlayerCount(self.room:getMode())
	local max_hp = math.min(self.player:getMark("plumshadow_removed") + self.player:getMark("#plumshadow"), count)
	
	if (max_hp == count and max_hp >= player:getMaxHp()) then return true end
	return max_hp > self.player:getHp() or (self:isWeak() and max_hp == self.player:getHp() and getCardsNum("Peach", self.player, self.player) == 0)
end







--戏志才==============================================
sgs.ai_skill_playerchosen.xianfu = function(self, targets)
	
	local AI_test = false
	
	if AI_test then
		for _, aplayer in sgs.qlist(self.room:getAllPlayers()) do
			if (aplayer:getNextAlive():objectName() == self.player:objectName()) then
				return aplayer
			end
		end
	end
	
	
	if self.player:getRole() == "loyalist" and self.room:getLord() then
		return self.room:getLord()
	end

	for _, p in sgs.qlist(targets) do
		if p:hasSkill("shibei") then return p end
	end
	for _, p in sgs.qlist(targets) do
		if p:hasSkill("jijiu") then return p end
	end
	for _, p in sgs.qlist(targets) do
		if p:hasSkill("huituo") then return p end
	end
	for _, p in sgs.qlist(targets) do
		if p:hasSkill("nosrende") then return p end
	end
	for _, p in sgs.qlist(targets) do
		if p:hasSkill("rende") then return p end
	end
	for _, p in sgs.qlist(targets) do
		if p:hasSkill("jieyin") then return p end
	end
	for _, p in sgs.qlist(targets) do
		if p:hasSkill("yuce") then return p end
	end
	for _, p in sgs.qlist(targets) do
		if p:hasSkill("yongsi") then return p end
	end
	for _, p in sgs.qlist(targets) do
		if p:hasSkill("lijian") then return p end
	end
	for _, p in sgs.qlist(targets) do
		if p:hasSkill("haoshi") then return p end
	end
	targets = sgs.QList2Table(targets)
	return targets[math.random(1, #targets)]
end

sgs.ai_skill_invoke.chouce = true

sgs.ai_skill_playerchosen.chouce = function(self, targets)
	if self.player:hasFlag("ChouceAIDiscard") then
		local target = self:findPlayerToDiscard("hej", true, true, nil, false)
		if target then return target end
	else
		local AssistTarget = self.player:getTag("XianfuTarget"):toPlayer()
		if AssistTarget and self:isFriend(AssistTarget) then
			return AssistTarget
		end
		if self:isWeak(self.player) then return self.player end
		self:sort(self.friends_noself, "handcard")
		for _, friend in ipairs(self.friends_noself) do
			if friend:hasSkills(sgs.cardneed_skill) and not self:needKongcheng(friend) and not friend:hasSkill("manjuan") then
				return friend
			end
		end
		return self.player
	end
end


sgs.ai_playerchosen_intention.chouce = function(self, from, to)
	if not from:hasFlag("ChouceAIDiscard") then
		local AssistTarget = from:getTag("XianfuTarget"):toPlayer()
		if AssistTarget:objectName() == to:objectName() then
			sgs.updateIntention(from, to, -80)
		else
			sgs.updateIntention(from, to, -20)
		end
	end
end

--董允============================

--秉正


sgs.ai_skill_playerchosen.bingzheng = function(self, targets)
	--优先考虑手牌数比体力值少1的队友（包括高手牌上限的自己）
	
	--手牌不溢出的情况下拆手牌数比体力值多1的敌人

	--考虑拆手牌为1的敌人
	
	--让队友摸牌
	
	--拆敌人牌
	
	
end

























--舍宴















sgs.ai_skill_playerchosen.beizhan = function(self, targets)
	return self.room:getOtherPlayers(self.player):first()
end

local gdlonghun_skill={}
gdlonghun_skill.name="gdlonghun"
table.insert(sgs.ai_skills, gdlonghun_skill)
gdlonghun_skill.getTurnUseCard = function(self)
	if self.player:getHp()>1 then return end
	local cards = sgs.QList2Table(self.player:getCards("he"))
	self:sortByUseValue(cards,true)
	for _, card in ipairs(cards) do
		if card:getSuit() == sgs.Card_Diamond and self:slashIsAvailable() then
			return sgs.Card_Parse(("fire_slash:gdlonghun[%s:%s]=%d"):format(card:getSuitString(), card:getNumberString(), card:getId()))
		end
	end
end

sgs.ai_view_as.gdlonghun = function(card, player, card_place)
	local suit = card:getSuitString()
	local number = card:getNumberString()
	local card_id = card:getEffectiveId()
	if player:getHp() > 1 or card_place == sgs.Player_PlaceSpecial then return end
	if card:getSuit() == sgs.Card_Diamond then
		return ("fire_slash:gdlonghun[%s:%s]=%d"):format(suit, number, card_id)
	elseif card:getSuit() == sgs.Card_Club then
		return ("jink:gdlonghun[%s:%s]=%d"):format(suit, number, card_id)
	elseif card:getSuit() == sgs.Card_Heart and player:getMark("Global_PreventPeach") == 0 then
		return ("peach:gdlonghun[%s:%s]=%d"):format(suit, number, card_id)
	elseif card:getSuit() == sgs.Card_Spade then
		return ("nullification:gdlonghun[%s:%s]=%d"):format(suit, number, card_id)
	end
end

sgs.gdlonghun_suit_value = {
	heart = 6.7,
	spade = 5,
	club = 4.2,
	diamond = 3.9,
}

function sgs.ai_cardneed.gdlonghun(to, card, self)
	if to:getCardCount() > 3 then return false end
	if to:isNude() then return true end
	return card:getSuit() == sgs.Card_Heart or card:getSuit() == sgs.Card_Spade
end







