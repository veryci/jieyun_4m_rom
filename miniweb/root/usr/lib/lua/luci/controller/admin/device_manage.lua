local io     = require "io"
local nixio  = require "nixio" 
local fs     = require "nixio.fs"
local uci    = require "luci.model.uci"
local vendor = require "luci.vendor"
local sys    = require "luci.sys"
--local luci  = {}

local dhcp   = require "luci.controller.admin.dhcp"

local bit    =  nixio.bit
module ("luci.controller.admin.device_manage",package.seeall)

function index()
	local page

	page = entry({"admin","device_manage", "form_client"},call("form_client"),nil)
	page = entry({"admin","device_manage", "rename"},call("rename"),nil)
	page.leaf = true
end

-- Returns the current statistics-per-ip entries as two-dimensional table.
-- @return	Table of table containing the current ip stats entries.
--			The following fields are defined for ip stats entry objects:
--			{ "Ip", "mac", "startTime", "rxBytes", "txBytes", "rtInBytes", "rtOutBytes", "IFTYPE" }
function statsperip(callback)
	local stats = (not callback) and {} or nil
	local e, r, v
	if fs.access("/proc/net/statsPerIp") then
		for e in io.lines("/proc/net/statsPerIp") do
			local r = { }, v
			for v in e:gmatch("%S+") do
				r[#r+1] = v
			end

			if r[1] ~= "Ip" then
				local x = {
					["Ip"]		= r[1],
					["mac"]		= r[2],
					["startTime"]   = r[3],
					["rxBytes"]	= r[4],
					["txBytes"]     = r[5],
					["rtInBytes"]   = r[6],
					["rtOutBytes"]  = r[7],
					["IFTYPE"]	= r[8]
				}

				if callback then
					callback(x)
				else
					stats = stats or { }
					stats[#stats+1] = x
				end
			end
		end
	end
	return stats 
end

--根据mac地址查找对应的dhcp客户端的hostname
function hostname_by_mac(tofind)
	local mac, ip, name, hostname
	if fs.access("/var/dhcp.leases") then
		for e in io.lines("/var/dhcp.leases") do
			mac, ip, name = e:match("^%d+ (%S+) (%S+) (%S+)")
			mac = string.upper(mac)
			if mac and ip and mac == tofind then
				hostname = (name ~= "*") and name
				break
			end
		end
	end
	return hostname
end

--根据mac地址获取对应的厂商设备名称
function devname_by_mac(mac)
	local cur = uci.cursor()
	local list,list_bound = dhcp.client_list()
	local flag = false
	local info = {
		device_name = "Unknown",
		is_bind = 0
	}

	for _,lease in ipairs(list) do
		if lease.macaddr == mac then
			info.device_name = lease.hostname
			info.is_bind = lease.is_bind
			flag = true
			break
		end
	end
	if flag == false then
		for _,lease in ipairs(list_bound) do
			if lease.macaddr == mac then
				info.device_name = lease.hostname
				info.is_bind = 1
				flag = true
				break
			end
		end
	end
	if flag == false then
		local section_name = string.gsub(mac,":","_")
		local common_section = cur:get_all("common_host",section_name)
		if common_section ~= nil then
			info.device_name = common_section.hostname
		end
	end

	return info
end

--根据mac地址获取当前在线用户的IP地址
function client_online_by_mac(mac)
	local ip = nil
	--在线用户信息列表
	statsperip(function(e)
		e.mac = string.upper(e.mac)
		if mac == e["mac"] then
			ip = e["Ip"]
		end
	end)
	
	return ip
end

--得到在线用户信息列表、离线用户列表和被禁止上网用户列表
function clientsinfo()
	local cur = uci.cursor()
	local clients_online, clients_offline, clients_forbid

	--将所有配置的is_online设置为0
	cur:foreach("device_manage","limit",function(client)
		cur:set("device_manage",client[".name"],"is_bind",0)
		cur:set("device_manage",client[".name"],"is_online",0)
	end)

	--在线用户信息列表
	statsperip(function(e)
		clients_online = clients_online or {}
		local client = nil
		local cli = nil
		local info = nil

		e.mac = string.upper(e.mac)
		name = string.gsub(e.mac,":","_")
		cli = cur:get_all("device_manage",name)
		info = devname_by_mac(e["mac"])
		if cli then
			local inbyte,outbyte
			if cli["mac"] == e["mac"] then
				inbyte = math.floor(tonumber(e["rtInBytes"])/1024)
				outbyte = math.floor(tonumber(e["rtOutBytes"])/1024)
				if tonumber(cli.dRate) > 0 and inbyte > tonumber(cli.dRate) and inbyte > 10 then 
					inbyte = tonumber(cli.dRate) + math.mod(inbyte,10)
				end
				if tonumber(cli.uRate) > 0 and outbyte > tonumber(cli.uRate) and outbyte > 10 then 
					outbyte = tonumber(cli.uRate) + math.mod(outbyte,10)
				end

				client = {
					ip = e["Ip"],
					mac = e["mac"],
					deviceName = info.device_name,
					rtInBytes = inbyte,
					rtOutBytes = outbyte,
					dRate = cli.dRate,
					uRate = cli.uRate,
					deviceRename = cli.deviceRename,
					internet_enable = cli.internet_enable,
					is_online = 1,
					is_bind = info.is_bind,
					icon = vendor.icon_path_by_mac(e["mac"])
				}
			end
		else
			client = {
				["ip"]              = e["Ip"],
				["mac"]             = e["mac"],
				["deviceName"]	    = info.device_name,
				["internet_enable"] = 1,			--默认允许上网
				["dRate"]           = 0,				--默认值0，表示未限速
				["uRate"]           = 0,				--默认值0，表示未限速
				["is_online"]       = 1,			--在线用户标识
				["rtInBytes"]       = math.floor(tonumber(e["rtInBytes"])/1024),		--下载速率
				["rtOutBytes"]      = math.floor(tonumber(e["rtOutBytes"])/1024), 	--上传速率
				["deviceRename"]    = 0, 				--别名变动标识
				["is_bind"]			= info.is_bind, 				--是否被绑定
				["icon"] 			= vendor.icon_path_by_mac(e["mac"])
			}
		end

		if tonumber(client.internet_enable) == 1 then
			clients_online[#clients_online+1] = client
		end
	end)

	cur:foreach("device_manage","limit",function(client)
		--被禁止上网的用户信息列表
		if client.internet_enable == "0" then
			clients_forbid = clients_forbid or {}
			clients_forbid[#clients_forbid+1] = client
		--已经离线的用户信息列表
		else
			if client.is_online == "0" then
				clients_offline = clients_offline or {}
				clients_offline[#clients_offline+1] = client
			end
		end
	end)


	return clients_online or {}, clients_forbid or {}
end

function rename()
	dhcp.rename_host()

	luci.http.redirect(
		luci.dispatcher.build_url("admin", "device_manage")
	)
end

--获取当前连接设备的用户数量
function client_num_online()
	local tmp = luci.util.exec("wc /proc/net/statsPerIp -l")
	local num = tonumber(tmp:match("%d+ %s*"))
	return num - 1
end

function form_client()
	local cur = uci.cursor()
	local ip,mac,deviceName,internet_enable,dRate,uRate,is_online
	local opt,cur_section,dhcp_section
	local name

	opt = luci.http.formvalue("opt")

	--mac地址作为section的名称，作为唯一的标识
	mac = luci.http.formvalue("mac")
	ip = luci.http.formvalue("ip")
	deviceName = luci.http.formvalue("deviceName")
	internet_enable = tonumber(luci.http.formvalue("internet_enable"))
	--iptables处理速率，单位是字节Byte,因此在写规则时，应将dRate进行处理
	dRate = tonumber(luci.http.formvalue("dRate"))--从页面得到的数据是KB/s
	uRate = tonumber(luci.http.formvalue("uRate"))

    if dRate == nil then
        dRate = 0
    end
    if uRate == nil then
        uRate = 0
    end
	local online_ip = client_online_by_mac(mac)
	if online_ip then
	    is_online = 1
	    ip = online_ip --如果当前mac在线，那么将IP设置成mac对应的IP，否则说明该用户已经离线，IP地址保持不变
	else
	    is_online = 0
	end

	name = string.gsub(mac,":","_")
	cur_section = cur:get_all("device_manage",name)
	if cur_section == nil then
		if deviceName == nil or #deviceName > 32 then
			luci.http.redirect(
				luci.dispatcher.build_url("admin", "device_manage")
			)
			return
		end
		dhcp.rename_host()
		--添加新配置,创建新的section
		cur:section("device_manage","limit",name,{ip=ip,mac=mac,deviceName=deviceName,internet_enable=internet_enable,dRate=dRate,uRate=uRate,deviceRename = 0})
		if is_online == 1 then
			if internet_enable == 1 then
                if (dRate < 0 or dRate > 4096) or (uRate < 0 or uRate > 4096)  then
                    luci.http.redirect(
                        luci.dispatcher.build_url("admin", "device_manage")
                    )
                    return
                end
                if dRate > 0 then
                    luci.sys.call("iptables -t mangle -w -A limit_chain --dst " .. ip .. " -m hashlimit --hashlimit-name dst_" .. name .. " --hashlimit " .. dRate .. "kb/s --hashlimit-burst " .. dRate .. "kb/s --hashlimit-mode dstip -j RETURN".. " > /dev/null 2>&1")
                    luci.sys.call("iptables -t mangle -w -A limit_chain --dst " .. ip .. " -j DROP" .. " > /dev/null 2>&1")
                end
                if uRate > 0 then
                    luci.sys.call("iptables -t mangle -w -A limit_chain --src " .. ip .. " -m hashlimit --hashlimit-name src_" .. name .. " --hashlimit " .. uRate .. "kb/s --hashlimit-burst " .. uRate .. "kb/s --hashlimit-mode srcip -j RETURN".. " > /dev/null 2>&1")
                    luci.sys.call("iptables -t mangle -w -A limit_chain --src " .. ip .. " -j DROP" .. " > /dev/null 2>&1")
                end
			else
				luci.sys.call("iptables -t mangle -w -A limit_chain -m mac --mac-source " .. mac .. " -j DROP" .. " > /dev/null 2>&1")
			end
		end
	else
		if opt ~= "delete" then
			if deviceName == nil or #deviceName > 32 then
			    luci.http.redirect(
				    luci.dispatcher.build_url("admin", "device_manage")
			    )
			    return
			end
			if internet_enable == 1 and (dRate < 0 or dRate > 4096 or uRate < 0 or uRate > 4096) then
			    luci.http.redirect(
				    luci.dispatcher.build_url("admin", "device_manage")
			    )
			    return
			end
		end
		--配置已经存在，当前配置规则清除,从配置文件中拿到的数值也是字符串类型,比较时应该加引号
		if is_online == 1 then
			if cur_section.internet_enable == "1" then
				if cur_section.dRate ~= "0" then
					luci.sys.call("iptables -t mangle -w -D limit_chain --dst " .. cur_section.ip .. " -m hashlimit --hashlimit-name dst_" .. name .." --hashlimit " .. cur_section.dRate .. "kb/s --hashlimit-burst " .. cur_section.dRate .. "kb/s --hashlimit-mode dstip -j RETURN" .. " > /dev/null 2>&1")
					luci.sys.call("iptables -t mangle -w -D limit_chain --dst " .. cur_section.ip .. " -j DROP" .. " > /dev/null 2>&1")
				end
				if cur_section.uRate ~= "0" then
					luci.sys.call("iptables -t mangle -w -D limit_chain --src " .. cur_section.ip .. " -m hashlimit --hashlimit-name src_" .. name .." --hashlimit " .. cur_section.uRate .. "kb/s --hashlimit-burst " .. cur_section.uRate .. "kb/s --hashlimit-mode srcip -j RETURN" .. " > /dev/null 2>&1")
					luci.sys.call("iptables -t mangle -w -D limit_chain --src " .. cur_section.ip .. " -j DROP" .. " > /dev/null 2>&1")
				end
			else
				luci.sys.call("iptables -t mangle -w -D limit_chain -m mac --mac-source " .. cur_section.mac .. " -j DROP")
			end
		end

		if opt == "delete" then
			--删除一条限速配置
			cur:delete("device_manage",name)
		else
			if is_online == 1 then
				if internet_enable == 1 then
					if dRate > 0 then
						luci.sys.call("iptables -t mangle -w -A limit_chain --dst " .. ip .. " -m hashlimit --hashlimit-name dst_".. name .." --hashlimit " .. dRate .. "kb/s --hashlimit-burst " .. dRate .. "kb/s --hashlimit-mode dstip -j RETURN" .. " >/dev/null 2>&1")
						luci.sys.call("iptables -t mangle -w -A limit_chain --dst " .. ip .. " -j DROP" .. ">/dev/null 2>&1")
					end
                    if uRate > 0 then
                        luci.sys.call("iptables -t mangle -w -A limit_chain --src " .. ip .. " -m hashlimit --hashlimit-name src_" .. name .. " --hashlimit " .. uRate .. "kb/s --hashlimit-burst " .. uRate .. "kb/s --hashlimit-mode srcip -j RETURN".. " > /dev/null 2>&1")
                        luci.sys.call("iptables -t mangle -w -A limit_chain --src " .. ip .. " -j DROP" .. " > /dev/null 2>&1")
                    end
				else
					luci.sys.call("iptables -t mangle -w -A limit_chain -m mac --mac-source " .. mac .. " -j DROP" .. " > /dev/null 2>&1")
				end
			end
			
			if deviceName ~= cur_section.deviceName then
				cur_section.deviceRename = 1
			end
			dhcp.rename_host()
			cur_section.mac = mac
			cur_section.ip = ip
			cur_section.deviceName = deviceName
			cur_section.internet_enable = internet_enable
			cur_section.dRate = dRate
			cur_section.uRate = uRate
			cur:tset("device_manage", name, cur_section)
		end
	end

	luci.sys.addhistory("device_manage")
	cur:save("device_manage")
	cur:commit("device_manage")
	
	luci.http.redirect(
		luci.dispatcher.build_url("admin", "device_manage")
	)
end
