--[[
	太阳神三国杀游戏工具扩展包·战功荣耀（AI部分）
	适用版本：V2 - 愚人版（版本号：20150401）清明补丁（版本号：20150405）
]]--
dofile "glory/data/glory-class.lua"
--[[****************************************************************
	----------------------------------------------------------------
								核心架构
	----------------------------------------------------------------
]]--****************************************************************
--[[****************************************************************
	控制参数
	1、允许作弊
		所有战功在已开启作弊功能的情况下依旧有效。
	2、允许联机
		所有战功在有多名人类玩家联机的环境下依旧有效。
	3、匹配副将
		比如怀旧马岱的战功“暗箭难防”，当马岱为副将时也同样有效。
	4、忽略已完成的战功
		比如一局游戏中，中了三次乐不思蜀获得战功“安乐公”，再中三次乐不思蜀时将不再获得相同的战功。
	5、额外经验奖励
		每当完成一次战功后，额外获得10点经验
	6、启用额外技能
		杀死一名角色后，随机获得一定数目的技能钥匙；游戏开始时，可以使用一把技能钥匙并失去1点体力，获得一个随机技能。
	7、珍贵的经验
		一名己方角色阵亡后，若场上人数不少于3且自己为己方仅存角色，可以消耗一定的经验执行一个效果。
]]--****************************************************************
EnableCheat = true --允许作弊
EnableMoreHumans = true --允许联机
MatchSecondaryHero = true --匹配副将
IgnoreAfterFinished = false --忽略已完成的战功
ExtraAward = true --额外经验奖励
EnableExtraSkill = true --启用额外技能
PreciousExperience = true --珍贵的经验
--[[****************************************************************
	数据信息
]]--****************************************************************
sgs.glory_info = {} --战功信息表
sgs.glory_data = {} --战功数据表
sgs.finish_items = {} --完成信息表
sgs.special_mode = {} --特殊模式登记表
sgs.match_mode = {} --模式匹配表
sgs.same_camp = {} --同阵营判定表
sgs.am_winner = {} --胜利判定表
sgs.game_over = {} --游戏结束判定表
sgs.game_over_works = {} --游戏结束时的工作登记表
dofile "glory/data/glory-mode-info.lua"
--[[****************************************************************
	系统控制
]]--****************************************************************
--接入AI系统·途径A
local system_filterEvent = SmartAI.filterEvent
function SmartAI:filterEvent(event, player, data)
	--启动
	GloryStart(self.room)
	--正常工作
	system_filterEvent(self, event, player, data)
end
--接入AI系统·途径B
sgs.ai_event_callback[sgs.GameStart].glory = function(self, player, data)
	--启动
	GloryStart(self.room)
end
--“荣耀”系统启动
function GloryStart(room)
	--防止再次启动
	SmartAI.filterEvent = system_filterEvent
	sgs.ai_event_callback[sgs.GameStart].glory = nil
	--检查本扩展包是否已被启用
	local banPackages = sgs.Sanguosha:getBanPackages()
	for _,pack in ipairs(banPackages) do
		if pack == "glory" then
			GloryClear(room)
			return false
		end
	end
	--检查是否已开启作弊
	if not EnableCheat then
		if sgs.GetConfig("EnableCheat", false) then
			GloryClear(room)
			return false
		end
	end
	--获得所有人类玩家
	local alives = room:getAlivePlayers()
	local humans = sgs.SPlayerList()
	for _,player in sgs.qlist(alives) do
		if player:getState() ~= "robot" then
			humans:append(player)
		end
	end
	--检查人类玩家是否唯一并确定主视角角色
	local source = nil
	if humans:length() > 1 then
		if EnableMoreHumans then
			source = humans:first()
			source = room:askForPlayerChosen(source, humans, "glory", "@glory", false)
		end
	elseif humans:length() == 1 then
		source = humans:first()
	end
	--启动“荣耀”系统
	if source then
		--记录主视角角色
		sgs.glory_data["room"] = room
		sgs.glory_data["player"] = source
		sgs.glory_data["player_objectName"] = source:objectName()
		--记录游戏模式
		local game_mode = room:getMode()
		sgs.glory_data["mode"] = game_mode
		if sgs.GetConfig("EnableHegemony", false) then
			sgs.glory_data["hegemony"] = true
			sgs.glory_data["roles_mode"] = false
		else
			sgs.glory_data["hegemony"] = false
			if sgs.special_mode[game_mode] or string.match("_mini_", game_mode) then
				sgs.glory_data["roles_mode"] = false
			else
				sgs.glory_data["roles_mode"] = true
			end
		end
		--载入数据
		LoadResult()
		--系统清理
		GloryClean()
		--通知系统已启动
		source:speak("“荣耀”已启动！")
		local msg = sgs.LogMessage()
		msg.type = "#glory_confirm"
		msg.from = source
		room:sendLog(msg) --发送提示信息
		--获得额外的技能
		if EnableExtraSkill then
			gainAnExtraSkill(room, source)
		end
		--触发游戏开始事件
		room:getThread():trigger(sgs.NonTrigger, room, source, sgs.QVariant("gloryGameStart"))
		return true
	end
	--启动失败时移除“荣耀”系统
	GloryClear(room)
	return false
