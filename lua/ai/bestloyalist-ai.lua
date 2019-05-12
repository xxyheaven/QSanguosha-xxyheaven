function SmartAI:useCardAllArmy(card, use)
	local enemies = self:exclude(self.enemies, card)

	local zhanghe = self.room:findPlayerBySkillName("qiaobian")
	local zhanghe_seat = zhanghe and zhanghe:faceUp() and not zhanghe:isKongcheng() and not self:isFriend(zhanghe) and zhanghe:getSeat() or 0

	local sb_daqiao = self.room:findPlayerBySkillName("yanxiao")
	local yanxiao = sb_daqiao and not self:isFriend(sb_daqiao) and sb_daqiao:faceUp() and
					(getKnownCard(sb_daqiao, self.player, "diamond", nil, "he") > 0
					or sb_daqiao:getHandcardNum() + self:ImitateResult_DrawNCards(sb_daqiao, sb_daqiao:getVisibleSkillList(true)) > 3
					or sb_daqiao:containsTrick("YanxiaoCard"))

	if #enemies == 0 then return end

	local getvalue = function(enemy)
		if enemy:containsTrick("all_army") or enemy:containsTrick("YanxiaoCard") then return -100 end
		if enemy:getMark("juao") > 0 then return -100 end
		if enemy:hasSkill("qiaobian") and not enemy:containsTrick("all_army") and not enemy:containsTrick("indulgence") then return -100 end
		if zhanghe_seat > 0 and (self:playerGetRound(zhanghe) <= self:playerGetRound(enemy) and self:enemiesContainsTrick() <= 1 or not enemy:faceUp()) then
			return - 100 end
		if yanxiao and (self:playerGetRound(sb_daqiao) <= self:playerGetRound(enemy) and self:enemiesContainsTrick(true) <= 1 or not enemy:faceUp()) then
			return -100 end

		local value = 0 - enemy:getHandcardNum()

		if self:hasSkills("yongsi|haoshi|tuxi|noslijian|lijian|fanjian|neofanjian|dimeng|jijiu|jieyin|manjuan|beige",enemy)
		  or (enemy:hasSkill("zaiqi") and enemy:getLostHp() > 1)
			then value = value + 10
		end
		if self:hasSkills(sgs.cardneed_skill,enemy) or self:hasSkills("zhaolie|tianxiang|qinyin|yanxiao|zhaoxin|toudu|renjie",enemy)
			then value = value + 5
		end
		if self:hasSkills("yingzi|shelie|xuanhuo|buyi|jujian|jiangchi|mizhao|hongyuan|chongzhen|duoshi",enemy) then value = value + 1 end
		if enemy:hasSkill("zishou") then value = value + enemy:getLostHp() end
		if self:isWeak(enemy) then value = value + 5 end
		if enemy:isLord() then value = value + 3 end

		if self:objectiveLevel(enemy) < 3 then value = value - 10 end
		if not enemy:faceUp() then value = value - 10 end
		if self:hasSkills("keji|shensu|qingyi", enemy) then value = value - enemy:getHandcardNum() end
		if self:hasSkills("guanxing|xiuluo|tiandu|guidao|noszhenlie", enemy) then value = value - 5 end
		if not sgs.isGoodTarget(enemy, self.enemies, self) then value = value - 1 end
		if self:needKongcheng(enemy) then value = value - 1 end
		if enemy:getMark("@kuiwei") > 0 then value = value - 2 end
		return value
	end

	local cmp = function(a,b)
		return getvalue(a) > getvalue(b)
	end

	table.sort(enemies, cmp)

	local target = enemies[1]
	if getvalue(target) > -100 then
		use.card = card
		if use.to then use.to:append(target) end
		return
	end
end

sgs.ai_use_value.AllArmy = 7
sgs.ai_keep_value.AllArmy = 3.48
sgs.ai_use_priority.AllArmy = 0.5
sgs.ai_card_intention.AllArmy = 50

sgs.dynamic_value.control_usecard.AllArmy = true


function SmartAI:useCardMoreTroops(card, use)
	local friends = self:exclude(self.friends, card)
    self:sort(friends)
    for _,friend in ipairs(friends) do
        if not self:needKongcheng(friend) then
            use.card = card
            if use.to then use.to:append(friend) end
            return
        end
    end
end

sgs.ai_use_value.MoreTroops = 7
sgs.ai_keep_value.MoreTroops = 4
sgs.ai_use_priority.MoreTroops = 7
sgs.ai_card_intention.MoreTroops = -120

sgs.dynamic_value.benefit.MoreTroops = true


function SmartAI:useCardBeatAnother(card, use)
    local enemies = self:exclude(self.enemies, card)
    self:sort(enemies)
    if not use.to then use.to = sgs.SPlayerList() end
    for _,enemy in ipairs(enemies) do
        if self.player:distanceTo(enemy) == 1 and not self:needKongcheng(enemy, true) and not self:hasSkills(sgs.lose_equip_skill, enemy) and (enemy:getCards("he"):length() > 0 and enemy:getCards("he"):length() <= 2) then
            use.card = card
            use.to:append(enemy)
            break
        end
    end
    
    self:sort(self.friends_noself)
    if use.to:length() == 0 then
        for _,friend in ipairs(self.friends_noself) do
            if self.player:distanceTo(friend) == 1 and self:hasSkills(sgs.lose_equip_skill, friend) or (self:needKongcheng(friend, true) and friend:getCards("he"):length() > 0) then
                use.to:append(friend)
                break
            end
        end
    end
    
    
    for _,friend in ipairs(self.friends_noself) do
        if not self:needKongcheng(friend) then
            use.to:append(friend)
            break
        end
    end
    if use.card and use.to and use.to:length() == 2 then
        return
    end
    use.card = nil
    return
