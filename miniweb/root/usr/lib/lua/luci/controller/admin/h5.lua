
--[[

    DISCREPTION   : LuCI - Lua Configuration Interface - lanSet support
    CREATED DATE  : 2016-05-25
    MODIFIED DATE : 
]]--

require "luci.sys"
require "luci.util"
require "luci.template"
require "luci.dispatcher"
require "luci.controller.admin.system"
require "luci.controller.admin.quickguide"
module("luci.controller.admin.h5", package.seeall)

function index()
    entry({"admin", "h5"}, alias("admin", "h5", "welcome"), _("welcome"), 88).index = true
    entry({"admin", "h5", "welcome"},call("do_welcome"), _("Overview"), 1)
    entry({"admin", "h5", "autodetectfinish"},call("do_autodetectfinish"), _("Overview"), 2)
    entry({"admin", "h5", "wanconnectfail"},call("do_wanconnectfail"), _("Overview"), 3)
    entry({"admin", "h5", "internetset"},call("do_internetset"), _("Overview"), 4)
    entry({"admin", "h5", "wirelessset"},call("do_wirelessset"), _("Overview"), 5)
    entry({"admin", "h5", "login"},call("do_login"), _("Overview"), 6)
    entry({"admin", "h5", "saveconfig"},call("do_saveconfig"), _("Overview"), 7)
    entry({"admin", "h5", "showwlconfig"},call("do_showwlconfig"), _("Overview"), 8)
    entry({"admin", "h5", "internetcheck"},call("do_internetcheck"), _("Overview"), 9)
    entry({"admin", "h5", "sohoping"},call("do_sohoping"), _("Overview"), 10)
    entry({"admin", "h5", "link"},call("do_link"), _("Overview"), 12)
    entry({"admin", "h5", "linktowifi"},call("do_linktowifi"), _("Overview"), 13)
    entry({"admin", "h5", "linktointernet"},call("do_linktointernet"), _("Overview"), 14)
    entry({"admin", "h5", "clearcursession"},call("do_clearcursession"), _("Overview"), 15)
    entry({"admin", "h5", "autodetect"},call("do_autodetect"), _("Overview"), 16)
    entry({"admin", "h5", "force2internetset"},call("do_force2internetset"), _("Overview"), 17)
    entry({"admin", "h5", "internetset_autodetect"},call("do_internetset_autodetect"), _("Overview"), 18)
end

function do_internetset_autodetect()
    luci.template.render("h5/internetset",{trigefrom = "autodetect"})
end

function do_force2internetset()
    luci.template.render("h5/internetset")
end

function do_link()
    local connect = luci.util.exec("sleep 1;ping -c 1 -W 1 www.baidu.com | grep -w \"0% packet loss\"")
    if string.len(connect) > 0 then
        luci.template.render("h5/link",{internetok = "ok"})
    else
        luci.template.render("h5/link",{internetok = "fail"})
    end
end

function do_linktowifi()
    luci.template.render("h5/wirelessset",{linktowifi = "enable"})
end
function do_linktointernet()
    if not iswanconnectok() then
        luci.http.redirect(luci.dispatcher.build_url("admin", "h5", "wanconnectfail"))
        return
    end
    luci.template.render("h5/internetset")
end

function do_login()
    if luci.http.formvalue("login") then
        luci.http.redirect(luci.dispatcher.build_url("admin", "h5", "link"))
    else
        luci.template.render("h5/login")
    end
end