end
--“荣耀”系统终止
function GloryClose(room)
	--后期处理
	local me = getTarget()
	for name, callback in pairs(sgs.game_over_works) do
		if type(callback) == "function" then
			callback(room, me)
		end
	end
	--保存数据
	SaveResult()
	--系统移除
	GloryClear(room)
end
--“荣耀”系统清理
function GloryClean()
	local player = sgs.glory_data["player"]
	local game_mode = sgs.glory_data["mode"]
	local hegemony = sgs.glory_data["hegemony"]
	local function matchMode(mode)
		if mode == game_mode then
			return true
		elseif mode == "" then
			return true
		end
		local callback = sgs.match_mode[mode]
		if type(callback) == "function" then
			local result = callback(game_mode, player, hegemony)
			if type(result) == "boolean" then
				return result
			end
		end
		return false
	end
	for item, info in pairs(sgs.glory_info) do
		if type(item) == "string" and type(info) == "table" then
			local ok = false
			local state = info["state"] or ""
			if state == "" or state == "验证通过" then
				local modes = info["mode"] or {"all_modes"}
				if type(modes) == "string" then
					modes = modes:split("|")
				end
				for _,mode in ipairs(modes) do
					if matchMode(mode) then
						ok = true
						break
					end
				end
				local general = info["general"] 
				if type(general) == "string" and general ~= "" then
					ok = ok and isGeneral(player, general, false) 
				end
			end
			if not ok then
				local events = info["events"] or {}
				for _,event in ipairs(events) do
					sgs.ai_event_callback[event][item] = nil
				end
			end
		end
	end
end
--“荣耀”系统移除
function GloryClear(room)
	room:setTag("GlorySystemClosed", sgs.QVariant(true))
	for item, info in pairs(sgs.glory_info) do
		if type(item) == "string" and type(info) == "table" then
			local events = info["events"] or {}
			for _,event in ipairs(events) do
				sgs.ai_event_callback[event][item] = nil
			end
			local data = info["data"] 
			if type(data) == "string" then
				sgs[data] = nil
			end
		end
	end
	sgs.ai_event_callback[sgs.NonTrigger].glory = nil
	sgs.ai_event_callback[sgs.Death].PreciousExperience = nil
end
--“荣耀”系统关闭准备
sgs.ai_event_callback[sgs.NonTrigger].glory = function(self, player, data)
	local cmd = data:toString()
	if cmd == "gloryAfterGameOverJudge" and isGameOver() then
		GloryClose(self.room)
	end
end
--“荣耀”系统针对变身/换将的处理
local system_initialize = SmartAI.initialize
function SmartAI:initialize(player)
	system_initialize(self, player)
	local room = sgs.glory_data["room"]
	if room and sgs.glory_data["hegemony"] then
		sgs.glory_changeHero_player = player
		if isGameOver() then
			GloryClose(room)
		end
		sgs.glory_changeHero_player = nil
	end
end
--[[****************************************************************
	文件操作
]]--****************************************************************
function LoadResult()
	local file = GFile()
	local section = "general"
	for line in io.lines("glory/data/glory-data.ini") do
		local glory = file:addLine(line)
		if glory.type == "section" then
			section = glory.text
		elseif glory.type == "data" then
			if glory.key == "glory" then
				sgs.finish_items[section] = tonumber(glory.value) or 0
			elseif glory.key ~= "" then
if not (sgs.glory_info[section].data and sgs[sgs.glory_info[section].data]) then continue end
				setInfo(section, glory.key, glory.value)
			end
		end
	end
	sgs.glory_data["file"] = file
end
function SaveResult()
	local file = io.open("glory/data/glory-data.ini", "w")
	local glory = sgs.glory_data["file"]
	local section = "general"
	while true do
		local line = glory:nextLine()
		if line then
			if line.type == "section" then
				section = line.text
				global_flag = ( string.sub(section, 1, 1) == "g" )
			elseif line.type == "data" then
				if line.key == "glory" then
					local count = sgs.finish_items[section] or 0
					line:setValue(count)
				elseif string.sub(line.key, 1, 7) == "Global_" then
					local data = getInfo(section, line.key, "")
					line:setValue(data)
				end
			end
			file:write(line:tostring())
			file:write("\n")
		else
			break
		end
	end
	io.close(file)
end
function tempWrite(section, key, value)
	local target = io.open("glory/data/glory-data.ini", "w")
	local source = sgs.glory_data["file"]
	local group = "general"
	for index, line in ipairs(source.lines) do
		if line.type == "data" then
			if group == section and line.key == key then
				line:setValue(value)
				target:write(line:tostring())
			end
		elseif line.type == "section" then
			group = line.text
			target:write(line:tostring())
		else
			target:write(line:tostring())
		end
		target:write("\n")
	end
	io.close(target)
end
--[[****************************************************************
	工具函数
]]--****************************************************************
function isTarget(player)
	return player:objectName() == sgs.glory_data["player_objectName"]
