BinghuoSlash = function(self, card, target, source, value)
	local slash = sgs.Sanguosha:cloneCard("slash", sgs.Card_SuitToBeDecided, -1)
	slash:setSkillName("_ss_binghuo")
	if card then
	    slash:addSubcard(card)
	end
	if not source then
	    source = self.player
	end
	if source:canSlash(target, slash, false) then
	    if value then
		    if value == 0 then
		        if not self:slashIsEffective(slash, target, source) then
			        return slash:toString() .. "->" .. target:objectName()
			    else
				    return false
				end
		    elseif value == 1 then
	            if self:slashIsEffective(slash, target, source) then
			        return slash:toString() .. "->" .. target:objectName()
			    else
				    return false
				end
		    end
		end
		return slash:toString() .. "->" .. target:objectName()
	end
	return false
end


sgs.ai_skill_use["@@binghuo_slash"] = function(self, prompt)
	local cards = sgs.QList2Table(self.player:getHandcards())
    self:sortByKeepValue(cards)
	local source = findPlayerByObjectName(self.room, prompt:split(":")[2])
	local blank_slash = sgs.Sanguosha:cloneCard("slash", sgs.Card_NoSuit, 0)
	if not source or source:isDead() or not source:canSlash(self.player, nil, false) or not self:slashIsEffective(blank_slash, self.player, source) then
	    local card
		if self:needToThrowArmor() then
		    card = self.player:getArmor()
		else
		    local slash = sgs.Sanguosha:cloneCard("slash")
		    for _, c in ipairs(cards) do
			    if self:getKeepValue(c, cards) <= self:getKeepValue(slash, cards) then
			    	card = c
					break
				end
		    end
		end
		if not card then
		    card = self.player:getOffensiveHorse()
		end
		if card then
		    local slash = sgs.Sanguosha:cloneCard("slash", sgs.Card_SuitToBeDecided, -1)
	        slash:setSkillName("_ss_binghuo")
			slash:addSubcard(card)
		    local dummy_use = { isDummy = true, to = sgs.SPlayerList() }
	        self:useBasicCard(slash, dummy_use)
	        if dummy_use and dummy_use.to then
	            return slash:toString() .. "->" .. dummy_use.to:first():objectName()
	        end
		end
		return "."
	end
	local friends, enemies, others = {}, {}, {}
	for _, p in sgs.qlist(self.room:getOtherPlayers(self.player)) do
	    if p:hasFlag("SlashAssignee") then
	        if self:isEnemy(p) then
			    table.insert(enemies, p)
			elseif self:isFriend(p) then
			    table.insert(friends, p)
			else
			    table.insert(others, p)
			end
	    end
	end
	self:sort(enemies, "defenseSlash")
	self:sort(friends, "defenseSlash")
	friends = sgs.reverse(friends)
	local new = {}
	while #others > 0 do
		local value = others[math.random(1,#others)]
		table.insert(new, value)
		table.removeOne(others, value)
	end
	others = new
	for i = 1, 2, 1 do
	    for _, p in ipairs(enemies) do
	        if self:needToThrowArmor() then
	            local str = BinghuoSlash(self, self.player:getArmor(), p, self.player, i)
			    if str then
			        return str
			    end
		    end
	        for _, c in ipairs(cards) do
			    if self:getKeepValue(c, cards) <= self:getKeepValue(blank_slash, cards) or self:isWeak(p) then
			        local str = BinghuoSlash(self, c, p, self.player, i)
			        if str then
			            return str
			        end
		        end
		    end
		    if self.player:getOffensiveHorse() then
		        local str = BinghuoSlash(self, self.player:getOffensiveHorse(), p, self.player, i)
			    if str then
			        return str
			    end
		    end
		    if self.player:getWeapon() then
		        local str = BinghuoSlash(self, self.player:getWeapon(), p, self.player, i)
			    if str then
			        return str
			    end
		    end
	    end
	end
	    for _, p in ipairs(others) do
	        if self:needToThrowArmor() then
	            local str = BinghuoSlash(self, self.player:getArmor(), p, self.player, 2)
			    if str then
			        return str
			    end
		    end
	        for _, c in ipairs(cards) do
			    if self:getKeepValue(c, cards) <= self:getKeepValue(blank_slash, cards) then
			        local str = BinghuoSlash(self, c, p, self.player, 2)
			        if str then
			            return str
			        end
		        end
		    end
		    if self.player:getOffensiveHorse() then
		        local str = BinghuoSlash(self, self.player:getOffensiveHorse(), p, self.player, 2)
			    if str then
			        return str
			    end
		    end
		    if self.player:getWeapon() then
		        local str = BinghuoSlash(self, self.player:getWeapon(), p, self.player, 2)
			    if str then
			        return str
			    end
		    end
	    end
	    for _, p in ipairs(friends) do
	        if self:needToThrowArmor() then
	            local str = BinghuoSlash(self, self.player:getArmor(), p, self.player, 0)
			    if str then
			        return str
			    end
		    end
	        for _, c in ipairs(cards) do
			    if self:getKeepValue(c, cards) <= self:getKeepValue(blank_slash, cards) then
			        local str = BinghuoSlash(self, c, p, self.player, 0)
			        if str then
			            return str
			        end
		        end
		    end
		    if self.player:getOffensiveHorse() then
		        local str = BinghuoSlash(self, self.player:getOffensiveHorse(), p, self.player, 0)
			    if str then
			        return str
			    end
		    end
		    if self.player:getWeapon() then
		        local str = BinghuoSlash(self, self.player:getWeapon(), p, self.player, 0)
			    if str then
			        return str
			    end
		    end
	    end
	if self:isWeak() then
	    local all = {}
	    for _, p in ipairs(enemies) do
		    table.insert(all, p)
		end
		for _, p in ipairs(others) do
		    table.insert(all, p)
		end
		for _, p in ipairs(friends) do
		    table.insert(all, p)
		end
	    for _, p in ipairs(all) do
	        if self:needToThrowArmor() then
	            local str = BinghuoSlash(self, self.player:getArmor(), p, self.player, 2)
			    if str then
			        return str
			    end
		    end
	        for _, c in ipairs(cards) do
			    if not self:isValuableCard(c) or self:isEnemy(p) then
				    local str = BinghuoSlash(self, c, p, self.player, 2)
			        if str then
			            return str
			        end
				end
		    end
		    if self.player:getOffensiveHorse() then
		        local str = BinghuoSlash(self, self.player:getOffensiveHorse(), p, self.player, 2)
			    if str then
			        return str
			    end
		    end
		    if self.player:getWeapon() then
		        local str = BinghuoSlash(self, self.player:getWeapon(), p, self.player, 2)
			    if str then
			        return str
			    end
		    end
	    end
	end
	return "."
end



sgs.ai_skill_invoke.jimeng_ask = function(self, data)
	local prompt = data:toString()
	local source = findPlayerByObjectName(self.room, prompt:split(":")[2])
	if self:isEnemy(source) then return false end
	if not self:toTurnOver(self.player, 0, "ss_jimeng") then
	    return true
	end
	if self:isWeak() then return true end
	local damage = self.player:getTag("JimengDamage"):toDamage()
	return not self:needToLoseHp(self.player, damage.from)
end


sgs.ai_skill_invoke.huoxin_slash = function(self, data)
	local prompt = data:toString()
	local source = findPlayerByObjectName(self.room, prompt:split(":")[2])
	local target = findPlayerByObjectName(self.room, prompt:split(":")[3])
	if not target then return false end
	if not source then
	    return self:isEnemy(target)
	else
	    if self:isFriend(source) then
		    return self:isEnemy(target)
		else
		    return not self:isFriend(target) or not self:isWeak(target)
		end
	end
	return false
end

sgs.ai_skill_invoke.yinjie_draw = function(self, data)
	local prompt = data:toString()
	local target = findPlayerByObjectName(self.room, prompt:split(":")[2])
	if self:isFriend(target) and not self:needKongcheng(target)then
		return true
	else
		if not (target:getPhase() ~= sgs.Player_NotActive and (target:hasSkills(sgs.Active_cardneed_skill) or target:hasWeapon("Crossbow")))
			    and not (target:getPhase() == sgs.Player_NotActive and target:hasSkills(sgs.notActive_cardneed_skill))
			    or self:needKongcheng(target) then
			return true
		end
	end
end











































