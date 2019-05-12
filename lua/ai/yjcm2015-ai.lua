sgs.ai_skill_playerchosen.huituo = function(self, targets)
    return self.player
	--self:sort(self.friends, "defense")
    --return self.friends[1]
end

sgs.ai_judge_value.huituo = function(self, card, target)
    if card:isRed() then 
        if not target:isWounded() then return 0 end
        return 2 
    end
    if card:isBlack() then
        if card:getSuit() == sgs.Card_Spade and target:hasSkill("hongyan") then 
            if not target:isWounded() then return 0 end
            return 2 
        end
        return 1
    end
    return 0
end

sgs.ai_playerchosen_intention.huituo = -80

local mingjian_skill = {}
mingjian_skill.name = "mingjian"
table.insert(sgs.ai_skills, mingjian_skill)
mingjian_skill.getTurnUseCard = function(self)
	if self.player:hasUsed("MingjianCard") or self.player:isKongcheng() then return end
	if self:needBear() then return end
	local parsed_card = sgs.Card_Parse("@MingjianCard=.")
	return parsed_card
end

sgs.ai_skill_use_func.MingjianCard = function(card, use, self)
	local target
	self:sort(self.friends_noself, "defense")

	for _, friend in ipairs(self.friends_noself) do
		if not friend:hasSkill("manjuan") then
			target = friend
			break
		end
	end
	if target then
		for _, acard in sgs.qlist(self.player:getHandcards()) do
			if isCard("Peach", acard, self.player) and self.player:getHandcardNum() > 1 and self.player:isWounded() and not self:needToLoseHp(self.player) then
				use.card = acard
				return
			end
		end
		use.card = card
		if use.to then
			use.to:append(target)
		end
	end
end

sgs.ai_use_priority.MingjianCard = 1.5
sgs.ai_card_intention.MingjianCard = 0
sgs.ai_playerchosen_intention.mingjian = -80

sgs.ai_skill_invoke.xingshuai = sgs.ai_skill_invoke.niepan

sgs.ai_skill_invoke._xingshuai = function(self)
    local lord = self.room:getCurrentDyingPlayer()
    local save = false
    if lord and lord:getLostHp() > 0 then
        if self:isFriend(lord) then
            save = true
        elseif self.role == "renegade" and lord:isLord() and self.room:alivePlayerCount() > 2 then
            if lord:getHp() <= 0 then
                save = true
            end
        end
    end
    if save then
        local hp_max = lord:getHp() + self:getAllPeachNum(lord)
        local must_save = ( hp_max <= 0 )
        local others = self.room:getOtherPlayers(lord)
        others:removeOne(self.player)
        local count = 0
        for _,p in sgs.qlist(others) do
            if p:getKingdom() == "wei" and p:getMark("xingshuai_act") == 0 then
                if not self:isEnemy(p, lord) then
                    count = count + 1
                end
            end
        end
        local am_safe = false
        if self.player:getHp() > 1 then
            am_safe = true
        elseif self.player:hasSkill("buqu") then
            am_safe = true
        elseif self.player:hasSkill("nosbuqu") then
            am_safe = true
        elseif self.player:hasSkill("niepan") and self.player:getMark("@nirvana") > 0 then
            am_safe = true
        elseif self.player:hasSkill("fuli") and self.player:getMark("@laoji") > 0 then
            am_safe = true
        elseif self.player:hasSkill("zhichi") and self.player:getMark("@late") > 0 then
            am_safe = true
        elseif self.player:hasSkill("fenyong") and self.player:getMark("@fenyong") > 0 then
            am_safe = true
        elseif self.player:getMark("@fog") > 0 then
            am_safe = true
        elseif self.player:hasSkill("sizhan") then
            am_safe = true
        elseif self:getAllPeachNum() > 0 then
            am_safe = true
        end
        if self:hasSkills(sgs.masochism_skill) and am_safe then
            return true
        elseif must_save then
            if am_safe then
                return true
            elseif hp_max + count <= 0 then
                return true
            elseif self.role == "renegade" or self.role == "lord" then
                return false
            end
        end
        if am_safe then
            if getBestHp(self.player) > self.player:getHp() then
                return true
            end
        elseif self.player:hasSkill("wuhun") and self:needDeath(self.player) then
            if self.role ~= "renegade" and self.role ~= "lord" then
                return true
            end
        end
    end
    return false
end
sgs.ai_choicemade_filter["skillInvoke"]["_xingshuai"] = function(self, player, promptlist)
    if #promptlist == "yes" then
        local lord = self.room:getCurrentDyingPlayer()
        if lord and not self:hasSkills(sgs.masochism_skill, player) then
            sgs.updateIntention(player, lord, -60)
        end
    end
end

