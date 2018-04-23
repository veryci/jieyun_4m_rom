module("luci.controller.admin.ddns", package.seeall)

function index()
    if not nixio.fs.access("/etc/config/ddns") then
	return
    end 

    local page

    page = entry({"admin", "more_ddns"}, cbi("ddns"), _("ddns"), 73)
    page.dependent = true
end
