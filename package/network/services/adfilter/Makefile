##############################################
# OpenWrt Makefile for helloworld program
#
#
# Most of the variables used here are defined in
# the include directives below. We just need to
# specify a basic description of the package,
# where to build our program, where to find
# the source files, and where to install the
# compiled program on the router.
#
# Be very careful of spacing in this file.
# Indents should be tabs, not spaces, and
# there should be no trailing whitespace in
# lines that are not commented.
#
##############################################
include $(TOPDIR)/rules.mk
# Name and release number of this package
PKG_NAME:= adfilter
PKG_RELEASE:=1

# This specifies the directory where we're going to build the program.
# The root build directory, $(BUILD_DIR), is by default the build_mipsel
# directory in your OpenWrt SDK directory
PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

# Specify package information for this program.
# The variables defined here should be self explanatory.
# If you are running Kamikaze, delete the DESCRIPTION
# variable below and uncomment the Kamikaze define
# directive for the description below
define Package/adfilter
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=Web Servers/Proxies
  DEPENDS:=+libevent2 +libprotobuf-c +librt +libopenssl
  TITLE:=Redirect any TCP connection to a SOCKS or HTTPS proxy server
endef

# Uncomment portion below for Kamikaze and delete DESCRIPTION variable above
define Package/redself/description
 If you can't figure out what this program does, you're probably
 brain-dead and need immediate medical attention.
endef

define Package/adfilter/description
  Redsocks is a daemon running on the local system, that will transparently
  tunnel any TCP connection via a remote SOCKS4, SOCKS5 or HTTP proxy server. It
  uses the system firewall's redirection facility to intercept TCP connections,
  thus the redirection is system-wide, with fine-grained control, and does
  not depend on LD_PRELOAD libraries.

  Redsocks supports tunneling TCP connections and UDP packets. It has
  authentication support for both, SOCKS and HTTP proxies.

  Also included is a small DNS server returning answers with the "truncated" flag
  set for any UDP query, forcing the resolver to use TCP.
endef

# Specify what needs to be done to prepare for building the package.
# In our case, we need to copy the source files to the build directory.
# This is NOT the default. The default uses the PKG_SOURCE_URL and the
# PKG_SOURCE which is not defined here to download the source from the web.
# In order to just build a simple program that we have just written, it is
# much easier to do it this way.
define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) -rf ./adfilter-0.4/* $(PKG_BUILD_DIR)/
endef


define Package/adfilter/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/adfilter $(1)/usr/sbin/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/adfilter.init $(1)/etc/init.d/adfilter
	$(INSTALL_BIN) ./files/adfilter_update.sh $(1)/usr/sbin
	$(INSTALL_BIN) ./files/adfilter_task.sh $(1)/usr/sbin
	$(INSTALL_DIR) $(1)/etc
	$(INSTALL_CONF) $(PKG_BUILD_DIR)/adfilter.conf.example $(1)/etc/adfilter.conf
	$(INSTALL_CONF) ./files/hartbeat.dat $(1)/etc/adfilter.dat
endef

$(eval $(call BuildPackage,adfilter))
