sgs.ai_skill_invoke.st_tuogu = true

sgs.ai_skill_invoke.tuogu_invoke = true

sgs.ai_skill_invoke.st_xingbing = sgs.ai_skill_invoke.niepan

sgs.ai_skill_choice.xingbing_recover = function(self, choices)
    local x = self.player:getMaxHp()
	local y = self.player:getHp()
	local i = math.floor((x + y) / 2)
	if i >= x then
	    i = x - 1
	end
	return tostring(i)
end

sgs.ai_skill_invoke.st_xuehen = function(self, data)
	local target = data:toPlayer()
	if self:isEnemy(target) then
	    if not self:damageIsEffective(target, sgs.DamageStruct_Normal, self.player) then return false end
		if not self:isWeak() and self:isWeak(target) then return true end
		if self.player:getHp() > 2 and not self:needToLoseHp(target, self.player) then return true end
	end
	return false
end


sgs.ai_skill_discard.st_dingpin = function(self, discard_num, min_num, optional, include_equip)
    if not self:toTurnOver(self.player) then return {} end
	return self:askForDiscard("dummy", discard_num, min_num, false, include_equip)
end