--HuaiyiCard:Play
huaiyi_skill = {
    name = "huaiyi",
    getTurnUseCard = function(self, inclusive)
        if self.player:hasUsed("HuaiyiCard") or self.player:isKongcheng() then
            return nil
        end
        local handcards = self.player:getHandcards()
        local red, black = false, false
        for _,c in sgs.qlist(handcards) do
            if c:isRed() and not red then
                red = true
                if black then
                    break
                end
            elseif c:isBlack() and not black then
                black = true
                if red then
                    break
                end
            end
        end
        if red and black then
            return sgs.Card_Parse("@HuaiyiCard=.")
        end
    end,
}
table.insert(sgs.ai_skills, huaiyi_skill)

local function remove_repeat(target_table)
	local new_table = {}
	for _, p in ipairs(target_table) do
		if not table.contains(new_table, p) then
			table.insert(new_table, p)
		end
	end
	return new_table
end

sgs.ai_skill_use_func["HuaiyiCard"] = function(card, use, self)
    local handcards = sgs.QList2Table(self.player:getHandcards())
	self:sortByUseValue(handcards, true)
    local reds, blacks = {}, {}
    for _,c in ipairs(handcards) do
        local dummy_use = {
            isDummy = true,
        }
        if c:isKindOf("BasicCard") then
            self:useBasicCard(c, dummy_use)
        elseif c:isKindOf("EquipCard") then
            self:useEquipCard(c, dummy_use)
        elseif c:isKindOf("TrickCard") then
            self:useTrickCard(c, dummy_use)
        end
        if dummy_use.card then
            return --It seems that self.player should use this card first.
        end
        if c:isRed() then
            table.insert(reds, c)
        else
            table.insert(blacks, c)
        end
    end

    local targets = self:findPlayerToDiscard("he", false, false, nil, true)
	targets = remove_repeat(targets)

    local n_reds, n_blacks, n_targets = #reds, #blacks, #targets
    if n_targets == 0 then
        return 
    elseif n_reds - n_targets >= 2 and n_blacks - n_targets >= 2 and handcards:length() - n_targets >= 5 then
        return 
    end
    --[[------------------
        Haven't finished.
    ]]--------------------
    use.card = card
end
--room->askForChoice(source, "huaiyi", "black+red")
sgs.ai_skill_choice["huaiyi"] = function(self, choices, data)
    local choice = sgs.huaiyi_choice
    if choice then
        sgs.huaiyi_choice = nil
        return choice
    end
    local handcards = self.player:getHandcards()
    local reds, blacks = {}, {}
    for _,c in sgs.qlist(handcards) do
        if c:isRed() then
            table.insert(reds, c)
        else
            table.insert(blacks, c)
        end
    end
    if #reds < #blacks then
        return "red"
    else
        return "black"
    end
end
--room->askForUseCard(source, "@@huaiyi", "@huaiyi:::" + QString::number(n), -1, Card::MethodNone);
sgs.ai_skill_use["@@huaiyi"] = function(self, prompt, method)
    local n = self.player:getMark("huaiyi_num")
    if n >= 2 then
        if self:needToLoseHp() then
        elseif self:isWeak() then
            n = 1
        end
    end
    if n == 0 then
        return "."
    end
    local targets = self:findPlayerToDiscard("he", false, false, nil, true)
	targets = remove_repeat(targets)
    local names = {}
    for index, target in ipairs(targets) do
        if index <= n then
            table.insert(names, target:objectName())
        else
            break
        end
    end
    return "@HuaiyiSnatchCard=.->"..table.concat(names, "+")
end

sgs.ai_skill_invoke.jigong = function(self)
    if self.player:isKongcheng() then return true end
    for _, c in sgs.qlist(self.player:getHandcards()) do
        local x = nil
        if isCard("ArcheryAttack", c, self.player) then
            x = sgs.Sanguosha:cloneCard("ArcheryAttack")
        elseif isCard("SavageAssault", c, self.player) then
            x = sgs.Sanguosha:cloneCard("SavageAssault")
        else continue end

        local du = { isDummy = true }
        self:useTrickCard(x, du)
        if (du.card) then return true end
    end

    return false
end

function sgs.ai_cardsview_valuable.shifei(self, class_name, player)
	if player:hasFlag("Global_ShifeiFailed") or class_name ~= "Jink" then return end
	local l = {}
    for _, p in sgs.qlist(self.room:getAlivePlayers()) do
        l[p:objectName()] = p:getHandcardNum()
        if (p:objectName() == self.room:getCurrent():objectName()) then
            l[p:objectName()] = p:getHandcardNum() + 1
        end
    end

    local most = {}
    for k, t in pairs(l) do
        if #most == 0 then
            table.insert(most, k)
            continue
        end

        if (t > l[most[1]]) then
            most = {}
        end

        table.insert(most, k)
    end

	local invoke_fail = table.contains(most, self.room:getCurrent():objectName()) and #most == 1
    if invoke_fail then
		if self:isFriend(self.room:getCurrent()) then
			return "@ShifeiCard=."
		end
    else
		for _, p in ipairs(most) do
			local _p = findPlayerByObjectName(self.room, p)
			if _p and self:isEnemy(_p) and not self:doNotDiscard(_p, "he", true, 1) then return "@ShifeiCard=." end
		end
		if self:getCardsNum("Jink") == 0 then
		    return "@ShifeiCard=."
		end
	end