end
function isLeader(player) --For 06_3v3 & 06_XMode
	return string.match("lord|renegade", player:getRole())
end
function isMember(player) --For 06_3v3 & 06_XMode
	return string.match("loyalist|rebel", player:getRole())
end
function getTarget()
	return sgs.glory_data["player"]
end
function getTargetName()
	return sgs.glory_data["player_objectName"]
end
function addInfo(name, key, extraValue)
	local branch = sgs.glory_info[name].data
	local value = sgs[branch][key] or 0
	sgs[branch][key] = value + extraValue
end
function setInfo(name, key, value)
	local branch = sgs.glory_info[name].data
	sgs[branch][key] = value
end
function getInfo(name, key, defaultValue)
	local branch = sgs.glory_info[name].data
	return sgs[branch][key] or defaultValue
end
function addFinishTag(name)
	--计数
	local times = sgs.finish_items[name] or 0
	sgs.finish_items[name] = times + 1
	--显示
	local msg = sgs.LogMessage()
	msg.type = "#glory_achieved"
	msg.from = sgs.glory_data["player"]
	msg.arg = name
	sgs.glory_data["room"]:sendLog(msg) --发送提示信息
	--奖励经验
	if ExtraAward then
		addInfo("gainExperience", "point", 10)
	end
	--移除
	if IgnoreAfterFinished or sgs.glory_info[name].once_only then
		ignoreItem(name)
		return true
	else
		resetItem(name)
	end
	return false
end
function ignoreItem(name)
	local info = sgs.glory_info[name]
	local events = info["events"] or {}
	for index, event in ipairs(events) do
		sgs.ai_event_callback[event][name] = nil
	end
end
function resetItem(name)
	local info = sgs.glory_info[name]
	local branch = info["data"]
	local keys = info["keys"]
	for index, key in ipairs(keys) do
		sgs[branch][key] = 0
	end
end
function isGeneral(player, names, exactly)
	local general = player:getGeneralName()
	if exactly then
		for _,name in ipairs(names:split("|")) do
			if general == name then
				return true
			end
		end
	else
		for _,name in ipairs(names:split("|")) do
			if string.match(general, name) then
				return true
			end
		end
	end
	return false
end
if MatchSecondaryHero then
	function isGeneral(player, names, exactly)
		local generalA = player:getGeneralName()
		local generalB = player:getGeneral2Name()
		if exactly then
			for _,name in ipairs(names:split("|")) do
				if generalA == name then
					return true
				elseif generalB == name then
					return true
				end
			end
		else
			for _,name in ipairs(names:split("|")) do
				if string.match(generalA, name) then
					return true
				elseif string.match(generalB, name) then
					return true
				end
			end
		end
		return false
	end
end
function isSameCamp(playerA, playerB)
	if playerA:objectName() == playerB:objectName() then
		return true
	end
	local mode = sgs.glory_data["mode"] 
	local roleA, roleB = playerA:getRole(), playerB:getRole()
	local callback = sgs.same_camp[mode]
	if type(callback) == "function" then
		local result = callback(playerA, roleA, playerB, roleB)
		if type(result) == "boolean" then
			return result
		end
	end
	local hegemony = sgs.glory_data["hegemony"] 
	if hegemony then
		callback = sgs.same_camp["hegemony"]
		if type(callback) == "function" then
			local result = callback(playerA, roleA, playerB, roleB)
			if type(result) == "boolean" then
				return result
			end
		end
	else
		callback = sgs.same_camp["roles"]
		if type(callback) == "function" then
			local result = callback(playerA, roleA, playerB, roleB)
			if type(result) == "boolean" then
				return result
			end
		end
	end
	return false
end
function amGeneral(name, exactly)
	return isGeneral(sgs.glory_data["player"], name, exactly)
end
function amLord()
	return sgs.glory_data["player"]:getRole() == "lord"
end
function amLoyalist()
	return sgs.glory_data["player"]:getRole() == "loyalist"
end
function amRenegade()
	return sgs.glory_data["player"]:getRole() == "renegade"
end
function amRebel()
	return sgs.glory_data["player"]:getRole() == "rebel"
end
function amMale()
	return sgs.glory_data["player"]:isMale()
end
function amFemale()
	return sgs.glory_data["player"]:isFemale()
end
function amAlive()
	return sgs.glory_data["player"]:isAlive()
end
function amDead()
	return sgs.glory_data["player"]:isDead()
end
function amLeader() --For 06_3v3 & 06_XMode
	return string.match("lord|renegade", sgs.glory_data["player"]:getRole())
end
function amMember() --For 06_3v3 & 06_XMode
	return string.match("loyalist|rebel", sgs.glory_data["player"]:getRole())
