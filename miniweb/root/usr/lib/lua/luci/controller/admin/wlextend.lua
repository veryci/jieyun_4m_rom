module ("luci.controller.admin.wlextend",package.seeall)


function index()
    entry({"admin", "more_wlextend"}, template("admin_network/wlextend"), _("Wireless Extend Setting"), 22)
    entry({"admin", "ap_list"}, template("admin_network/ap_list"), _("ap_list"), 16)
    entry({"admin", "more_wlextend", "trial"}, call("wirTrial"), _("Wireless Extend trialing"), 17)
    entry({"admin", "more_wlextend", "wirExte"}, call("wirExte"), _("Wireless Extend Setting")).leaf = true
    entry({"admin", "more_wlextend", "reboot"}, call("reboot"), _("Wireless Extend Setting")).leaf = true
end

function wirTrial()

	local uci = require("luci.model.uci").cursor()
	local http = require("luci.http")
	local sys = require("luci.sys")
	local utl = require "luci.util"

	local enable = luci.http.formvalue("wirExte")
	luci.sys.call("killall -SIGUSR2 apcli_daemon");
	if enable == "0" then
		result ="0"
	elseif enable == "1" then 
		local ssid = luci.http.formvalue("wir_ssid") or ""
		ssid = (string.gsub(ssid, "\\\"", "\""))
		local authmode = luci.http.formvalue("safeSelect") or ""
		local aptype = luci.http.formvalue("apcli_aptype") or ""
		local wpapsk = ""
		local encryp = ""
		local bssid = ""

		if authmode == "WPAPSK" or authmode == "WPA2PSK" then
			wpapsk = luci.http.formvalue("pskkey") or ""
			wpapsk = (string.gsub(wpapsk, "\\\"", "\""))
			encryp = luci.http.formvalue("encSelect") or ""
		else
			encryp = "NONE"
		end
		bssid = luci.http.formvalue("apcli_bssid") or ""
	end
	luci.sys.updatahistory("more_wlextend")
	luci.sys.call("/etc/init.d/network enable > /dev/null; /etc/init.d/network restart > /dev/null")
	luci.http.redirect(
		luci.dispatcher.build_url("admin","more_wlextend")
	)
end

function wirExte()
	local http = require("luci.http")
	local sys = require("luci.sys")
	local utl = require "luci.util"
	local uci = require("luci.model.uci").cursor()
	local enable = luci.http.formvalue("wirExte")

	if enable == "0" then
		local name_24g = uci:get_name_by_ifname("wireless", "wifi-iface", "ifname", "ra0")
		uci:set("wireless", name_24g, "ApCliEnable", "0")   
		uci:set("network","wan","ifname","eth0.2")   
		uci:commit("network")
		uci:commit("wireless")
		local data = {
			result = "0"
        	}
		local rv = {}
		rv[#rv+1] = data
		luci.http.prepare_content("application/json")
		luci.http.write_json(rv)
	else
		local ssid = luci.http.formvalue("wir_ssid") or ""
		--传输过程中需要为双引号添加反斜杠，在这里要去掉
		ssid = (string.gsub(ssid, "\\\"", "\""))
		local authmode = luci.http.formvalue("safeSelect") or ""
		local aptype = luci.http.formvalue("apcli_aptype") or ""
		local wpapsk = ""
		local encryp = ""
		local bssid = ""
		local tmp = {
			result = "0"
		}
		local rv = {}

		if authmode == "WPAPSK" or authmode == "WPA2PSK" then
			wpapsk = luci.http.formvalue("pskkey") or ""
			wpapsk = (string.gsub(wpapsk, "\\\"", "\""))
			encryp = luci.http.formvalue("encSelect") or ""
		else
			encryp = "NONE"
		end
		bssid = luci.http.formvalue("apcli_bssid") or ""
		channel = luci.http.formvalue("apcli_channel") or "0"
		local name_24g = uci:get_name_by_ifname("wireless", "wifi-iface", "ifname", "ra0")
		local dev_24g = uci:get_name_by_ifname("wireless", "wifi-device", "band", "2.4G")
		uci:set("wireless", dev_24g, "channel", channel)   
		uci:set("network","wan","ifname","apcli0")   
		uci:set("wireless", name_24g, "ApCliEnable", "1")   
		uci:set("wireless", name_24g, "ApCliBssid", bssid)   
		uci:set("wireless", name_24g, "ApCliSsid", ssid)   
		uci:set("wireless", name_24g, "ApCliAuthMode", authmode)   
		uci:set("wireless", name_24g, "ApCliEncrypType", encryp)   
		uci:set("wireless", name_24g, "ApCliWPAPSK", wpapsk)   
		uci:commit("network")                  
		uci:commit("wireless")                  
		--在包含特殊字符的情况下这里是不能这样打印的，会出问题
		--luci.sys.call("echo ssid: \"%s\" bssid: \"%s\" authmode:\"%s\" encryp: \"%s\" wpapsk: \"%s\" aptype: \"%s\" > /dev/console" % {ssid, bssid, authmode, encryp, wpapsk, aptype})
		local data = {
			result = "0"
		}
		rv[#rv+1] = data
		luci.http.prepare_content("application/json")
		luci.http.write_json(rv)
	end
	--luci.sys.updatahistory("more_wlextend")
end

function reboot()
	local sys = require("luci.controller.admin.system")
	sys.fork_exec("/etc/init.d/network enable > /dev/null; /etc/init.d/network restart > /dev/null")
	local rv = {}	
	local data = {
		result = "0"
	}
	rv[#rv+1] = data
	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end