end

sgs.ai_skill_playerchosen.shifei = function(self, targets)
    local target = self:findPlayerToDiscard("he", true, true, targets, false)
	if target then
		return target
	end
end

function sgs.ai_slash_prohibit.shifei(self, from, to)
	if self:isFriend(to, from) then return false end
	local current = self.room:getCurrent()
	local l = {}
    for _, p in sgs.qlist(self.room:getAlivePlayers()) do
        l[p:objectName()] = p:getHandcardNum()
        if (p:objectName() == current:objectName()) then
            l[p:objectName()] = p:getHandcardNum() + 1
        end
    end

    local most = {}
    for k, t in pairs(l) do
        if #most == 0 then
            table.insert(most, k)
            continue
        end

        if (t > l[most[1]]) then
            most = {}
        end

        table.insert(most, k)
    end
	if table.contains(most, current:objectName()) and #most == 1 then return false end

    for _, p in ipairs(most) do
        local _p = findPlayerByObjectName(self.room, p)
		if _p and self:isFriend(_p) then
			return true
		end
    end

	return false
end



zhanjue_skill = {name = "zhanjue"}
table.insert(sgs.ai_skills, zhanjue_skill)
zhanjue_skill.getTurnUseCard = function(self)
    if (self.player:getMark("zhanjuedraw") >= 2) then return nil end

    if (self.player:isKongcheng()) then return nil end
    
    local cards = self.player:getHandcards()
	cards = sgs.QList2Table(cards)

    for _, card in ipairs(cards) do
        if card:isKindOf("Analeptic") or (card:isKindOf("Peach") and self.player:isWounded()) then
            return card
        end
	end

    local duel = sgs.Sanguosha:cloneCard("duel", sgs.Card_SuitToBeDecided, -1)
    duel:addSubcards(self.player:getHandcards())
    duel:setSkillName("zhanjue")

    return duel
end

sgs.ai_skill_cardask["@qinwang-discard"]=function(self)
    local cards = sgs.QList2Table(self.player:getHandcards())
    for _,card in ipairs(cards) do
        if isCard("Slash", card, self.player) or isCard("Peach", card, self.player) then
            return "."
        end
    end
    table.insert(cards, sgs.QList2Table(self.player:getEquips()))
    local to_discard = self:askForDiscard("dummyreason", 1, 1, false, true)
    if #to_discard > 0 then return "$" .. to_discard[1] end
end

-- zhenshan buhui!!!

sgs.ai_skill_playerchosen.yaoming = function(self, targets)
    return nil
end


yanzhu_skill = {name = "yanzhu"}
table.insert(sgs.ai_skills, yanzhu_skill)
yanzhu_skill.getTurnUseCard = function(self)
    if (self.player:hasUsed("YanzhuCard")) then return nil end
    return sgs.Card_Parse("@YanzhuCard=.")
end


sgs.ai_skill_use_func.YanzhuCard = function(card, use, self)

    self:sort(self.enemies, "threat")
    for _, p in ipairs(self.enemies) do
        if not p:isNude() and not self:doNotDiscard(p, "he", true, 1) and p:getEquips():length() ~= 1 then
            use.card = card
            if (use.to) then use.to:append(p) end
            -- use.from = self.player
            return
        end
    end

    for _, p in ipairs(self.friends_noself) do
        if self:needToThrowArmor(p) and p:getArmor() and not p:isJilei(p:getArmor()) then
            use.card = card
            if (use.to) then use.to:append(p) end
            -- use.from = self.player
            return
        end
    end

    return nil
end

sgs.ai_use_priority.YanzhuCard = sgs.ai_use_priority.Dismantlement - 0.1

sgs.ai_skill_discard.yanzhu = function(self, _, __, optional)
    if not optional then return self:askForDiscard("dummyreason", 1, 1, false, true) end

    if self:needToThrowArmor() and self.player:getArmor() and not self.player:isJilei(self.player:getArmor()) then return self.player:getArmor():getEffectiveId() end

    if self.player:getTreasure() then
        if (self.player:getCardCount() == 1) then return self.player:getTreasure():getEffectiveId()
        elseif not self.player:isKongcheng() then return self:askForDiscard("dummyreason", 1, 1, false, false) end
    end

    if self.player:getEquips():length() > 2 and not self.player:isKongcheng() then return self:askForDiscard("dummyreason", 1, 1, false, false) end

    if self.player:getEquips():length() == 1 then return {} end

     return self:askForDiscard("dummyreason", 1, 1, false, true)
