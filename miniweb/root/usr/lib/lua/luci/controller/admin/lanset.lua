--[[

    DISCREPTION   : LuCI - Lua Configuration Interface - lanSet support
    CREATED DATE  : 2016-04-26
    MODIFIED DATE : 
]]--
require "luci.controller.admin.system"
local uci_t = require "luci.model.uci".cursor()
local sys = require "luci.sys"

module("luci.controller.admin.lanset", package.seeall)

function index()
	if not nixio.fs.access("/etc/config/network") then
		return
	end

	local page

	entry({"admin", "more_wanset"}, alias("admin", "more_wanset", "lanset"), _("lanset"), 10)
	page = entry({"admin", "more_wanset", "lanset"}, call("do_lanset"), _("lanset"), 11)
	page.dependent = true
end

function do_lanset()
	if luci.http.formvalue("autosave") == "1" then
		local origip = uci_t:get("network", "lan", "ipaddr")
		local ip = luci.http.formvalue("ipaddr") or ""
		local nm = luci.http.formvalue("netmask") or ""
		uci_t:set("network", "lan", "ipaddr", ip)
		uci_t:set("network", "lan", "netmask", nm)

		if origip ~= ip then
			luci.sys.call("cp /rom/etc/config/appportfwd /etc/config")
			luci.sys.call("cp /rom/etc/config/DMZ /etc/config")
		end

		uci_t:save("network")
		uci_t:commit("network")
		luci.sys.addhistory("lanset");
		--luci.controller.admin.system.fork_exec("/sbin/luci-reload network")
		--luci.controller.admin.system.fork_exec("killall -USR1 guest_control")
		luci.controller.admin.system.fork_exec("reboot")
	else
		luci.template.render("lanset/lanset")
	end
end
