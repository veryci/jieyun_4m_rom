local disp = require "luci.dispatcher"	
local request = disp.context.request
require "luci.fs"
require "luci.veryci"
local uci_t = require "luci.model.uci".cursor()
data ={ wan= {} } 
local has_network = luci.fs.stat("/etc/config/network")
if has_network then
	data.wan.ifname        = uci_t:get("network","wan","ifname")    
	if data.wan.ifname == "apcli0"    then
		data.wan.macaddr    = luci.sys.exec("ifconfig apcli0 | grep HWaddr | cut -c 39-55") 
	elseif data.wan.ifname == "apclii0"    then
		data.wan.macaddr    = luci.sys.exec("ifconfig apclii0 | grep HWaddr | cut -c 39-55") 
	else   
		if uci_t:get("network","wan", "ignore") == "1" then
        	data.wan.macaddr    = uci_t:get("network","wan","macaddr")
        else
            data.wan.macaddr    = uci_t:get("network","wan","mac_addr")
			if data.wan.macaddr == nil then
                data.wan.macaddr    = uci_t:get("network","wan","macaddr")
			end
        end
    end

        data.wan.proto        = uci_t:get("network","wan","proto")
else
    data.wan.macaddr    = "00:00:00:00:00:00" 
end
	
function get_wan_mac()                                                                  
	local mac = "00:00:00:00:00:00"   
    if data.wan.macaddr then
       	 mac = data.wan.macaddr          
    end                                                
    return mac    
end   
