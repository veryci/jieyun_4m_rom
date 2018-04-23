--[[

    DISCREPTION   : LuCI - Lua Configuration Interface - upnpset support
    CREATED DATE  : 2016-05
    MODIFIED DATE : 2016-06-02
]]--

module ("luci.controller.admin.APPset",package.seeall)
local dispatcher = require "luci.dispatcher"
local uci_t = require "luci.model.uci".cursor()

function index()
    local page
    entry({"admin", "more_sysset"}, alias("admin", "more_sysset", "APPset"), _("APPset"), 90)
    page = entry({"admin", "more_sysset", "APPset"}, call("action_APPset"), _("APPset"), 81)
end

function action_APPset()
	if luci.http.formvalue("button_change") then
		uci_t:set("jsprocess", "config", "ignore", luci.http.formvalue("button_change"))
		uci_t:save("jsprocess")
		uci_t:commit("jsprocess")
		luci.sys.addhistory("APPset")
		if luci.http.formvalue("button_change") == "0" then
			luci.sys.call("killall -61 jsprocess > /dev/null");
			luci.sys.call("/etc/init.d/dnsmasq restart > /dev/null ");
		elseif luci.http.formvalue("button_change") == "1" then
			luci.sys.call("killall -60 jsprocess > /dev/null");
			luci.sys.call("/etc/init.d/dnsmasq restart > /dev/null ");
		end
	end
	luci.template.render("APPset/APPset");
end