end
function amWinner()
	if sgs.glory_data["am_winner"] then
		return true
	end
	local mode = sgs.glory_data["mode"] or ""
	local room = sgs.glory_data["room"]
	local player = sgs.glory_data["player"]
	local role = player:getRole()
	sgs.updateAlivePlayerRoles()
	local n_lord = sgs.current_mode_players["lord"]
	local n_loyalist = sgs.current_mode_players["loyalist"]
	local n_renegade = sgs.current_mode_players["renegade"]
	local n_rebel = sgs.current_mode_players["rebel"]
	local callback = sgs.am_winner[mode]
	if type(callback) == "function" then
		local result = callback(room, player, role, n_lord, n_loyalist, n_renegade, n_rebel)
		if type(result) == "boolean" then
			return result
		end
	end
	local hegemony = sgs.glory_data["hegemony"]
	if hegemony then
		callback = sgs.am_winner["hegemony"]
		if type(callback) == "function" then
			local result = callback(room, player, role, n_lord, n_loyalist, n_renegade, n_rebel)
			if type(result) == "boolean" then
				return result
			end
		end
	else
		callback = sgs.am_winner["roles"]
		if type(callback) == "function" then
			local result = callback(room, player, role, n_lord, n_loyalist, n_renegade, n_rebel)
			if type(result) == "boolean" then
				return result
			end
		end
	end
	return false
end
function isGameOver()
	if sgs.glory_data["game_over"] then
		return true
	end
	local mode = sgs.glory_data["mode"] or ""
	local room = sgs.glory_data["room"]
	local player = sgs.glory_data["player"]
	sgs.updateAlivePlayerRoles()
	local n_lord = sgs.current_mode_players["lord"] or 0
	local n_loyalist = sgs.current_mode_players["loyalist"] or 0
	local n_renegade = sgs.current_mode_players["renegade"] or 0
	local n_rebel = sgs.current_mode_players["rebel"] or 0
	local callback = sgs.game_over[mode]
	if type(callback) == "function" then
		local result = callback(room, player, n_lord, n_loyalist, n_renegade, n_rebel)
		if type(result) == "boolean" then
			return result
		end
	end
	local hegemony = sgs.glory_data["hegemony"]
	if hegemony then
		callback = sgs.game_over["hegemony"]
		if type(callback) == "function" then
			local result = callback(room, player, n_lord, n_loyalist, n_renegade, n_rebel)
			if type(result) == "boolean" then
				return result
			end
		end
	else
		callback = sgs.game_over["roles"]
		if type(callback) == "function" then
			local result = callback(room, player, n_lord, n_loyalist, n_renegade, n_rebel)
			if type(result) == "boolean" then
				return result
			end
		end
	end
	return false
end
--[[****************************************************************
	----------------------------------------------------------------
								通用战功
	----------------------------------------------------------------
]]--****************************************************************
dofile "glory/data/glory-for-game.lua"
--[[****************************************************************
	----------------------------------------------------------------
							武将专属战功
	----------------------------------------------------------------
]]--****************************************************************
dofile "glory/data/glory-for-generals.lua"
--[[****************************************************************
	----------------------------------------------------------------
							模式专属战功
	----------------------------------------------------------------
]]--****************************************************************
dofile "glory/data/glory-for-special.lua"
--[[****************************************************************
	----------------------------------------------------------------
								附加功能
	----------------------------------------------------------------
]]--****************************************************************
--[[****************************************************************
	功能：胜率统计
	描述：统计玩家在身份局模式和国战模式下各身份/势力的胜率
]]--****************************************************************
sgs.glory_info["statistics"] = {
	name = "statistics",
	state = "验证通过",
	mode = "all_modes",
	events = {sgs.NonTrigger},
	data = "statistics_data",
	keys = {
		--"Global_roles_play_times",
		--"Global_roles_finish_times",
		--"Global_lord_times",
		--"Global_lord_win_times",
		--"Global_loyalist_times",
		--"Global_loyalist_win_times",
		--"Global_renegade_times",
		--"Global_renegade_win_times",
		--"Global_rebel_times",
		--"Global_rebel_win_times",
		--"Global_hegemony_play_times",
		--"Global_hegemony_finish_times",
		--"Global_wei_times",
		--"Global_wei_win_times",
		--"Global_shu_times",
		--"Global_shu_win_times",
		--"Global_wu_times",
		--"Global_wu_win_times",
		--"Global_qun_times",
		--"Global_qun_win_times",
	},
}
sgs.statistics_data = {}
sgs.ai_event_callback[sgs.NonTrigger].statistics = function(self, player, data)
	local cmd = data:toString()
	if cmd == "gloryGameStart" then
		if sgs.glory_data["roles_mode"] then
			local key = string.format("Global_%s_times", player:getRole())
			addInfo("statistics", key, 1)
			addInfo("statistics", "Global_roles_play_times", 1)
			-- local new_value = getInfo("statistics", "Global_roles_play_times", 0)
			-- tempWrite("statistics", "Global_roles_play_times", new_value)
			sgs.glory_data["start_statistics"] = true
		elseif sgs.glory_data["hegemony"] then
			local kingdom = player:getKingdom()
			if kingdom == "god" then
				local names = player:property("basara_generals"):toString():split("+")
				local general = sgs.Sanguosha:getGeneral(names[1])
				kingdom = general:getKingdom()
				if kingdom == "god" then
					return 
				end
			end
			local key = string.format("Global_%s_times", kingdom)
			addInfo("statistics", key, 1)
			addInfo("statistics", "Global_hegemony_play_times", 1)
			-- local new_value = getInfo("statistics", "Global_hegemony_play_times", 0)
			-- tempWrite("statistics", "Global_hegemony_play_times", new_value)
			sgs.glory_data["start_statistics"] = true
		end
	end
