--[[

    DISCREPTION   : LuCI - Lua Configuration Interface - lanSet support
    CREATED DATE  : 2016-05-07
    MODIFIED DATE : 
]]--

local m, s, o

--m = Map("DMZ", translate("DMZset"))
    
m = Map("DMZ", translate("DMZ主机"))
m:addmodule("DMZset")

s = m:section(NamedSection, "DMZ", "DMZ")
s.addremove = false
s.anonymous = true

s:tab("dmz_enable", "")

switch = s:taboption("dmz_enable",ListValue, "enable", translate("DMZ host:"))
switch:value("1",translate("enable"))
switch:value("0",translate("disable"))
switch.widget = "radio"
switch.advance_second_title = true
switch.orientation = "horizontal"
switch.min_size = true

dmzhost = s:taboption("dmz_enable",Value, "dmz_ip", translate("dmz host ip:"))
dmzhost.size = long
dmzhost.notnull = true
dmzhost.datatype = "ipv4"
dmzhost.rmempty = false
dmzhost.primpterror = true
dmzhost.display = "none"
dmzhost:depends("enable", "1")

return m
