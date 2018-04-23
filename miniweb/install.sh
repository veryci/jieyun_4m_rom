#!/usr/bin/bash
TOPDIR=$(pwd)../
echo $(TOP)
	cp -fpR $(TOPDIR)/miniweb/lighttpd $(TARGET_DIR)/etc/
	cp -fpR $(TOPDIR)/miniweb/root/etc/config/luci $(TARGET_DIR)/etc/config/luci
	cp -fpR $(TOPDIR)/miniweb/root/etc/config/powersave $(TARGET_DIR)/etc/config/powersave
	cp -fpR $(TOPDIR)/miniweb/root/etc/config/factory $(TARGET_DIR)/etc/config/factory
	cp -fpR $(TOPDIR)/miniweb/root/etc/config/tnat $(TARGET_DIR)/etc/config/tnat
	cp -fpR $(TOPDIR)/miniweb/root/etc/config/safeset $(TARGET_DIR)/etc/config/safeset
	cp -fpR $(TOPDIR)/miniweb/root/etc/config/ucitrack $(TARGET_DIR)/etc/config/ucitrack
	#cp -fpR $(TOPDIR)/miniweb/root/etc/config/jsprocess $(TARGET_DIR)/etc/config/jsprocess
	cp -fpR $(TOPDIR)/miniweb/root/etc/config/remote $(TARGET_DIR)/etc/config/remote
	cp -fpR $(TOPDIR)/miniweb/root/etc/config/ddns $(TARGET_DIR)/etc/config/ddns
	cp -fpR $(TOPDIR)/miniweb/root/etc/config/upnpd $(TARGET_DIR)/etc/config/upnpd
	cp -fpR $(TOPDIR)/miniweb/root/etc/config/parentctl $(TARGET_DIR)/etc/config/parentctl
	cp -fpR $(TOPDIR)/miniweb/root/etc/config/DMZ $(TARGET_DIR)/etc/config/DMZ
	cp -fpR $(TOPDIR)/miniweb/root/etc/config/UPnP $(TARGET_DIR)/etc/config/UPnP
	cp -fpR $(TOPDIR)/miniweb/root/etc/config/wps_config $(TARGET_DIR)/etc/config/wps_config
	cp -fpR $(TOPDIR)/miniweb/root/etc/config/light_manage $(TARGET_DIR)/etc/config/light_manage
	cp -fpR $(TOPDIR)/miniweb/root/etc/config/appportfwd $(TARGET_DIR)/etc/config/appportfwd
	cp -fpR $(TOPDIR)/miniweb/root/etc/config/system $(TARGET_DIR)/etc/config/system
	cp -fpR $(TOPDIR)/miniweb/ralink_common.sh  $(TARGET_DIR)/lib/wifi/
	cp -fpR $(TOPDIR)/miniweb/speed.lua  $(TARGET_DIR)/sbin/
	mv $(TARGET_DIR)/usr/lib/lua/luci/template/parser.so $(TARGET_DIR)/
	rm -rf $(TARGET_DIR)/usr/lib/lua/luci
	cp -fpR $(TOPDIR)/miniweb/root/usr/lib/lua/luci $(TARGET_DIR)/usr/lib/lua/
	mv $(TARGET_DIR)/parser.so  $(TARGET_DIR)/usr/lib/lua/luci/template/
	rm -rf $(TARGET_DIR)/www
	cp -fpR $(TOPDIR)/miniweb/root/www $(TARGET_DIR)/
	cp -fp $(TOPDIR)/miniweb/autonetwork $(TARGET_DIR)/sbin/