end
sgs.game_over_works["statistics"] = function(room, player)
	if sgs.glory_data["start_statistics"] then
		if sgs.glory_data["roles_mode"] then
			addInfo("statistics", "Global_roles_finish_times", 1)
			if amWinner() then
				local key = string.format("Global_%s_win_times", player:getRole())
				addInfo("statistics", key, 1)
			end
		elseif sgs.glory_data["hegemony"] then
			addInfo("statistics", "Global_hegemony_finish_times", 1)
			if amWinner() then
				local kingdom = player:getKingdom()
				if kingdom == "god" then
					local names = player:property("basara_generals"):toString():split("+")
					local general = sgs.Sanguosha:getGeneral(names[1])
					kingdom = general:getKingdom()
					if kingdom == "god" then
						return 
					end
				end
				local key = string.format("Global_%s_win_times", kingdom)
				addInfo("statistics", key, 1)
			end
		end
	end
end
--[[****************************************************************
	功能：文功统计
	描述：每使用或打出一张锦囊牌，获得1点文功
]]--****************************************************************
sgs.glory_info["gainWenGong"] = {
	name = "gainWenGong",
	state = "验证通过",
	mode = "all_modes",
	events = {sgs.CardUsed, sgs.CardResponded},
	data = "gainWenGong_data",
	keys = {
		--"Global_point",
		"point",
	},
}
sgs.gainWenGong_data = {}
sgs.ai_event_callback[sgs.CardUsed].gainWenGong = function(self, player, data)
	local use = data:toCardUse()
	local source = use.from
	if source and source:objectName() == player:objectName() and isTarget(player) then
		if use.card:isKindOf("TrickCard") then
			addInfo("gainWenGong", "point", 1)
		end
	end
end
sgs.ai_event_callback[sgs.CardResponded].gainWuGong = function(self, player, data)
	local response = data:toCardResponse()
	if response.m_card:isKindOf("TrickCard") and isTarget(player) then
		addInfo("gainWenGong", "point", 1)
	end
end
sgs.game_over_works["gainWenGong"] = function(room, player)
	local point = getInfo("gainWenGong", "point", 0)
	if point ~= 0 then
		local original = getInfo("gainWenGong", "Global_point", 0)
		local total = original + point
		sgs.glory_data["WenGong"] = total
		setInfo("gainWenGong", "Global_point", total)
		local msg = sgs.LogMessage()
		msg.type = "#gainWenGong"
		msg.from = player
		msg.arg = point
		room:sendLog(msg) --发送提示信息
	end
end
--[[****************************************************************
	功能：武功统计
	描述：每使用或打出一张杀，获得1点武功
]]--****************************************************************
sgs.glory_info["gainWuGong"] = {
	name = "gainWuGong",
	state = "验证通过",
	mode = "all_modes",
	events = {sgs.CardUsed, sgs.CardResponded},
	data = "gainWuGong_data",
	keys = {
		--"Global_point",
		"point",
	},
}
sgs.gainWuGong_data = {}
sgs.ai_event_callback[sgs.CardUsed].gainWuGong = function(self, player, data)
	local use = data:toCardUse()
	local source = use.from
	if source and source:objectName() == player:objectName() and isTarget(player) then
		if use.card:isKindOf("Slash") then
			addInfo("gainWuGong", "point", 1)
		end
	end
end
sgs.ai_event_callback[sgs.CardResponded].gainWuGong = function(self, player, data)
	local response = data:toCardResponse()
	if response.m_card:isKindOf("Slash") and isTarget(player) then
		addInfo("gainWuGong", "point", 1)
	end
end
sgs.game_over_works["gainWuGong"] = function(room, player)
	local point = getInfo("gainWuGong", "point", 0)
	if point ~= 0 then
		local original = getInfo("gainWuGong", "Global_point", 0)
		local total = original + point
		sgs.glory_data["WuGong"] = total
		setInfo("gainWuGong", "Global_point", total)
		local msg = sgs.LogMessage()
		msg.type = "#gainWuGong"
		msg.from = player
		msg.arg = point
		room:sendLog(msg) --发送提示信息
	end
end
--[[****************************************************************
	功能：经验统计
	描述：每造成1点伤害，获得1点经验，每次伤害最多获得8点经验
]]--****************************************************************
sgs.glory_info["gainExperience"] = {
	name = "gainExperience",
	state = "验证通过",
	mode = "all_modes",
	events = {sgs.NonTrigger},
	data = "gainExperience_data",
	keys = {
		--"Global_point",
		"point",
	},
}
sgs.gainExperience_data = {}
sgs.ai_event_callback[sgs.NonTrigger].gainExperience = function(self, player, data)
	local cmd = data:toString()
	if cmd == "gloryDamageDone_Source" and isTarget(player) then
		local damage = self.room:getTag("gloryData"):toDamage()
		local point = math.min( 8, damage.damage )
		addInfo("gainExperience", "point", point)
	end
