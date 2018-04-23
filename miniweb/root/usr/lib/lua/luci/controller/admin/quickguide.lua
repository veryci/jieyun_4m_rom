--[[

    DISCREPTION   : LuCI - Lua Configuration Interface - lanSet support
    CREATED DATE  : 2016-05-05
    MODIFIED DATE : 
]]--

require "luci.sys"
require "luci.util"
require "luci.template"
require "luci.dispatcher"
require "luci.controller.admin.system"

module("luci.controller.admin.quickguide", package.seeall)

function index()
    entry({"admin", "quickguide"}, alias("admin", "quickguide", "welcome"), _("welcome"), 99).index = true
    entry({"admin", "quickguide", "welcome"},call("do_welcome"), _("Overview"), 20)
    entry({"admin", "quickguide", "wireless_setting"},call("do_wireless_setting"), _("internet_setting"), 31)
    entry({"admin", "quickguide", "quickguide_finish"},call("do_quickguide_finish"), _("internet_setting"), 32)
    entry({"admin", "quickguide", "internet_setting"},call("do_internet_setting"), _("internet_setting"), 33)

end

function do_welcome()
    if luci.dispatcher.urlfilter() then
       return 
    end
    local uci_t = require "luci.model.uci".cursor()
    local userprotocol = luci.http.formvalue("userprotocol") or ""
    local configclick = luci.http.formvalue("configclick") or ""
    if userprotocol and (userprotocol == "0" or userprotocol == "1") then
        local uci_t = require "luci.model.uci".cursor()
        uci_t:set("luci", "main", "userprotocol", userprotocol)
        uci_t:save("luci")
        uci_t:commit("luci")
    end
    if configclick and configclick == "click" then
        luci.template.render("quickguide/welcome",{autodetect = "show"})
    else
        luci.template.render("quickguide/welcome")
    end
end

function dns_set(uci_t, pd)
    uci_t:set("network", "wan", "dns1", pd)
    local wan_section = uci_t:get_all("network","wan")

    local value = pd
    local dns1value = value
    local dns2value 

    uci_t:foreach("network", "interface",
        function(s)
            if s.ifname then
                if s.ifname == "eth0.2" or s.ifname == "apclii0" or s.ifname == "apcli0" then
                    if s.dns2 then
                        dns2value = s.dns2
                    end
                end
            end
        end) 

    if dns1value then
        value = dns1value
        if dns2value then
        value = value .. " " .. dns2value
        end
    else 
        value = dns2value
    end
                                                                         
        local t = { }                                                          
    t = {value}
    value = table.concat(t," ")
                                                        
    uci_t:set("network", "wan", "dns", value)       
end

function do_internet_setting()
    if luci.dispatcher.urlfilter() then
       return 
    end
    local uci_t = require "luci.model.uci".cursor()
    if luci.http.formvalue("autodetect") == "1" then
        luci.template.render("quickguide/internet_setting",{fromautodetect = 1})
        luci.controller.admin.system.fork_exec("autonetwork")
    elseif luci.http.formvalue("autodetect") == "2" then
        luci.template.render("quickguide/internet_setting",{fromautodetect = 2})
    elseif luci.http.formvalue("autodetect") == "0" then
        if luci.http.formvalue("connectionType") == "DHCP" then
            luci.sys.call("uci set network.wan.proto=dhcp > /dev/null")
            luci.sys.call("uci set network.wan.peerdns=1 > /dev/null")
        elseif luci.http.formvalue("connectionType") == "PPPOE" then
            luci.sys.call("uci set network.wan.proto=pppoe > /dev/null")
            local usr = luci.http.formvalue("pppoeUser") or ""
            local pwd = luci.http.formvalue("pppoePass") or ""
	    uci_t:set("network", "wan", "username", usr)
	    uci_t:set("network", "wan", "password", pwd)
            luci.sys.call("uci set network.wan.peerdns=1 > /dev/null")
        elseif luci.http.formvalue("connectionType") == "STATIC" then
            luci.sys.call("uci set network.wan.proto=static > /dev/null")
            local ip = luci.http.formvalue("staticIp") or ""
            local nm = luci.http.formvalue("staticNetmask") or ""
            local gw = luci.http.formvalue("staticGateway") or ""
            local pd = luci.http.formvalue("staticPriDns") or ""
            uci_t:set("network", "wan", "proto", "static")
            uci_t:set("network", "wan", "ipaddr", ip)
            uci_t:set("network", "wan", "netmask", nm)
            uci_t:set("network", "wan", "gateway", gw)
            uci_t:set("network", "wan", "peerdns", "0")
            uci_t:set("network", "wan", "dns_opt", "0")
            uci_t:set("network", "wan", "static_dns1", pd)
            uci_t:set("network", "wan", "dns", pd)
        end
        uci_t:save("network")

        luci.http.redirect(luci.dispatcher.build_url("admin", "quickguide", "wireless_setting"))
    else
        luci.template.render("quickguide/internet_setting")
    end
end

function do_wireless_setting()
    if luci.dispatcher.urlfilter() then
       return 
    end
    local uci_t = require "luci.model.uci".cursor()
    if luci.http.formvalue("savevalidate") == "1" then
        local ssid = luci.http.formvalue("ssid") or ""
        local key = luci.http.formvalue("key") or ""
        local inic_ssid = luci.http.formvalue("inic_ssid") or ""
        local inic_key = luci.http.formvalue("inic_key") or ""
        
        local name_24g = uci_t:get_name_by_ifname("wireless", "wifi-iface", "ifname", "ra0")

        uci_t:set("wireless", name_24g, "ssid", ssid)
        uci_t:set("wireless", name_24g, "key", key)
        uci_t:set("wireless", name_24g, "disabled", "0")
        
        if key ~= ""  then
			uci_t:set("wireless", name_24g, "encryption", "psk-mixed+tkip+ccmp")
        else
			uci_t:set("wireless", name_24g, "encryption", "none")
        end
        
        
        local firststart = uci_t:get("luci", "main", "firststart")
        local firstconfig = uci_t:get("luci", "main", "first_config")
        if(firststart == "1") then
            if(firstconfig == "0") then
                uci_t:set("luci", "main", "first_config", "1")
            end        
            uci_t:set("luci", "main", "firststart", "0")
            uci_t:save("luci")
            uci_t:commit("luci")
            luci.dispatcher.domain_hijack_close()
        end
        
        uci_t:commit("wireless")
        luci.template.render("quickguide/wireless_setting",{showprocess = 1})
        luci.controller.admin.system.fork_exec("/sbin/luci-reload network")
    elseif luci.http.formvalue("savevalidate") == "2" then
        luci.http.redirect(luci.dispatcher.build_url("admin", "quickguide", "quickguide_finish"))
    else
        luci.template.render("quickguide/wireless_setting")
    end
end

function do_quickguide_finish()
    if luci.http.formvalue("chg2moreset") == "1" then    
        luci.http.redirect(luci.dispatcher.build_url("admin", "index"))
    else
        luci.template.render("quickguide/quickguide_finish")
    end
end