end

sgs.ai_skill_use["@@xingxue"] = function(self)

    local n = (self.player:hasSkill("yanzhu", true)) and self.player:getHp() or self.player:getMaxHp()

    self:sort(self.friends, "handcard")
	self.friends = sgs.reverse(self.friends)

    n = math.min(n, #self.friends)

    local l = sgs.SPlayerList()
    local s = {}
    for i = 1, n, 1 do
        l:append(self.friends[i])
        table.insert(s, self.friends[i]:objectName())
    end

    if #s == 0 then return "." end

    sgs.xingxuelist = l

    return "@XingxueCard=.->" .. (table.concat(s, "+"))
end

-- 兴学剩下的部分大家想象吧，我懒得想
-- sgs.ai_skill_discard.xingxue = function(self) end

sgs.ai_skill_invoke.qiaoshi = true

yanyu_skill = {name = "yanyu"}
table.insert(sgs.ai_skills, yanyu_skill)
yanyu_skill.getTurnUseCard = function(self)
    return sgs.Card_Parse("@YanyuCard=.")
end

sgs.ai_skill_use_func.YanyuCard = function(card, use, self)
    local n = self.player:getMark("yanyu")

    if n >= 2 then return nil end

    local ns, fs, ts = {}, {}, {}
    for _, c in sgs.qlist(self.player:getHandcards()) do
        if (c:isKindOf("Slash")) then
            n = n + 1
            if (c:isKindOf("FireSlash")) then
                table.insert(fs, c)
            elseif (c:isKindOf("ThunderSlash")) then
                table.insert(ts, c)
            else
                table.insert(ns, c)
            end
        end
    end

    if n < 2 then return nil end

    local hasmale = false
    for _, p in ipairs(self.friends_noself) do
        if (p:isMale()) then
            hasmale = true
            break
        end
    end

    if not hasmale then return nil end

    local id = nil
    if #ns > 0 then
        id = ns[1]:getEffectiveId()
    elseif #ts > 0 then
        id = ts[1]:getEffectiveId()
    elseif #fs > 0 then
        id  = fs[1]:getEffectiveId()
    end

    if not id then return nil end

    use.card = sgs.Card_Parse("@YanyuCard=" .. tostring(id))
end

sgs.ai_skill_playerchosen.yanyu = function(self)

    self:sort(self.friends_noself, "handcard")

    for i = #self.friends_noself, 1, -1 do
        if self.friends_noself[i]:isMale() then
            return self.friends_noself[i]
        end
    end

    return nil
end

sgs.ai_use_priority.YanyuCard = sgs.ai_use_priority.Slash - 0.01

-- furong 懒得想
-- 出牌阶段限一次，你可以令一名其他角色与你同时展示一张手牌：
-- 若你展示的是【杀】且该角色展示的不是【闪】，则你弃置此【杀】并对其造成1点伤害；
-- 若你展示的不是【杀】且该角色展示的是【闪】，则你弃置你展示的牌并获得其一张牌。 

function getKnownHandcards(player, target)
    local handcards = target:getHandcards()
    
    if player:objectName() == target:objectName() then
        return sgs.QList2Table(handcards), {}
    end
    
    local dongchaee = global_room:getTag("Dongchaee"):toString() or ""
    if dongchaee == target:objectName() then
        local dongchaer = global_room:getTag("Dongchaer"):toString() or ""
        if dongchaer == player:objectName() then
            return sgs.QList2Table(handcards), {}
        end
    end
    
    local knowns, unknowns = {}, {}
    local flag = string.format("visible_%s_%s", player:objectName(), target:objectName())
    for _,card in sgs.qlist(handcards) do
        if card:hasFlag("visible") or card:hasFlag(flag) then
            table.insert(knowns, card)
        else
            table.insert(unknowns, card)
        end
    end
    return knowns, unknowns
end

furong_skill = {name = "furong"}
table.insert(sgs.ai_skills, furong_skill)
furong_skill.getTurnUseCard = function(self)
    if self.player:hasUsed("FurongCard") or self.player:isKongcheng() then return nil end
    return sgs.Card_Parse("@FurongCard=.")
end

sgs.ai_skill_use_func.FurongCard = function(card, use, self)
    local handcards = self.player:getHandcards()
    local my_slashes, my_cards = {}, {}
    for _,c in sgs.qlist(handcards) do
        if c:isKindOf("Slash") then
            table.insert(my_slashes, c)
        else
            table.insert(my_cards, c)
        end
    end
    local no_slash = ( #my_slashes == 0 ) 
    local all_slash = ( #my_cards == 0 ) 
    local need_slash, target = nil, nil
    
    --自己展示的一定不是【杀】，目标展示的必须是【闪】，方可获得目标一张牌
    if no_slash then
        local others = self.room:getOtherPlayers(self.player)
        local targets = self:findPlayerToDiscard("he", false, false, others, true)
        for _,p in ipairs(targets) do
            if not p:isKongcheng() then
                local knowns, unknowns = getKnownHandcards(self.player, p)
                if #unknowns == 0 then
                    local all_jink = true
                    for _,jink in ipairs(knowns) do
                        if not jink:isKindOf("Jink") then
                            all_jink = false
                            break
                        end
                    end
                    if all_jink then
                        need_slash, target = false, p
                        break
                    end
                end
            end
        end
    end
    
    --自己展示的一定是【杀】，目标展示的不是【闪】时，可对目标造成1点伤害
    if all_slash and not target then
        local targets = self:findPlayerToDamage(1, self.player, nil, nil, false, 5, true)
        for _,p in ipairs(targets) do
            if not p:isKongcheng() then
                local knowns, unknowns = getKnownHandcards(self.player, p)
                if self:isFriend(p) then
                    local all_jink = true
                    if #unknowns == 0 then
                        for _,c in ipairs(knowns) do
                            if not c:isKindOf("Jink") then
                                all_jink = false
                                break
                            end
                        end
                    else
                        all_jink = false --队友会配合不展示【闪】的
                    end
                    if not all_jink then
                        need_slash, target = true, p
                        break
                    end
                else
                    local all_jink = false
                    if #unknowns == 0 then
                        for _,c in ipairs(knowns) do
                            if c:isKindOf("Jink") then
                                all_jink = true
                                break
                            end
                        end
                    end
                    if not all_jink then
                        need_slash, target = true, p
                        break
                    end
                end
            end
        end
    end
    
    --自己展示的不一定是【杀】，可根据目标情况决定展示的牌
    if not target then
        local friends, enemies, others = {}, {}, {}
        local other_players = self.room:getOtherPlayers(self.player)
        for _,p in sgs.qlist(other_players) do
            if not p:isKongcheng() then
                if self:isFriend(p) then
                    table.insert(friends, p)
                elseif self:isEnemy(p) then
                    table.insert(enemies, p)
                else
                    table.insert(others, p)
                end
            end
        end
        
        local to_damage = self:findPlayerToDamage(1, self.player, nil, enemies, false, 5, true)
        for _,enemy in ipairs(to_damage) do
            local knowns, unknowns = getKnownHandcards(self.player, enemy)
            local no_jink = true
            if #unknowns == 0 then
                for _,jink in ipairs(knowns) do
                    if jink:isKindOf("Jink") then
                        no_jink = false
                        break
                    end
                end
            else
                no_jink = false
            end
            if no_jink then
                need_slash, target = true, enemy
                break
            end
        end
        
        if not target then
            local other_players = self.room:getOtherPlayers(self.player)
            local to_obtain = self:findPlayerToDiscard("he", false, false, other_players, true)
            for _,p in ipairs(to_obtain) do
                if not p:isKongcheng() then
                    local knowns, unknowns = getKnownHandcards(self.player, p)
                    if self:isFriend(p) then
                        local has_jink = false
                        for _,jink in ipairs(knowns) do
                            if jink:isKindOf("Jink") then
                                has_jink = true
                                break
                            end
                        end
                        if has_jink then
                            need_slash, target = false, p
                            break
                        end
                    else
                        local all_jink = true
                        if #unknowns == 0 then
                            for _,c in ipairs(knowns) do
                                if not c:isKindOf("Jink") then
                                    all_jink = false
                                    break
                                end
                            end
                        else
                            all_jink = false
                        end
                        if all_jink then
                            need_slash, target = false, p
                            break
                        end
                    end
                end
            end
        end
        
        if not target then
            to_damage = self:findPlayerToDamage(1, self.player, nil, friends, false, 25, true)
            for _,friend in ipairs(to_damage) do
                local knowns, unknowns = getKnownHandcards(self.player, friend)
                local all_jink = true
                for _,c in ipairs(knowns) do
                    if not c:isKindOf("Jink") then
                        all_jink = false
                        break
                    end
                end
                if not all_jink then
                    need_slash, target = true, friend
                    break
                end
            end
        end
        
        if not target then
            local victim = self:findPlayerToDamage(1, self.player, nil, others, false, 5)
            if victim then
                need_slash, target = true, victim
            end
        end
        
        --只是为了看牌……
        if not target and #my_cards > 0 and self:getOverflow() > 0 then
            if #enemies > 0 then
                self:sort(enemies, "handcard")
                need_slash, target = false, enemies[1]
            elseif #others > 0 then
                self:sort(others, "threat")
                need_slash, target = false, others[1]
            end
        end
        
        if not target and #enemies > 0 then
            self:sort(enemies, "defense")
            need_slash, target = (math.random(0, 1) == 0), enemies[1]
        end
    end
    
    if target and not target:isKongcheng() then
        local use_cards = need_slash and my_slashes or my_cards
        if #use_cards > 0 then
            self:sortByUseValue(use_cards)
			sgs.furong_card = use_cards[1]
            use.card = card
            if use.to then
                use.to:append(target)
            end
        end
    end
end
sgs.ai_use_priority["FurongCard"] = 3.1
sgs.ai_use_value["FurongCard"] = 4.5

sgs.ai_cardshow.furong = function(self, requestor)
    if self.player:objectName() == requestor:objectName() then
        local card = sgs.furong_card
        assert(card)
        return card
    else
        local handcards = sgs.QList2Table(self.player:getHandcards())
        self:sortByKeepValue(handcards)
        local my_jinkes, my_cards = {}, {}
        for _,c in ipairs(handcards) do
            if c:isKindOf("Jink") then
                table.insert(my_jinkes, c)
            else
                table.insert(my_cards, c)
            end
        end
        if #my_jinkes == 0 or #my_cards == 0 then
            return handcards[1]
        end
        if self:isWeak() then return my_jinkes[1] end
        if math.random(0, 4) < 2 then
            return my_cards[1]
        else
            return my_jinkes[1]
        end
    end
end

-- huomo buhui !!!
-- 每当你需要使用一张你于此回合内未使用过的基本牌时，你可以将一张黑色非基本牌置于牌堆顶，然后视为你使用了此基本牌。
sgs.ai_cardsview_valuable.huomo = function(self, class_name, player) 
    if sgs.Sanguosha:getCurrentCardUseReason() ~= sgs.CardUseStruct_CARD_USE_REASON_RESPONSE_USE then return nil end
	local pattern = nil
    if class_name == "Slash" and player:getMark("Huomo_Slash") == 0 then
        pattern = "slash"
    elseif class_name == "Jink" and player:getMark("Huomo_Jink") == 0 then
        pattern = "jink"
    elseif class_name == "Peach" and player:getMark("Huomo_Peach") == 0 then
        pattern = "peach"
    elseif class_name == "Analeptic" and player:getMark("Huomo_Analeptic") == 0 then
        pattern = "analeptic"
    end
    if pattern then
        local cards = player:getCards("he")
        local blacks = {}
        for _,c in sgs.qlist(cards) do
            if c:isKindOf("BasicCard") then
            elseif c:isBlack() then
                table.insert(blacks, c)
            end
        end
        if #blacks > 0 then
            local card = blacks[1]
            if card:isKindOf(class_name) then
            else
                local card_str = "@HuomoCard="..card:getEffectiveId()..":"..pattern
                return card_str
            end
        end
    end
end

function SmartAI:findSlashKindToUse(range_fix)
    local to_use_kind, use_value
    local use_to = sgs.SPlayerList()
    if sgs.Slash_IsAvailable(self.player) then
        local slashs = {"slash-Slash", "fire_slash-FireSlash", "thunder_slash-ThunderSlash"}
        for _,sla in ipairs(slashs) do
            local slash_name = sla:split("-")[1]
            local slash_class = sla:split("-")[2]
            local slash = sgs.Sanguosha:cloneCard(slash_name)
            if slash ~= nil then
                slash:deleteLater()
                local dummy_use = {
                    isDummy = true,
                    to = sgs.SPlayerList()
                }
                self:useBasicCard(slash, dummy_use)
                if dummy_use.card then
                    local targets = sgs.SPlayerList()
                    for _,p in sgs.qlist(dummy_use.to) do
                        if self.player:canSlash(p, slash, true, range_fix) then
                            targets:append(p)
                        end
                    end
                    if not targets:isEmpty() then
                        local value = sgs.ai_use_value[slash_class] or 0
                        if use_value == nil or value > use_value then
                            to_use_kind, use_value = slash_name, value
                            use_to = targets
                        end
                    end
                end
            end
        end
    end
    if to_use_kind and use_to:length() > 0 then
        return to_use_kind, use_to, use_value
    end
    return nil, nil, nil
end

huomo_skill = {name = "huomo"}
table.insert(sgs.ai_skills, huomo_skill)
huomo_skill.getTurnUseCard = function(self)
    if self.player:isNude() then
        return 
    elseif self.player:getMark("Huomo_Peach") == 0 and self.player:getLostHp() > 0 then
        return sgs.Card_Parse("@HuomoCard=.:peach")
    elseif self.player:getMark("Huomo_Analpetic") == 0 and sgs.Analeptic_IsAvailable(self.player) then
        return sgs.Card_Parse("@HuomoCard=.:analeptic")
    elseif self.player:getMark("Huomo_Slash") == 0 and sgs.Slash_IsAvailable(self.player) then
        return sgs.Card_Parse("@HuomoCard=.:slash")
    end
end

sgs.ai_skill_use_func.HuomoCard = function(card, use, self) 
    local handcards = self.player:getHandcards()
    local can_use = {}
    for _,black in sgs.qlist(handcards) do
        if black:isBlack() and not black:isKindOf("BasicCard") then
            table.insert(can_use, black)
        end
    end
    if #can_use == 0 or self:hasSkills(sgs.lose_equip_skill) then
        local equips = self.player:getEquips()
        for _,equip in sgs.qlist(equips) do
            if equip:isBlack() then
                table.insert(can_use, equip)
            end
        end
    end
    if #can_use == 0 then
        return 
    end
    
    self:sortByKeepValue(can_use)
    local to_use, pattern = nil, nil
    local use_peach, use_anal, use_slash = false, false, false
    
    if self.player:getMark("Huomo_Peach") == 0 then
        if self.player:getLostHp() > 0 and self:isWeak() then
            local peach = sgs.Sanguosha:cloneCard("peach")
            peach:deleteLater()
            local dummy_use = {
                isDummy = true,
            }
            self:useBasicCard(peach, dummy_use)
            if dummy_use.card then
                use_peach = true
                local value = sgs.ai_use_value["Peach"] or 0
                for _,c in ipairs(can_use) do
                    if self:getUseValue(c) <= value then
                        to_use, pattern = c, "peach"
                        break
                    end
                end
            end
        end
    end
    
    if not to_use and self.player:getMark("Huomo_Analeptic") == 0 then
        if sgs.Analeptic_IsAvailable(self.player) then
            local anal = sgs.Sanguosha:cloneCard("analeptic")
            anal:deleteLater()
            local dummy_use = {
                isDummy = true,
            }
            self:useBasicCard(anal, dummy_use)
            if dummy_use.card then
                use_anal = true
                local value = sgs.ai_use_value["Analeptic"] or 0
                for _,c in ipairs(can_use) do
                    if self:getUseValue(c) <= value then
                        to_use, pattern = c, "analeptic"
                        use.to:append(self.player)
                        break
                    end
                end
            end
        end
    end
    
    if not to_use and self.player:getMark("Huomo_Slash") == 0 then
        if sgs.Slash_IsAvailable(self.player) then
            local slashs = {"slash-Slash", "fire_slash-FireSlash", "thunder_slash-ThunderSlash"}
            for _,sla in ipairs(slashs) do
                local slash_name = sla:split("-")[1]
                local slash_class = sla:split("-")[2]
                local slash = sgs.Sanguosha:cloneCard(slash_name)
                if slash ~= nil then
                    slash:deleteLater()
                    local dummy_use = {
                        isDummy = true,
                        to = sgs.SPlayerList()
                    }
                    self:useBasicCard(slash, dummy_use)
                    if dummy_use.card then
                        use_slash = true
                        local value = sgs.ai_use_value[slash_class] or 0
                        for _,c in ipairs(can_use) do
                            if self:getUseValue(c) <= value then
                                if self.player:getWeapon() then
                                    local weaponrange = self.player:getWeapon():getRealCard():toWeapon():getRange()
                                    for _,p in sgs.qlist(dummy_use.to) do
                                        if self.player:canSlash(p, slash, true, weaponrange) then
                                            use.to:append(p)
                                            to_use, pattern = c, slash_name
                                        end
                                    end
                                else
                                    to_use, pattern = c, slash_name
                                end
                                break
                            end
                        end
                    end
                end
            end
        end
    end
    
    if not to_use and self:getOverflow() > 0 then
        if use_anal then
            to_use, pattern = can_use[1], "analeptic"
        elseif use_slash then
            local weaponrange = 0
            if self.player:getWeapon() and can_use[1]:getEffectiveId() == self.player:getWeapon():getEffectiveId() then
                weaponrange = self.player:getWeapon():getRealCard():toWeapon():getRange()
            end
            to_use, pattern, use.to = can_use[1], self:findSlashKindToUse(weaponrange)
        end
    end
    
    if not to_use and self.player:getMark("Huomo_Peach") == 0 then
        if not use_peach and self.player:getLostHp() > 0 then
            local peach = sgs.Sanguosha:cloneCard("Peach")
            peach:deleteLater()
            local dummy_use = {
                isDummy = true,
            }
            self:useBasicCard(peach, dummy_use)
            if dummy_use.card then
                use_peach = true
                local value = sgs.ai_use_value["Peach"] or 0
                for _,c in ipairs(can_use) do
                    if self:getUseValue(c) <= value then
                        to_use, pattern = c, "peach"
                        break
                    end
                end
            end
        end
        if use_peach then
            to_use = to_use or can_use[1]
            pattern = "peach"
        end
    end
    
    if to_use and pattern then
        local card = sgs.Sanguosha:cloneCard(pattern)
        if card:isKindOf("Slash") and (not use.to or use.to:isEmpty()) then 
            use.card = nil
            return nil 
        end
        local card_str = "@HuomoCard="..to_use:getEffectiveId()..":"..pattern
        local acard = sgs.Card_Parse(card_str)
        use.card = acard
    end
end
sgs.ai_use_value["HuomoCard"] = 3.7
sgs.ai_use_priority["HuomoCard"] = 2.5

sgs.ai_skill_playerchosen.zuoding = function(self, targets)
    local l = {}
    for _, p in sgs.qlist(targets) do
        if table.contains(self.friends, p, true) then
            table.insert(l, p)
        end
    end

    if #l == 0 then return nil end

    self:sort(l, "defense")
    return l[#l]
end

anguo_skill = {name = "anguo"}
table.insert(sgs.ai_skills, anguo_skill)
anguo_skill.getTurnUseCard = function(self)
    --if (not self.player:hasUsed("AnguoCard")) then
    --    return sgs.Card_Parse("@AnguoCard=.")
    --end

    return nil
end

sgs.ai_skill_use_func.AnguoCard = function(card, use, self)
    local l = {}
    function calculateMinus(player, range)
        if range <= 0 then return 0 end
        local n = 0
        for _, p in sgs.qlist(self.room:getAlivePlayers()) do
            if player:inMyAttackRange(p) and not player:inMyAttackRange(p, range) then n = n + 1 end
        end
        return n
    end

    function filluse(to, id)
        use.card = card
        if (use.to) then use.to:append(to) end
        self.anguoid = id
    end

    for _, p in ipairs(self.friends_noself) do
        if (self:needToThrowArmor(p) and p:getArmor()) then
            filluse(p, p:getArmor():getEffectiveId())
            return
        end
        if self:hasSkills(sgs.lose_equip_skill, p) then
            local equips = sgs.QList2Table(p:getCards("e"))
            self:sortByKeepValue(equips)
            filluse(p, equips[1]:getEffectiveId())
        end
    end

    for _,p in ipairs(self.enemies) do
        if not self:hasSkills(sgs.lose_equip_skill, p) then
            if (p:getWeapon()) then
                local weaponrange = p:getWeapon():getRealCard():toWeapon():getRange()
                local n = calculateMinus(p, weaponrange - 1)
                table.insert(l, {player = p, id = p:getWeapon():getEffectiveId(), minus = n})
            end
            if (p:getOffensiveHorse()) then
                local n = calculateMinus(p, 1)
                table.insert(l, {player = p, id = p:getOffensiveHorse():getEffectiveId(), minus = n})
            end
        end
    end

    if #l > 0 then
        function sortByMinus(a, b)
            return a.minus > b.minus
        end

        table.sort(l, sortByMinus)
        if l[1].minus > 0 then
            filluse(l[1].player, l[1].id)
            return
        end
    end

    for _, p in ipairs(self.enemies) do
        if not self:hasSkills(sgs.lose_equip_skill, p) then
            if (p:getTreasure()) then
                filluse(p, p:getTreasure():getEffectiveId())
                return
            end
        end
    end

    self:sort(self.enemies, "threat")
    for _, p in ipairs(self.enemies) do
        if not self:hasSkills(sgs.lose_equip_skill, p) then
            if (p:getArmor() and not p:getArmor():isKindOf("GaleShell")) then
                filluse(p, p:getArmor():getEffectiveId())
                return
            end
        end
    end

    for _, p in ipairs(self.enemies) do
        if not self:hasSkills(sgs.lose_equip_skill, p) then
            if (p:getDefensiveHorse()) then
                filluse(p, p:getDefensiveHorse():getEffectiveId())
                return
            end
        end
    end
end

sgs.ai_use_priority.AnguoCard = sgs.ai_use_priority.ExNihilo + 0.01

sgs.ai_skill_cardchosen.anguo = function(self)
    return self.anguoid
end

sgs.ai_skill_discard.qingxi = function(self, discard_num, min_num, optional, include_equip)
    local to_discard = self:askForDiscard("dummyreason", discard_num, min_num, false, include_equip)
    if #to_discard < discard_num then return {} end
    for _, id in ipairs(to_discard) do
        if isCard("Peach", sgs.Sanguosha:getCard(id), self.player) then return {}
        elseif 1 == self.player:getHp() and isCard("Analeptic", sgs.Sanguosha:getCard(id), self.player) then return {}
        end
    end
end

sgs.ai_skill_invoke.qingxi = function(self, data)
    local target = data:toPlayer()
    if not self:isEnemy(target) then return false end
    if target:hasArmorEffect("silver_lion") then return false end
    return true
end

sgs.ai_cardneed.qingxi = function(to, card)
    return card:getTypeId() == sgs.Card_TypeEquip or card:isKindOf("Slash")
end