function do_clearcursession()
    local clearstatus = "fail"
    local dsp = require "luci.dispatcher"
    local sauth = require "luci.sauth"
    if dsp.context.authsession then
        sauth.kill(dsp.context.authsession)
        dsp.context.urltoken.stok = nil
        clearstatus = "success"
    end

    local rv   = { }
    local data = {
        stokclear   = clearstatus,
    }

    rv[#rv+1] = data

    if #rv > 0 then
        luci.http.prepare_content("application/json")
        luci.http.write_json(rv)
        return
    end
end

function do_sohoping()
    local pingstatus = "fail"
    local uci_t = require "luci.model.uci".cursor()
    luci.controller.admin.system.fork_exec("/etc/init.d/dnsmasq stop")
    local connect = luci.util.exec("sleep 1;ping -c 1 -W 1 www.baidu.com | grep -w \"0% packet loss\"")
    luci.controller.admin.system.fork_exec("/etc/init.d/dnsmasq start")
    if string.len(connect) > 0 then
        pingstatus = "success"
    end
    
    local rv   = { }
    local data = {
        internetok   = pingstatus,
    }

    rv[#rv+1] = data

    if #rv > 0 then
        luci.http.prepare_content("application/json")
        luci.http.write_json(rv)
        return
    end
end

function do_autodetect()
    local info = {
        status = "fail"
    }
    local autodetect = luci.http.formvalue("action")
    if autodetect == "detect" then
        info.status = "success"
        luci.controller.admin.system.fork_exec("autonetwork")
    end
    
    luci.http.prepare_content("application/json")
    luci.http.write_json(info)
end

function do_showwlconfig()
    luci.template.render("h5/showwlconfig")
end

function do_internetcheck()
    luci.template.render("h5/internetcheck")
end

function do_saveconfig()
    luci.template.render("h5/saveconfig")
end

function do_welcome()
    if luci.dispatcher.urlfilter() then
       return 
    end
    local uci_t = require "luci.model.uci".cursor()
    if luci.http.formvalue("welcomeaction") == "pcweb" then
        local firststart = uci_t:get("luci", "main", "firststart")
        if(firststart == "1") then
            luci.http.redirect(luci.dispatcher.build_url("admin", "quickguide"))
        else
            luci.http.redirect(luci.dispatcher.build_url("admin", "logout"))
        end
    elseif luci.http.formvalue("welcomeaction") == "autodetect" then
        local userprotocol = uci_t:get("luci", "main", "userprotocol")
        if not (userprotocol and userprotocol ~= "1") then   
            uci_t:set("luci", "main", "userprotocol", "1")
            uci_t:save("luci")
            uci_t:commit("luci")
        end
        if not iswanconnectok() then
            luci.http.redirect(luci.dispatcher.build_url("admin", "h5", "wanconnectfail"))
            return
        end
        luci.template.render("h5/welcome",{autodetect = 1})
        luci.controller.admin.system.fork_exec("autonetwork")
    else
        luci.template.render("h5/welcome")
    end
end

function do_wanconnectfail()
    luci.template.render("h5/wanconnectfail")
end

function iswanconnectok()
    local stat, iwinfo = pcall(require, "iwinfo")                                    
    local phy_status = nil
    levle = iwinfo.get_phy_connect("wan")
    for k, v in ipairs(levle or { }) do
        if k or v then
            phy_status = v
        end
    end
    if phy_status and phy_status == 1 then
        return true
    else
        return false
    end
end

function do_internetset()
    local uci_t = require "luci.model.uci".cursor()
    
    if not iswanconnectok() then
        luci.http.redirect(luci.dispatcher.build_url("admin", "h5", "wanconnectfail"))
        return
    end
    
    if luci.http.formvalue("autodetect") == "on" then
        uci_t:delete("network","wan","detectwanproto")
        uci_t:save("network")
        luci.template.render(("h5/saveconfig"),{trigefrom = "autodetect"})
        luci.controller.admin.system.fork_exec("autonetwork")
    elseif luci.http.formvalue("autodetect") == "off" then
        if luci.http.formvalue("wanlinkWay") == "dhcp" then
            luci.sys.call("uci set network.wan.proto=dhcp > /dev/null")
            luci.sys.call("uci set network.wan.peerdns=1 > /dev/null")

            luci.util.exec("uci delete network.wan.netmask")
            luci.util.exec("uci delete network.wan.gateway")
            luci.util.exec("uci delete network.wan.static_dns1")
            luci.util.exec("uci delete network.wan.static_dns2")
            luci.util.exec("uci delete network.wan.dns")
        elseif luci.http.formvalue("wanlinkWay") == "pppoe" then
            luci.sys.call("uci set network.wan.proto=pppoe > /dev/null")
            local usr = luci.http.formvalue("pppoeUser") or ""
            local pwd = luci.http.formvalue("pppoePass") or ""
            local service = luci.http.formvalue("pppoeService") or ""
		uci_t:set("network","wan","username", usr)
		uci_t:set("network","wan","password", pwd)
            if(service ~= "") then
		uci_t:set("network","wan","service", service)
            else
                luci.sys.call("uci delete network.wan.service > /dev/null")
            end
            luci.sys.call("uci set network.wan.peerdns=1 > /dev/null")
            luci.util.exec("uci delete network.wan.netmask")
            luci.util.exec("uci delete network.wan.gateway")
            luci.util.exec("uci delete network.wan.static_dns1")
            luci.util.exec("uci delete network.wan.static_dns2")
            luci.util.exec("uci delete network.wan.dns")
        elseif luci.http.formvalue("wanlinkWay") == "static" then
            luci.sys.call("uci set network.wan.proto=static > /dev/null")
            local ip = luci.http.formvalue("staticIp") or ""
            local nm = luci.http.formvalue("staticNetmask") or ""
            local gw = luci.http.formvalue("staticGateway") or ""
            local pd = luci.http.formvalue("staticPriDns") or ""
            uci_t:set("network", "wan", "proto", "static")
            uci_t:set("network", "wan", "ipaddr", ip)
            uci_t:set("network", "wan", "netmask", "255.255.255.0")
            uci_t:set("network", "wan", "gateway", gw)
            uci_t:set("network", "wan", "peerdns", "0")
            uci_t:set("network", "wan", "dns_opt", "0")
            uci_t:set("network", "wan", "static_dns1", pd)
            uci_t:set("network", "wan", "dns", pd)
            luci.util.exec("uci delete network.wan.dns1")
            luci.util.exec("uci delete network.wan.dns2")
        end
        uci_t:save("network")
        if uci_t:get("luci", "main", "firststart") == "0" then
            luci.template.render("h5/saveconfig",{trigefrom = "internetconfig"})
            luci.controller.admin.system.fork_exec("sleep 3;/sbin/luci-reload network")
        else
            luci.http.redirect(luci.dispatcher.build_url("admin", "h5", "wirelessset"))
        end
    else
        luci.template.render("h5/internetset")
    end
end

function do_wirelessset()
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

        local password = luci.http.formvalue("password")
        uci_t:commit("system.weblogin")
        
        local firststart = uci_t:get("luci", "main", "firststart")
        local firstconfig = uci_t:get("luci", "main", "first_config")
        if(firststart == "1") then
            if(firstconfig == "0") then
                uci_t:set("luci", "main", "first_config", "2")
            end        
            uci_t:set("luci", "main", "firststart", "0")
            uci_t:save("luci")
            uci_t:commit("luci")
            luci.dispatcher.domain_hijack_close()
        end
		uci_t:commit("wireless")
        luci.http.redirect(luci.dispatcher.build_url("admin", "h5", "showwlconfig"))
        luci.controller.admin.system.fork_exec("/sbin/luci-reload network")
    else
        luci.template.render("h5/wirelessset")
    end
end

function do_autodetectfinish()
    local stat, iwinfo = pcall(require, "iwinfo")                                    
    local phy_status = nil
    levle = iwinfo.get_phy_connect("wan")
    for k, v in ipairs(levle or { }) do
        if k or v then
            phy_status = v
        end
    end
    
    if phy_status and phy_status == 0 then
        luci.http.redirect(luci.dispatcher.build_url("admin", "h5", "wanconnectfail"))
    else
        luci.http.redirect(luci.dispatcher.build_url("admin", "h5", "internetset_autodetect"))
    end
end
