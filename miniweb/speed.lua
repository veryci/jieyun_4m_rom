#!/usr/bin/lua
local px     = require "posix"
local fs     = require "nixio.fs"
local nixio     = require "nixio"
local pid_file   = "/var/run/speedtestd.pid"
local uci    = require "luci.model.uci"
local resfile = "/var/run/speedres"
local file = nixio.meta_file 
local spdf



function system_exit()
	os.execute("rm -rf " .. pid_file)
        os.exit(0)
end

function system_init()
	afile = io.open(pid_file, "rw")	
	if (afile) then
		local pid = px.getpid("pid")
		afile:write(pid)
		afile:close()
		print(afile)
	end
	print(resfile)
	--print(nixio)
--[[
	for i,v in pairs(nixio or {}) do
		if type(v) == "table" then
			print (i, type(v))
			for ii,vv in pairs(v or {}) do
			print("-----"..ii,type(vv) )
			end
		else
		print(i, type(v))
		end
	end
]]--
	spdf = nixio.open(resfile, "w")
	if (spdf) then
		file.lock(spdf, "lock")
		--printf(spdf.."=======")
		file.write(spdf, "======")
		file.lock(spdf, "ulock")
		file.close(spdf)
	end
	px.signal(px.SIGTERM,
                function ()
                        logger(10,'signal TERM to stop speed test.')
                        system_exit()
                end)

        px.signal(px.SIGINT,
                function ()
                        logger(10,'signal INT to stop speed test.')
                        system_exit()
                end)

end
function do_speed_test_now()
	print("========")	
end
function if_data_collect(ifname)
        local line
        local face, r_bytes, r_packets, r_errs, r_drop, r_fifo, r_frame, r_compressed, r_multicast
        local t_bytes, t_packets, t_errs, t_drop, t_fifo, t_colls, t_carrier, t_compressed

        if fs.access("/proc/net/dev") then
                for line in io.lines("/proc/net/dev") do
                        _, _, face, r_bytes, r_packets, r_errs, r_drop, r_fifo, r_frame, r_compressed, r_multicast,
                        t_bytes, t_packets, t_errs, t_drop, t_fifo, t_colls, t_carrier, t_compressed = string.find(line,
                        '%s*(%S+):%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s*')
                        if (face == ifname) then
                                return {
                                        r_bytes = tonumber(r_bytes),
                                        r_packets = tonumber(r_packets),
                                        t_bytes = tonumber(t_bytes),
                                        t_packets = tonumber(t_packets)
                                }
                        end
                end
        end
end
function generate_downrate_data(dotdata)
        local j = 1
        local data = {}
        local diff_time

        if #dotdata < 2 then
                logger(3, "down dotdata error, num:".. #dotdata)
                return {}
        end

        while j+1 <= #dotdata do
                diff_time =  dotdata[j+1].uptime - dotdata[j].uptime
                if diff_time <= 0 then
                        logger(3, "generate downrate error, diff_time <= 0")
                        return {}
                end
                data[#data + 1] = (dotdata[j+1].r_bytes - dotdata[j].r_bytes) / diff_time / 1024
                j = j + 1
                logger(10, data[#data])
        end

        for j = 1, #data do
                if data[j] <= 0 then
                        return {}
                end
        end
        return data
end
function generate_uprate_data(dotdata)
        local j = 1
        local data = {}
        local diff_time

        if #dotdata < 2 then
                logger(3, "up dotdata error, num:".. #dotdata)
                return {}
        end

        while j+1 <= #dotdata do
                diff_time =  dotdata[j+1].uptime - dotdata[j].uptime
                if diff_time <= 0 then
                        logger(3, "generate uprate error, diff_time <= 0")
                        return {}
                end
                data[#data + 1] = (dotdata[j+1].t_bytes - dotdata[j].t_bytes) / diff_time / 1024
                j = j + 1
                logger(10, data[#data])
        end

        for j = 1, #data do
                if data[j] <= 0 then
                        return {}
                end
        end
        return data
end
function do_avg_remove_max_min(tdata)
        local sum = 0

        table.sort(tdata)
        table.remove(tdata, #tdata)
        table.remove(tdata,1)

        for _, value in pairs(tdata) do
                sum = sum + value
        end

        return sum / #tdata
end

function wan_data_collect()
	local uci = uci.cursor()
	local wan_ifname=uci:get("network", "wan", "ifname")
        local data = if_data_collect(wan_ifname)
	return data;
end

function main_loop()
	local data0 
	local data1
	local upspeed
	local downspeed
	data1= wan_data_collect()
	
	while true do
		data0 = data1
		px.sleep(2)
		data1= wan_data_collect()
		downspeed = (data1.r_bytes -  data0.r_bytes)
		upspeed = (data1.t_bytes -  data0.t_bytes)

		spdf = nixio.open(resfile, "w")
		if (spdf) then
			file.lock(spdf, "lock")
			--printf(spdf.."=======")
			file.seek(spdf,0)
			file.write(spdf, downspeed ..","..upspeed )
			file.lock(spdf, "ulock")
			file.close(spdf)
		end
	end
end

function main()

	local pid = nixio.fork()
	if pid == 0 then
                system_init(pid)
                px.sleep(1)
		local s, e = pcall(main_loop)
                if not s then
                	--logger(10, e)
                        --logger(10, "22pcall exit!")
			print(e)
                end
                system_exit()
        end

end

main()
