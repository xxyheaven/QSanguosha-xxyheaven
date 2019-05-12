sum13 = function(selected, to_select)
    local sum = 0
    for _, id in ipairs(selected) do
        sum = sum + sgs.Sanguosha:getCard(id):getNumber()
    end
    if sgs.Sanguosha:getCard(to_select) then
        if table.contains(selected, to_select) then
            sum = sum - sgs.Sanguosha:getCard(to_select):getNumber()
        else
            sum = sum + sgs.Sanguosha:getCard(to_select):getNumber()
        end
    end
    if sum > 13 then
        return false
    else
        return true
    end
end

sum12 = function(selected, to_select)
    local sum = 0
    for _, id in ipairs(selected) do
        sum = sum + sgs.Sanguosha:getCard(id):getNumber()
    end
    if sgs.Sanguosha:getCard(to_select) then
        if table.contains(selected, to_select) then
            sum = sum - sgs.Sanguosha:getCard(to_select):getNumber()
        else
            sum = sum + sgs.Sanguosha:getCard(to_select):getNumber()
        end
    end
    if sum > 12 then
        return false
    else
        return true
    end
end

differentsuit = function(selected, to_select)
    local sum = 0
	--if table.contains(selected, to_select) then return true end
    for _, id in ipairs(selected) do
        if sgs.Sanguosha:getCard(to_select):getSuit() == sgs.Sanguosha:getCard(id):getSuit() then
			return false
		end
	end
	return true
end

heart = function(selected, to_select)
	if to_select == -1 then
		for _, id in ipairs(selected) do
			if sgs.Sanguosha:getCard(id):getSuit() ~= sgs.Card_Heart then
				return false
			end
		end
		return true
	end
	return sgs.Sanguosha:getCard(to_select):getSuit() == sgs.Card_Heart
end

