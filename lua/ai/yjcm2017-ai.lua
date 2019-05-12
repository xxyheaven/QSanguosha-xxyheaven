--辛宪英


--吴苋







--徐氏

--问卦
--1.自己发动问卦

local wengua_skill = {}
wengua_skill.name = "wengua"
table.insert(sgs.ai_skills, wengua_skill)
wengua_skill.getTurnUseCard = function(self)
	if not self.player:hasUsed("WenguaCard") then
		return sgs.Card_Parse("@WenguaCard=.")
	end
end

sgs.ai_skill_use_func.WenguaCard = function(card, use, self)

	local zhiheng = sgs.Card_Parse("@ZhihengCard=.")
	local dummy_use = { isDummy = true }
	self:useSkillCard(zhiheng, dummy_use)
	if dummy_use.card then
		use.card = sgs.Card_Parse("@WenguaCard=" .. dummy_use.card:getSubcards():first() .. ":bottom")
		return
	end
end

sgs.ai_use_priority.WenguaCard = 2.61
sgs.dynamic_value.benefit.WenguaCard = true



--2.其他角色发动问卦

local wengua_attach_skill = {}
wengua_attach_skill.name = "wengua_attach"
table.insert(sgs.ai_skills, wengua_attach_skill)
wengua_attach_skill.getTurnUseCard = function(self)
	return sgs.Card_Parse("@WenguaAttachCard=.")
end

sgs.ai_skill_use_func.WenguaAttachCard = function(card, use, self)
	local xushis = {}
	for _, p in sgs.qlist(self.room:getOtherPlayers(self.player)) do
		if p:hasSkill("wengua") and not p:hasFlag("WenguaInvoked") and self:isFriend(p) then
			table.insert(xushis, p)
		end
	end
	if #xushis == 0 then return end
	
	
	local zhiheng = sgs.Card_Parse("@ZhihengCard=.")
	local dummy_use = { isDummy = true }
	self:useSkillCard(zhiheng, dummy_use)
	if dummy_use.card then
		use.card = sgs.Card_Parse("@WenguaAttachCard=" .. dummy_use.card:getSubcards():first())
		if use.to then use.to:append(xushis[1]) end
		return
	end
end

sgs.ai_use_priority.WenguaAttachCard = 2.61
sgs.dynamic_value.benefit.WenguaAttachCard = true


--3.选择将别人给的问卦牌放于牌堆顶/底

sgs.ai_skill_choice.wengua = function(self, choices)
	local target = self.room:getCurrent()
	local card = self.player:getMark("wenguacard_id")

	if not self:isEnemy(target) then
		return "bottom"
	end

	return "cancel"
end





--伏诛

sgs.ai_skill_invoke.fuzhu = function(self, data)
	local target = self.room:getCurrent()
	if target and self:isEnemy(target) and not self:slashProhibit(nil, target) then
		return true
	end
	return false
end





















