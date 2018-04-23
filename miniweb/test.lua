#!/usr/bin/lua
local sys = require "luci.sys"
local utl = require "luci.util"
local log = require "luci.log"
local s2 = {}
local s5 = {}          
local uci_t = require "luci.model.uci".cursor()
local name_24g = uci_t:get_name_by_ifname("wireless", "wifi-device", "band", "5G")
print(name_24g)
function new_scanlist()
	local k, v
	local l= {}

    luci.sys.call("iwpriv ra0 set SiteSurvey=1")
    luci.sys.call("iwpriv rai0 set SiteSurvey=1")
    luci.sys.call("sleep 3")
	
    s2 = new_get_scanlist_2g()
    s5 = new_get_scanlist_5g()
	return 0
end                       
--ra0       get_site_survey:
--Ch  SSID                             BSSID               Security               Siganl(%)W-Mode  ExtCH  NT WPS DPID
--luci.sys.call("iwpriv ra0 set SiteSurvey=1")
--luci.sys.call("sleep 3")                          
--1   VeryciDev                        88:1f:a1:2e:e4:9a   WPA2PSK/AES            100      11b/g/n NONE   In  NO 
function new_get_scanlist_5g()
	local k, v   
    local l = { } 
    local s2 = { }                              
    local swc = io.popen("iwpriv rai0 get_site_survey")
	local ch,ssid,bssid,secu,signal,mode
    if swc then                       
        while true do
			local line = swc:read("*l")
            if not line then break end 
	
			ch,ssid,bssid,secu,signal,mode =line:match("(%d+)%s(.-)%s([0-9a-fA-F:]+)%s+([%w/]+)%s+(%d+)%s+([%w/]+)")
				print(line)
			if ch ~= nil then
				--cha=string.gsub(ch, "^%s*(.-)%s*$", "%1")
				cha=ssid:match("^%s*(.-)%s*$")
				if cha == nil then
					print("+++++++")
				end
				local data = {
					channel = ch;
					ssid = cha;
					bssid = bssid;
					authmode = mode;
					quality = signal;
					security = secu;
					aptype = 2;
				}
				print(ch..","..cha..","..bssid..","..secu..","..signal)
				if not s2[data.bssid] then
                	l[#l+1] = data
                	s2[data.bssid] = true
            	end

			end
         end                    
         swc:close()
    end               
                                                                                                 
    return l                                                                         
end                                                                                      

function new_get_scanlist_2g()
	local k, v   
    local l = { } 
    local s2 = { }                              
    local swc = io.popen("iwpriv ra0 get_site_survey")
	local ch,ssid,bssid,secu,signal,mode
    if swc then                       
        while true do
			local line = swc:read("*l")
            if not line then break end 
	
			ch,ssid,bssid,secu,signal,mode =line:match("(%d+)%s(.-)%s([0-9a-fA-F:]+)%s+([%w/]+)%s+(%d+)%s+([%w/]+)")
			print(line)
			if ch ~= nil then
				cha=ssid:match("^%s*(.-)%s*$")
				local data = {
					channel = ch;
					ssid = cha;
					bssid = bssid;
					authmode = mode;
					quality = signal;
					security = secu;
					aptype = 1;
				}
				--print(ch,cha,bssid,secu,signal)
				if not s2[data.bssid] then
                	l[#l+1] = data
                	s2[data.bssid] = true
            	end

				print(ch..","..cha..","..bssid..","..secu..","..signal)
				--print(data.ch)
			--print(line)
			end
         end                    
         swc:close()
    end               
                                                                                                 
    return l                                                                         
end                                                                                      

function new_scanlist_5g()                                                               
	local k, v                                                                       
    local l = { }                                                                    
    local s5 = { }                                                                   
    return l                                                                         
end                                

--new_scanlist()
