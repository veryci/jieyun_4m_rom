--[[

    DISCREPTION   : LuCI - Lua Configuration Interface - lanSet support
    CREATED DATE  : 2016-05-07
    MODIFIED DATE : 
]]--

module("luci.controller.admin.DMZ", package.seeall)

function index()
    if not nixio.fs.access("/etc/config/DMZ") then
	return
    end 

    local page

    entry({"admin", "more_forward"}, alias("admin", "more_forward", "DMZset"), _("DMZset"), 75)
    page = entry({"admin", "more_forward", "DMZset"}, cbi("DMZ_config",{autoapply=true}), _("DMZset"), 76)
    page.dependent = true
end
