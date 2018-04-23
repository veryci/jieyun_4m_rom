--[[

    DISCREPTION   : LuCI - Lua Configuration Interface - lanSet support
    CREATED DATE  : 2016-04-26
    MODIFIED DATE : 
]]--

module("luci.controller.admin.wps", package.seeall)

function index()
  
    local page
    entry({"admin", "more_wps"}, template("wl_wps"), _("wpsset"),64)
--  entry({"admin", "more_wps","wpspincode_2g"}, call("getwpspincode_2g"), _("wpssetpincode"),nil)
--  entry({"admin", "more_wps","wpspincode_5g"}, call("getwpspincode_5g"), _("wpssetpincode"),nil)
    entry({"admin", "more_wps","wps_enable"}, call("wpsenable_setting"), _("wps_enable"),nil)
    entry({"admin", "more_wps","wps_pbcconfig"}, call("wpspbc_setting"), _("wps_enable"),nil)
--  entry({"admin", "more_wps","wps_pinconnect"}, call("wps_pinhandle"), _("wps_pinconnect"),nil)
    entry({"admin", "more_wps","wps_pbcconnect"}, call("wps_pbchandle"), _("wps_pbcconnect"),nil)
    entry({"admin", "more_wps","wps_uciclose"}, call("uciclosewps"), _("wps_uciclose"),nil)
    entry({"admin", "more_wps","wps_enable_hidenssid"}, call("wps_hidessid_handle"), _("wps_enable_hidenssid"),nil)
end
function uciclosewps()
    local uci_t = (require "luci.model.uci").cursor()	
    uci_t:set("wps_config","wps_para", "wps_enable", "0")
    uci_t:save("wps_config")
    uci_t:commit("wps_config")
end
function wps_hidessid_handle()
    local hidemain_2g_ssid = luci.http.formvalue("hidemain_2g_ssid")
    --local hidemain_5g_ssid = luci.http.formvalue("hidemain_5g_ssid")
    local wifi_2g_disable = luci.http.formvalue("wifi_2g_disable")
    --local wifi_5g_disable = luci.http.formvalue("wifi_5g_disable")
    local uci_t = (require "luci.model.uci").cursor()

    if hidemain_2g_ssid == "1" and wifi_2g_disable == "0" then
        luci.sys.call("iwpriv ra0 set HideSSID=0")
        uci_t:set("wps_config","wps_para", "wps_enable", "1")
        uci_t:save("wps_config")
        uci_t:commit("wps_config")
    end
    --if hidemain_5g_ssid == "1" and wifi_5g_disable == "0" then
    --    luci.sys.call("iwpriv rai0 set HideSSID=0")
    --    uci_t:set("wps_config","wps_para", "wps_enable","1")
    --    uci_t:save("wps_config")
    --    uci_t:commit("wps_config")
    --end

    luci.http.redirect(luci.dispatcher.build_url("admin","more_wps"))
end

function wpsenable_setting() 
	
        local wps_enable = luci.http.formvalue("wps_enable")
        local uci_t = (require "luci.model.uci").cursor()
        luci.sys.addhistory("more_wps")
        if wps_enable == "1" then
            uci_t:set("wps_config","wps_para", "wps_enable", wps_enable)
            uci_t:save("wps_config")
	    uci_t:commit("wps_config")
            luci.sys.call("iwpriv ra0 set WscConfMode=0")
            --luci.sys.call("iwpriv rai0 set WscConfMode=0")
            luci.sys.call("iwpriv ra0 set WscStop=1")
            --luci.sys.call("iwpriv rai0 set WscStop=1")
            luci.sys.fork_exec("/usr/bin/btnd wps 38 &")
        elseif wps_enable == "0" then
            uci_t:set("wps_config","wps_para", "wps_enable", wps_enable)
            uci_t:save("wps_config")
	    uci_t:commit("wps_config")
            luci.sys.fork_exec("pid1=`pidof btnd` ; kill -9 $pid1 >/dev/null 2>&1")
            luci.sys.call("iwpriv ra0 set WscConfMode=0")
            --luci.sys.call("iwpriv rai0 set WscConfMode=0")
            luci.sys.call("iwpriv ra0 set WscStop=1")
            --luci.sys.call("iwpriv rai0 set WscStop=1")
        end
	luci.http.redirect(luci.dispatcher.build_url("admin","more_wps"))
end


function wpspbc_setting() 
        local wpspbc_switch = luci.http.formvalue("wps_pbc_switch")
        local uci_t = (require "luci.model.uci").cursor()
        luci.sys.addhistory("more_wps")
        if wpspbc_switch == "1" then
            uci_t:set("wps_config","wps_para", "wps_pbc_switch", wpspbc_switch)
            uci_t:save("wps_config")
	    uci_t:commit("wps_config")

        elseif wpspbc_switch == "0" then
            uci_t:set("wps_config","wps_para", "wps_pbc_switch", wpspbc_switch)
            uci_t:save("wps_config")
	    uci_t:commit("wps_config")
        end
	luci.http.redirect(luci.dispatcher.build_url("admin","more_wps"))
end


function wps_pbchandle() 
	
         local pbc_freq = luci.http.formvalue("pbc_freq")
         local uci_t = (require "luci.model.uci").cursor()
         if pbc_freq == "0" then
            luci.sys.call("iwpriv ra0 set WscConfMode=7")
	    luci.sys.call("iwpriv ra0 set WscMode=2")  
	    luci.sys.call("iwpriv ra0 set WscGetConf=1")  
        elseif pbc_freq == "1" then
            --luci.sys.call("iwpriv rai0 set WscConfMode=7")
	    --luci.sys.call("iwpriv rai0 set WscMode=2")  
	    --luci.sys.call("iwpriv rai0 set WscGetConf=1")  
        end
	luci.http.redirect(luci.dispatcher.build_url("admin","more_wps"))
end