end
sgs.game_over_works["gainExperience"] = function(room, player)
	local point = getInfo("gainExperience", "point", 0)
	if point ~= 0 then
		local original = getInfo("gainExperience", "Global_point", 0)
		local total = original + point
		sgs.glory_data["Experience"] = total
		setInfo("gainExperience", "Global_point", total)
		local msg = sgs.LogMessage()
		msg.type = "#gainExperience"
		msg.from = player
		msg.arg = point
		room:sendLog(msg) --发送提示信息
	end
end
--[[****************************************************************
	功能：技能钥匙
	描述：杀死一名角色后，随机获得一定数目的技能钥匙。
	说明：游戏开始时，若已获得至少一把技能钥匙，系统将随机选取至多5个场上不存在的技能；
		可以使用一把技能钥匙并失去1点体力，选择并获得其中的一个技能。
]]--****************************************************************
sgs.glory_info["gainExtraSkill"] = {
	name = "gainExtraSkill",
	state = "验证通过",
	mode = "all_modes",
	events = {sgs.NonTrigger},
	data = "gainExtraSkill_data",
	keys = {
		--Global_count,
	},
}
sgs.gainExtraSkill_data = {}
if EnableExtraSkill then
	local function meetSkillCard(room, source, count)
		addInfo("gainExtraSkill", "Global_count", count)
		local msg = sgs.LogMessage()
		msg.type = "#gainExtraSkill"
		msg.from = source
		msg.arg = count
		msg.arg2 = getInfo("gainExtraSkill", "Global_count", 0)
		room:sendLog(msg) --发送提示信息
	end
	sgs.ai_event_callback[sgs.NonTrigger].gainExtraSkill = function(self, player, data)
		local cmd = data:toString()
		if cmd == "gloryGameOverJudge_Killer" and isTarget(player) then
			local result = math.random(0, 100)
			if result <= 75 then
				meetSkillCard(self.room, player, 1)
			elseif result <= 80 then
				meetSkillCard(self.room, player, 2)
			elseif result == 81 then
				meetSkillCard(self.room, player, 5)
				addFinishTag("vTouDengDaJiang")
			end
		end
	end
