#ifndef __AD_CONFIG_H__
#define __AD_CONFIG_H__

#define AD_VERSION		"1.0.0.2"
#define AD_VERSION_RPT	"1.0.0.2_0"
#define AD_SUB_VER		"0"
#define AD_CHANNEL		"x200"
#define AD_DMSG			0

#define DEFAULT_LOCAL_PORT	12345
#define ADFILTER_PATH	"/etc/adfilter.create"
#define ADFILTER_HELLO	"hello debugger"

#define BRLAN_MAC_PATH	"/sys/devices/virtual/net/%s/address"

#define BRLAN_NAME2		"br0"
#define BRLAN_NAME		"br-lan"

#define ADFILTER_RPT_CACHE	"/tmp/adfilter/rpt_cache"
#endif // __AD_CONFIG_H__	