end


sgs.ai_use_value.BeatAnother = 7
sgs.ai_keep_value.BeatAnother = 3
sgs.ai_use_priority.BeatAnother = 7
sgs.ai_card_intention.BeatAnother = 60

sgs.dynamic_value.control_card.BeatAnother = true

sgs.ai_skill_invoke.xunzhi = function(self)
    if self.player:getHp() == 1 then return false end

    function getHpNum(players, n)
        local total = 0
        for _,p in ipairs(players) do
            if p:getHp() == n then
                total = total + 1
            end
        end
        return total
    end
    
    local players = sgs.QList2Table(self.room:getOtherPlayers(self.player))
    local now = getHpNum(players, self.player:getHp())
    local after = getHpNum(players, self.player:getHp() - 1)
    if after >= now then return true end
    return false
end



function SmartAI:willUseThunder(card)
	if not card then self.room:writeToConsole(debug.traceback()) return false end
	if self.player:containsTrick("Thunder") then return end
	if self.room:isProhibited(self.player, self.player, card) then return end

	local function hasDangerousFriend()
		local hashy = false
		for _, aplayer in ipairs(self.enemies) do
			if aplayer:hasSkill("hongyan") then hashy = true break end
		end
		for _, aplayer in ipairs(self.enemies) do
			if aplayer:hasSkill("guanxing") or (aplayer:hasSkill("gongxin") and hashy)
			or aplayer:hasSkill("xinzhan") then
				if self:isFriend(aplayer:getNextAlive()) then return true end
			end
		end
		return false
	end

	if self:getFinalRetrial(self.player) == 2 then
	return
	elseif self:getFinalRetrial(self.player) == 1 then
		return true
	elseif not hasDangerousFriend() then
		local players = self.room:getAllPlayers()
		players = sgs.QList2Table(players)

		local friends = 0
		local enemies = 0

		for _,player in ipairs(players) do
			if self:objectiveLevel(player) >= 4 and not player:hasSkill("hongyan") and not player:hasSkill("wuyan")
			  and not (player:hasSkill("weimu") and card:isBlack()) then
				enemies = enemies + 1
			elseif self:isFriend(player) and not player:hasSkill("hongyan") and not player:hasSkill("wuyan")
			  and not (player:hasSkill("weimu") and card:isBlack()) then
				friends = friends + 1
			end
		end

		local ratio

		if friends == 0 then ratio = 999
		else ratio = enemies/friends
		end

		if ratio > 1.5 then
			return true
		end
	end
end

function SmartAI:useCardThunder(card, use)
	if self:willUseThunder(card) then
		use.card = card
	end
end

sgs.ai_use_priority.Thunder = 0
sgs.dynamic_value.lucky_chance.Thunder = true

sgs.ai_keep_value.Thunder = -1


sgs.ai_view_as.silver_armor = function(card, player, card_place)
    local suit = card:getSuitString()
    local number = card:getNumberString()
    local card_id = card:getEffectiveId()
    if card_place == sgs.Player_PlaceHand then
        return ("jink:silver_armor[%s:%s]=%d"):format(suit, number, card_id)
    end
end

sgs.ai_use_priority.TreasuredSword = 2.645
sgs.weapon_range.TreasuredSword = 2

function sgs.ai_weapon_value.TreasuredSword(self, enemy)
	if enemy:getLostHp() == 0 then return 4 end
	if enemy and enemy:getArmor() and enemy:hasArmorEffect(enemy:getArmor():objectName()) then return 3 end
end

function sgs.ai_slash_weaponfilter.TreasuredSword(self, enemy, player)
	if player:distanceTo(enemy) > math.max(sgs.weapon_range.TreasuredSword, player:getAttackRange()) then return end
	if enemy:getArmor() and enemy:hasArmorEffect(enemy:getArmor():objectName())
		and (sgs.card_lack[enemy:objectName()] == 1 or getCardsNum("Jink", enemy, self.player) < 1) then
		return true
	end
	if enemy:getLostHp() == 0 then return true end
end


sgs.ai_skill_invoke.steel_spear = function(self, data)
	
	return false
end

sgs.ai_use_priority.SteelSpear = 2.645
sgs.weapon_range.SteelSpear = 3

function sgs.ai_weapon_value.SteelSpear(self, enemy)
	if enemy and not self:doNotDiscard(enemy, "h") then return 3 end
end

function sgs.ai_slash_weaponfilter.SteelSpear(self, enemy, player)
	if player:distanceTo(enemy) > math.max(sgs.weapon_range.SteelSpear, player:getAttackRange()) then return end
	if enemy and not self:doNotDiscard(enemy, "h") then
		return true
	end
end

sgs.ai_skill_use["Escape"] = function(self, prompt, method)
	local cards = sgs.QList2Table(self.player:getHandcards())
	self:sortByUseValue(cards)
	for _, card in ipairs(cards) do
		if card:isKindOf("Escape") then
			return card:toString()
		end
	end
	return "."
end

function SmartAI:useCardThrowEquips(card, use)
	
end

sgs.ai_use_value.ThrowEquips = 5.6
sgs.ai_use_priority.ThrowEquips = 4.4
sgs.ai_keep_value.ThrowEquips = 3.44

sgs.dynamic_value.control_card.ThrowEquips = true