end
function gainAnExtraSkill(room, source)
	local keys = getInfo("gainExtraSkill", "Global_count", 0)
	if tonumber(keys) <= 0 then
		return 
	end
	local generals = sgs.Sanguosha:getLimitedGeneralNames()
	local exist_generals = {}
	local exist_record = {}
	--产生已登场的武将
	local allplayers = room:getPlayers()
	for _,p in sgs.qlist(allplayers) do
		local nameA, nameB = p:getGeneralName(), p:getGeneral2Name()
		if nameA ~= "" and not exist_record[nameA] then
			table.insert(exist_generals, nameA)
			exist_record[nameA] = true
		end
		if nameB ~= "" and not exist_record[nameB] then
			table.insert(exist_generals, nameB)
			exist_record[nameB] = true
		end
	end
	--整理禁配表
	local generalA, generalB = source:getGeneralName(), source:getGeneral2Name()
	dofile "lua/config.lua"
	local banPairs = config["pairs_ban"] or {}
	local banGenerals = {}
	if generalA ~= "" then
		for _,item in ipairs(banPairs) do
			if string.find(item, generalA) then
				local names = item:split("+")
				if #names == 2 then
					if names[1] == generalA then
						banGenerals[names[2]] = true
					elseif names[2] == generalA then
						banGenerals[names[1]] = true
					end
				end
			end
		end
	end
	if generalB ~= "" then
		for _,item in ipairs(banPairs) do
			if string.find(item, generalB) then
				local names = item:split("+")
				if #names == 2 then
					if names[1] == generalB then
						banGenerals[names[2]] = true
					elseif names[2] == generalB then
						banGenerals[names[1]] = true
					end
				end
			end
		end
	end
	--排除已登场的武将和禁配表中的武将组合
	for index = #generals, 1, -1 do
		local general = generals[index]
		if exist_record[general] then
			table.remove(generals, index)
		elseif banGenerals[general] then
			table.remove(generals, index)
		end
	end
	if #generals == 0 then
		return 
	end
	--产生可获得的技能
	local all_skills = {}
	local exist_skills = {}
	for _,name in ipairs(generals) do
		local general = sgs.Sanguosha:getGeneral(name)
		if general then
			local skills = general:getVisibleSkillList()
			for _,skill in sgs.qlist(skills) do
				if exist_skills[skill:objectName()] then
				elseif source:hasSkill(skill:objectName()) then
				elseif skill:inherits("SPConvertSkill") then
				elseif skill:isAttachedLordSkill() then
				else
					table.insert(all_skills, skill:objectName())
					exist_skills[skill:objectName()] = true
				end
			end
		end
	end
	--产生选项
	local count = math.min(5, #all_skills)
	if count == 0 then
		return 
	end
	local choices = {}
	for i=1, count, 1 do
		local index = math.random(1, #all_skills)
		table.insert(choices, all_skills[index])
		table.remove(all_skills, index)
	end
	table.insert(choices, "cancel")
	choices = table.concat(choices, "+")
	--获得技能
	local choice = room:askForChoice(source, "gainExtraSkill", choices)
	if choice == "cancel" then
		return 
	end
	addInfo("gainExtraSkill", "Global_count", -1)
	local hp = source:getHp() - 1
	room:setPlayerProperty(source, "hp", sgs.QVariant(hp))
	room:handleAcquireDetachSkills(source, choice)
end
--[[****************************************************************
	功能：珍贵的经验
	描述：一名己方角色阵亡后，若场上人数不少于3且自己为己方仅存角色，可以消耗一定的经验执行一个效果：
		（1）消耗50点经验：令所有其他未翻面的角色翻面。
		（2）消耗125点经验：重置武将并摸三张牌。
		（3）消耗300点经验：随机挂满装备并将手牌补至体力上限。
		（4）消耗500点经验：令一名角色失去一项技能。
		（5）消耗985点经验：令一名角色失去1点体力上限。
		（6）消耗2350点经验：进入濒死时自动脱离（限X次，X为当前场上角色数）。
		（7）消耗5050点经验：复活一名已阵亡的角色。
		（8）消耗12580点经验：将一名其他角色的身份变为与自己一致并重置其AI。
		（9）消耗32767点经验：投降免收皮肉之苦……
]]--****************************************************************
if PreciousExperience then
	local function doPreciousExperience(room, source, experience)
		local choices = {}
		if experience >= 50 then
			table.insert(choices, "itemA")
		end
		if experience >= 125 then
			table.insert(choices, "itemB")
		end
		if experience >= 300 then
			table.insert(choices, "itemC")
		end
		if experience >= 500 then
			table.insert(choices, "itemD")
		end
		if experience >= 985 then
			table.insert(choices, "itemE")
		end
		if experience >= 2350 then
			table.insert(choices, "itemF")
		end
		if experience >= 5050 then
			table.insert(choices, "itemG")
		end
		if experience >= 12580 then
			table.insert(choices, "itemH")
		end
		if experience >= 32767 then
			table.insert(choices, "itemI")
		end
		table.insert(choices, "cancel")
		if #choices == 1 then
			return 
		end
		choices = table.concat(choices, "+")
		local item = room:askForChoice(source, "glory", choices)
		if item == "itemA" then
			addInfo("gainExperience", "point", -50)
			local others = room:getOtherPlayers(source)
			for _,p in sgs.qlist(others) do
				if p:faceUp() then
					p:turnOver()
				end
			end
		elseif item == "itemB" then
			addInfo("gainExperience", "point", -125)
			if not source:faceUp() then
				source:turnOver()
			end
			if source:isChained() then
				room:setPlayerProperty(source, "chained", sgs.QVariant(false))
				room:setEmotion(source, "chained")
				room:broadcastProperty(source, "chained")
				room:getThread():trigger(sgs.ChainStateChanged, room, source)
			end
			local judges = source:getJudgingAreaID()
			if not judges:isEmpty() then
				local move = sgs.CardsMoveStruct()
				move.from = source
				move.from_place = sgs.Player_PlaceDelayedTrick
				move.to = nil
				move.to_place = sgs.Player_DiscardPile
				move.card_ids = judges
				move.reason = sgs.CardMoveReason(sgs.CardMoveReason_S_REASON_DISCARD, source:objectName())
				room:moveCardsAtomic(move, true)
			end
			room:drawCards(source, 3, "glory")
		elseif item == "itemC" then
			addInfo("gainExperience", "point", -300)
			local weapons, armors, dhorses, ohorses, treasures = {}, {}, {}, {}, {}
			for id = 0, sgs.Sanguosha:getCardCount()-1, 1 do
				local equip = sgs.Sanguosha:getCard(id)
				if equip:isKindOf("Weapon") then
					table.insert(weapons, id)
				elseif equip:isKindOf("Armor") then
					table.insert(armors, id) 
				elseif equip:isKindOf("DefensiveHorse") then
					table.insert(dhorses, id)
				elseif equip:isKindOf("OffensiveHorse") then
					table.insert(ohorses, id)
				elseif equip:isKindOf("Treasure") then
					table.insert(treasures, id)
				end
			end
			local function useEquip(equip)
				local move = sgs.CardsMoveStruct()
				move.card_ids:append(equip)
				move.to = source
				move.to_place = sgs.Player_PlaceEquip
				move.reason = sgs.CardMoveReason(sgs.CardMoveReason_S_REASON_PUT, source:objectName())
				room:moveCardsAtomic(move, true)
			end
			if not source:getWeapon() and #weapons > 0 then
				local weapon = weapons[math.random(1, #weapons)]
				useEquip(weapon)
			end
			if not source:getArmor() and #armors > 0 then
				local armor = armors[math.random(1, #armors)]
				useEquip(armor)
			end
			if not source:getDefensiveHorse() and #dhorses > 0 then
				local dhorse = dhorses[math.random(1, #dhorses)]
				useEquip(dhorse)
			end
			if not source:getOffensiveHorse() and #ohorses > 0 then
				local ohorse = ohorses[math.random(1, #ohorses)]
				useEquip(ohorse)
			end
			if not source:getTreasure() and #treasures > 0 then
				local treasure = treasures[math.random(1, #treasures)]
				useEquip(treasure)
			end
			local delt = source:getMaxHp() - source:getHandcardNum()
			if delt > 0 then
				room:drawCards(source, delt, "glory")
			end
		elseif item == "itemD" then
			addInfo("gainExperience", "point", -500)
			local alives = room:getAlivePlayers()
			local targets = sgs.SPlayerList()
			for _,p in sgs.qlist(alives) do
				local skills = p:getVisibleSkillList()
				for _,skill in sgs.qlist(skills) do
					if skill:inherits("SPConvertSkill") then
					elseif skill:isAttachedLordSkill() then
					elseif skill:isLordSkill() then
						if p:hasLordSkill(skill:objectName()) then
							targets:append(p)
							break
						end
					else
						targets:append(p)
						break
					end
				end
			end
			if not targets:isEmpty() then
				local target = room:askForPlayerChosen(source, targets, "glory", "@glory-detachSkill", true)
				if target then
					local to_detach = {}
					local skills = target:getVisibleSkillList()
					for _,skill in sgs.qlist(skills) do
						if skill:inherits("SPConvertSkill") then
						elseif skill:isAttachedLordSkill() then
						elseif skill:isLordSkill() then
							if target:hasLordSkill(skill:objectName()) then
								table.insert(to_detach, skill:objectName())
							end
						else
							table.insert(to_detach, skill:objectName())
						end
					end
					table.insert(to_detach, "cancel")
					to_detach = table.concat(to_detach, "+")
					local skill = room:askForChoice(source, "glory", to_detach)
					if skill ~= "cancel" then
						room:detachSkillFromPlayer(target, skill)
					end
				end
			end
		elseif item == "itemE" then
			addInfo("gainExperience", "point", -985)
			local others = room:getOtherPlayers(source)
			for _,p in sgs.qlist(others) do
				room:loseMaxHp(p, 1)
			end
		elseif item == "itemF" then
			addInfo("gainExperience", "point", -2350)
			local x = room:alivePlayerCount()
			room:setPlayerMark(source, "glory_itemF", x)
		elseif item == "itemG" then
			addInfo("gainExperience", "point", -5050)
			local targets = {}
			local players = room:getPlayers()
			for _,p in sgs.qlist(players) do
				if p:isDead() then
					table.insert(targets, p:screenName())
				end
			end
			table.insert(targets, "cancel")
			targets = table.concat(targets, "+")
			local target = room:askForChoice(source, "glory", targets, sgs.QVariant("itemG"))
			if target ~= cancel then
				for _,p in sgs.qlist(players) do
					if p:screenName() == target and p:isDead() then
						room:revivePlayer(p, true)
						break
					end
				end
			end
		elseif item == "itemH" then
			addInfo("gainExperience", "point", -12580)
			local others = room:getOtherPlayers(source)
			local target = room:askForPlayerChosen(source, others, "glory", "@glory-resetRole", false)
			if target then
				local myrole, role = source:getRole(), target:getRole()
				if myrole == "lord" or role == "lord" then
					local players = room:getPlayers()
					for _,p in sgs.qlist(players) do
						if p:isDead() and isSameCamp(source, p) then
							myrole = p:getRole()
							break
						end
					end
				end
				room:setPlayerProperty(target, "role", sgs.QVariant(myrole))
				room:resetAI(target)
			end
		elseif item == "itemI" then
			addInfo("gainExperience", "point", -32767)
			addFinishTag("vSuperMan")
			sgs.glory_data["game_over"] = true
			room:getThread():trigger(sgs.NonTrigger, room, source, sgs.QVariant("gloryAfterGameOverJudge"))
			local alives = room:getAlivePlayers()
			local winner = {}
			for _,p in sgs.qlist(alives) do
				table.insert(winner, p:objectName())
			end
			winner = table.concat(winner, "+")
			room:gameOver(winner)
		elseif item == "cancel" then
			local msg = sgs.LogMessage()
			msg.type = "#gloryPECancel"
			msg.from = source
			room:sendLog(msg) --发送提示信息
		end
	end
	sgs.ai_event_callback[sgs.Death].PreciousExperience = function(self, player, data)
		local death = data:toDeath()
		local victim = death.who
		if victim and victim:objectName() == player:objectName() then
			local me = getTarget()
			if me:isAlive() and isSameCamp(me, victim) and self.room:alivePlayerCount() >= 3 then
				local others = self.room:getOtherPlayers(me)
				for _,p in sgs.qlist(others) do
					if isSameCamp(me, p) then
						return 
					end
				end
				if sgs.gainExperience_data then
					local experience = sgs.gainExperience_data["Global_point"] or 0
					doPreciousExperience(self.room, me, tonumber(experience))
				end
			end
		end
	end
end