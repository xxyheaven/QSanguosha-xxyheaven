sgs.weapon_range.SPMoonSpear = 3

sgs.ai_skill_playerchosen.sp_moonspear = function(self, targets)
    targets = sgs.QList2Table(targets)
    self:sort(targets, "defense")
    for _, target in ipairs(targets) do
        if self:isEnemy(target) and self:damageIsEffective(target) and sgs.isGoodTarget(target, targets, self) then
            return target
        end
    end
    return nil
end

sgs.ai_playerchosen_intention.sp_moonspear = 80

sgs.ai_skill_use["@jijiang"] = function(self, prompt)
    if self.player:hasFlag("Global_JijiangFailed") then return "." end
    local card = sgs.Card_Parse("@JijiangCard=.")
    local dummy_use = { isDummy = true }
    self:useSkillCard(card, dummy_use)
    if dummy_use.card then
        local jijiang = {}
        if sgs.jijiangtarget then
            for _, p in ipairs(sgs.jijiangtarget) do
                table.insert(jijiang, p:objectName())
            end
            return "@JijiangCard=.->" .. table.concat(jijiang, "+")
        end
    end
    return "."
end

--[[
    技能：庸肆（弃牌部分）
    备注：为了解决场上有古锭刀时弃白银狮子的问题而重写此弃牌方案。
]]--
sgs.ai_skill_discard.yongsi = function(self, discard_num, min_num, optional, include_equip)
    local flag = "h"
    local equips = self.player:getEquips()
    if include_equip and not (equips:isEmpty() or self.player:isJilei(equips:first())) then flag = flag .. "e" end
    local cards = self.player:getCards(flag)
    local to_discard = {}
    cards = sgs.QList2Table(cards)
    local aux_func = function(card)
        local place = self.room:getCardPlace(card:getEffectiveId())
        if place == sgs.Player_PlaceEquip then
            if card:isKindOf("SilverLion") then
                local players = self.room:getOtherPlayers(self.player)
                for _,p in sgs.qlist(players) do
                    local blade = p:getWeapon()
                    if blade and blade:isKindOf("GudingBlade") then
                        if p:inMyAttackRange(self.player) then
                            if self:isEnemy(p, self.player) then
                                return 6
                            end
                        else
                            break --因为只有一把古锭刀，检测到有人装备了，其他人就不会再装备了，此时可跳出检测。
                        end
                    end
                end
                if self.player:isWounded() then
                    return -2
                end
            elseif card:isKindOf("Weapon") and self.player:getHandcardNum() < discard_num + 2 and not self:needKongcheng() then return 0
            elseif card:isKindOf("OffensiveHorse") and self.player:getHandcardNum() < discard_num + 2 and not self:needKongcheng() then return 0
            elseif card:isKindOf("OffensiveHorse") then return 1
            elseif card:isKindOf("Weapon") then return 2
            elseif card:isKindOf("DefensiveHorse") then return 3
            elseif self:hasSkills("bazhen|yizhong") and card:isKindOf("Armor") then return 0
            elseif card:isKindOf("Armor") then
                return 4
            end
        elseif self:hasSkills(sgs.lose_equip_skill) then
            return 5
        else
            return 0
        end
        return 0
    end
    local compare_func = function(a, b)
        if aux_func(a) ~= aux_func(b) then return aux_func(a) < aux_func(b) end
        return self:getKeepValue(a) < self:getKeepValue(b)
    end

    table.sort(cards, compare_func)
    local least = min_num
    if discard_num - min_num > 1 then
        least = discard_num -1
    end
    for _, card in ipairs(cards) do
        if not self.player:isJilei(card) then
            table.insert(to_discard, card:getId())
        end
        if (self.player:hasSkill("qinyin") and #to_discard >= least) or #to_discard >= discard_num then
            break
        end
    end
    return to_discard
end

--三国杀OL2.79修订的三名SP武将，SP贾诩，SP庞德，SP马超

--JuesiCard:Play
--room->askForCard(target, "..", "@juesi-discard", QVariant())
local juesi_skill = {}
juesi_skill.name = "juesi"
table.insert(sgs.ai_skills, juesi_skill)
juesi_skill.getTurnUseCard = function(self)
    local first = false
    local cards = {}
    if #self.enemies == 0 then return nil end
    for _,p in ipairs(self.enemies) do
        if self.player:inMyAttackRange(p) and not p:isNude() then
            first = true
            break
        end
    end
    if self:getCardsNum("Slash")<1 or not first then return nil end
    for _,c in sgs.qlist(self.player:getCards("h")) do
        if c:isKindOf("Slash") then
            if self.player:canDiscard(self.player,c:getId()) then
                table.insert(cards,c)
            end
        end
    end 
    if #cards > 0 then
        return sgs.Card_Parse("@JuesiCard="..cards[1]:getEffectiveId())
    end
end

sgs.ai_skill_use_func.JuesiCard = function(card, use, self) 
    local targets = {}
    local target
    for _,p in ipairs(self.enemies) do
        if self.player:inMyAttackRange(p) and not (p:getArmor() and p:getArmor():isKindOf("SilverLion"))then
            table.insert(targets,p)
        end
    end
    self:sort(targets,"handcard")
    for _,p in ipairs(targets) do
        if not self:needToLoseHp(p,self.player) and p:getHp() >= self.player:getHp() and not p:isNude() then
            target = p
            break
        end
    end
    if not target then
        self:sort(targets,"hp")
        for _,p in ipairs(targets) do
            if not self:needToLoseHp(p,self.player) and not p:isNude() then
                target = p
                break
            end
        end
    end
    if target then 
        use.card = card
        if use.to then
            use.to:append(target)
            if use.to:length() >= 1 then return end
        end
    end
end

sgs.ai_use_value.JuesiCard = 4.5
sgs.ai_use_priority.JuesiCard = 2.5
sgs.ai_card_intention.JuesiCard = 10

sgs.ai_skill_cardask["@juesi-discard"] = function(self, data)
     local cards = {}
     for _,c in sgs.qlist(self.player:getCards("h")) do
        if c:isKindOf("Slash") then
            if self.player:canDiscard(self.player,c:getId()) then
                table.insert(cards,c)
            end
        end
    end
    if #cards == 0 then 
        local hcards = self.player:getCards("he")
        hcards = sgs.QList2Table(hcards)
        self:sortByKeepValue(hcards)
        return "$" .. hcards[1]:getId()
    end
    self:sortByKeepValue(cards)
    return "$" .. cards[1]:getId()
end

sgs.juesi_keep_value = {
    Slash           = 5.4
}

--JianshuCard:Play
--from->pindian(to, "jianshu", NULL)
local jianshu_skill = {}
jianshu_skill.name = "jianshu"
table.insert(sgs.ai_skills, jianshu_skill)
jianshu_skill.getTurnUseCard = function(self)
    if self.player:getMark("@alienation") <= 0 then return nil end
    if #self.enemies < 2 then return nil end
    local invoke = false
    for _,p in ipairs(self.enemies) do
        if not p:isKongcheng() then 
            invoke  = true
        end
    end
    if not invoke then return nil end
    
    local cards = {}
    for _,acard in sgs.qlist(self.player:getCards("h")) do
        if acard:isBlack() then
            table.insert(cards,acard)
        end
    end
    self:sortByUseValue(cards, true)
    if #cards > 0 then
        return sgs.Card_Parse("@JianshuCard="..cards[1]:getEffectiveId())
    end
    return nil
end

sgs.ai_skill_use_func.JianshuCard = function(card, use, self)
    self:sort(self.enemies, "defense")
    for _,p in ipairs(self.enemies) do
        if self:isWeak(p) then
            for _,sp in ipairs(self.enemies) do
                if sp:inMyAttackRange(p) then
                    if (p:isKongcheng() and not sp:isKongcheng()) then
                        use.card = card
                        if use.to then
                            use.to:append(p)
                            use.to:append(sp)
                            return
                        end
                    elseif not p:isKongcheng() then
                        use.card = card
                        if use.to then
                            use.to:append(sp)
                            use.to:append(p)
                            return
                        end
                    end
                end
            end
        end
    end
    return
end

sgs.ai_use_value.JianshuCard = 8
sgs.ai_use_priority.JianshuCard = 8
sgs.ai_card_intention.JianshuCard = 30

function sgs.ai_skill_pindian.jianshu(minusecard, self, requestor, maxcard)
    local req
    if self.player:objectName() == requestor:objectName() then
        for _, p in sgs.qlist(self.room:getOtherPlayers(self.player)) do
            if p:hasFlag("JianshuPindianTarget") then
                req = p
                break
            end
        end
    else
        req = requestor
    end
    local cards, maxcard = sgs.QList2Table(self.player:getHandcards())
    local max_value = 0
    self:sortByKeepValue(cards)
    max_value = self:getKeepValue(cards[#cards])
    local function compare_func1(a, b)
        return a:getNumber() > b:getNumber()
    end
    local function compare_func2(a, b)
        return a:getNumber() < b:getNumber()
    end
    if self:isFriend(req) and self.player:getHp() > req:getHp() then
        table.sort(cards, compare_func2)
    else
        table.sort(cards, compare_func1)
    end
    for _, card in ipairs(cards) do
        if max_value > 7 or self:getKeepValue(card) < 7 or card:isKindOf("EquipCard") then maxcard = card break end
    end
    return maxcard or cards[1]
end

--room->askForPlayerChosen(player, males, objectName(), "@yongdi", true, true)
sgs.ai_skill_playerchosen.yongdi = function(self, targets)
    self:sort(self.friends_noself, "defense")
    self.friends_noself = sgs.reverse(self.friends_noself)
    for _,p in ipairs(self.friends_noself) do
        if (self:hasSkills(sgs.need_maxhp_skill, p) or p:hasSkill("hunzi")) and targets:contains(p) then
            return p
        end
    end
    for _,p in ipairs(self.friends_noself) do
        if self:hasSkills(sgs.masochism_skill, p) and targets:contains(p) then
            return p
        end
    end
    for _,p in ipairs(self.friends_noself) do
        if targets:contains(p) then
            return p
        end
    end
    return nil
end

sgs.ai_playerchosen_intention.yongdi = -80

--room->askForUseCard(player, "@@shichou", "@shichou-add:::" + QString::number(player->getLostHp()))
sgs.ai_skill_use["@@shichou"] = function(self, prompt)
    local use = self.player:getTag("shichou-use"):toCardUse()
    local x = self.player:getLostHp()
    local target_table = {}

    for i = 0, x - 1, 1 do
        local dummy_use = { isDummy = true, to = sgs.SPlayerList(), current_targets = {} }
        for _, p in sgs.qlist(use.to) do
            table.insert(dummy_use.current_targets, p:objectName())
        end
        for _, p_name in ipairs(target_table) do
            table.insert(dummy_use.current_targets, p_name)
        end
        self:useCardSlash(use.card, dummy_use)
        if dummy_use.card and dummy_use.to:length() > 0 then
            table.insert(target_table, dummy_use.to:first():objectName())
        else
            break
        end
    end

    if #target_table > 0 then
        return "@ShichouCard=.->" .. table.concat(target_table, "+")
    end
    return ""
end
--finished

sgs.ai_skill_invoke.danlao = function(self, data)
    local effect = data:toCardUse()
    local current = self.room:getCurrent()
    if effect.card:isKindOf("GodSalvation") and self.player:isWounded() or effect.card:isKindOf("ExNihilo") then
        return false
    elseif effect.card:isKindOf("AmazingGrace") and
        (self.player:getSeat() - current:getSeat()) % (global_room:alivePlayerCount()) < global_room:alivePlayerCount()/2 then
        return false
    else
        return true
    end
end

sgs.ai_skill_invoke.jilei = function(self, data)
    local target = data:toPlayer()
    self.jilei_source = target
    return self:isEnemy(target)
end

sgs.ai_skill_choice.jilei = function(self, choices)
    local tmptrick = sgs.Sanguosha:cloneCard("ex_nihilo")
    if (self:hasCrossbowEffect(self.jilei_source) and self.jilei_source:inMyAttackRange(self.player))
        or self.jilei_source:isCardLimited(tmptrick, sgs.Card_MethodUse, true) then
        return "BasicCard"
    else
        return "TrickCard"
    end
end

sgs.ai_skill_playerchosen["chenqing"] = function(self, targets)
    local victim = self.room:getCurrentDyingPlayer()
    local help = false
    local careLord = false
    if victim then
        if self:isFriend(victim) then
            help = true
        elseif self.role == "renegade" and victim:isLord() and self.room:alivePlayerCount() > 2 then
            help = true
            careLord = true
        end
    end
    local friends, enemies = {}, {}
    for _,p in sgs.qlist(targets) do
        if self:isFriend(p) then
            table.insert(friends, p)
        else
            table.insert(enemies, p)
        end
    end
    local compare_func = function(a, b)
        local nA = a:getCardCount(true)
        local nB = b:getCardCount(true)
        if nA == nB then
            return a:getHandcardNum() > b:getHandcardNum()
        else
            return nA > nB
        end
    end
    if help and #friends > 0 then
        table.sort(friends, compare_func)
        for _,friend in ipairs(friends) do
            if not hasManjuanEffect(friend) then
                return friend
            end
        end
    end
    if careLord and #enemies > 0 then
        table.sort(enemies, compare_func)
        for _,enemy in ipairs(enemies) do
            if sgs.evaluatePlayerRole(enemy) == "loyalist" then
                return enemy
            end
        end
    end
	--如果当前回合角色和之后的角色都是自己人
	
	
	
	
end

--有必要调整弃装备牌的策略，还有敌人时候不弃四色= =
sgs.ai_skill_cardask["@chenqing-discard"] = function(self, data)
    local victim = self.room:getCurrentDyingPlayer()
    local help = false
    if victim then
        if self:isFriend(victim) then
            help = true
        elseif self.role == "renegade" and victim:isLord() and self.room:alivePlayerCount() > 2 then
            help = true
        end
    end
    local cards = self.player:getCards("he")
    cards = sgs.QList2Table(cards)
    self:sortByKeepValue(cards)
    if help then
        local peach_num = 0
        local spade, heart, club, diamond = nil, nil, nil, nil
        for _,c in ipairs(cards) do
            if isCard("Peach", c, self.player) then
                peach_num = peach_num + 1
            else
                local suit = c:getSuit()
                if not spade and suit == sgs.Card_Spade then
                    spade = c:getEffectiveId()
                elseif not heart and suit == sgs.Card_Heart then
                    heart = c:getEffectiveId()
                elseif not club and suit == sgs.Card_Club then
                    club = c:getEffectiveId()
                elseif not diamond and suit == sgs.Card_Diamond then
                    diamond = c:getEffectiveId()
                end
            end
        end
        if peach_num + victim:getHp() <= 0 then
            if spade and heart and club and diamond then
                return "$" .. spade .. "+" .. heart .. "+" .. club .. "+" .. diamond
            end
        end
    end
    local select_cards = self:askForDiscard("dummy", 4, 4, false, true)
    return "$" .. table.concat(select_cards, "+")
end

sgs.ai_skill_use["@@mozhi"] = function(self, prompt)
    local mozhi_card = self.player:property("mozhi"):toString()
    local use_card = sgs.Sanguosha:cloneCard(mozhi_card)
    use_card:setSkillName("mozhi")
    if use_card then
        local cards = sgs.QList2Table(self.player:getHandcards())
        self:sortByKeepValue(cards)
        for _, card in ipairs(cards) do
            if self:getKeepValue(card, cards) <= self:getKeepValue(use_card, cards) then
                use_card:addSubcard(card)
                local dummy_use = { isDummy = true, to = sgs.SPlayerList() }
                self:useCardByClassName(use_card, dummy_use)
                if dummy_use.card then
                    if dummy_use.to:isEmpty() then
                        return dummy_use.card:toString()
                    else
                        local target_objectname = {}
                        for _, p in sgs.qlist(dummy_use.to) do
                            table.insert(target_objectname, p:objectName())
                        end
                        return dummy_use.card:toString() .. "->" .. table.concat(target_objectname, "+")
                    end
                else
                    break
                end
            end
        end
    end
end

local function yuanhu_validate(self, equip_type, is_handcard)
    local is_SilverLion = false
    if equip_type == "SilverLion" then
        equip_type = "Armor"
        is_SilverLion = true
    end
    local targets
    if is_handcard then targets = self.friends else targets = self.friends_noself end
    if equip_type ~= "Weapon" then
        if equip_type == "DefensiveHorse" or equip_type == "OffensiveHorse" then self:sort(targets, "hp") end
        if equip_type == "Armor" then self:sort(targets, "handcard") end
        if is_SilverLion then
            for _, enemy in ipairs(self.enemies) do
                if enemy:hasSkill("kongcheng") and enemy:isKongcheng() then
                    local seat_diff = enemy:getSeat() - self.player:getSeat()
                    local alive_count = self.room:alivePlayerCount()
                    if seat_diff < 0 then seat_diff = seat_diff + alive_count end
                    if seat_diff > alive_count / 2.5 + 1 then return enemy  end
                end
            end
            for _, enemy in ipairs(self.enemies) do
                if self:hasSkills("bazhen|yizhong", enemy) then
                    return enemy
                end
            end
        end
        for _, friend in ipairs(targets) do
            local has_equip = false
            for _, equip in sgs.qlist(friend:getEquips()) do
                if equip:isKindOf(equip_type) then
                    has_equip = true
                    break
                end
            end
            if not has_equip then
                if equip_type == "Armor" then
                    if not self:needKongcheng(friend, true) and not self:hasSkills("bazhen|yizhong", friend) then return friend end
                else
                    if friend:isWounded() and not (friend:hasSkill("longhun") and friend:getCardCount(true) >= 3) then return friend end
                end
            end
        end
    else
        for _, friend in ipairs(targets) do
            local has_equip = false
            for _, equip in sgs.qlist(friend:getEquips()) do
                if equip:isKindOf(equip_type) then
                    has_equip = true
                    break
                end
            end
            if not has_equip then
                for _, aplayer in sgs.qlist(self.room:getAllPlayers()) do
                    if friend:distanceTo(aplayer) == 1 then
                        if self:isFriend(aplayer) and not aplayer:containsTrick("YanxiaoCard")
                            and (aplayer:containsTrick("indulgence") or aplayer:containsTrick("supply_shortage")
                                or (aplayer:containsTrick("lightning") and self:hasWizard(self.enemies))) then
                            aplayer:setFlags("AI_YuanhuToChoose")
                            return friend
                        end
                    end
                end
                self:sort(self.enemies, "defense")
                for _, enemy in ipairs(self.enemies) do
                    if friend:distanceTo(enemy) == 1 and self.player:canDiscard(enemy, "he") then
                        enemy:setFlags("AI_YuanhuToChoose")
                        return friend
                    end
                end
            end
        end
    end
    return nil
end

sgs.ai_skill_use["@@yuanhu"] = function(self, prompt)
    local cards = self.player:getHandcards()
    cards = sgs.QList2Table(cards)
    self:sortByKeepValue(cards)
    if self.player:hasArmorEffect("silver_lion") then
        local player = yuanhu_validate(self, "SilverLion", false)
        if player then return "@YuanhuCard=" .. self.player:getArmor():getEffectiveId() .. "->" .. player:objectName() end
    end
    if self.player:getOffensiveHorse() then
        local player = yuanhu_validate(self, "OffensiveHorse", false)
        if player then return "@YuanhuCard=" .. self.player:getOffensiveHorse():getEffectiveId() .. "->" .. player:objectName() end
    end
    if self.player:getWeapon() then
        local player = yuanhu_validate(self, "Weapon", false)
        if player then return "@YuanhuCard=" .. self.player:getWeapon():getEffectiveId() .. "->" .. player:objectName() end
    end
    if self.player:getArmor() and self.player:getLostHp() <= 1 and self.player:getHandcardNum() >= 3 then
        local player = yuanhu_validate(self, "Armor", false)
        if player then return "@YuanhuCard=" .. self.player:getArmor():getEffectiveId() .. "->" .. player:objectName() end
    end
    for _, card in ipairs(cards) do
        if card:isKindOf("DefensiveHorse") then
            local player = yuanhu_validate(self, "DefensiveHorse", true)
            if player then return "@YuanhuCard=" .. card:getEffectiveId() .. "->" .. player:objectName() end
        end
    end
    for _, card in ipairs(cards) do
        if card:isKindOf("OffensiveHorse") then
            local player = yuanhu_validate(self, "OffensiveHorse", true)
            if player then return "@YuanhuCard=" .. card:getEffectiveId() .. "->" .. player:objectName() end
        end
    end
    for _, card in ipairs(cards) do
        if card:isKindOf("Weapon") then
            local player = yuanhu_validate(self, "Weapon", true)
            if player then return "@YuanhuCard=" .. card:getEffectiveId() .. "->" .. player:objectName() end
        end
    end
    for _, card in ipairs(cards) do
        if card:isKindOf("SilverLion") then
            local player = yuanhu_validate(self, "SilverLion", true)
            if player then return "@YuanhuCard=" .. card:getEffectiveId() .. "->" .. player:objectName() end
        end
        if card:isKindOf("Armor") and yuanhu_validate(self, "Armor", true) then
            local player = yuanhu_validate(self, "Armor", true)
            if player then return "@YuanhuCard=" .. card:getEffectiveId() .. "->" .. player:objectName() end
        end
    end
end

sgs.ai_skill_playerchosen.yuanhu = function(self, targets)
    targets = sgs.QList2Table(targets)
    for _, p in ipairs(targets) do
        if p:hasFlag("AI_YuanhuToChoose") then
            p:setFlags("-AI_YuanhuToChoose")
            return p
        end
    end
    return targets[1]
end

sgs.ai_card_intention.YuanhuCard = function(self, card, from, to)
    if to[1]:hasSkill("bazhen") or to[1]:hasSkill("yizhong") or (to[1]:hasSkill("kongcheng") and to[1]:isKongcheng()) then
        if sgs.Sanguosha:getCard(card:getEffectiveId()):isKindOf("SilverLion") then
            sgs.updateIntention(from, to[1], 10)
            return
        end
    end
    sgs.updateIntention(from, to[1], -50)
end

sgs.ai_cardneed.yuanhu = sgs.ai_cardneed.equip

sgs.yuanhu_keep_value = {
    Peach = 6,
    Jink = 5.1,
    Weapon = 4.7,
    Armor = 4.8,
    Horse = 4.9
}

sgs.ai_cardneed.xueji = function(to, card)
    return to:getHandcardNum() < 3 and card:isRed()
end

local xueji_skill = {}
xueji_skill.name = "xueji"
table.insert(sgs.ai_skills, xueji_skill)
xueji_skill.getTurnUseCard = function(self)
    if self.player:hasUsed("XuejiCard") then return end

    local card
    local cards = self.player:getCards("he")
    cards = sgs.QList2Table(cards)
    self:sortByUseValue(cards, true)

    for _, acard in ipairs(cards) do
        if acard:isRed() then
            card = acard
            break
        end
    end
    if card then
        card = sgs.Card_Parse("@XuejiCard=" .. card:getEffectiveId())
        return card
    end

    return nil
end

local function can_be_selected_as_target_xueji(self, card, who)
    if self:isEnemy(who) and self:damageIsEffective(who) and not self:cantbeHurt(who) and not self:getDamagedEffects(who) and not self:needToLoseHp(who) then
        if not self.player:hasSkill("jueqing") then
            if who:hasSkill("guixin") and (self.room:getAliveCount() >= 4 or not who:faceUp()) and not who:hasSkill("manjuan") then return false end
            if (who:hasSkill("ganglie") or who:hasSkill("neoganglie")) and (self.player:getHp() == 1 and self.player:getHandcardNum() <= 2) then return false end
            if who:hasSkill("jieming") then
                for _, enemy in ipairs(self.enemies) do
                    if enemy:getHandcardNum() <= enemy:getMaxHp() - 2 and not enemy:hasSkill("manjuan") then return false end
                end
            end
            if who:hasSkill("fangzhu") then
                for _, enemy in ipairs(self.enemies) do
                    if not enemy:faceUp() then return false end
                end
            end
            if who:hasSkill("yiji") then
                local huatuo = self.room:findPlayerBySkillName("jijiu")
                if huatuo and self:isEnemy(huatuo) and huatuo:getHandcardNum() >= 3 then
                    return false
                end
            end
        end
        return true
    elseif self:isFriend(who) then
        if who:hasSkill("yiji") and not self.player:hasSkill("jueqing") then
            local huatuo = self.room:findPlayerBySkillName("jijiu")
            if (huatuo and self:isFriend(huatuo) and huatuo:getHandcardNum() >= 3 and huatuo ~= self.player)
                or (who:getLostHp() == 0 and who:getMaxHp() >= 3) then
                return true
            end
        end
        if who:hasSkill("hunzi") and who:getMark("hunzi") == 0
          and who:objectName() == self.player:getNextAlive():objectName() and who:getHp() == 2 then
            return true
        end
        if self:cantbeHurt(who) and not self:damageIsEffective(who) and not (who:hasSkill("manjuan") and who:getPhase() == sgs.Player_NotActive)
          and not (who:hasSkill("kongcheng") and who:isKongcheng()) then
            return true
        end
        return false
    end
    return false
end

sgs.ai_skill_use_func.XuejiCard = function(card, use, self)
    if self.player:hasUsed("XuejiCard") then return end
    self:sort(self.enemies)
    local to_use = false
    for _, enemy in ipairs(self.enemies) do
        if can_be_selected_as_target_xueji(self, card, enemy) then
            to_use = true
            break
        end
    end
    if not to_use then
        for _, friend in ipairs(self.friends_noself) do
            if can_be_selected_as_target_xueji(self, card, friend) then
                to_use = true
                break
            end
        end
    end
    if to_use then
        use.card = card
        if use.to then
            for _, enemy in ipairs(self.enemies) do
                if can_be_selected_as_target_xueji(self, card, enemy) then
                    use.to:append(enemy)
                    if use.to:length() == math.max(self.player:getLostHp(), 1) then return end
                end
            end
            for _, friend in ipairs(self.friends_noself) do
                if can_be_selected_as_target_xueji(self, card, friend) then
                    use.to:append(friend)
                    if use.to:length() == math.max(self.player:getLostHp(), 1) then return end
                end
            end
            assert(use.to:length() > 0)
        end
    end
end

sgs.ai_card_intention.XuejiCard = function(self, card, from, tos)
    local room = from:getRoom()
    local huatuo = room:findPlayerBySkillName("jijiu")
    for _,to in ipairs(tos) do
        local intention = 60
        if to:hasSkill("yiji") and not from:hasSkill("jueqing") then
            if (huatuo and self:isFriend(huatuo) and huatuo:getHandcardNum() >= 3 and huatuo:objectName() ~= from:objectName()) then
                intention = -30
            end
            if to:getLostHp() == 0 and to:getMaxHp() >= 3 then
                intention = -10
            end
        end
        if to:hasSkill("hunzi") and to:getMark("hunzi") == 0 then
            if to:objectName() == from:getNextAlive():objectName() and to:getHp() == 2 then
                intention = -20
            end
        end
        if self:cantbeHurt(to) and not self:damageIsEffective(to) then intention = -20 end
        sgs.updateIntention(from, to, intention)
    end
end

sgs.ai_use_value.XuejiCard = 3
sgs.ai_use_priority.XuejiCard = 2.35

sgs.ai_skill_use["@@bifa"] = function(self, prompt)
    local cards = self.player:getHandcards()
    cards = sgs.QList2Table(cards)
    self:sortByKeepValue(cards)
    self:sort(self.enemies, "hp")
    if #self.enemies < 0 then return "." end
    for _, enemy in ipairs(self.enemies) do
        if not (self:needToLoseHp(enemy) and not self:hasSkills(sgs.masochism_skill, enemy)) then
            for _, c in ipairs(cards) do
                if c:isKindOf("EquipCard") then return "@BifaCard=" .. c:getEffectiveId() .. "->" .. enemy:objectName() end
            end
            for _, c in ipairs(cards) do
                if c:isKindOf("TrickCard") and not (c:isKindOf("Nullification") and self:getCardsNum("Nullification") == 1) then
                    return "@BifaCard=" .. c:getEffectiveId() .. "->" .. enemy:objectName()
                end
            end
            for _, c in ipairs(cards) do
                if c:isKindOf("Slash") then
                    return "@BifaCard=" .. c:getEffectiveId() .. "->" .. enemy:objectName()
                end
            end
        end
    end
end

sgs.ai_skill_cardask["@bifa-give"] = function(self, data)
    local card_type = data:toString()
    local cards = self.player:getHandcards()
    cards = sgs.QList2Table(cards)
    if self:needToLoseHp() and not self:hasSkills(sgs.masochism_skill) then return "." end
    self:sortByUseValue(cards)
    for _, c in ipairs(cards) do
        if c:isKindOf(card_type) and not isCard("Peach", c, self.player) and not isCard("ExNihilo", c, self.player) then
            return "$" .. c:getEffectiveId()
        end
    end
    return "."
end

sgs.ai_card_intention.BifaCard = 30

sgs.bifa_keep_value = {
    Peach = 6,
    Jink = 5.1,
    Nullification = 5,
    EquipCard = 4.9,
    TrickCard = 4.8
}

local songci_skill = {}
songci_skill.name = "songci"
table.insert(sgs.ai_skills, songci_skill)
songci_skill.getTurnUseCard = function(self)
    return sgs.Card_Parse("@SongciCard=.")
end

sgs.ai_skill_use_func.SongciCard = function(card,use,self)
    self:sort(self.friends, "handcard")
    for _, friend in ipairs(self.friends) do
        if friend:getMark("songci" .. self.player:objectName()) == 0 and friend:getHandcardNum() < friend:getHp() and not (friend:hasSkill("manjuan") and self.room:getCurrent() ~= friend) then
            if not (friend:hasSkill("kongcheng") and friend:isKongcheng()) then
                use.card = sgs.Card_Parse("@SongciCard=.")
                if use.to then use.to:append(friend) end
                return
            end
        end
    end

    self:sort(self.enemies, "handcard")
    self.enemies = sgs.reverse(self.enemies)
    for _, enemy in ipairs(self.enemies) do
        if enemy:getMark("songci" .. self.player:objectName()) == 0 and enemy:getHandcardNum() > enemy:getHp() and not enemy:isNude()
            and not self:doNotDiscard(enemy, "nil", false, 2) then
            use.card = sgs.Card_Parse("@SongciCard=.")
            if use.to then use.to:append(enemy) end
            return
        end
    end
end

sgs.ai_use_value.SongciCard = 3
sgs.ai_use_priority.SongciCard = 3

sgs.ai_card_intention.SongciCard = function(self, card, from, to)
    sgs.updateIntention(from, to[1], to[1]:getHandcardNum() > to[1]:getHp() and 80 or -80)
end

sgs.ai_skill_cardask["@xingwu"] = function(self, data)
    local cards = sgs.QList2Table(self.player:getHandcards())
    if #cards <= 1 and self.player:getPile("xingwu"):length() == 1 then return "." end

    local good_enemies = {}
    for _, enemy in ipairs(self.enemies) do
        if enemy:isMale() and ((self:damageIsEffective(enemy) and not self:cantbeHurt(enemy, self.player, 2))
                                or (not self:damageIsEffective(enemy) and not enemy:getEquips():isEmpty()
                                    and not (enemy:getEquips():length() == 1 and enemy:getArmor() and self:needToThrowArmor(enemy)))) then
            table.insert(good_enemies, enemy)
        end
    end
    if #good_enemies == 0 and (not self.player:getPile("xingwu"):isEmpty() or not self.player:hasSkill("luoyan")) then return "." end

    local red_avail, black_avail
    local n = self.player:getMark("xingwu")
    if bit32.band(n, 2) == 0 then red_avail = true end
    if bit32.band(n, 1) == 0 then black_avail = true end

    self:sortByKeepValue(cards)
    local xwcard = nil
    local heart = 0
    local to_save = 0
    for _, card in ipairs(cards) do
        if self.player:hasSkill("tianxiang") and card:getSuit() == sgs.Card_Heart and heart < math.min(self.player:getHp(), 2) then
            heart = heart + 1
        elseif isCard("Jink", card, self.player) then
            if self.player:hasSkill("liuli") and self.room:alivePlayerCount() > 2 then
                for _, p in sgs.qlist(self.room:getOtherPlayers(self.player)) do
                    if self:canLiuli(self.player, p) then
                        xwcard = card
                        break
                    end
                end
            end
            if not xwcard and self:getCardsNum("Jink") >= 2 then
                xwcard = card
            end
        elseif to_save > self.player:getMaxCards()
                or (not isCard("Peach", card, self.player) and not (self:isWeak() and isCard("Analeptic", card, self.player))) then
            xwcard = card
        else
            to_save = to_save + 1
        end
        if xwcard then
            if (red_avail and xwcard:isRed()) or (black_avail and xwcard:isBlack()) then
                break
            else
                xwcard = nil
                to_save = to_save + 1
            end
        end
    end
    if xwcard then return "$" .. xwcard:getEffectiveId() else return "." end
end

sgs.ai_skill_playerchosen.xingwu = function(self, targets)
    local good_enemies = {}
    for _, enemy in ipairs(self.enemies) do
        if enemy:isMale() then
            table.insert(good_enemies, enemy)
        end
    end
    if #good_enemies == 0 then return targets:first() end

    local getCmpValue = function(enemy)
        local value = 0
        if self:damageIsEffective(enemy) then
            local dmg = enemy:hasArmorEffect("silver_lion") and 1 or 2
            if enemy:getHp() <= dmg then value = 5 else value = value + enemy:getHp() / (enemy:getHp() - dmg) end
            if not sgs.isGoodTarget(enemy, self.enemies, self) then value = value - 2 end
            if self:cantbeHurt(enemy, self.player, dmg) then value = value - 5 end
            if enemy:isLord() then value = value + 2 end
            if enemy:hasArmorEffect("silver_lion") then value = value - 1.5 end
            if self:hasSkills(sgs.exclusive_skill, enemy) then value = value - 1 end
            if self:hasSkills(sgs.masochism_skill, enemy) then value = value - 0.5 end
        end
        if not enemy:getEquips():isEmpty() then
            local len = enemy:getEquips():length()
            if enemy:hasSkills(sgs.lose_equip_skill) then value = value - 0.6 * len end
            if enemy:getArmor() and self:needToThrowArmor() then value = value - 1.5 end
            if enemy:hasArmorEffect("silver_lion") then value = value - 0.5 end

            if enemy:getWeapon() then value = value + 0.8 end
            if enemy:getArmor() then value = value + 1 end
            if enemy:getDefensiveHorse() then value = value + 0.9 end
            if enemy:getOffensiveHorse() then value = value + 0.7 end
            if self:getDangerousCard(enemy) then value = value + 0.3 end
            if self:getValuableCard(enemy) then value = value + 0.15 end
        end
        return value
    end

    local cmp = function(a, b)
        return getCmpValue(a) > getCmpValue(b)
    end
    table.sort(good_enemies, cmp)
    return good_enemies[1]
end

sgs.ai_playerchosen_intention.xingwu = 80

function sgs.ai_cardsview_valuable.aocai(self, class_name, player)
    if player:hasFlag("Global_AocaiFailed") or player:getPhase() ~= sgs.Player_NotActive then return end
    if class_name == "Slash" then
        return "@AocaiCard=.:slash"
    elseif class_name == "Jink" then
        return "@AocaiCard=.:jink"
    elseif (class_name == "Peach" and player:getMark("Global_PreventPeach") == 0) or class_name == "Analeptic" then
        local dying = self.room:getCurrentDyingPlayer()
        if dying and dying:objectName() == player:objectName() then
            local user_string = "peach+analeptic"
            if player:getMark("Global_PreventPeach") > 0 then user_string = "analeptic" end
            return "@AocaiCard=.:" .. user_string
        else
            local user_string
            if class_name == "Analeptic" then user_string = "analeptic" else user_string = "peach" end
            return "@AocaiCard=.:" .. user_string
        end
    end
end

sgs.ai_skill_cardask["@aocai-view"] = function(self, data)
    local aocai_list = self.player:property("aocai"):toString():split("+")
    for _, id in ipairs(aocai_list) do
        local num_id = tonumber(id)
        local hcard = sgs.Sanguosha:getCard(num_id)
        if hcard:isKindOf("Jink") and self.player:hasFlag("dahe") and hcard:getSuit() ~= sgs.Card_Heart then
            continue
        end
        return "$" .. num_id
    end
end

function SmartAI:getSaveNum(isFriend)
    local num = 0
    for _, player in sgs.qlist(self.room:getAllPlayers()) do
        if (isFriend and self:isFriend(player)) or (not isFriend and self:isEnemy(player)) then
            if not self.player:hasSkill("wansha") or player:objectName() == self.player:objectName() then
                if player:hasSkill("jijiu") then
                    num = num + self:getSuitNum("heart", true, player)
                    num = num + self:getSuitNum("diamond", true, player)
                    num = num + player:getHandcardNum() * 0.4
                end
                if player:hasSkill("nosjiefan") and getCardsNum("Slash", player, self.player) > 0 then
                    if self:isFriend(player) or self:getCardsNum("Jink") == 0 then num = num + getCardsNum("Slash", player, self.player) end
                end
                num = num + getCardsNum("Peach", player, self.player)
            end
            if player:hasSkill("buyi") and not player:isKongcheng() then num = num + 0.3 end
            if player:hasSkill("chunlao") and not player:getPile("wine"):isEmpty() then num = num + player:getPile("wine"):length() end
            if player:hasSkill("jiuzhu") and player:getHp() > 1 and not player:isNude() then
                num = num + 0.9 * math.max(0, math.min(player:getHp() - 1, player:getCardCount(true)))
            end
            if player:hasSkill("renxin") and player:objectName() ~= self.player:objectName() and not player:isKongcheng() then num = num + 1 end
        end
    end
    return num
end

local duwu_skill = {}
duwu_skill.name = "duwu"
table.insert(sgs.ai_skills, duwu_skill)
duwu_skill.getTurnUseCard = function(self, inclusive)
    if self.player:hasFlag("DuwuEnterDying") or #self.enemies == 0 then return end
    return sgs.Card_Parse("@DuwuCard=.")
end

sgs.ai_skill_use_func.DuwuCard = function(card, use, self)
    local cmp = function(a, b)
        if a:getHp() < b:getHp() then
            if a:getHp() == 1 and b:getHp() == 2 then return false else return true end
        end
        return false
    end
    local enemies = {}
    for _, enemy in ipairs(self.enemies) do
        if self:canAttack(enemy, self.player) and self.player:inMyAttackRange(enemy) then table.insert(enemies, enemy) end
    end
    if #enemies == 0 then return end
    table.sort(enemies, cmp)
    if enemies[1]:getHp() <= 0 then
        use.card = sgs.Card_Parse("@DuwuCard=.")
        if use.to then use.to:append(enemies[1]) end
        return
    end

    -- find cards
    local card_ids = {}
    if self:needToThrowArmor() then table.insert(card_ids, self.player:getArmor():getEffectiveId()) end

    local zcards = self.player:getHandcards()
    local use_slash, keep_jink, keep_analeptic = false, false, false
    for _, zcard in sgs.qlist(zcards) do
        if not isCard("Peach", zcard, self.player) and not isCard("ExNihilo", zcard, self.player) then
            local shouldUse = true
            if zcard:getTypeId() == sgs.Card_TypeTrick then
                local dummy_use = { isDummy = true }
                self:useTrickCard(zcard, dummy_use)
                if dummy_use.card then shouldUse = false end
            end
            if zcard:getTypeId() == sgs.Card_TypeEquip and not self.player:hasEquip(zcard) then
                local dummy_use = { isDummy = true }
                self:useEquipCard(zcard, dummy_use)
                if dummy_use.card then shouldUse = false end
            end
            if isCard("Jink", zcard, self.player) and not keep_jink then
                keep_jink = true
                shouldUse = false
            end
            if self.player:getHp() == 1 and isCard("Analeptic", zcard, self.player) and not keep_analeptic then
                keep_analeptic = true
                shouldUse = false
            end
            if shouldUse then table.insert(card_ids, zcard:getId()) end
        end
    end
    local hc_num = #card_ids
    local eq_num = 0
    if self.player:getOffensiveHorse() then
        table.insert(card_ids, self.player:getOffensiveHorse():getEffectiveId())
        eq_num = eq_num + 1
    end
    if self.player:getWeapon() and self:evaluateWeapon(self.player:getWeapon()) < 5 then
        table.insert(card_ids, self.player:getWeapon():getEffectiveId())
        eq_num = eq_num + 2
    end

    local function getRangefix(index)
        if index <= hc_num then return 0
        elseif index == hc_num + 1 then
            if eq_num == 2 then
                return sgs.weapon_range[self.player:getWeapon():getClassName()] - self.player:getAttackRange(false)
            else
                return 1
            end
        elseif index == hc_num + 2 then
            return sgs.weapon_range[self.player:getWeapon():getClassName()]
        end
    end

    for _, enemy in ipairs(enemies) do
        if enemy:getHp() > #card_ids then continue end
        if enemy:getHp() <= 0 then
            use.card = sgs.Card_Parse("@DuwuCard=.")
            if use.to then use.to:append(enemy) end
            return
        elseif enemy:getHp() > 1 then
            local hp_ids = {}
            if self.player:distanceTo(enemy, getRangefix(enemy:getHp())) <= self.player:getAttackRange() then
                for _, id in ipairs(card_ids) do
                    table.insert(hp_ids, id)
                    if #hp_ids == enemy:getHp() then break end
                end
                use.card = sgs.Card_Parse("@DuwuCard=" .. table.concat(hp_ids, "+"))
                if use.to then use.to:append(enemy) end
                return
            end
        else
            if not self:isWeak() or self:getSaveNum(true) >= 1 then
                if self.player:distanceTo(enemy, getRangefix(1)) <= self.player:getAttackRange() then
                    use.card = sgs.Card_Parse("@DuwuCard=" .. card_ids[1])
                    if use.to then use.to:append(enemy) end
                    return
                end
            end
        end
    end
end

sgs.ai_use_priority.DuwuCard = 0.6
sgs.ai_use_value.DuwuCard = 2.45
sgs.dynamic_value.damage_card.DuwuCard = true
sgs.ai_card_intention.DuwuCard = 80

sgs.ai_skill_invoke.moukui = function(self, data)
    local target = data:toPlayer()
    sgs.moukui_target = target
    if self:isFriend(target) then return self:needToThrowArmor(target) else return true end
end

sgs.ai_skill_choice.moukui = function(self, choices, data)
    local target = sgs.moukui_target
    if self:isEnemy(target) and self:doNotDiscard(target) then
        return "draw"
    end
    return "discard"
end

sgs.ai_skill_invoke.tianming = true
sgs.ai_skill_invoke.tianming_draw = true
sgs.ai_skill_cardask["@tianming-discard"] = function(self, data)
    local player = self.player
    local use = data:toCardUse()
    if hasManjuanEffect(player) then return "." end
    local source = use.from
    local slash = use.card
    if source and not self:slashIsEffective(slash, player, source) and player:getCards("he"):length() < 3 then return "." end
    local n = player:getMark("tianming_count")
    
    local dangerous = false
    local i = 1
    if source:hasSkill("wushuang") or (source:hasSkill("rouling") and player:isFemale()) or (player:hasSkill("rouling") and source:isFemale()) then
        i = 2
    end
    if self:getCardsNum("Jink") < i and (self:hasHeavySlashDamage(source, slash) or self:isWeak(player)) then
        dangerous = true
    end
    
    local card_ids = {}
    local select_armor = false
    if player:hasArmorEffect("silver_lion") and player:isWounded() then
        table.insert(card_ids, player:getArmor():getEffectiveId())
        select_armor = true
    end
    if slash:isKindOf("FireSlash") and not select_armor and player:hasArmorEffect("vine") and self:getCardsNum("Jink") < i then
        table.insert(card_ids, player:getArmor():getEffectiveId())
        select_armor = true
    end
    local unpreferedCards = {}
    local equips = {}
    local ValuableCard = {}
    local cards = sgs.QList2Table(player:getHandcards())
    self:sortByKeepValue(cards)
    if dangerous then
        for _, card in ipairs(cards) do
            if not player:isJilei(card) then
                if card:isKindOf("Jink") or card:isKindOf("Peach") or card:isKindOf("Analeptic") then 
                    table.insert(ValuableCard, card:getId())
                else
                    table.insert(unpreferedCards, card:getId())
                end
            end
        end
        if player:getTreasure() and not player:isJilei(player:getTreasure()) and player:getPile("wooden_ox"):length() == 0 then 
            table.insert(equips, player:getTreasure():getEffectiveId())
        end
        if player:getWeapon() and not player:isJilei(player:getWeapon()) then 
            table.insert(equips, player:getWeapon():getEffectiveId())
        end
        if player:getOffensiveHorse() and not player:isJilei(player:getOffensiveHorse()) then 
            table.insert(equips, player:getOffensiveHorse():getEffectiveId())
        end
        if player:getDefensiveHorse() and not player:isJilei(player:getDefensiveHorse()) then 
            table.insert(equips, player:getDefensiveHorse():getEffectiveId())
        end
        while #card_ids < n do
            if #unpreferedCards > 0 then
                table.insert(card_ids, unpreferedCards[1])
                table.removeOne(unpreferedCards, unpreferedCards[1])
            elseif #equips > 0 then
                table.insert(card_ids, equips[1])
                table.removeOne(equips, equips[1])
            else
                break
            end
        end
    else
        for _, card in ipairs(cards) do
            if not player:isJilei(card) then
                if not self:isValuableCard(card) then 
                    table.insert(unpreferedCards, card:getId())
                end
            end
        end
        if player:getTreasure() and not player:isJilei(player:getTreasure()) and player:getPile("wooden_ox"):length() == 0 then 
            table.insert(equips, player:getTreasure():getEffectiveId())
        end
        if player:getWeapon() and not player:isJilei(player:getWeapon()) then 
            table.insert(equips, player:getWeapon():getEffectiveId())
        end
        if player:getOffensiveHorse() and not player:isJilei(player:getOffensiveHorse()) then 
            table.insert(equips, player:getOffensiveHorse():getEffectiveId())
        end
        if #unpreferedCards + #equips < n then return "." end
        while #card_ids < n do
            if #unpreferedCards > 0 then
                table.insert(card_ids, unpreferedCards[1])
                table.removeOne(unpreferedCards, unpreferedCards[1])
            elseif #equips > 0 then
                table.insert(card_ids, equips[1])
                table.removeOne(unpreferedCards, equips[1])
            else
                break
            end
        end
    end
    if #card_ids >= n then
        return "$" .. table.concat(card_ids, "+")
    else
        return "."
    end
end

sgs.ai_skill_discard.tianming = function(self, discard_num, min_num, optional, include_equip)
    local player = self.player
    local card_ids = {}
    local select_armor = false
    if self:needToThrowArmor() then
        table.insert(card_ids, player:getArmor():getEffectiveId())
        if #card_ids == discard_num then return card_ids end
        select_armor = true
    end
    if self:hasSkills(sgs.lose_equip_skill, player) then
        if player:hasSkill("xiaoji") then
            if player:getWeapon() and not player:getWeapon():isKindOf("Crossbow") then 
                table.insert(card_ids, player:getWeapon():getEffectiveId())
                if #card_ids == discard_num then return card_ids end
            end
            if player:getOffensiveHorse() then 
                table.insert(card_ids, player:getOffensiveHorse():getEffectiveId())
                if #card_ids == discard_num then return card_ids end
            end
            if player:getArmor() and not select_armor and not self:isWeak() then 
                table.insert(card_ids, player:getArmor():getEffectiveId())
                if #card_ids == discard_num then return card_ids end
            end
            if player:getDefensiveHorse() and not self:isWeak() then 
                table.insert(card_ids, player:getDefensiveHorse():getEffectiveId())
            end
        elseif not select_armor then
            if player:getWeapon() and not player:getWeapon():isKindOf("Crossbow") then table.insert(card_ids, player:getWeapon():getEffectiveId())
            elseif player:getOffensiveHorse() then table.insert(card_ids, player:getOffensiveHorse():getEffectiveId())
            elseif player:getArmor() and not self:isWeak() then table.insert(card_ids, player:getArmor():getEffectiveId())
            elseif player:getDefensiveHorse() and not self:isWeak() then table.insert(card_ids, player:getDefensiveHorse():getEffectiveId())
            end
        end
    end
    if #card_ids == discard_num then return card_ids end
    local unpreferedCards = {}
    local equips = {}
    local cards = sgs.QList2Table(player:getHandcards())
    self:sortByUseValue(cards)
    for _, card in ipairs(cards) do
        if not player:isJilei(card) then
            if not self:isValuableCard(card) then 
                table.insert(unpreferedCards, card:getId())
            end
        end
    end
    if player:getWeapon() and not player:isJilei(player:getWeapon()) then 
        table.insert(equips, player:getWeapon():getEffectiveId())
    end
    if player:getOffensiveHorse() and not player:isJilei(player:getOffensiveHorse()) then 
        table.insert(equips, player:getOffensiveHorse():getEffectiveId())
    end
    if #unpreferedCards + #equips < discard_num then return {} end
    while #card_ids < discard_num do
        if #unpreferedCards > 0 then
            table.insert(card_ids, unpreferedCards[1])
            table.removeOne(unpreferedCards, unpreferedCards[1])
        elseif #equips > 0 then
            table.insert(card_ids, equips[1])
            table.removeOne(unpreferedCards, equips[1])
        else
            break
        end
    end
    if #card_ids == discard_num then return card_ids end
end

local mizhao_skill = {}
mizhao_skill.name = "mizhao"
table.insert(sgs.ai_skills, mizhao_skill)
mizhao_skill.getTurnUseCard = function(self)
    if self.player:hasUsed("MizhaoCard") or self.player:isKongcheng() then return end
    if self:needBear() then return end
    local parsed_card = sgs.Card_Parse("@MizhaoCard=.")
    return parsed_card
end

sgs.ai_skill_use_func.MizhaoCard = function(card, use, self)
    local handcardnum = self.player:getHandcardNum()
    local trash = self:getCard("Disaster") or self:getCard("GodSalvation") or self:getCard("AmazingGrace") or self:getCard("Slash") or self:getCard("FireAttack")
    local count = 0
    local target
    for _, enemy in ipairs(self.enemies) do
        if not enemy:isKongcheng() then count = count + 1 end
    end
    if handcardnum == 1 and trash and count >= 1 and #self.enemies > 1 then
        self:sort(self.enemies, "handcard")
        for _, enemy in ipairs(self.enemies) do
            if not (enemy:hasSkill("manjuan") and enemy:isKongcheng()) and not enemy:hasSkills("tuntian+zaoxian") then
                target = enemy
                break
            end
        end
    end
    if not target then
        self:sort(self.friends_noself, "defense")
        self.friends_noself = sgs.reverse(self.friends_noself)
        if count < 1 then return end
        for _, friend in ipairs(self.friends_noself) do
            if friend:hasSkills("tuntian+zaoxian") and not friend:hasSkill("manjuan") and not self:isWeak(friend) then
                target = friend
                break
            end
        end
        if not target then
            for _, friend in ipairs(self.friends_noself) do
                if not friend:hasSkill("manjuan") then
                    target = friend
                    break
                end
            end
        end
    end
    if target then
        for _, acard in sgs.qlist(self.player:getHandcards()) do
            if isCard("Peach", acard, self.player) and self.player:getHandcardNum() > 1 and self.player:isWounded()
                and not self:needToLoseHp(self.player) then
                    use.card = acard
                    return
            end
        end
        use.card = card
        if use.to then
            target:setFlags("AI_MizhaoTarget")
            use.to:append(target)
        end
    end
end

sgs.ai_use_priority.MizhaoCard = 1.5
sgs.ai_card_intention.MizhaoCard = 0
sgs.ai_playerchosen_intention.mizhao = 10

sgs.ai_skill_playerchosen.mizhao = function(self, targets)
    self:sort(self.enemies, "defense")
    local slash = sgs.Sanguosha:cloneCard("slash")
    local from
    for _, player in sgs.qlist(self.room:getOtherPlayers(self.player)) do
        if player:hasFlag("AI_MizhaoTarget") then
            from = player
            from:setFlags("-AI_MizhaoTarget")
            break
        end
    end
    if from then
        for _, to in ipairs(self.enemies) do
            if targets:contains(to) and self:slashIsEffective(slash, to, from) and not self:getDamagedEffects(to, from, true)
                and not self:needToLoseHp(to, from, true) and not self:findLeijiTarget(to, 50, from) then
                return to
            end
        end
    end
    for _, to in ipairs(self.enemies) do
        if targets:contains(to) then
            return to
        end
    end
end

function sgs.ai_skill_pindian.mizhao(minusecard, self, requestor, maxcard)
    local req
    if self.player:objectName() == requestor:objectName() then
        for _, p in sgs.qlist(self.room:getOtherPlayers(self.player)) do
            if p:hasFlag("MizhaoPindianTarget") then
                req = p
                break
            end
        end
    else
        req = requestor
    end
    local cards, maxcard = sgs.QList2Table(self.player:getHandcards())
    local max_value = 0
    self:sortByKeepValue(cards)
    max_value = self:getKeepValue(cards[#cards])
    local function compare_func1(a, b)
        return a:getNumber() > b:getNumber()
    end
    local function compare_func2(a, b)
        return a:getNumber() < b:getNumber()
    end
    if self:isFriend(req) and self.player:getHp() > req:getHp() then
        table.sort(cards, compare_func2)
    else
        table.sort(cards, compare_func1)
    end
    for _, card in ipairs(cards) do
        if max_value > 7 or self:getKeepValue(card) < 7 or card:isKindOf("EquipCard") then maxcard = card break end
    end
    return maxcard or cards[1]
end

sgs.ai_skill_cardask["@jieyuan-increase"] = function(self, data)
    local damage = data:toDamage()
    local target = damage.to
    if self:isFriend(target) then return "." end
    if target:hasArmorEffect("silver_lion") then return "." end
    local cards = sgs.QList2Table(self.player:getHandcards())
    self:sortByKeepValue(cards)
    for _,card in ipairs(cards) do
        if card:isBlack() then return "$" .. card:getEffectiveId() end
    end
    return "."
end

sgs.ai_skill_cardask["@jieyuan-decrease"] = function(self, data)
    local damage = data:toDamage()
    local cards = sgs.QList2Table(self.player:getHandcards())
    self:sortByKeepValue(cards)
    if damage.card and damage.card:isKindOf("Slash") then
        if self:hasHeavySlashDamage(damage.from, damage.card, self.player) then
            for _,card in ipairs(cards) do
                if card:isRed() then return "$" .. card:getEffectiveId() end
            end
        end
    end
    if self:getDamagedEffects(self.player, damage.from) and damage.damage <= 1 then return "." end
    if self:needToLoseHp(self.player, damage.from) and damage.damage <= 1 then return "." end
    for _,card in ipairs(cards) do
        if card:isRed() then return "$" .. card:getEffectiveId() end
    end
    return "."
end

function sgs.ai_cardneed.jieyuan(to, card)
    return to:getHandcardNum() < 4 and (to:getHp() >= 3 and true or card:isRed())
end

sgs.ai_skill_invoke.fenxin = function(self, data)
    local target = data:toPlayer()
    local target_role = sgs.evaluatePlayerRole(target)
    local self_role = self.player:getRole()
    if target_role == "renegade" or target_role == "neutral" then return false end
    local process = sgs.gameProcess(self.room)
    return (target_role == "rebel" and self.role ~= "rebel" and process:match("rebel"))
            or (target_role == "loyalist" and self.role ~= "loyalist" and process:match("loyal"))
end

function getNextJudgeReason(self, player)
    if self:playerGetRound(player) > 2 then
        if player:hasSkills("ganglie|vsganglie") then return end
        local caiwenji = self.room:findPlayerBySkillName("beige")
        if caiwenji and caiwenji:canDiscard(caiwenji, "he") and self:isFriend(caiwenji, player) then return end
        if player:hasArmorEffect("eight_diagram") or player:hasSkill("bazhen") then
            if self:playerGetRound(player) > 3 and self:isEnemy(player) then return "EightDiagram"
            else return end
        end
    end
    if self:isFriend(player) and player:hasSkill("luoshen") then return "luoshen" end
    if not player:getJudgingArea():isEmpty() and not player:containsTrick("YanxiaoCard") then
        return player:getJudgingArea():last():objectName()
    end
    if player:hasSkill("qianxi") then return "qianxi" end
    if player:hasSkill("nosmiji") and player:getLostHp() > 0 then return "nosmiji" end
    if player:hasSkill("tuntian") then return "tuntian" end
    if player:hasSkill("tieji") then return "tieji" end
    if player:hasSkill("nosqianxi") then return "nosqianxi" end
    if player:hasSkill("caizhaoji_hujia") then return "caizhaoji_hujia" end
end

local zhoufu_skill = {}
zhoufu_skill.name = "zhoufu"
table.insert(sgs.ai_skills, zhoufu_skill)
zhoufu_skill.getTurnUseCard = function(self)
    if self.player:hasUsed("ZhoufuCard") or self.player:isKongcheng() then return end
    return sgs.Card_Parse("@ZhoufuCard=.")
end

sgs.ai_skill_use_func.ZhoufuCard = function(card, use, self)
    local cards = {}
    for _, card in sgs.qlist(self.player:getHandcards()) do
        table.insert(cards, sgs.Sanguosha:getEngineCard(card:getEffectiveId()))
    end
    self:sortByKeepValue(cards)
    self:sort(self.friends_noself)
    local zhenji
    for _, friend in ipairs(self.friends_noself) do
        if friend:getPile("incantation"):length() > 0 then continue end
        local reason = getNextJudgeReason(self, friend)
        if reason then
            if reason == "luoshen" then
                zhenji = friend
            elseif reason == "indulgence" then
                for _, card in ipairs(cards) do
                    if card:getSuit() == sgs.Card_Heart or (friend:hasSkill("hongyan") and card:getSuit() == sgs.Card_Spade)
                        and (friend:hasSkill("tiandu") or not self:isValuableCard(card)) then
                        use.card = sgs.Card_Parse("@ZhoufuCard=" .. card:getEffectiveId())
                        if use.to then use.to:append(friend) end
                        return
                    end
                end
            elseif reason == "supply_shortage" then
                for _, card in ipairs(cards) do
                    if card:getSuit() == sgs.Card_Club and (friend:hasSkill("tiandu") or not self:isValuableCard(card)) then
                        use.card = sgs.Card_Parse("@ZhoufuCard=" .. card:getEffectiveId())
                        if use.to then use.to:append(friend) end
                        return
                    end
                end
            elseif reason == "lightning" and not friend:hasSkills("hongyan|wuyan") then
                for _, card in ipairs(cards) do
                    if (card:getSuit() ~= sgs.Card_Spade or card:getNumber() == 1 or card:getNumber() > 9)
                        and (friend:hasSkill("tiandu") or not self:isValuableCard(card)) then
                        use.card = sgs.Card_Parse("@ZhoufuCard=" .. card:getEffectiveId())
                        if use.to then use.to:append(friend) end
                        return
                    end
                end
            elseif reason == "nosmiji" then
                for _, card in ipairs(cards) do
                    if card:getSuit() == sgs.Card_Club or (card:getSuit() == sgs.Card_Spade and not friend:hasSkill("hongyan")) then
                        use.card = sgs.Card_Parse("@ZhoufuCard=" .. card:getEffectiveId())
                        if use.to then use.to:append(friend) end
                        return
                    end
                end
            elseif reason == "nosqianxi" or reason == "tuntian" then
                for _, card in ipairs(cards) do
                    if (card:getSuit() ~= sgs.Card_Heart and not (card:getSuit() == sgs.Card_Spade and friend:hasSkill("hongyan")))
                        and (friend:hasSkill("tiandu") or not self:isValuableCard(card)) then
                        use.card = sgs.Card_Parse("@ZhoufuCard=" .. card:getEffectiveId())
                        if use.to then use.to:append(friend) end
                        return
                    end
                end
            elseif reason == "tieji" or reason == "caizhaoji_hujia" then
                for _, card in ipairs(cards) do
                    if (card:isRed() or card:getSuit() == sgs.Card_Spade and friend:hasSkill("hongyan"))
                        and (friend:hasSkill("tiandu") or not self:isValuableCard(card)) then
                        use.card = sgs.Card_Parse("@ZhoufuCard=" .. card:getEffectiveId())
                        if use.to then use.to:append(friend) end
                        return
                    end
                end
            end
        end
    end
    if zhenji then
        for _, card in ipairs(cards) do
            if card:isBlack() and not (zhenji:hasSkill("hongyan") and card:getSuit() == sgs.Card_Spade) then
                use.card = sgs.Card_Parse("@ZhoufuCard=" .. card:getEffectiveId())
                if use.to then use.to:append(zhenji) end
                return
            end
        end
    end
    self:sort(self.enemies)
    for _, enemy in ipairs(self.enemies) do
        if enemy:getPile("incantation"):length() > 0 then continue end
        local reason = getNextJudgeReason(self, enemy)
        if not enemy:hasSkill("tiandu") and reason then
            if reason == "indulgence" then
                for _, card in ipairs(cards) do
                    if not (card:getSuit() == sgs.Card_Heart or (enemy:hasSkill("hongyan") and card:getSuit() == sgs.Card_Spade))
                        and not self:isValuableCard(card) then
                        use.card = sgs.Card_Parse("@ZhoufuCard=" .. card:getEffectiveId())
                        if use.to then use.to:append(enemy) end
                        return
                    end
                end
            elseif reason == "supply_shortage" then
                for _, card in ipairs(cards) do
                    if card:getSuit() ~= sgs.Card_Club and not self:isValuableCard(card) then
                        use.card = sgs.Card_Parse("@ZhoufuCard=" .. card:getEffectiveId())
                        if use.to then use.to:append(enemy) end
                        return
                    end
                end
            elseif reason == "lightning" and not enemy:hasSkills("hongyan|wuyan") then
                for _, card in ipairs(cards) do
                    if card:getSuit() == sgs.Card_Spade and card:getNumber() >= 2 and card:getNumber() <= 9 then
                        use.card = sgs.Card_Parse("@ZhoufuCard=" .. card:getEffectiveId())
                        if use.to then use.to:append(enemy) end
                        return
                    end
                end
            elseif reason == "nosmiji" then
                for _, card in ipairs(cards) do
                    if card:isRed() or card:getSuit() == sgs.Card_Spade and enemy:hasSkill("hongyan") then
                        use.card = sgs.Card_Parse("@ZhoufuCard=" .. card:getEffectiveId())
                        if use.to then use.to:append(enemy) end
                        return
                    end
                end
            elseif reason == "nosqianxi" or reason == "tuntian" then
                for _, card in ipairs(cards) do
                    if (card:getSuit() == sgs.Card_Heart or card:getSuit() == sgs.Card_Spade and enemy:hasSkill("hongyan"))
                        and not self:isValuableCard(card) then
                        use.card = sgs.Card_Parse("@ZhoufuCard=" .. card:getEffectiveId())
                        if use.to then use.to:append(enemy) end
                        return
                    end
                end
            elseif reason == "tieji" or reason == "caizhaoji_hujia" then
                for _, card in ipairs(cards) do
                    if (card:getSuit() == sgs.Card_Club or (card:getSuit() == sgs.Card_Spade and not enemy:hasSkill("hongyan")))
                        and not self:isValuableCard(card) then
                        use.card = sgs.Card_Parse("@ZhoufuCard=" .. card:getEffectiveId())
                        if use.to then use.to:append(enemy) end
                        return
                    end
                end
            end
        end
    end

    local has_indulgence, has_supplyshortage
    local friend
    for _, p in ipairs(self.friends) do
        if getKnownCard(p, self.player, "Indulgence", true, "he") > 0 then
            has_indulgence = true
            friend = p
            break
        end
        if getKnownCard(p, self.player, "SupplySortage", true, "he") > 0 then
            has_supplyshortage = true
            friend = p
            break
        end
    end
    if has_indulgence then
        local indulgence = sgs.Sanguosha:cloneCard("indulgence")
        for _, enemy in ipairs(self.enemies) do
            if enemy:getPile("incantation"):length() > 0 then continue end
            if self:hasTrickEffective(indulgence, enemy, friend) and self:playerGetRound(friend) < self:playerGetRound(enemy) and not self:willSkipPlayPhase(enemy) then
                for _, card in ipairs(cards) do
                    if not (card:getSuit() == sgs.Card_Heart or (enemy:hasSkill("hongyan") and card:getSuit() == sgs.Card_Spade))
                        and not self:isValuableCard(card) then
                        use.card = sgs.Card_Parse("@ZhoufuCard=" .. card:getEffectiveId())
                        if use.to then use.to:append(enemy) end
                        return
                    end
                end
            end
        end
    elseif has_supplyshortage then
        local supplyshortage = sgs.Sanguosha:cloneCard("supply_shortage")
        local distance = self:getDistanceLimit(supplyshortage, friend)
        for _, enemy in ipairs(self.enemies) do
            if enemy:getPile("incantation"):length() > 0 then continue end
            if self:hasTrickEffective(supplyshortage, enemy, friend) and self:playerGetRound(friend) < self:playerGetRound(enemy)
                and not self:willSkipDrawPhase(enemy) and friend:distanceTo(enemy) <= distance then
                for _, card in ipairs(cards) do
                    if card:getSuit() ~= sgs.Card_Club and not self:isValuableCard(card) then
                        use.card = sgs.Card_Parse("@ZhoufuCard=" .. card:getEffectiveId())
                        if use.to then use.to:append(enemy) end
                        return
                    end
                end
            end
        end
    end
end

sgs.ai_card_intention.ZhoufuCard = 0
sgs.ai_use_value.ZhoufuCard = 2
sgs.ai_use_priority.ZhoufuCard = sgs.ai_use_priority.Indulgence - 0.1

local function getKangkaiCard(self, target, data)
    local use = data:toCardUse()
    local weapon, armor, def_horse, off_horse = {}, {}, {}, {}
    for _, card in sgs.qlist(self.player:getHandcards()) do
        if card:isKindOf("Weapon") then table.insert(weapon, card)
        elseif card:isKindOf("Armor") then table.insert(armor, card)
        elseif card:isKindOf("DefensiveHorse") then table.insert(def_horse, card)
        elseif card:isKindOf("OffensiveHorse") then table.insert(off_horse, card)
        end
    end
    if #armor > 0 then
        for _, card in ipairs(armor) do
            if ((not target:getArmor() and not target:hasSkills("bazhen|yizhong"))
                or (target:getArmor() and self:evaluateArmor(card, target) >= self:evaluateArmor(target:getArmor(), target)))
                and not (card:isKindOf("Vine") and use.card:isKindOf("FireSlash") and self:slashIsEffective(use.card, target, use.from)) then
                return card:getEffectiveId()
            end
        end
    end
    if self:needToThrowArmor()
        and ((not target:getArmor() and not target:hasSkills("bazhen|yizhong"))
            or (target:getArmor() and self:evaluateArmor(self.player:getArmor(), target) >= self:evaluateArmor(target:getArmor(), target)))
        and not (self.player:getArmor():isKindOf("Vine") and use.card:isKindOf("FireSlash") and self:slashIsEffective(use.card, target, use.from)) then
        return self.player:getArmor():getEffectiveId()
    end
    if #def_horse > 0 then return def_horse[1]:getEffectiveId() end
    if #weapon > 0 then
        for _, card in ipairs(weapon) do
            if not target:getWeapon()
                or (self:evaluateArmor(card, target) >= self:evaluateArmor(target:getWeapon(), target)) then
                return card:getEffectiveId()
            end
        end
    end
    if self.player:getWeapon() and self:evaluateWeapon(self.player:getWeapon()) < 5
        and (not target:getArmor()
            or (self:evaluateArmor(self.player:getWeapon(), target) >= self:evaluateArmor(target:getWeapon(), target))) then
        return self.player:getWeapon():getEffectiveId()
    end
    if #off_horse > 0 then return off_horse[1]:getEffectiveId() end
    if self.player:getOffensiveHorse()
        and ((self.player:getWeapon() and not self.player:getWeapon():isKindOf("Crossbow")) or self.player:hasSkills("mashu|tuntian")) then
        return self.player:getOffensiveHorse():getEffectiveId()
    end
end

sgs.ai_skill_invoke.kangkai = function(self, data)
    self.kangkai_give_id = nil
    if hasManjuanEffect(self.player) then return false end
    local target = data:toPlayer()
    if not target then return false end
    if target:objectName() == self.player:objectName() then
        return true
    elseif not self:isFriend(target) then
        return hasManjuanEffect(target)
    else
        local id = getKangkaiCard(self, target, self.player:getTag("KangkaiSlash"))
        if id then return true else return not self:needKongcheng(target, true) end
    end
end

sgs.ai_skill_cardask["@kangkai_give"] = function(self, data, pattern, target)
    if self:isFriend(target) then
        local id = getKangkaiCard(self, target, data)
        if id then return "$" .. id end
        if self:getCardsNum("Jink") > 1 then
            for _, card in sgs.qlist(self.player:getHandcards()) do
                if isCard("Jink", card, target) then return "$" .. card:getEffectiveId() end
            end
        end
        for _, card in sgs.qlist(self.player:getHandcards()) do
            if not self:isValuableCard(card) then return "$" .. card:getEffectiveId() end
        end
    else
        local to_discard = self:askForDiscard("dummyreason", 1, 1, false, true)
        if #to_discard > 0 then return "$" .. to_discard[1] end
    end
end

sgs.ai_skill_invoke.kangkai_use = function(self, data)
    local use = self.player:getTag("KangkaiSlash"):toCardUse()
    local card = self.player:getTag("KangkaiGivenCard"):toCard()
    if not use.card or not card then return false end
    if card:isKindOf("Vine") and use.card:isKindOf("FireSlash") and self:slashIsEffective(use.card, self.player, use.from) then return false end
    if ((card:isKindOf("DefensiveHorse") and self.player:getDefensiveHorse())
        or (card:isKindOf("OffensiveHorse") and (self.player:getOffensiveHorse() or (self.player:hasSkill("drmashu") and self.player:getDefensiveHorse()))))
        and not self.player:hasSkills(sgs.lose_equip_skill) then
        return false
    end
    if card:isKindOf("Armor")
        and ((self.player:hasSkills("bazhen|yizhong") and not self.player:getArmor())
            or (self.player:getArmor() and self:evaluateArmor(card) < self:evaluateArmor(self.player:getArmor()))) then return false end
    if card:isKindOf("Weanpon") and (self.player:getWeapon() and self:evaluateArmor(card) < self:evaluateArmor(self.player:getWeapon())) then return false end
    return true
end

sgs.ai_skill_use["@@yingjian"] = function(self, prompt)
    local slash = sgs.Sanguosha:cloneCard("slash")
    slash:setSkillName("yingjian")
	slash:setFlags("Global_NoDistanceChecking")
    local dummy_use = { isDummy = true, to = sgs.SPlayerList() }
    self:useBasicCard(slash, dummy_use)
    if dummy_use.card and not dummy_use.to:isEmpty() then
        local target_objectname = {}
        for _, p in sgs.qlist(dummy_use.to) do
            table.insert(target_objectname, p:objectName())
        end
        return dummy_use.card:toString() .. "->" .. table.concat(target_objectname, "+")
    end
end

--星彩
local qiangwu_skill = {}
qiangwu_skill.name = "qiangwu"
table.insert(sgs.ai_skills, qiangwu_skill)
qiangwu_skill.getTurnUseCard = function(self)
    if self.player:hasUsed("QiangwuCard") then return end
    return sgs.Card_Parse("@QiangwuCard=.")
end

sgs.ai_skill_use_func.QiangwuCard = function(card, use, self)
    if self.player:hasUsed("QiangwuCard") then return end
    use.card = card
end

sgs.ai_use_value.QiangwuCard = 3
sgs.ai_use_priority.QiangwuCard = 11

--祖茂
sgs.ai_skill_use["@@yinbing"] = function(self, prompt)
    --手牌
    local otherNum = self.player:getHandcardNum() - self:getCardsNum("BasicCard")
    if otherNum == 0 then return "." end

    local slashNum = self:getCardsNum("Slash")
    local jinkNum = self:getCardsNum("Jink")
    local enemyNum = #self.enemies
    local friendNum = #self.friends

    local value = 0
    if otherNum > 1 then value = value + 0.3 end
    for _,card in sgs.qlist(self.player:getHandcards()) do
        if card:isKindOf("EquipCard") then value = value + 1 end
    end
    if otherNum == 1 and self:getCardsNum("Nullification") == 1 then value = value - 0.2 end

    --已有引兵
    if self.player:getPile("yinbing"):length() > 0 then value = value + 0.2 end

    --双将【空城】
    if self:needKongcheng() and self.player:getHandcardNum() == 1 then value = value + 3 end

    if enemyNum == 1 then value = value + 0.7 end
    if friendNum - enemyNum > 0 then value = value + 0.2 else value = value - 0.3 end
    local slash = sgs.Sanguosha:cloneCard("slash")
    --关于 【杀】和【决斗】
    if slashNum == 0 then value = value - 0.1 end
    if jinkNum == 0 then value = value - 0.5 end
    if jinkNum == 1 then value = value + 0.2 end
    if jinkNum > 1 then value = value + 0.5 end
    if self.player:getArmor() and self.player:getArmor():isKindOf("EightDiagram") then value = value + 0.4 end
    for _,enemy in ipairs(self.enemies) do
        if enemy:canSlash(self.player, slash) and self:slashIsEffective(slash, self.player, enemy) and (enemy:inMyAttackRange(self.player) or ememy:hasSkills("zhuhai|shensu")) then 
            if ((enemy:getWeapon() and enemy:getWeapon():isKindOf("Crossbow")) or enemy:hasSkills("paoxiao|tianyi|xianzhen|jiangchi|fuhun|gongqi|longyin|qiangwu")) and enemy:getHandcardNum() > 1 then
                value = value - 0.2
            end
            if enemy:hasSkills("tieqi|wushuang|yijue|liegong|mengjin|qianxi") then
                value = value - 0.2
            end
            value = value - 0.2 
        end
        if enemy:hasSkills("lijian|shuangxiong|mingce|mizhao") then
            value = value - 0.2
        end
    end
    --肉盾
    local yuanshu = self.room:findPlayerBySkillName("tongji")
    if yuanshu and yuanshu:getHandcardNum() > yuanshu:getHp() then value = value + 0.4 end
    for _,friend in ipairs(self.friends) do
        if friend:hasSkills("fangquan|zhenwei|kangkai") then value = value + 0.4 end
    end

    if value < 0 then return "." end

    local card_ids = {}
    local nulId
    for _,card in sgs.qlist(self.player:getHandcards()) do
        if not card:isKindOf("BasicCard") then
            if card:isKindOf("Nullification") then
                nulId = card:getEffectiveId()
            else
                table.insert(card_ids, card:getEffectiveId())
            end
        end
    end
    if nulId and #card_ids == 0 then
        table.insert(card_ids, nulId)
    end
    return "@YinbingCard=" .. table.concat(card_ids, "+") .. "->."
end

sgs.yinbing_keep_value = {
    EquipCard = 5,
    TrickCard = 4
}

sgs.ai_skill_invoke.juedi = function(self, data)
    for _, friend in ipairs(self.friends_noself) do
        if friend:getLostHp() > 0 then return true end
    end
    if self:isWeak() then return true end
    return false
end

sgs.ai_skill_playerchosen.juedi  = function(self, targets)
    targets = sgs.QList2Table(targets)
    self:sort(targets, "defense")
    for _,p in ipairs(targets) do
        if self:isFriend(p) then return p end
    end
    return
end

sgs.ai_skill_invoke.meibu = function (self, data)
    local target = self.room:getCurrent()
    if self:isFriend(target) then
        --锦囊不如杀重要的情况
        local trick = sgs.Sanguosha:cloneCard("nullification")
        if target:hasSkill("wumou") or target:isJilei(trick) then return true end
        local slash = sgs.Sanguosha:cloneCard("Slash")
        dummy_use = {isDummy = true, from = target, to = sgs.SPlayerList()}
        self:useBasicCard(slash, dummy_use)
        if target:getWeapon() and target:getWeapon():isKindOf("Crossbow") and not dummy_use.to:isEmpty() then return true end
        if target:hasSkills("paoxiao|tianyi|xianzhen|jiangchi|fuhun|qiangwu") and not self:isWeak(target) and not dummy_use.to:isEmpty() then return true end
    else
        local slash2 = sgs.Sanguosha:cloneCard("Slash")
        if target:isJilei(slash2) then return true end
        if target:getWeapon() and target:getWeapon():isKindOf("blade") then return false end
        if target:hasSkills("paoxiao|tianyi|xianzhen|jiangchi|fuhun|qiangwu") or (target:getWeapon() and target:getWeapon():isKindOf("Crossbow")) then return false end
        if target:hasSkills("wumou|gongqi") then return false end
        if target:hasSkills("guose|qixi|duanliang|luanji") and target:getHandcardNum() > 1 then return true end
        if target:hasSkills("shuangxiong") and not self:isWeak(target) then return true end
        if not self:slashIsEffective(slash2, self.player, target) and not self:isWeak() then return true end
        if self.player:getArmor() and self.player:getArmor():isKindOf("Vine") and not self:isWeak() then return true end
        if self.player:getArmor() and not self:isWeak() and self:getCardsNum("Jink") > 0 then return true end
        if self.player:getMark("mumu") > 0 then return true end
    end
    return false
end


local mumu_skill = {}
mumu_skill.name = "mumu"
table.insert(sgs.ai_skills, mumu_skill)
mumu_skill.getTurnUseCard = function(self)
    if self.player:hasUsed("MumuCard") then return end
    self.mumu_choice = ""
    local players = self:findPlayerToDiscard("e", true, true, nil, true)
    local equip, armor = #players > 0, (self.player:getArmor() and self.player:getArmor():isKindOf("SilverLion") and self.player:getLostHp() > 0)

    local armorPlayersF = {}
    local armorPlayersE = {}

    for _,p in ipairs(self.friends_noself) do
        if p:getArmor() then
            table.insert(armorPlayersF, p)
        end
    end
    for _,p in ipairs(self.enemies) do
        if p:getArmor() then
            table.insert(armorPlayersE, p)
        end
    end

    if not armor and #armorPlayersF > 0 then
        for _,friend in ipairs(armorPlayersF) do
            if (friend:getArmor():isKindOf("Vine") and not self.player:getArmor() and not friend:hasSkills("kongcheng|zhiji")) or (friend:getArmor():isKindOf("SilverLion") and friend:getLostHp() > 0) then
                armor = true
                break
            end
        end
    end

    if not armor and #armorPlayersE > 0 then
        armor = true
    end

    if armor then
        if self.player:getArmor() and equip then
            self.mumu_choice = "discard_equip"
        else
            self.mumu_choice = "obtain_armor"
        end
    elseif equip then
        self.mumu_choice = "discard_equip"
    else
        return
    end
    local to_discard = self:askForDiscard("mumu", 1, 1, false, true)
    if #to_discard < 1 then return end
    return sgs.Card_Parse("@MumuCard=" .. to_discard[1])
end

sgs.ai_skill_use_func.MumuCard = function(card, use, self)
    if self.player:hasUsed("MumuCard") then return end
    use.card = card
end

sgs.ai_use_priority.MumuCard = 5.5
sgs.ai_use_value.MumuCard = 5

sgs.ai_skill_choice.mumu = function(self, choices)
    local choice = self.mumu_choice
    if table.contains(choices, choice) then
        return choice
    end
    return choices[1]
end

sgs.ai_skill_playerchosen.mumu = function(self, targets)
    local choice = self.player:getTag("MumuChoice"):toString()
    if choice == "obtain_armor" then
        if self.player:getArmor() and self.player:getArmor():isKindOf("SilverLion") and self.player:getLostHp() > 0 then
            return self.player
        end
        for _,p in ipairs(self.friends_noself) do
            if p:getArmor() and self:needToThrowArmor(p) then
                return p
            end
        end
        for _,p in ipairs(self.enemies) do
            if p:getArmor() and not self:needToThrowArmor(p) and not p:hasSkills(sgs.lose_equip_skill) then
                return p
            end
        end
    end
    local players = self:findPlayerToDiscard("e", true, true, nil, true)
    if #players > 0 then
        return players[1]
    end
    return targets[1]
end

--马良
local xiemu_skill = {}
xiemu_skill.name = "xiemu"
table.insert(sgs.ai_skills, xiemu_skill)
xiemu_skill.getTurnUseCard = function(self)
    if self.player:hasUsed("XiemuCard") then return end
    if self:getCardsNum("Slash") == 0 then return end

    local kingdomDistribute = {}
    kingdomDistribute["wei"] = 0
    kingdomDistribute["shu"] = 0
    kingdomDistribute["wu"] = 0
    kingdomDistribute["qun"] = 0
    for _,p in sgs.qlist(self.room:getAlivePlayers()) do
		if not kingdomDistribute[p:getKingdom()] then continue end
        if kingdomDistribute[p:getKingdom()] and self:isEnemy(p) and p:inMyAttackRange(self.player) 
            then kingdomDistribute[p:getKingdom()] = kingdomDistribute[p:getKingdom()] + 1
            else kingdomDistribute[p:getKingdom()] = kingdomDistribute[p:getKingdom()] + 0.2 end
        if p:hasSkill("luanji") and p:getHandcardNum() > 2 then kingdomDistribute["qun"] = kingdomDistribute["qun"] + 3 end
        if p:hasSkill("qixi") and self:isEnemy(p) and p:getHandcardNum() > 2 then kingdomDistribute["wu"] = kingdomDistribute["wu"] + 2 end
        if p:hasSkill("zaoxian") and self:isEnemy(p) and p:getPile("field"):length() > 1 then kingdomDistribute["wei"] = kingdomDistribute["wei"] + 2 end
    end
    maxK = "wei"
    if kingdomDistribute["shu"] > kingdomDistribute[maxK] then maxK = "shu" end
    if kingdomDistribute["wu"] > kingdomDistribute[maxK] then maxK = "wu" end
    if kingdomDistribute["qun"] > kingdomDistribute[maxK] then maxK = "qun" end
    if kingdomDistribute[maxK] < 1 then return end
    local subcard
    for _,c in sgs.qlist(self.player:getHandcards()) do
        if c:isKindOf("Slash") then subcard = c end
    end
    if not subcard then return end
    return sgs.Card_Parse("@XiemuCard=" .. subcard:getEffectiveId() .. ":" .. maxK)
end

sgs.ai_skill_use_func.XiemuCard = function(card, use, self)
    if self.player:hasUsed("XiemuCard") then return end
    use.card = card
end

sgs.ai_use_value.XiemuCard = 5
sgs.ai_use_priority.XiemuCard = 10

sgs.ai_skill_choice.xiemu = "yes"

sgs.ai_skill_invoke.naman = function(self, data)
    if self:needKongcheng(self.player, true) and self.player:getHandcardNum() == 0 then return false end
    return true
end


--new maliang

--zishu

--yingyuan

sgs.ai_skill_playerchosen.yingyuan = function(self, targets)
    
	return nil
end








--chengyi

--黄巾雷使
sgs.ai_view_as.fulu = function(card, player, card_place)
    local suit = card:getSuitString()
    local number = card:getNumberString()
    local card_id = card:getEffectiveId()
    if card_place ~= sgs.Player_PlaceSpecial and card:getClassName() == "Slash" and not card:hasFlag("using") then
        return ("thunder_slash:fulu[%s:%s]=%d"):format(suit, number, card_id)
    end
end

sgs.ai_skill_invoke.fulu = function(self, data)
    local use = data:toCardUse()
    for _, player in sgs.qlist(use.to) do
        if self:isEnemy(player) and self:damageIsEffective(player, sgs.DamageStruct_Thunder) and sgs.isGoodTarget(player, self.enemies, self) then
            return true
        end
    end
    return false
end

local fulu_skill = {}
fulu_skill.name = "fulu"
table.insert(sgs.ai_skills, fulu_skill)
fulu_skill.getTurnUseCard = function(self, inclusive)
    local cards = self.player:getCards("h")
    cards = sgs.QList2Table(cards)

    local slash
    self:sortByUseValue(cards, true)
    for _, card in ipairs(cards) do
        if card:getClassName() == "Slash" then
            slash = card
            break
        end
    end

    if not slash then return nil end
    local dummy_use = { to = sgs.SPlayerList(), isDummy = true }
    self:useCardThunderSlash(slash, dummy_use)
    if dummy_use.card and dummy_use.to:length() > 0 then
        local use = sgs.CardUseStruct()
        use.from = self.player
        use.to = dummy_use.to
        use.card = slash
        local data = sgs.QVariant()
        data:setValue(use)
        if not sgs.ai_skill_invoke.fulu(self, data) then return nil end
    else return nil end

    if slash then
        local suit = slash:getSuitString()
        local number = slash:getNumberString()
        local card_id = slash:getEffectiveId()
        local card_str = ("thunder_slash:fulu[%s:%s]=%d"):format(suit, number, card_id)
        local mySlash = sgs.Card_Parse(card_str)

        assert(mySlash)
        return mySlash
    end
end

sgs.ai_skill_invoke.zhuji = function(self, data)
    local damage = data:toDamage()
    if self:isFriend(damage.from) and not self:isFriend(damage.to) then return true end
    return false
end

--文聘
sgs.ai_skill_cardask["@zhenwei"] = function(self, data)
    local use = data:toCardUse()
    if use.to:length() ~= 1 or not use.from or not use.card then return "." end
    if not self:isFriend(use.to:at(0)) or self:isFriend(use.from) then return "." end
    if use.to:at(0):hasSkills("liuli|tianxiang") and use.card:isKindOf("Slash") and use.to:at(0):getHandcardNum() > 1 then return "." end
    if use.card:isKindOf("Slash") and not self:slashIsEffective(use.card, use.to:at(0), use.from) then return "." end
    if use.to:at(0):hasSkills(sgs.masochism_skill) and not use.to:at(0):isWeak() then return "." end
    if self.player:getHandcardNum() + self.player:getEquips():length() < 2 and not self:isWeak(use.to:at(0)) then return "." end
    local to_discard = self:askForDiscard("zhenwei", 1, 1, false, true)
    if #to_discard > 0 then
        if not (use.card:isKindOf("Slash") and  self:isWeak(use.to:at(0))) and sgs.Sanguosha:getCard(to_discard[1]):isKindOf("Peach") then return "." end
        return "$" .. to_discard[1] 
    else 
        return "." 
    end
end

sgs.ai_skill_choice.zhenwei = function(self, choices, data)
    local use = data:toCardUse()
    if self:isWeak() or self.player:getHandcardNum() < 2 then return "null" end
    if use.card:isKindOf("TrickCard") and use.from:hasSkill("jizhi") then return "draw" end
    if use.card:isKindOf("Slash") and (use.from:hasSkills("paoxiao|tianyi|xianzhen|jiangchi|fuhun|qiangwu") 
        or (use.from:getWeapon() and use.from:getWeapon():isKindOf("Crossbow"))) and self:getCardsNum("Jink") == 0 then return "null" end
    if use.card:isKindOf("SupplyShortage") then return "null" end
    if use.card:isKindOf("Slash") and self:getCardsNum("Jink") == 0 and self.player:getLostHp() > 0 then return "null" end
    if use.card:isKindOf("Indulgence") and self.player:getHandcardNum() + 1 > self.player:getHp() then return "null" end
    if use.card:isKindOf("Slash") and use.from:hasSkills("tieqi|wushuang|yijue|liegong|mengjin|qianxi") and not (use.from:getWeapon() and use.from:getWeapon():isKindOf("Crossbow")) then return "null" end
    return "draw"
end

--司马朗
local quji_skill = {}
quji_skill.name = "quji"
table.insert(sgs.ai_skills, quji_skill)
quji_skill.getTurnUseCard = function(self)
    if self.player:getHandcardNum() < self.player:getLostHp() then return nil end
    if self.player:usedTimes("QujiCard") > 0 then return nil end
    if self.player:getLostHp() == 0 then return end

    local cards = self.player:getHandcards()
    cards = sgs.QList2Table(cards)

    local arr1, arr2 = self:getWoundedFriend(false, true)
    if #arr1 + #arr2 < self.player:getLostHp() then return end

    local compare_func = function(a, b)
        local v1 = self:getKeepValue(a) + ( a:isBlack() and 50 or 0 ) + ( a:isKindOf("Peach") and 50 or 0 )
        local v2 = self:getKeepValue(b) + ( b:isBlack() and 50 or 0 ) + ( b:isKindOf("Peach") and 50 or 0 )
        return v1 < v2
    end
    table.sort(cards, compare_func)

    if cards[1]:isBlack() and self:getLostHp() > 0 then return end
    if self.player:getLostHp() == 2 and (cards[1]:isBlack() or cards[2]:isBlack()) then return end
    
    local card_str = "@QujiCard="..cards[1]:getId()
    local left = self.player:getLostHp() - 1
    while left > 0 do
        card_str = card_str.."+"..cards[self.player:getLostHp() + 1 - left]:getId()
        left = left - 1
    end

    return sgs.Card_Parse(card_str)
end

sgs.ai_skill_use_func.QujiCard = function(card, use, self)
    local arr1, arr2 = self:getWoundedFriend(false, true)
    local target = nil
    local num = self.player:getLostHp()
    for num = 1, self.player:getLostHp() do
        if #arr1 > num - 1 and (self:isWeak(arr1[num]) or self:getOverflow() >= 1) and arr1[num]:getHp() < getBestHp(arr1[num]) then target = arr1[num] end
        if target then
            if use.to then use.to:append(target) end
        else
            break
        end
    end

    if num < self.player:getLostHp() then 
        if #arr2 > 0 then
            for _, friend in ipairs(arr2) do
                if not friend:hasSkills("hunzi|longhun") then
                    if use.to then 
                        use.to:append(friend) 
                        num = num + 1
                        if num == self.player:getLostHp() then break end
                    end
                end
            end
        end
    end
    use.card = card
    return
end

sgs.ai_use_priority.QujiCard = 4.2
sgs.ai_card_intention.QujiCard = -100
sgs.dynamic_value.benefit.QujiCard = true

sgs.quji_suit_value = {
    heart = 6,
    diamond = 6
}

sgs.ai_cardneed.quji = function(to, card)
    return card:isRed()
end

sgs.ai_skill_choice.junbing = function(self, choices)
	if self:needKongcheng(self.player, true) then return "no" end
	local simalang = self.room:findPlayerBySkillName("junbing")
    if self:isFriend(simalang) or simalang:isNude() then return "yes" end
	for _, card in sgs.qlist(self.player:getHandcards()) do
		if self:isValuableCard(card) then return "no" end
	end
	return "yes"
end

--孙皓
sgs.ai_skill_invoke.canshi = function(self, data)
    local n = 0
    for _,p in sgs.qlist(self.room:getAllPlayers()) do
        if p:isWounded() then n = n + 1 end
    end
    if n == 0 then return false end
    if n > 2 then return true end
    return false 
end

--OL专属--

--李丰
--屯储
--player->askForSkillInvoke("tunchu")
sgs.ai_skill_invoke["tunchu"] = function(self, data)
    if #self.enemies == 0 then
        return true
    end
    local callback = sgs.ai_skill_choice.jiangchi
    local choice = callback(self, "jiang+chi+cancel")
    if choice == "jiang" then
        return true
    end
    return false
end
--room->askForExchange(player, "tunchu", 1, 1, false, "@tunchu-put")
--输粮
--room->askForCard(p, "@@shuliang", "@shuliang:" + player->objectName(), data, Card::MethodNone);
sgs.ai_skill_cardask["@shuliang"] = function(self, data)
    local target = self.room:getCurrent()
    if target and self:isFriend(target) then
        return "$" .. self.player:getPile("food"):first()
    end
    return "."
end

--朱灵
--战意
--ZhanyiCard:Play
--ZhanyiViewAsBasicCard:Response
--ZhanyiViewAsBasicCard:Play
--room->askForDiscard(p, "zhanyi_equip", 2, 2, false, true, "@zhanyiequip_discard")
--room->askForChoice(zhuling, "zhanyi_slash", guhuo_list.join("+"))
--room->askForChoice(zhuling, "zhanyi_saveself", guhuo_list.join("+"))

local zhanyi_skill = {}
zhanyi_skill.name = "zhanyi"
table.insert(sgs.ai_skills, zhanyi_skill)
zhanyi_skill.getTurnUseCard = function(self)

    if not self.player:hasUsed("ZhanyiCard") then
        return sgs.Card_Parse("@ZhanyiCard=.")
    end

    if self.player:getMark("ViewAsSkill_zhanyiEffect") > 0 then
        local use_basic = self:ZhanyiUseBasic()
        local cards = self.player:getCards("h")
        cards=sgs.QList2Table(cards)
        self:sortByUseValue(cards, true)
        local BasicCards = {}
        for _, card in ipairs(cards) do
            if card:isKindOf("BasicCard") then
                table.insert(BasicCards, card)
            end
        end
        if use_basic and #BasicCards > 0 then
            return sgs.Card_Parse("@ZhanyiViewAsBasicCard=" .. BasicCards[1]:getId() .. ":"..use_basic)
        end
    end
end

sgs.ai_skill_use_func.ZhanyiCard = function(card, use, self)
    if self.player:getMark("ViewAsSkill_zhanyiEffect") > 0 then
        use.card = card
        return
    end
    local to_discard
    local cards = self.player:getCards("h")
    cards=sgs.QList2Table(cards)
    self:sortByUseValue(cards, true)

    local TrickCards = {}
    for _, card in ipairs(cards) do
        if card:isKindOf("Disaster") or card:isKindOf("GodSalvation") or card:isKindOf("AmazingGrace") or self:getCardsNum("TrickCard") > 1 then
            table.insert(TrickCards, card)
        end
    end
    if #TrickCards > 0 and (self.player:getHp() > 2 or self:getCardsNum("Peach") > 0 ) and self.player:getHp() > 1 then
        to_discard = TrickCards[1]
    end

    local EquipCards = {}
    if self:needToThrowArmor() and self.player:getArmor() then table.insert(EquipCards,self.player:getArmor()) end
    for _, card in ipairs(cards) do
        if card:isKindOf("EquipCard") then
            table.insert(EquipCards, card)
        end
    end
    if not self:isWeak() and self.player:getDefensiveHorse() then table.insert(EquipCards,self.player:getDefensiveHorse()) end
    if self.player:hasTreasure("wooden_ox") and self.player:getPile("wooden_ox"):length() == 0 then table.insert(EquipCards,self.player:getTreasure()) end
    self:sort(self.enemies, "defense")
    if self:getCardsNum("Slash") > 0 and
    ((self.player:getHp() > 2 or self:getCardsNum("Peach") > 0 ) and self.player:getHp() > 1) then
        for _, enemy in ipairs(self.enemies) do
            if (self:isWeak(enemy)) or (enemy:getCardCount(true) <= 4 and enemy:getCardCount(true) >=1)
                and self.player:canSlash(enemy) and self:slashIsEffective(sgs.Sanguosha:cloneCard("slash"), enemy, self.player)
                and self.player:inMyAttackRange(enemy) and not self:needToThrowArmor(enemy) then
                to_discard = EquipCards[1]
                break
            end
        end
    end

    local BasicCards = {}
    for _, card in ipairs(cards) do
        if card:isKindOf("BasicCard") then
            table.insert(BasicCards, card)
        end
    end
    local use_basic = self:ZhanyiUseBasic()
    if (use_basic == "peach" and self.player:getHp() > 1 and #BasicCards > 3)
    --or (use_basic == "analeptic" and self.player:getHp() > 1 and #BasicCards > 2)
    or (use_basic == "slash" and self.player:getHp() > 1 and #BasicCards > 1)
    then
        to_discard = BasicCards[1]
    end

    if to_discard then
        use.card = sgs.Card_Parse("@ZhanyiCard=" .. to_discard:getEffectiveId())
        return
    end
end

sgs.ai_use_priority.ZhanyiCard = 10

sgs.ai_skill_use_func.ZhanyiViewAsBasicCard=function(card,use,self)
    local userstring=card:toString()
    userstring=(userstring:split(":"))[3]
    local zhanyicard=sgs.Sanguosha:cloneCard(userstring, card:getSuit(), card:getNumber())
    zhanyicard:setSkillName("zhanyi")
    if zhanyicard:getTypeId() == sgs.Card_TypeBasic then
        if not use.isDummy and use.card and zhanyicard:isKindOf("Slash") and (not use.to or use.to:isEmpty()) then return end
        self:useBasicCard(zhanyicard, use)
    end
    if not use.card then return end
    use.card=card
end

sgs.ai_use_priority.ZhanyiViewAsBasicCard = 8

function SmartAI:ZhanyiUseBasic()
    local has_slash = false
    local has_peach = false
    --local has_analeptic = false

    local cards = self.player:getCards("h")
    cards=sgs.QList2Table(cards)
    self:sortByUseValue(cards, true)
    local BasicCards = {}
    for _, card in ipairs(cards) do
        if card:isKindOf("BasicCard") then
            table.insert(BasicCards, card)
            if card:isKindOf("Slash") then has_slash = true end
            if card:isKindOf("Peach") then has_peach = true end
            --if card:isKindOf("Analeptic") then has_analeptic = true end
        end
    end

    if #BasicCards <= 1 then return nil end

    local ban = table.concat(sgs.Sanguosha:getBanPackages(), "|")
    self:sort(self.enemies, "defense")
    for _, enemy in ipairs(self.enemies) do
        if (self:isWeak(enemy))
        and self.player:canSlash(enemy) and self:slashIsEffective(sgs.Sanguosha:cloneCard("slash"), enemy, self.player)
        and self.player:inMyAttackRange(enemy) then
            --if not has_analeptic and not ban:match("maneuvering") and self.player:getMark("drank") == 0
                --and getKnownCard(enemy, self.player, "Jink") == 0 and #BasicCards > 2 then return "analeptic" end
            if not has_slash then return "slash" end
        end
    end

    if self:isWeak() and not has_peach then return "peach" end

return nil
end

sgs.ai_skill_choice.zhanyi_saveself = function(self, choices)
    if self:getCard("Peach") or not self:getCard("Analeptic") then return "peach" else return "analeptic" end
end

sgs.ai_skill_choice.zhanyi_slash = function(self, choices)
    return self:findSlashKindToUse()
end

--刘表
--自守
--player->askForSkillInvoke(this)
sgs.ai_skill_invoke["rezishou"] = sgs.ai_skill_invoke["zishou"]

sgs.ai_card_intention.QingyiCard = sgs.ai_card_intention.Slash

sgs.ai_skill_invoke.conqueror= function(self, data)
    local target = data:toPlayer()
    if self:isFriend(target) and not self:needToThrowArmor(target) then
    return false end
return true
end

sgs.ai_skill_choice.conqueror = function(self, choices, data)
    local target = data:toPlayer()
    if (self:isFriend(target) and not self:needToThrowArmor(target)) or (self:isEnemy(target) and target:getEquips():length() == 0) then
    return "EquipCard" end
    local choice = {}
    table.insert(choice, "EquipCard")
    table.insert(choice, "TrickCard")
    table.insert(choice, "BasicCard")
    if (self:isEnemy(target) and not self:needToThrowArmor(target)) or (self:isFriend(target) and target:getEquips():length() == 0) then
        table.removeOne(choice, "EquipCard")
        if #choice == 1 then return choice[1] end
    end
    if (self:isEnemy(target) and target:getHandcardNum() < 2) then
        table.removeOne(choice, "BasicCard")
        if #choice == 1 then return choice[1] end
    end
    if (self:isEnemy(target) and target:getHandcardNum() > 3) then
        table.removeOne(choice, "TrickCard")
        if #choice == 1 then return choice[1] end
    end
    return choice[math.random(1, #choice)]
end

sgs.ai_skill_cardask["@conqueror"] = function(self, data)
    local has_card
    local cards = sgs.QList2Table(self.player:getCards("he"))
    self:sortByUseValue(cards, true)
    for _,cd in ipairs(cards) do
        if self:getArmor("SilverLion") and card:isKindOf("SilverLion") then
            has_card = cd
            break
        end
        if cd:isKindOf("Peach") and not card:isKindOf("Analeptic") and not (self:getArmor() and cd:objectName() == self.player:getArmor():objectName()) then
            has_card = cd
            break
        end
    end
    if has_card then
        return "$" .. has_card:getEffectiveId()
    else
        return ".."
    end
end

sgs.ai_skill_playerchosen.fentian = function(self, targets)
    self:sort(self.enemies,"defense")
    for _, enemy in ipairs(self.enemies) do
        if (not self:doNotDiscard(enemy) or self:getDangerousCard(enemy) or self:getValuableCard(enemy)) and not enemy:isNude() and self.player:inMyAttackRange(enemy) then
            return enemy
        end
    end
    for _, friend in ipairs(self.friends) do
        if(self:hasSkills(sgs.lose_equip_skill, friend) and not friend:getEquips():isEmpty())
        or (self:needToThrowArmor(friend) and friend:getArmor()) or self:doNotDiscard(friend) and self.player:inMyAttackRange(friend) then
            return friend
        end
    end
    for _, enemy in ipairs(self.enemies) do
        if not enemy:isNude() and self.player:inMyAttackRange(enemy) then
            return enemy
        end
    end
    for _, friend in ipairs(self.friends) do
        if not friend:isNude() and self.player:inMyAttackRange(friend) then
            return friend
        end
    end
end

sgs.ai_playerchosen_intention.fentian = 20

local getXintanCard = function(pile)
    if #pile > 1 then return pile[1], pile[2] end
    return nil
end

local xintan_skill = {}
xintan_skill.name = "xintan"
table.insert(sgs.ai_skills, xintan_skill)
xintan_skill.getTurnUseCard=function(self)
    if self.player:hasUsed("XintanCard") then return end
    if self.player:getPile("burn"):length() <= 1 then return end
    local ints = sgs.QList2Table(self.player:getPile("burn"))
    local a, b = getXintanCard(ints)
    if a and b then
        return sgs.Card_Parse("@XintanCard=" .. tostring(a) .. "+" .. tostring(b))
    end
end

sgs.ai_skill_use_func.XintanCard = function(card, use, self)
    local target
    self:sort(self.enemies, "hp")
    for _, enemy in ipairs(self.enemies) do
        if not self:needToLoseHp(enemy, self.player) and ((self:isWeak(enemy) or enemy:getHp() == 1) or self.player:getPile("burn"):length() > 3)  then
            target = enemy
        end
    end
    if not target then
        for _, friend in ipairs(self.friends) do
            if self:needToLoseHp(friend, self.player) then
                target = friend
            end
        end
    end
    if target then
        use.card = card
        if use.to then use.to:append(target) end
        return
    end
end

sgs.ai_use_priority.XintanCard = 7
sgs.ai_use_value.XintanCard = 3
sgs.ai_card_intention.XintanCard = 80

sgs.ai_skill_use["@@shefu"] = function(self, data)
    local record
    for _, friend in ipairs(self.friends) do
        if self:isWeak(friend) then
            for _, enemy in ipairs(self.enemies) do
                if enemy:inMyAttackRange(friend) then
                    if self.player:getMark("Shefu_slash") == 0 then
                        record = "slash"
                    end
                end
            end
        end
    end
    if not record then
        for _, enemy in ipairs(self.enemies) do
            if self:isWeak(enemy) then
                for _, friend in ipairs(self.friends) do
                    if friend:inMyAttackRange(enemy) then
                        if self.player:getMark("Shefu_peach") == 0 then
                            record = "peach"
                        elseif self.player:getMark("Shefu_jink") == 0 then
                            record = "jink"
                        end
                    end
                end
            end
        end
    end
    if not record then
        for _, enemy in ipairs(self.enemies) do
            if enemy:getHp() == 1 then
                if self.player:getMark("Shefu_peach") == 0 then
                    record = "peach"
                end
            end
        end
    end
    if not record then
        for _, enemy in ipairs(self.enemies) do
            if getKnownCard(enemy, self.player, "ArcheryAttack", false) > 0 or (enemy:hasSkill("luanji") and enemy:getHandcardNum() > 3)
            and self.player:getMark("Shefu_archery_attack") == 0 then
                record = "archery_attack"
            elseif getKnownCard(enemy, self.player, "SavageAssault", false) > 0
            and self.player:getMark("Shefu_savage_assault") == 0 then
                record = "savage_assault"
            elseif getKnownCard(enemy, self.player, "Indulgence", false) > 0 or (enemy:hasSkills("guose|nosguose") and enemy:getHandcardNum() > 2)
            and self.player:getMark("Shefu_indulgence") == 0 then
                record = "indulgence"
            end
        end
    end
    for _, player in sgs.qlist(self.room:getAlivePlayers()) do
        if player:containsTrick("lightning") and self:hasWizard(self.enemies) then
            if self.player:getMark("Shefu_lightning") == 0 then
                record = "lightning"
            end
        end
    end
    if not record then
        if self.player:getMark("Shefu_slash") == 0 then
            record = "slash"
        elseif self.player:getMark("Shefu_peach") == 0 then
            record = "peach"
        end
    end

    local cards = sgs.QList2Table(self.player:getHandcards())
    local use_card
    self:sortByKeepValue(cards)
    for _,card in ipairs(cards) do
        if not card:isKindOf("Peach") and not (self:isWeak() and card:isKindOf("Jink"))then
            use_card = card
        end
    end
    if record and use_card then
        return "@ShefuCard="..use_card:getEffectiveId()..":"..record
    end
end

sgs.ai_skill_invoke.shefu_cancel = function(self)
    local data = self.room:getTag("ShefuData")
    local use = data:toCardUse()
    local from = use.from
    local to = use.to:first()
    if from and self:isEnemy(from) then
        if (use.card:isKindOf("Jink") and self:isWeak(from))
        or (use.card:isKindOf("Peach") and self:isWeak(from))
        or use.card:isKindOf("Indulgence")
        or use.card:isKindOf("ArcheryAttack") or use.card:isKindOf("SavageAssault") then
            return true
        end
    end
    if to and self:isFriend(to) then
        if (use.card:isKindOf("Slash") and self:isWeak(to))
        or use.card:isKindOf("Lightning") then
            return true
        end
    end
return false
end

sgs.ai_skill_invoke.benyu = function(self, data)
    return true
end

sgs.ai_skill_cardask["@@benyu"] = function(self, data)
    local damage = self.room:getTag("CurrentDamageStruct"):toDamage()
    if not damage.from or self.player:isKongcheng() or not self:isEnemy(damage.from) then return "." end

    local needcard_num = damage.from:getHandcardNum() + 1
    local cards = self.player:getCards("he")
    local to_discard = {}
    cards = sgs.QList2Table(cards)
    self:sortByKeepValue(cards)
    for _, card in ipairs(cards) do
        if not card:isKindOf("Peach") or damage.from:getHp() == 1 then
            table.insert(to_discard, card:getEffectiveId())
            if #to_discard == needcard_num then break end
        end
    end

    if #to_discard == needcard_num then
        return "$" .. table.concat(to_discard, "+")
    end

return "."
end

sgs.ai_skill_choice.liangzhu = function(self, choices, data)
    local current = self.room:getCurrent()
    if self:isFriend(current) then
        return "letdraw"
    end
    return "draw"
end

sgs.ai_skill_invoke.kunfen = function(self, data)
    if not self:isWeak() and (self.player:getHp() > 2 or (self:getCardsNum("Peach") > 0 and self.player:getHp() > 1)) then
        return true
    end
return false
end

local chixin_skill={}
chixin_skill.name="chixin"
table.insert(sgs.ai_skills,chixin_skill)
chixin_skill.getTurnUseCard = function(self, inclusive)
    local cards = self.player:getCards("he")
    cards=sgs.QList2Table(cards)

    local diamond_card

    self:sortByUseValue(cards,true)

    local useAll = false
    self:sort(self.enemies, "defense")
    for _, enemy in ipairs(self.enemies) do
        if enemy:getHp() == 1 and not enemy:hasArmorEffect("EightDiagram") and self.player:distanceTo(enemy) <= self.player:getAttackRange() and self:isWeak(enemy)
            and getCardsNum("Jink", enemy, self.player) + getCardsNum("Peach", enemy, self.player) + getCardsNum("Analeptic", enemy, self.player) == 0 then
            useAll = true
            break
        end
    end

    local disCrossbow = false
    if self:getCardsNum("Slash") < 2 or self.player:hasSkill("paoxiao") then
        disCrossbow = true
    end


    for _,card in ipairs(cards)  do
        if card:getSuit() == sgs.Card_Diamond
        and (not isCard("Peach", card, self.player) and not isCard("ExNihilo", card, self.player) and not useAll)
        and (not isCard("Crossbow", card, self.player) and not disCrossbow)
        and (self:getUseValue(card) < sgs.ai_use_value.Slash or inclusive or sgs.Sanguosha:correctCardTarget(sgs.TargetModSkill_Residue, self.player, sgs.Sanguosha:cloneCard("slash")) > 0) then
            diamond_card = card
            break
        end
    end

    if not diamond_card then return nil end
    local suit = diamond_card:getSuitString()
    local number = diamond_card:getNumberString()
    local card_id = diamond_card:getEffectiveId()
    local card_str = ("slash:chixin[%s:%s]=%d"):format(suit, number, card_id)
    local slash = sgs.Card_Parse(card_str)
    assert(slash)

    return slash

end

sgs.ai_view_as.chixin = function(card, player, card_place, class_name)
    local suit = card:getSuitString()
    local number = card:getNumberString()
    local card_id = card:getEffectiveId()
    if card_place ~= sgs.Player_PlaceSpecial and card:getSuit() == sgs.Card_Diamond and not card:isKindOf("Peach") and not card:hasFlag("using") then
        if class_name == "Slash" then
            return ("slash:chixin[%s:%s]=%d"):format(suit, number, card_id)
        elseif class_name == "Jink" then
            return ("jink:chixin[%s:%s]=%d"):format(suit, number, card_id)
        end
    end
end

sgs.ai_cardneed.chixin = function(to, card)
    return card:getSuit() == sgs.Card_Diamond
end

sgs.ai_skill_playerchosen.suiren = function(self, targets)
    if self.player:getMark("@suiren") == 0 then return "." end
    if self:isWeak() and (self:getOverflow() < -2 or not self:willSkipPlayPhase()) then return self.player end
    self:sort(self.friends_noself, "defense")
    for _, friend in ipairs(self.friends) do
        if self:isWeak(friend) and not self:needKongcheng(friend) then
            return friend
        end
    end
    self:sort(self.enemies, "defense")
    for _, enemy in ipairs(self.enemies) do
        if (self:isWeak(enemy) and enemy:getHp() == 1)
            and self.player:getHandcardNum() < 2 and not self:willSkipPlayPhase() and self.player:inMyAttackRange(enemy) then
            return self.player
        end
    end
end

sgs.ai_playerchosen_intention.suiren = -60

sgs.ai_skill_invoke.biluan = function(self, data)
    return true
end

sgs.ai_skill_choice.lixia = function(self, choices, data)
    return "self"
end

sgs.ai_skill_invoke.fengpo = true

sgs.ai_skill_choice.fengpo = function(self, choices, data)
    return "addDamage"
end

sgs.ai_skill_invoke.yishe = true

sgs.ai_skill_invoke.bushi_obtain = function(self, data)
    local prompt = data:toString()
    local zhanglu = findPlayerByObjectName(self.room, prompt:split(":")[2])
    if not zhanglu or zhanglu:getPile("rice"):length() < 1 then return false end
    if self:isEnemy(zhanglu) and zhanglu:getPile("rice"):length() == 1 and zhanglu:isWounded() then return false end
    if self:isFriend(zhanglu) and (not (zhanglu:getPile("rice"):length() == 1 and zhanglu:isWounded())) and self:getOverflow() > 1 then return false end
    return true
end

sgs.ai_skill_cardask["@midao-card"] = function(self, data)
    local judge = data:toJudge()
    local ids = self.player:getPile("rice")
    if self.room:getMode():find("_mini_46") and not judge:isGood() then return "$" .. ids:first() end
    if self:needRetrial(judge) then
        local cards = {}
        for _,id in sgs.qlist(ids) do
            table.insert(cards, sgs.Sanguosha:getCard(id))
        end
        local card_id = self:getRetrialCardId(cards, judge)
        if card_id ~= -1 then
            return "$" .. card_id
        end
    end
    return "."
end

local function will_discard_zhendu(self)
    local current = self.room:getCurrent()
    local need_damage = self:getDamagedEffects(current, self.player) or self:needToLoseHp(current, self.player)
    if self:isFriend(current) then
        if current:getMark("drank") > 0 and not need_damage then return -1 end
        if (getKnownCard(current, self.player, "Slash") > 0 or (getCardsNum("Slash", current, self.player) >= 1 and current:getHandcardNum() >= 2))
            and (not self:damageIsEffective(current, nil, self.player) or current:getHp() > 2 or (getCardsNum("Peach", current, self.player) > 1 and not self:isWeak(current))) then
            local slash = sgs.Sanguosha:cloneCard("slash")
            local trend = 3
            if current:hasWeapon("Axe") then trend = trend - 1
            elseif current:hasSkills("liegong|tieqi|wushuang|niaoxiang") then trend = trend - 0.4 end
            for _, enemy in ipairs(self.enemies) do
                if ((enemy:getHp() < 3 and enemy:getHandcardNum() < 3) or (enemy:getHandcardNum() < 2)) and current:canSlash(enemy) and not self:slashProhibit(slash, enemy, current)
                    and self:slashIsEffective(slash, enemy, current) and sgs.isGoodTarget(enemy, self.enemies, self, true) then
                    return trend
                end
            end
        end
        if need_damage then return 3 end
    elseif self:isEnemy(current) then
        if current:getHp() == 1 then return 1 end
        if need_damage or current:getHandcardNum() >= 2 then return -1 end
        if getKnownCard(current, self.player, "Slash") == 0 and getCardsNum("Slash", current, self.player) < 0.5 then return 3.5 end
    end
    return -1
end

sgs.ai_skill_cardask["@zhendu-discard"] = function(self, data)
    local discard_trend = will_discard_zhendu(self)
    if discard_trend <= 0 then return "." end
    if self.player:getHandcardNum() + math.random(1, 100) / 100 >= discard_trend then
        local cards = sgs.QList2Table(self.player:getHandcards())
        self:sortByKeepValue(cards)
        for _, card in ipairs(cards) do
            if not self:isValuableCard(card, self.player) then return "$" .. card:getEffectiveId() end
        end
    end
    return "."
end

sgs.ai_skill_cardask["@xiaoguo"] = function(self, data)
    local currentplayer = self.room:getCurrent()

    local has_analeptic, has_slash, has_jink
    for _, acard in sgs.qlist(self.player:getHandcards()) do
        if acard:isKindOf("Analeptic") then has_analeptic = acard
        elseif acard:isKindOf("Slash") then has_slash = acard
        elseif acard:isKindOf("Jink") then has_jink = acard
        end
    end

    local card

    if has_slash then card = has_slash
    elseif has_jink then card = has_jink
    elseif has_analeptic then
        if (getCardsNum("EquipCard", currentplayer, self.player) == 0 and not self:isWeak()) or self:getCardsNum("Analeptic") > 1 then
            card = has_analeptic
        end
    end

    if not card then return "." end
    if self:isFriend(currentplayer) then
        if self:needToThrowArmor(currentplayer) then
            if card:isKindOf("Slash") or (card:isKindOf("Jink") and self:getCardsNum("Jink") > 1) then
                return "$" .. card:getEffectiveId()
            else return "."
            end
        end
    elseif self:isEnemy(currentplayer) then
        if not self:damageIsEffective(currentplayer) then return "." end
        if self:getDamagedEffects(currentplayer) or self:needToLoseHp(currentplayer, self.player) then return "." end
        if self:needToThrowArmor() then return "." end
        if currentplayer:getHp() > 2 and (currentplayer:getHandcardNum() > 2 or currentplayer:getCards("e"):length() > 1)then return "." end
        if currentplayer:getHp() > 1 and (currentplayer:getHandcardNum() > 3 or currentplayer:getCards("e"):length() > 2)then return "." end
        if self:hasSkills(sgs.lose_equip_skill, currentplayer) and currentplayer:getCards("e"):length() > 0 then return "." end
        return "$" .. card:getEffectiveId()
    end
    return "."
end

sgs.ai_choicemade_filter.cardResponded["@xiaoguo"] = function(self, player, promptlist)
    if promptlist[#promptlist] ~= "_nil_" then
        local current = self.room:getCurrent()
        if not current then return end
        local intention = 10
        if self:hasSkills(sgs.lose_equip_skill, current) and current:getCards("e"):length() > 0 then intention = 0 end
        if self:needToThrowArmor(current) then return end
        sgs.updateIntention(player, current, intention)
    end
end

sgs.ai_skill_cardask["@xiaoguo-discard"] = function(self, data)
    local yuejin = self.room:findPlayerBySkillName("xiaoguo")
    local player = self.player

    if self:needToThrowArmor() then
        return "$" .. player:getArmor():getEffectiveId()
    end

    if not self:damageIsEffective(player, sgs.DamageStruct_Normal, yuejin) then
        return "."
    end
    if self:getDamagedEffects(self.player, yuejin) then
        return "."
    end
    if self:needToLoseHp(player, yuejin) then
        return "."
    end

    local card_id
    if self:hasSkills(sgs.lose_equip_skill, player) then
        if player:getWeapon() then card_id = player:getWeapon():getId()
        elseif player:getOffensiveHorse() then card_id = player:getOffensiveHorse():getId()
        elseif player:getArmor() then card_id = player:getArmor():getId()
        elseif player:getDefensiveHorse() then card_id = player:getDefensiveHorse():getId()
        end
    end

    if not card_id then
        for _, card in sgs.qlist(player:getCards("h")) do
            if card:isKindOf("EquipCard") then
                card_id = card:getEffectiveId()
                break
            end
        end
    end

    if not card_id then
        if player:getWeapon() then card_id = player:getWeapon():getId()
        elseif player:getOffensiveHorse() then card_id = player:getOffensiveHorse():getId()
        elseif self:isWeak(player) and player:getArmor() then card_id = player:getArmor():getId()
        elseif self:isWeak(player) and player:getDefensiveHorse() then card_id = player:getDefensiveHorse():getId()
        end
    end

    if not card_id then return "." else return "$" .. card_id end
end

sgs.ai_cardneed.xiaoguo = function(to, card)
    return getKnownCard(to, global_room:getCurrent(), "BasicCard", true) == 0 and card:getTypeId() == sgs.Card_Basic
end

sgs.ai_skill_choice.shushen = function(self, choices)
    return self.shushenchoice
end

sgs.ai_skill_playerchosen.shushen = function(self, targets)
    if #self.friends_noself == 0 then return nil end
    local target
    self:sort(self.friends_noself, "defense")
    for _, friend in ipairs(self.friends_noself) do
        if self:isWeak(friend) then
            target = friend break
        end
    end
    if target then
        self.shushenchoice = "recover"
    else
        target = self:findPlayerToDraw(false, 2)
        self.shushenchoice = "draw"
    end
    return target
end

sgs.ai_playerchosen_intention.shushen = -80

sgs.ai_skill_invoke.shenzhi = function(self, data)
    if self:getCardsNum("Peach") > 0 then return false end
    if self.player:getHandcardNum() >= 3 then return false end
    if self.player:getHandcardNum() >= self.player:getHp() and self.player:isWounded() then return true end
    if self.player:hasSkill("beifa") and self.player:getHandcardNum() == 1 and self:needKongcheng() then return true end
    if self.player:hasSkill("sijian") and self.player:getHandcardNum() == 1 then return true end
    return false
end

function sgs.ai_cardneed.shenzhi(to, card)
    return to:getHandcardNum() < to:getHp()
end

local fenxun_skill = {}
fenxun_skill.name = "fenxun"
table.insert(sgs.ai_skills, fenxun_skill)
fenxun_skill.getTurnUseCard = function(self)
    if self.player:hasUsed("FenxunCard") then return end
    if #self.enemies == 0 then return end
    if self:needBear() then return end
    if not self.player:isNude() then
        local card_id
        local slashcount = self:getCardsNum("Slash")
        local jinkcount = self:getCardsNum("Jink")
        local cards = self.player:getHandcards()
        cards = sgs.QList2Table(cards)
        self:sortByKeepValue(cards)

        if self:needToThrowArmor() then
            return sgs.Card_Parse("@FenxunCard=" .. self.player:getArmor():getId())
        elseif self.player:getHandcardNum() > 0 then
            local lightning = self:getCard("Lightning")
            if lightning and not self:willUseLightning(lightning) then
                card_id = lightning:getEffectiveId()
            else
                for _, acard in ipairs(cards) do
                    if (acard:isKindOf("AmazingGrace") or acard:isKindOf("EquipCard")) then
                        card_id = acard:getEffectiveId()
                        break
                    end
                end
            end
            if not card_id and jinkcount > 1 then
                for _, acard in ipairs(cards) do
                    if acard:isKindOf("Jink") then
                        card_id = acard:getEffectiveId()
                        break
                    end
                end
            end
            if not card_id and slashcount > 1 then
                for _, acard in ipairs(cards) do
                    if acard:isKindOf("Slash") then
                        slashcount = slashcount - 1
                        card_id = acard:getEffectiveId()
                        break
                    end
                end
            end
        end

        if not card_id and self.player:getWeapon() then
            card_id = self.player:getWeapon():getId()
        end

        if not card_id then
            for _, acard in ipairs(cards) do
                if (acard:isKindOf("AmazingGrace") or acard:isKindOf("EquipCard") or acard:isKindOf("BasicCard"))
                    and not isCard("Peach", acard, self.player) and not isCard("Slash", acard, self.player) then
                    card_id = acard:getEffectiveId()
                    break
                end
            end
        end

        if slashcount > 0 and card_id then
            return sgs.Card_Parse("@FenxunCard=" .. card_id)
        end
    end
    return nil
end

sgs.ai_skill_use_func.FenxunCard = function(card, use, self)
    self:sort(self.enemies, "defense")
    local target
    for _, slash in ipairs(self:getCards("Slash")) do
        if slash:getEffectiveId() ~= card:getEffectiveId() then
            local target_num, hastarget = 0
            for _, enemy in ipairs(self.enemies) do
                if not self:slashProhibit(slash, enemy) and self.player:canSlash(enemy, slash, false) and sgs.isGoodTarget(enemy, self.enemies, self, true) then
                    if self.player:distanceTo(enemy) > 1 and not target then target = enemy
                    elseif self.player:distanceTo(enemy) == 1 then
                        hastarget = true
                    end
                    if self.player:inMyAttackRange(enemy) then
                        target_num = target_num + 1
                    end
                end
            end
            if hastarget and target_num >= 2 then return end
        end
    end
    if target and self:getCardsNum("Slash") > 0 then
        use.card = card
        if use.to then
            use.to:append(target)
        end
    end
end

sgs.ai_use_value.FenxunCard = 5.5
sgs.ai_use_priority.FenxunCard = 8
sgs.ai_card_intention.FenxunCard = 50

sgs.ai_skill_playerchosen.duanbing = sgs.ai_skill_playerchosen.slash_extra_targets

sgs.ai_skill_invoke.kuangfu = function(self, data)
    local target = data:toPlayer()
    if self:hasSkills(sgs.lose_equip_skill, target) then
        return self:isFriend(target) and not self:isWeak(target)
    end
    local benefit = (target:getCards("e"):length() == 1 and target:getArmor() and self:needToThrowArmor(target))
    if self:isFriend(target) then return benefit end
    return not benefit
end

sgs.ai_skill_choice.kuangfu = function(self, choices)
    return "move"
end

sgs.ai_skill_use["@@luanzhan"] = function(self, prompt)
    local use = self.player:getTag("luanzhan-use"):toCardUse()
    local x = self.player:getMark("#luanzhan")
    local target_table = {}
    if use.card:isKindOf("Snatch") or use.card:isKindOf("Dismantlement") then
        local trick = sgs.Sanguosha:cloneCard(use.card:objectName(), use.card:getSuit(), use.card:getNumber())
        trick:setSkillName("qiaoshui")
        for i = 0, x - 1, 1 do
            local dummy_use = { isDummy = true, to = sgs.SPlayerList(), current_targets = {} }
            for _, p in sgs.qlist(use.to) do
                table.insert(dummy_use.current_targets, p:objectName())
            end
            for _, p_name in ipairs(target_table) do
                table.insert(dummy_use.current_targets, p_name)
            end
            self:useCardSnatchOrDismantlement(trick, dummy_use)
            if dummy_use.card and dummy_use.to:length() > 0 then
                table.insert(target_table, dummy_use.to:first():objectName())
            else
                break
            end
        end
    elseif use.card:isKindOf("Slash") then
        local slash = sgs.Sanguosha:cloneCard(use.card:objectName(), use.card:getSuit(), use.card:getNumber())
        slash:setSkillName("qiaoshui")
        for i = 0, x - 1, 1 do
            local dummy_use = { isDummy = true, to = sgs.SPlayerList(), current_targets = {} }
            for _, p in sgs.qlist(use.to) do
                table.insert(dummy_use.current_targets, p:objectName())
            end
            for _, p_name in ipairs(target_table) do
                table.insert(dummy_use.current_targets, p_name)
            end
            self:useCardSlash(slash, dummy_use)
            if dummy_use.card and dummy_use.to:length() > 0 then
                table.insert(target_table, dummy_use.to:first():objectName())
            else
                break
            end
        end
    else
        for i = 0, x - 1, 1 do
            local dummy_use = { isDummy = true, to = sgs.SPlayerList(), current_targets = {} }
            for _, p in sgs.qlist(use.to) do
                table.insert(dummy_use.current_targets, p:objectName())
            end
            for _, p_name in ipairs(target_table) do
                table.insert(dummy_use.current_targets, p_name)
            end
            self:useCardByClassName(use.card, dummy_use)
            if dummy_use.card and dummy_use.to:length() > 0 then
                table.insert(target_table, dummy_use.to:first():objectName())
            else
                break
            end
        end
    end
    if #target_table > 0 then
        return "@LuanzhanCard=.->" .. table.concat(target_table, "+")
    end
    return ""
end

sgs.ai_skill_use["@@luanzhan_coll"] = function(self, prompt) -- extra target for Collateral
    local use = self.player:getTag("luanzhan-use"):toCardUse()
    local dummy_use = { isDummy = true, to = sgs.SPlayerList(), current_targets = {} }
    local list = self.player:property("extra_collateral_current_targets"):toString():split("+")
    for _, p_name in ipairs(list) do
        table.insert(dummy_use.current_targets, p_name)
    end
    self:useCardCollateral(use.card, dummy_use)
    if dummy_use.card and dummy_use.to:length() == 2 then
        local first = dummy_use.to:at(0):objectName()
        local second = dummy_use.to:at(1):objectName()
        return "@ExtraCollateralCard=.->" .. first .. "+" .. second
    end
    return ""
end

--星SB武将→_→
sgs.ai_skill_invoke.chongzhen = function(self, data)
    local target = data:toPlayer()
    if self:isFriend(target) then
        if hasManjuanEffect(self.player) then return false end
        if self:needKongcheng(target) and target:getHandcardNum() == 1 then return true end
        if self:getOverflow(target) > 2 then return true end
        return false
    else
        return not (self:needKongcheng(target) and target:getHandcardNum() == 1)
    end
end

sgs.ai_choicemade_filter.skillInvoke.chongzhen = function(self, player, promptlist)
    local target
    for _, p in sgs.qlist(self.room:getOtherPlayers(player)) do
        if p:hasFlag("ChongzhenTarget") then
            target = p
            break
        end
    end
    if target then
        local intention = 60
        if promptlist[3] == "yes" then
            if not self:hasLoseHandcardEffective(target) or (self:needKongcheng(target) and target:getHandcardNum() == 1) then
                intention = 0
            end
            if self:getOverflow(target) > 2 then intention = 0 end
            sgs.updateIntention(player, target, intention)
        else
            if self:needKongcheng(target) and target:getHandcardNum() == 1 then intention = 0 end
            sgs.updateIntention(player, target, -intention)
        end
    end
end

sgs.ai_slash_prohibit.chongzhen = function(self, from, to, card)
    if self:isFriend(to, from) then return false end
    if from:hasSkill("tieji") or self:canLiegong(to, from, card) then
        return false
    end
    if to:hasSkill("longdan") and to:getHandcardNum() >= 3 and from:getHandcardNum() > 1 then return true end
    return false
end

local lihun_skill = {}
lihun_skill.name = "lihun"
table.insert(sgs.ai_skills, lihun_skill)
lihun_skill.getTurnUseCard = function(self)
    if self.player:hasUsed("LihunCard") or self.player:isNude() then return end
    local card_id
    local cards = self.player:getHandcards()
    cards = sgs.QList2Table(cards)
    self:sortByKeepValue(cards)
    local lightning = self:getCard("Lightning")

    if self:needToThrowArmor() then
        card_id = self.player:getArmor():getId()
    elseif self.player:getHandcardNum() > self.player:getHp() then
        if lightning and not self:willUseLightning(lightning) then
            card_id = lightning:getEffectiveId()
        else
            for _, acard in ipairs(cards) do
                if (acard:isKindOf("BasicCard") or acard:isKindOf("EquipCard") or acard:isKindOf("AmazingGrace"))
                    and not acard:isKindOf("Peach") then
                    card_id = acard:getEffectiveId()
                    break
                end
            end
        end
    elseif not self.player:getEquips():isEmpty() then
        local player = self.player
        if player:getWeapon() then card_id = player:getWeapon():getId()
        elseif player:getOffensiveHorse() then card_id = player:getOffensiveHorse():getId()
        elseif player:getDefensiveHorse() then card_id = player:getDefensiveHorse():getId()
        elseif player:getArmor() and player:getHandcardNum() <= 1 then card_id = player:getArmor():getId()
        end
    end
    if not card_id then
        if lightning and not self:willUseLightning(lightning) then
            card_id = lightning:getEffectiveId()
        else
            for _, acard in ipairs(cards) do
                if (acard:isKindOf("BasicCard") or acard:isKindOf("EquipCard") or acard:isKindOf("AmazingGrace"))
                  and not acard:isKindOf("Peach") then
                    card_id = acard:getEffectiveId()
                    break
                end
            end
        end
    end
    if not card_id then
        return nil
    else
        return sgs.Card_Parse("@LihunCard=" .. card_id)
    end
end

sgs.ai_skill_use_func.LihunCard = function(card,use,self)
    local cards = self.player:getHandcards()
    cards = sgs.QList2Table(cards)

    if not self.player:hasUsed("LihunCard") then
        self:sort(self.enemies, "handcard")
        self.enemies = sgs.reverse(self.enemies)
        local target
        local jwfy = self.room:findPlayerBySkillName("shoucheng")
        for _, enemy in ipairs(self.enemies) do
            if enemy:isMale() and not enemy:hasSkill("kongcheng") then
                if ((enemy:hasSkill("lianying") or (jwfy and self:isFriend(jwfy, enemy))) and self:damageMinusHp(self, enemy, 1) > 0)
                    or (enemy:getHp() < 3 and self:damageMinusHp(self, enemy, 0) > 0 and enemy:getHandcardNum() > 0)
                    or (enemy:getHandcardNum() >= enemy:getHp() and enemy:getHp() > 2 and self:damageMinusHp(self, enemy, 0) >= -1)
                    or (enemy:getHandcardNum() - enemy:getHp() > 2) then
                    target = enemy
                    break
                end
            end
        end
        if not self.player:faceUp() and not target then
            for _, enemy in ipairs(self.enemies) do
                if enemy:isMale() and not enemy:isKongcheng() then
                    if enemy:getHandcardNum() >= enemy:getHp() then
                        target = enemy
                        break
                    end
                end
            end
        end

        if not target and (self:hasCrossbowEffect() or self:getCardsNum("Crossbow") > 0) then
            local slash = self:getCard("Slash") or sgs.Sanguosha:cloneCard("slash")
            for _, enemy in ipairs(self.enemies) do
                if enemy:isMale() and not enemy:isKongcheng() and self:slashIsEffective(slash, enemy) and self.player:distanceTo(enemy) == 1
                    and not enemy:hasSkills("fenyong|zhichi|fankui|vsganglie|ganglie|neoganglie|enyuan|nosenyuan|langgu|guixin|kongcheng")
                    and self:getCardsNum("Slash") + getKnownCard(enemy, self.player, "Slash") >= 3 then
                    target = enemy
                    break
                end
            end
        end
        if target then
            use.card = card
            if use.to then use.to:append(target) end
        end
    end
end

function SmartAI:isLihunTarget(player, drawCardNum)
    player = player or self.player
    drawCardNum = drawCardNum or 1
    if type(player) == "table" then
        if #player == 0 then return false end
        for _, ap in ipairs(player) do
            if self:isLihunTarget(ap, drawCardNum) then return true end
        end
        return false
    end

    local handCardNum = player:getHandcardNum() + drawCardNum
    if not player:isMale() then return false end

    local sb_diaochan = self.room:findPlayerBySkillName("lihun")
    local lihun = sb_diaochan and not sb_diaochan:hasUsed("LihunCard") and not self:isFriend(sb_diaochan)

    if not lihun then return false end

    if sb_diaochan:getPhase() == sgs.Player_Play then
        if (handCardNum - player:getHp() >= 2)
            or (handCardNum > 0 and handCardNum - player:getHp() >= -1 and not sb_diaochan:faceUp()) then
            return true
        end
    else
        if sb_diaochan:faceUp() and not self:willSkipPlayPhase(sb_diaochan)
            and self:playerGetRound(player) > self:playerGetRound(sb_diaochan) and handCardNum >= player:getHp() + 2 then
            return true
        end
    end

    return false
end

sgs.ai_skill_discard.lihun = function(self, discard_num, min_num, optional, include_equip)
    local to_discard = {}

    local cards = sgs.QList2Table(self.player:getCards("he"))
    self:sortByKeepValue(cards)
    local card_ids = {}
    for _,card in ipairs(cards) do
        table.insert(card_ids, card:getEffectiveId())
    end

    local temp = table.copyFrom(card_ids)
    for i = 1, #temp, 1 do
        local card = sgs.Sanguosha:getCard(temp[i])
        if self.player:getArmor() and temp[i] == self.player:getArmor():getEffectiveId() and self:needToThrowArmor() then
            table.insert(to_discard, temp[i])
            table.removeOne(card_ids, temp[i])
            if #to_discard == discard_num then
                return to_discard
            end
        end
    end

    temp = table.copyFrom(card_ids)

    for i = 1, #card_ids, 1 do
        local card = sgs.Sanguosha:getCard(card_ids[i])
        table.insert(to_discard, card_ids[i])
        if #to_discard == discard_num then
            return to_discard
        end
    end

    if #to_discard < discard_num then return {} end
end

sgs.ai_use_value.LihunCard = 8.5
sgs.ai_use_priority.LihunCard = 6
sgs.ai_card_intention.LihunCard = 80

-- new sp caoren

local weikui_skill = {}
weikui_skill.name = "weikui"
table.insert(sgs.ai_skills, weikui_skill)
weikui_skill.getTurnUseCard = function(self)
	if self.player:hasUsed("WeikuiCard") then return end
	return sgs.Card_Parse("@WeikuiCard=.")
end

sgs.ai_skill_use_func.WeikuiCard = function(card, use, self)
	if self.player:isWounded() and not self:needToLoseHp(self.player, nil, false) then return end

	local target
    self:sort(self.enemies, "defense")
	local slash = sgs.Sanguosha:cloneCard("slash")
	for _, enemy in ipairs(self.enemies) do
        if not self.player:canSlash(enemy, slash, false) then
		elseif self:slashProhibit(nil, enemy) then
		elseif enemy:isKongcheng() then
		else 
			target = enemy break
		end
	end	

	if not target then return end

	if use.to then
		use.to:append(target)
	end
	use.card = card
end

sgs.ai_use_priority["WeikuiCard"] = 2.7



sgs.ai_skill_use["@@lizhan"] = function(self)
    self:sort(self.friends)
	local targets = {}
	for _, friend in ipairs(self.friends) do
		if not hasManjuanEffect(friend) and not self:needKongcheng(friend, true) and friend:isWounded() then
			table.insert(targets, friend:objectName())
		end
	end

	return "@LizhanCard=.->" .. table.concat(targets, "+")
end

sgs.ai_card_intention.LizhanCard = function(self, card, from, tos)
	for _, to in ipairs(tos) do
		if self:needKongcheng(to, true) then sgs.updateIntention(from, to, 10)
		elseif hasManjuanEffect(to) then continue
		else sgs.updateIntention(from, to, -10) end
	end
end

sgs.ai_skill_invoke.manjuan = true
sgs.ai_skill_invoke.zuixiang = function(self)
    if self.player:hasFlag("AI_doNotInvoke_zuixiang") then
        self.player:setFlags("-AI_doNotInvoke_zuixiang")
        return
    end
end

sgs.ai_skill_askforag.manjuan = function(self, card_ids)
    local cards = {}
    for _, card_id in ipairs(card_ids) do
        table.insert(cards, sgs.Sanguosha:getCard(card_id))
    end
    for _, card in ipairs(cards) do
        if card:isKindOf("ExNihilo") then return card:getEffectiveId() end
        if card:isKindOf("IronChain") then return card:getEffectiveId() end
    end
    for _, card in ipairs(cards) do
        if card:isKindOf("Snatch") and #self.enemies > 0 then
                self:sort(self.enemies,"defense")
                if sgs.getDefense(self.enemies[1]) >= 8 then self:sort(self.enemies, "threat") end
                local enemies = self:exclude(self.enemies, card)
                for _,enemy in ipairs(enemies) do
                    if self:hasTrickEffective(card, enemy) then
                        return card:getEffectiveId()
                    end
                end
            end
        end
    for _, card in ipairs(cards) do
        if card:isKindOf("Peach") and self.player:isWounded() and self:getCardsNum("Peach") < self.player:getLostHp() then return card:getEffectiveId() end
    end
    for _, card in ipairs(cards) do
        if card:isKindOf("AOE") and self:getAoeValue(card) > 0 then return card:getEffectiveId() end
    end
    self:sortByCardNeed(cards)
    return cards[#cards]:getEffectiveId()
end

function hasManjuanEffect(player)
    return player:hasSkill("manjuan") and player:getPhase() == sgs.Player_NotActive
end

sgs.ai_cardneed.jie = function(to, card)
    return card:isRed() and isCard("Slash", card, to)
end

local dahe_skill = {}
dahe_skill.name = "dahe"
table.insert(sgs.ai_skills,dahe_skill)
dahe_skill.getTurnUseCard = function(self)
    if self:needBear() then return end
    if not self.player:hasUsed("DaheCard") and not self.player:isKongcheng() then return sgs.Card_Parse("@DaheCard=.") end
end

sgs.ai_skill_use_func.DaheCard=function(card,use,self)
    self:sort(self.enemies, "handcard")
    local max_card = self:getMaxCard(self.player)
    local max_point = max_card:getNumber()
    local slashcount = self:getCardsNum("Slash")
    if max_card:isKindOf("Slash") then slashcount = slashcount - 1 end
    if self.player:hasSkill("kongcheng") and self.player:getHandcardNum() == 1 then
        for _, enemy in ipairs(self.enemies) do
            if not enemy:isKongcheng() then
                self.dahe_card = max_card:getId()
                use.card = sgs.Card_Parse("@DaheCard=.")
                if use.to then use.to:append(enemy) end
                return
            end
        end
    end
    if slashcount > 0 then
        local slash = self:getCard("Slash")
        assert(slash)
        local dummy_use = {isDummy = true}
        self:useBasicCard(slash, dummy_use)
        for _, enemy in ipairs(self.enemies) do
            if not (enemy:hasSkill("kongcheng") and enemy:getHandcardNum() == 1 and enemy:getHp() > self.player:getHp())
                and not enemy:isKongcheng() and self.player:canSlash(enemy, nil, true) then
                local enemy_max_card = self:getMaxCard(enemy)
                local allknown = 0
                if self:getKnownNum(enemy) == enemy:getHandcardNum() then
                    allknown = allknown + 1
                end
                if (enemy_max_card and max_point > enemy_max_card:getNumber() and allknown > 0)
                    or (enemy_max_card and max_point > enemy_max_card:getNumber() and allknown < 1 and max_point > 10)
                    or (not enemy_max_card and max_point > 10) then
                    self.dahe_card = max_card:getId()
                    use.card = sgs.Card_Parse("@DaheCard=.")
                    if use.to then use.to:append(enemy) end
                    return
                end
            end
        end
    end
end

function sgs.ai_skill_pindian.dahe(minusecard, self, requestor)
    if self:isFriend(requestor) then return minusecard end
    return self:getMaxCard(self.player):getId()
end

sgs.ai_skill_playerchosen.dahe = function(self, targets)
    targets = sgs.QList2Table(targets)
    self:sort(targets, "defense")
    for _, target in ipairs(targets) do
        if target:hasSkill("kongcheng") and target:isKongcheng()
            and target:hasFlag("dahe") then
            return target
        end
    end
    for _, target in ipairs(targets) do
        if self:isFriend(target) and not self:needKongcheng(target, true) then return target end
    end
    return nil
end

sgs.ai_cardneed.dahe = sgs.ai_cardneed.bignumber
sgs.ai_card_intention.DaheCard = 60
sgs.dynamic_value.control_card.DaheCard = true

sgs.ai_use_value.DaheCard = 8.5
sgs.ai_use_priority.DaheCard = 8

local tanhu_skill = {}
tanhu_skill.name = "tanhu"
table.insert(sgs.ai_skills, tanhu_skill)
tanhu_skill.getTurnUseCard = function(self)
    if not self.player:hasUsed("TanhuCard") and not self.player:isKongcheng() then return sgs.Card_Parse("@TanhuCard=.") end
end

sgs.ai_skill_use_func.TanhuCard = function(card, use, self)
    local max_card = self:getMaxCard()
    local max_point = max_card:getNumber()
    local ptarget = self:getPriorTarget()
    if not ptarget then return end
    local slashcount = self:getCardsNum("Slash")
    if max_card:isKindOf("Slash") then slashcount = slashcount - 1 end
    if not ptarget:isKongcheng() and slashcount > 0 and self.player:canSlash(ptarget, nil, false)
        and not (ptarget:hasSkill("kongcheng") and ptarget:getHandcardNum() == 1) then
        self.tanhu_card = max_card:getEffectiveId()
        use.card = sgs.Card_Parse("@TanhuCard=.")
        if use.to then use.to:append(ptarget) end
        return
    end
    self:sort(self.enemies, "defense")

    for _, enemy in ipairs(self.enemies) do
        if self:getCardsNum("Snatch") > 0 and not enemy:isKongcheng() then
            local enemy_max_card = self:getMaxCard(enemy)
            local allknown = 0
            if self:getKnownNum(enemy) == enemy:getHandcardNum() then
                allknown = allknown + 1
            end
            if (enemy_max_card and max_point > enemy_max_card:getNumber() and allknown > 0)
                or (enemy_max_card and max_point > enemy_max_card:getNumber() and allknown < 1 and max_point > 10)
                or (not enemy_max_card and max_point > 10)
                and (self:getDangerousCard(enemy) or self:getValuableCard(enemy)) then
                    self.tanhu_card = max_card:getEffectiveId()
                    use.card = sgs.Card_Parse("@TanhuCard=.")
                    if use.to then use.to:append(enemy) end
                    return
            end
        end
    end
    local cards = sgs.QList2Table(self.player:getHandcards())
    self:sortByUseValue(cards, true)
    if self:getUseValue(cards[1]) >= 6 or self:getKeepValue(cards[1]) >= 6 then return end
    if self:getOverflow() > 0 then
        if not ptarget:isKongcheng() then
            self.tanhu_card = max_card:getEffectiveId()
            use.card = sgs.Card_Parse("@TanhuCard=.")
            if use.to then use.to:append(ptarget) end
            return
        end
        for _, enemy in ipairs(self.enemies) do
            if not (enemy:hasSkill("kongcheng") and enemy:getHandcardNum() == 1) and not enemy:isKongcheng() and not enemy:hasSkills("tuntian+zaoxian") then
                self.tanhu_card = cards[1]:getId()
                use.card = sgs.Card_Parse("@TanhuCard=.")
                if use.to then use.to:append(enemy) end
                return
            end
        end
    end
end

sgs.ai_cardneed.tanhu = sgs.ai_cardneed.bignumber
sgs.ai_card_intention.TanhuCard = 30
sgs.dynamic_value.control_card.TanhuCard = true
sgs.ai_use_priority.TanhuCard = 8

function sgs.ai_skill_pindian.tanhu(minusecard, self, requestor)
    if requestor:getHandcardNum() == 1 then
        local cards = sgs.QList2Table(self.player:getHandcards())
        self:sortByKeepValue(cards)
        return cards[1]
    end
end

local function need_mouduan(self)
    local cardsCount = self.player:getHandcardNum()
    if cardsCount <= 3 then return false end
    local current = self.room:getCurrent()
    local slash = sgs.Sanguosha:cloneCard("slash")
    if current:objectName() == self.player:objectName() then
        if (self:hasCrossbowEffect() or self:getCardsNum("Crossbow") > 0)
            and self:getCardsNum("Slash") >= 3
            and (not self:willSkipPlayPhase() or self.player:hasSkill("dangxian")) then
            local hasTarget = false
            for _, enemy in ipairs(self.enemies) do
                if not self:slashProhibit(slash, enemy) and self:slashIsEffective(slash, enemy) and sgs.isGoodTarget(enemy, self.enemies, self, true) then
                    hasTarget = true
                    break
                end
            end
            return hasTarget
        end
    elseif (cardsCount == 4 or cardsCount == 5) and #self.enemies > 1 then
        return true
    end
    return false
end

sgs.ai_skill_cardask["@mouduan"] = function(self, data)
    if not need_mouduan(self) then return "." end
    local to_discard = self:askForDiscard("mouduan", 1, 1, false, true)
    if #to_discard > 0 then return "$" .. to_discard[1] else return "." end
end

sgs.ai_skill_use_func.YanxiaoCard = function(card, use, self)
    local players = self.room:getOtherPlayers(self.player)
    local tricks
    self:sort(self.friends_noself, "defense")
    for _, friend in ipairs(self.friends_noself) do
        local need_yanxiao = (friend:containsTrick("lightning") and self:getFinalRetrial(friend) == 2)
                            or friend:containsTrick("indulgence") or friend:containsTrick("supply_shortage")
        if need_yanxiao and not friend:containsTrick("YanxiaoCard") then
            use.card = card
            if use.to then use.to:append(friend) end
            return
        end
    end
    if self:getOverflow() > 0 then
        if not self.player:containsTrick("YanxiaoCard") then
            use.card = card
            if use.to then use.to:append(self.player) end
            return
        end
        local lord = self.room:getLord()
        if lord and self:isFriend(lord) and not lord:containsTrick("YanxiaoCard") then
            use.card = card
            if use.to then use.to:append(lord) end
            return
        end

        for _, friend in ipairs(self.friends_noself) do
            if not friend:containsTrick("YanxiaoCard") then
                use.card = card
                if use.to then use.to:append(friend) end
                return
            end
        end
    end
end

sgs.ai_use_priority.YanxiaoCard = 3.9
sgs.ai_card_intention.YanxiaoCard = -80

local yanxiao_skill={}
yanxiao_skill.name="yanxiao"
table.insert(sgs.ai_skills,yanxiao_skill)
yanxiao_skill.getTurnUseCard = function(self)
    local cards = self.player:getCards("he")
    cards=sgs.QList2Table(cards)
    local diamond_card
    self:sortByUseValue(cards,true)

    for _,card in ipairs(cards)  do
        if card:getSuit() == sgs.Card_Diamond then
            diamond_card = card
            break
        end
    end

    if diamond_card then
        local suit = diamond_card:getSuitString()
        local number = diamond_card:getNumberString()
        local card_id = diamond_card:getEffectiveId()
        local card_str = ("YanxiaoCard:yanxiao[%s:%s]=%d"):format(suit, number, card_id)
        local yanxiaocard = sgs.Card_Parse(card_str)
        assert(yanxiaocard)
        return yanxiaocard
    end
end

sgs.yanxiao_suit_value = {
    diamond = 3.9
}

function sgs.ai_cardneed.yanxiao(to, card)
    return card:getSuit() == sgs.Card_Diamond
end

sgs.ai_skill_invoke.anxian = function(self, data)
    local damage = data:toDamage()
    local target = damage.to
    if self:isFriend(target) and not (self:getDamagedEffects(target, self.player) or self:needToLoseHp(target, self.player, nil, true)) then return true end
    if self:hasHeavySlashDamage(self.player, damage.card, damage.to) then return false end
    if self:isEnemy(target) and self:getDamagedEffects(target, self.player) and not self:doNotDiscard(target, "h") then return true end
    return false
end

sgs.ai_skill_cardask["@anxian-discard"] = function(self, data)
    local use = data:toCardUse()
    local from = use.from
    local to = self.player
    if self.player:isKongcheng() then return "." end
    local cards = self.player:getHandcards()
    cards = sgs.QList2Table(cards)
    self:sortByKeepValue(cards)

    if self:hasHeavySlashDamage(from, use.card, self.player) and self:canHit(to, from, true) then
        return "$" .. cards[1]:getEffectiveId()
    end
    if self:getDamagedEffects(self.player, use.from, true) then
        return "."
    end
    if self:needToLoseHp(self.player, use.from, true) then
        return "."
    end
    if self:isFriend(to, from) then return "$" .. cards[1]:getEffectiveId() end
    if self:needToLoseHp(self.player, use.from, true, true) then
        return "."
    end
    if self:canHit(to, from) then
        for _, card in ipairs(cards) do
            if not isCard("Peach", card, self.player) then
                return "$" .. card:getEffectiveId()
            end
        end
    end
    if self:getCardsNum("Jink") > 0 then
        return "."
    end

    if #cards == self:getCardsNum("Peach") then return "." end
    for _, card in ipairs(cards) do
        if not isCard("Peach", card, self.player) then
            return "$" .. card:getEffectiveId()
        end
    end
    return "."
end

local yinling_skill = {}
yinling_skill.name = "yinling"
table.insert(sgs.ai_skills, yinling_skill)
yinling_skill.getTurnUseCard = function(self, inclusive)
    if self.player:getPile("brocade"):length() >= 4 then return end
    local cards = self.player:getCards("he")
    cards = sgs.QList2Table(cards)
    self:sortByUseValue(cards, true)
    local black_card
    local has_weapon = false

    for _,card in ipairs(cards)  do
        if card:isKindOf("Weapon") and card:isBlack() then has_weapon=true end
    end

    for _,card in ipairs(cards)  do
        if card:isBlack()  and ((self:getUseValue(card) < sgs.ai_use_value.YinlingCard) or inclusive or self:getOverflow() > 0) then
            local shouldUse = true

            if card:isKindOf("Armor") then
                if not self.player:getArmor() then shouldUse = false
                elseif self.player:hasEquip(card) and not (card:isKindOf("SilverLion") and self.player:isWounded()) then shouldUse = false
                end
            end

            if card:isKindOf("Weapon") then
                if not self.player:getWeapon() then shouldUse = false
                elseif self.player:hasEquip(card) and not has_weapon then shouldUse=false
                end
            end

            if card:isKindOf("Slash") then
                local dummy_use = {isDummy = true}
                if self:getCardsNum("Slash") == 1 then
                    self:useBasicCard(card, dummy_use)
                    if dummy_use.card then shouldUse = false end
                end
            end

            if self:getUseValue(card) > sgs.ai_use_value.YinlingCard and card:isKindOf("TrickCard") then
                local dummy_use = {isDummy = true}
                self:useTrickCard(card, dummy_use)
                if dummy_use.card then shouldUse = false end
            end

            if shouldUse then
                black_card = card
                break
            end

        end
    end

    if black_card then
        local card_id = black_card:getEffectiveId()
        local card_str = ("@YinlingCard="..card_id)
        local yinling = sgs.Card_Parse(card_str)

        assert(yinling)

        return yinling
    end
end

sgs.ai_skill_use_func.YinlingCard = function(card, use, self)
    self:useCardSnatchOrDismantlement(card, use)
end

sgs.ai_use_value.YinlingCard = sgs.ai_use_value.Dismantlement + 1
sgs.ai_use_priority.YinlingCard = sgs.ai_use_priority.Dismantlement + 1
sgs.ai_card_intention.YinlingCard = 0 -- update later

sgs.ai_choicemade_filter.cardChosen.yinling = sgs.ai_choicemade_filter.cardChosen.snatch

sgs.ai_skill_use["@@junwei"] = function(self, data, method)
    if not method then method = sgs.Card_MethodNone end
    if data ~= "junwei-invoke" then return "." end
    local pile = sgs.QList2Table(self.player:getPile("brocade"))
    if #pile >= 3 then
        local tos = {}
        for _, target in ipairs(self.enemies) do
            if not (target:hasEquip() and self:doNotDiscard(target, "e")) then
                table.insert(tos, target)
            end
        end

        if #tos > 0 then
            self:sort(tos, "defense")
            if (tos[1]) then
                return "@JunweiCard=" .. tostring(pile[1]) .. "+" .. tostring(pile[2]) .. "+" .. tostring(pile[3]) .. "->" .. tos[1]:objectName()
            end
        end
    end
    return "."
end

sgs.ai_skill_invoke.junwei = function(self, data)
    for _, enemy in ipairs(self.enemies) do
        if not (enemy:hasEquip() and self:doNotDiscard(enemy, "e")) then return true end
    end
end

sgs.ai_playerchosen_intention.junwei = 80

sgs.ai_skill_playerchosen.junweigive = function(self, targets)
    local tos = {}
    for _, target in sgs.qlist(targets) do
        if self:isFriend(target) and not target:hasSkill("manjuan") and not self:needKongcheng(target, true) then
            table.insert(tos, target)
        end
    end

    if #tos > 0 then
        for _, to in ipairs(tos) do
            if to:hasSkills("leiji|nosleiji") then return to end
        end
        self:sort(tos, "defense")
        return tos[1]
    end
end

sgs.ai_playerchosen_intention.junweigive = -80

sgs.ai_skill_cardask["@junwei-show"] = function(self, data)
    if self.player:hasArmorEffect("silver_lion") and self.player:getEquips():length() == 1 then return "." end
    local ganning = data:toPlayer()
    local cards = self.player:getHandcards()
    cards = sgs.QList2Table(cards)
    for _,card in ipairs(cards) do
        if card:isKindOf("Jink") then
            return "$" .. card:getEffectiveId()
        end
    end
    return "."
end

sgs.yinling_suit_value = {
    spade = 3.9,
    club = 3.9
}

sgs.ai_skill_invoke.fenyong = function(self, data)
    self.fenyong_choice = nil
    if sgs.turncount <= 1 and #self.enemies == 0 then return end

    local current = self.room:getCurrent()
    if not current or current:getPhase() >= sgs.Player_Finish then return true end
    if self:isFriend(current) then
        self:sort(self.enemies, "defenseSlash")
        for _, enemy in ipairs(self.enemies) do
            local def = sgs.getDefenseSlash(enemy, self)
            local slash = sgs.Sanguosha:cloneCard("slash", sgs.Card_NoSuit, 0)
            local eff = self:slashIsEffective(slash, enemy) and sgs.isGoodTarget(enemy, self.enemies, self)

            if self.player:canSlash(enemy, nil, false) and not self:slashProhibit(nil, enemy) and eff and def < 5 then
                return true
            end
            if self.player:getLostHp() == 1 and self:needToThrowArmor(current) then return true end
        end
        return false
    end

    return true
end

function sgs.ai_slash_prohibit.fenyong(self, from, to)
    if from:hasSkill("jueqing") or (from:hasSkill("nosqianxi") and from:distanceTo(to) == 1) then return false end
    if from:hasFlag("NosJiefanUsed") then return false end
    return to:getMark("@fenyong") > 0 and to:hasSkill("fenyong")
end

sgs.ai_need_damaged.fenyong = function (self, attacker, player)
    if not player:hasSkill("fenyong") then return false end
    if not player:hasSkill("xuehen") then return false end
    for _, enemy in ipairs(self.enemies) do
        local def = sgs.getDefenseSlash(enemy, self)
        local slash = sgs.Sanguosha:cloneCard("slash", sgs.Card_NoSuit, 0)
        local eff = self:slashIsEffective(slash, enemy) and sgs.isGoodTarget(enemy, self.enemies, self)

        if self.player:canSlash(enemy, nil, false) and not self:slashProhibit(nil, enemy) and eff and def < 6 then
            return true
        end
    end
    return false
end

sgs.ai_skill_choice.xuehen = function(self, choices)
    if self.fenyong_choice then return self.fenyong_choice end
    local current = self.room:getCurrent()
    local n = self.player:getLostHp()
    if self:isEnemy(current) then
        if n >= 3 and current:getCardCount(true) >= 3 and not (self:needKongcheng(current) and current:getCards("e"):length() < 3)
            and not (self:hasSkills(sgs.lose_equip_skill, current) and current:getHandcardNum() < n) then
            return "discard"
        end
        if self:hasSkills("jijiu|tuntian+zaoxian|beige", current) and n >= 2 and current:getCardCount(true) >= 2 then return "discard" end
    end
    self:sort(self.enemies, "defenseSlash")
    for _, enemy in ipairs(self.enemies) do
        local def = sgs.getDefenseSlash(enemy, self)
        local slash = sgs.Sanguosha:cloneCard("slash")
        local eff = self:slashIsEffective(slash, enemy) and sgs.isGoodTarget(enemy, self.enemies, self)

        if self.player:canSlash(enemy, nil, false) and not self:slashProhibit(nil, enemy) and eff and def < 6 then
            self.xuehentarget = enemy
            return "slash"
        end
    end
    if self:isEnemy(current) then
        for _, enemy in ipairs(self.enemies) do
            local slash = sgs.Sanguosha:cloneCard("slash")
            local eff = self:slashIsEffective(slash, enemy)

            if self.player:canSlash(enemy, nil, false) and not self:slashProhibit(nil, enemy) and self:hasHeavySlashDamage(self.player, slash, enemy) then
                self.xuehentarget = enemy
                return "slash"
            end
        end
        local armor = current:getArmor()
        if armor and self:evaluateArmor(armor, current) >= 3 and not self:doNotDiscard(current, "e") and n <= 2 then return "discard" end
    end
    if self:isFriend(current) then
        if n == 1 and self:needToThrowArmor(current) then return "discard" end
        for _, enemy in ipairs(self.enemies) do
            local slash = sgs.Sanguosha:cloneCard("slash")
            local eff = self:slashIsEffective(slash, enemy)

            if self.player:canSlash(enemy, nil, false) and not self:slashProhibit(nil, enemy) then
                self.xuehentarget = enemy
                return "slash"
            end
        end
    end
    return "discard"
end

sgs.ai_skill_playerchosen.xuehen = function(self, targets)
    local to = self.xuehentarget
    if to then
        self.xuehentarget = nil
        return to
    end
    to = sgs.ai_skill_playerchosen.zero_card_as_slash(self, targets)
    return to or targets[1]
end

sgs.ai_suit_priority.jie = "club|spade|diamond|heart"
sgs.ai_suit_priority.yanxiao = "club|spade|heart|diamond"
sgs.ai_suit_priority.yinling = "diamond|heart|club|spade"

--AI for DIY generals
sgs.ai_skill_use["@@zhaoxin"] = function(self, prompt)
    local target
    self:sort(self.enemies, "defenseSlash")
    for _, enemy in ipairs(self.enemies) do
        local slash = sgs.Sanguosha:cloneCard("slash", sgs.Card_NoSuit, 0)
        local eff = self:slashIsEffective(slash, enemy) and sgs.isGoodTarget(enemy, self.enemies, self)
        if eff and self.player:canSlash(enemy) and not self:slashProhibit(nil, enemy) then
            return "@ZhaoxinCard=.->" .. enemy:objectName()
        end
    end
    return "."
end

sgs.ai_card_intention.ZhaoxinCard = 80

sgs.ai_skill_invoke.langgu = function(self, data)
    local target = data:toPlayer()
    return target and not self:isFriend(target)
end

sgs.ai_choicemade_filter.skillInvoke.langgu = function(self, player, promptlist)
    local damage = self.room:getTag("CurrentDamageStruct"):toDamage()
    if damage.from and promptlist[3] == "yes" then
        sgs.updateIntention(player, damage.from, 10)
    end
end

sgs.ai_skill_askforag.langgu = function(self, card_ids)
    return -1
end

sgs.ai_skill_cardask["@langgu-card"] = function(self, data)
    local judge = data:toJudge()
    local retrialForHongyan
    local damage = self.room:getTag("CurrentDamageStruct"):toDamage()
    if damage.from and damage.from:isAlive() and not damage.from:isKongcheng() and damage.from:hasSkill("hongyan")
        and getKnownCard(damage.from, self.player, "diamond", false) + getKnownCard(damage.from, self.player, "club", false) < damage.from:getHandcardNum() then
        retrialForHongyan = true
    end
    if retrialForHongyan then
        local cards = sgs.QList2Table(self.player:getHandcards())
        for _, card in ipairs(cards) do
            if card:getSuit() == sgs.Card_Heart and not isCard("Peach", card, self.player) then
                return "$" .. card:getId()
            end
        end
        if judge.card:getSuit() == sgs.Card_Spade then
            self:sortByKeepValue(cards)
            for _, card in ipairs(cards) do
                if card:getSuit() ~= sgs.Card_Spade and not isCard("Peach", card, self.player) then
                    return "$" .. card:getId()
                end
            end
        end
    end

    return "."
end

local fuluan_skill = {}
fuluan_skill.name = "fuluan"
table.insert(sgs.ai_skills, fuluan_skill)
fuluan_skill.getTurnUseCard = function(self)
    if self.player:hasUsed("FuluanCard") or self.player:hasFlag("ForbidFuluan") then return end
    local first_found, second_found, third_found = false, false, false
    local first_card, second_card, third_card
    if self.player:getCards("he"):length() >= 3 then
        local cards = self.player:getCards("he")
        local same_suit = false
        cards = sgs.QList2Table(cards)
        for _, fcard in ipairs(cards) do
            if not isCard("Peach", fcard, self.player) and not isCard("ExNihilo", fcard, self.player) then
                first_card = fcard
                first_found = true
                for _, scard in ipairs(cards) do
                    if first_card ~= scard and scard:getSuit() == first_card:getSuit()
                        and not isCard("Peach", scard, self.player) and not isCard("ExNihilo", scard, self.player) then
                        second_card = scard
                        second_found = true
                        for _, tcard in ipairs(cards) do
                            if first_card ~= tcard and second_card ~= tcard and tcard:getSuit() == first_card:getSuit()
                                and not isCard("Peach", tcard, self.player) and not isCard("ExNihilo", tcard, self.player) then
                                third_card = tcard
                                third_found = true
                                break
                            end
                        end
                    end
                    if third_found then break end
                end
            end
            if third_found and second_found then break end
        end
    end

    if first_found and second_found and third_found then
        local card_str = ("@FuluanCard=%d+%d+%d"):format(first_card:getId(), second_card:getId(), third_card:getId())
        assert(card_str)
        return sgs.Card_Parse(card_str)
    end
end

local function can_be_selected_as_target_fuluan(self, card, who)
    local subcards = card:getSubcards()
    if self.player:getWeapon() and subcards:contains(self.player:getWeapon():getId()) then
        local distance_fix = sgs.weapon_range[self.player:getWeapon():getClassName()] - self.player:getAttackRange(false)
        if self.player:getOffensiveHorse() and subcards:contains(self.player:getOffensiveHorse():getId()) then
            distance_fix = distance_fix + 1
        end
        return self.player:distanceTo(who, distance_fix) <= self.player:getAttackRange()
    elseif self.player:getOffensiveHorse() and subcards:contains(self.player:getOffensiveHorse():getId()) then
        return self.player:distanceTo(who, 1) <= self.player:getAttackRange()
    elseif self.player:inMyAttackRange(who) then
        return true
    end
    return false
end

sgs.ai_skill_use_func.FuluanCard = function(card, use, self)
    local subcards = card:getSubcards()
    self:sort(self.friends_noself)
    for _, friend in ipairs(self.friends_noself) do
        if not self:toTurnOver(friend, 0) then
            if can_be_selected_as_target_fuluan(self, card, friend) then
                use.card = card
                if use.to then use.to:append(friend) end
                return
            end
        end
    end
    self:sort(self.enemies, "defense")
    for _, enemy in ipairs(self.enemies) do
        if self:toTurnOver(enemy, 0) then
            if can_be_selected_as_target_fuluan(self, card, enemy) then
                use.card = card
                if use.to then use.to:append(enemy) end
                return
            end
        end
    end
end

sgs.ai_use_priority.FuluanCard = 2.3
sgs.ai_card_intention.FuluanCard = function(self, card, from, tos)
    sgs.updateIntention(from, tos[1], tos[1]:faceUp() and 80 or -80)
end

local function need_huangen(self, who)
    local card = sgs.Card_Parse(self.player:getTag("Huangen_user"):toString())
    if card == nil then return false end
    local from = self.room:getCurrent()
    if self:isEnemy(who) then
        if card:isKindOf("GodSalvation") and who:isWounded() and self:hasTrickEffective(card, who, from) then
            if hasManjuanEffect(who) then return true end
            if self:isWeak(who) then return true end
            if self:hasSkills(sgs.masochism_skill, who) then return true end
        end
        if card:isKindOf("ExNihilo") then return true end
        return false
    elseif self:isFriend(who) then
        if self:hasSkills("noswuyan", who) and from:objectName() ~= who:objectName() then return true end
        if card:isKindOf("GodSalvation") and not who:isWounded() then
            if hasManjuanEffect(who) then return false end
            if self:needKongcheng(who, true) then return false end
            return true
        end
        if card:isKindOf("GodSalvation") and who:isWounded() and self:hasTrickEffective(card, who, from) then
            if self:needToLoseHp(who, nil, nil, true, true) and not self:needKongcheng(who, true) then return true end
            return false
        end
        if card:isKindOf("IronChain") and (self:needKongcheng(who, true) or (who:isChained() and self:hasTrickEffective(card, who, from))) then
            return false
        end
        if card:isKindOf("AmazingGrace") then return not self:hasTrickEffective(card, who, from) end
        return true
    end
end

sgs.ai_skill_use["@@huangen"] = function(self, prompt)
    local card = sgs.Card_Parse(self.player:getTag("Huangen_user"):toString())
    local first_index, second_index, third_index, forth_index, fifth_index
    local i = 1
    local players = sgs.QList2Table(self.room:getAllPlayers())
    self:sort(players, "defense")
    for _, player in ipairs(players) do
        if player:hasFlag("HuangenTarget") then
            if not first_index and need_huangen(self, player) then
                first_index = i
            elseif not second_index and need_huangen(self, player) then
                second_index = i
            elseif not third_index and need_huangen(self, player) then
                third_index = i
            elseif not forth_index and need_huangen(self, player) then
                forth_index = i
            elseif need_huangen(self, player) then
                fifth_index = i
            end
            if fifth_index then break end
        end
        i = i + 1
    end
    if not first_index then return "." end

    local first, second, third, forth, fifth
    if first_index then
        first = players[first_index]:objectName()
    end
    if second_index then
        second = players[second_index]:objectName()
    end
    if third_index then
        third = players[third_index]:objectName()
    end
    if forth_index then
        forth = players[forth_index]:objectName()
    end
    if fifth_index then
        fifth = players[fifth_index]:objectName()
    end

    local hp = self.player:getHp()
    if fifth_index and hp >= 5 then
        return ("@HuangenCard=.->%s+%s+%s+%s+%s"):format(first, second, third, forth, fifth)
    elseif forth_index and hp >= 4 then
        return ("@HuangenCard=.->%s+%s+%s+%s"):format(first, second, third, forth)
    elseif third_index and hp >= 3 then
        return ("@HuangenCard=.->%s+%s+%s"):format(first, second, third)
    elseif second_index and hp >= 2 then
        return ("@HuangenCard=.->%s+%s"):format(first, second)
    elseif first_index and hp >= 1 then
        return ("@HuangenCard=.->%s"):format(first)
    end
end

sgs.ai_card_intention.HuangenCard = function(self, card, from, tos)
    local cardx = sgs.Card_Parse(from:getTag("Huangen_user"):toString())
    if not cardx then return end
    for _, to in ipairs(tos) do
        local intention = -80
        if cardx:isKindOf("GodSalvation") and to:isWounded() and (hasManjuanEffect(to) or self:isWeak(to)) then intention = 50 end
        if self:needKongcheng(to, true) then intention = 0 end
        if cardx:isKindOf("AmazingGrace") and self:hasTrickEffective(cardx, to) then intention = 0 end
        sgs.updateIntention(from, to, intention)
    end
end

sgs.ai_skill_invoke.hantong = true

sgs.ai_skill_invoke.hantong_acquire = function(self, data)
    local skill = data:toString()
    if skill == "hujia" and not self.player:hasSkill("hujia") then
        local can_invoke = false
        for _, friend in ipairs(self.friends_noself) do
            if friend:getKingdom() == "wei" and getCardsNum("Jink", friend, self.player) > 0 then can_invoke = true end
        end
        if can_invoke then
            local origin_data = self.player:getTag("HantongOriginData")
            return sgs.ai_skill_invoke.hujia(self, origin_data)
        end
    elseif skill == "jijiang" and not self.player:hasSkill("jijiang") then
        local can_invoke = false
        for _, friend in ipairs(self.friends_noself) do
            if friend:getKingdom() == "shu" and getCardsNum("Slash", friend, self.player) > 0 then can_invoke = true end
        end
        if can_invoke then
            local origin_data = self.player:getTag("HantongOriginData")
            return sgs.ai_skill_invoke.jijiang(self, origin_data)
        end
    elseif skill == "jiuyuan" and not self.player:hasSkill("jiuyuan") then
        return true
    elseif skill == "xueyi" and not self.player:hasSkill("xueyi") then
        local maxcards = self.player:getMaxCards()
        local can_invoke = false
        for _, player in sgs.qlist(self.room:getOtherPlayers(self.player)) do
            if player:getKingdom() == "qun" then can_invoke = true end
        end
        if can_invoke then return self.player:getHandcardNum() > maxcards end
    end
    return false
end

local hantong_skill = {}
hantong_skill.name = "hantong"
table.insert(sgs.ai_skills, hantong_skill)
hantong_skill.getTurnUseCard = function(self)
    if self.player:hasLordSkill("jijiang") or self.player:getPile("edict"):isEmpty() or not self:slashIsAvailable() then return end
    local can_invoke = false
    for _, friend in ipairs(self.friends_noself) do
        if friend:getKingdom() == "shu" and getCardsNum("Slash", friend) > 0 then can_invoke = true end
    end
    if not can_invoke then return end
    return sgs.Card_Parse("@HantongCard=.")
end

sgs.ai_skill_use_func.HantongCard = function(card, use, self)
    local jcard = sgs.Card_Parse("@JijiangCard=.")
    local dummy_use = { isDummy = true }
    self:useSkillCard(jcard, dummy_use)
    if dummy_use.card then use.card = card end
end

sgs.ai_use_value.HantongCard = sgs.ai_use_value.JijiangCard
sgs.ai_use_priority.HantongCard = sgs.ai_use_priority.JijiangCard
sgs.ai_card_intention.HantongCard = sgs.ai_card_intention.JijiangCard

function sgs.ai_cardsview_valuable.hantong(self, class_name, player)
    if class_name == "Slash" and player:getPile("edict"):length() > 0 and not player:hasSkill("jijiang") then
        local ret = sgs.ai_cardsview_valuable.jijiang(self, class_name, player, false)
        if ret then return "@HantongCard=." end
    end
end

sgs.ai_skill_use["@@diyyicong"] = function(self, prompt)
    local yicongcards = {}

    if self:needToThrowArmor() then
        return "@DIYYicongCard=" .. self.player:getArmor():getId() .. "->."
    end
    local cards = self.player:getCards("he")
    cards = sgs.QList2Table(cards)
    self:sortByKeepValue(cards)
    for _, card in ipairs(cards) do
        if self:getKeepValue(card) < 6
            and (not self.player:getArmor() or card:getId() ~= self.player:getArmor():getEffectiveId())
            and (not self.player:getDefensiveHorse() or card:getId() ~= self.player:getDefensiveHorse():getEffectiveId()) then
            table.insert(yicongcards, card:getId())
            break
        end
    end
    if #yicongcards > 0 then
        return "@DIYYicongCard=" .. table.concat(yicongcards, "+") .. "->."
    end
    return "."
end


sgs.ai_skill_invoke.shenqu = true

--[[

local jiwu_skill = {}
jiwu_skill.name = "jiwu"
table.insert(sgs.ai_skills, jiwu_skill)
jiwu_skill.getTurnUseCard = function(self)
	local slash, jink
	for _, c in sgs.qlist(self.player:getHandcards()) do
		if c:isKindOf("Slash") and not self.player:isLocked(c) then
			slash = c
		end
		if c:isKindOf("Jink") then
			jink = c
		end
	end
	if not jink then return nil end
	if self.player:usedTimes("JiwuCard") == 0 then
		return sgs.Card_Parse("@JiwuCard="..jink:getEffectiveId()..":lieren")
	elseif self.player:usedTimes("JiwuCard") == 1 then
		if slash then
			return nil
		else
			return sgs.Card_Parse("@JiwuCard="..jink:getEffectiveId()..":qiangxi")
		end
	elseif self.player:usedTimes("JiwuCard") == 2 then
		return sgs.Card_Parse("@JiwuCard="..jink:getEffectiveId()..":xuanfeng")
	elseif self.player:usedTimes("JiwuCard") == 3 then
		return sgs.Card_Parse("@JiwuCard="..jink:getEffectiveId()..":wansha")
	end
end


sgs.ai_skill_use_func.JiwuCard = function(card, use, self)
	use.card = card
end

sgs.ai_use_priority.JiwuCard = 20

]]










