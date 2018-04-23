module("luci.controller.admin.button", package.seeall)

function index()
    if not nixio.fs.access("/etc/config/button") then
		return
    end 

    local page

    page = entry({"admin", "more_sysset", "buttonset"}, cbi("button"), _("button"), 80)
end
