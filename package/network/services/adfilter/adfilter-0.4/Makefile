OBJS := parser.o main.o redsocks.o log.o http-connect.o socks4.o socks5.o http-relay.o base.o base64.o md5.o http-auth.o utils.o redudp.o dnstc.o gen/version.o http-filter.o actionmgr.o libh3/hash.o libh3/header_field.o libh3/header_field_list.o libh3/mempool.o libh3/request_header.o libh3/scanner.o action.pb-c.o http-helper.o simple-http.o businessmgr.o business.pb-c.o update.o nethelper.o report.o clientmgr.o ioswork.c uadata.pb-c.c

SRCS := $(OBJS:.o=.c)
HEADERS := $(OBJS:.h=.c)

CONF := config.h
DEPS := .depend
OUT := adfilter 
VERSION := 0.4

LIBS := -levent -lprotobuf-c -lcrypto
CFLAGS += -g -O2
override CFLAGS += -std=gnu99 -Wall

# $OBJS): $(HEADERS)

all: $(OUT)

.PHONY: all clean distclean

tags: *.c *.h
	ctags -R

$(CONF):
	@case `uname` in \
	Linux*) \
		echo "#define USE_IPTABLES" >$(CONF) \
		;; \
	OpenBSD) \
		echo "#define USE_PF" >$(CONF) \
		;; \
	*) \
		echo "Unknown system, only generic firewall code is compiled" 1>&2; \
		echo "/* Unknown system, only generic firewall code is compiled */" >$(CONF) \
		;; \
	esac

# Dependency on .git is useful to rebuild `version.c' after commit, but it breaks non-git builds.
gen/version.c: *.c *.h gen/.build
	rm -f $@.tmp
	echo '/* this file is auto-generated during build */' > $@.tmp
	echo '#include "../version.h"' >> $@.tmp
	echo 'const char* redsocks_version = ' >> $@.tmp
	if [ -d .git ]; then \
		echo '"redsocks.git/'`git describe --tags`'"'; \
		if [ `git status --porcelain | grep -v -c '^??'` != 0 ]; then \
			echo '"-unclean"'; \
		fi \
	else \
		echo '"redsocks/$(VERSION)"'; \
	fi >> $@.tmp
	echo ';' >> $@.tmp
	mv -f $@.tmp $@

gen/.build:
	mkdir -p gen
	touch $@

base.c: $(CONF)

# $(DEPS): $(SRCS)
#	gcc -MM $(SRCS) 2>/dev/null >$(DEPS) || \
#	( \
#		for I in $(wildcard *.h); do \
#			export $${I//[-.]/_}_DEPS="`sed '/^\#[ \t]*include \?"\(.*\)".*/!d;s//\1/' $$I`"; \
#		done; \
#		echo -n >$(DEPS); \
#		for SRC in $(SRCS); do \
#			echo -n "$${SRC%.c}.o: " >>$(DEPS); \
#			export SRC_DEPS="`sed '/\#[ \t]*include \?"\(.*\)".*/!d;s//\1/' $$SRC | sort`"; \
#			while true; do \
#				export SRC_DEPS_OLD="$$SRC_DEPS"; \
#				export SRC_DEEP_DEPS=""; \
#				for HDR in $$SRC_DEPS; do \
#					eval export SRC_DEEP_DEPS="\"$$SRC_DEEP_DEPS \$$$${HDR//[-.]/_}_DEPS\""; \
#				done; \
#				export SRC_DEPS="`echo $$SRC_DEPS $$SRC_DEEP_DEPS | sed 's/  */\n/g' | sort -u`"; \
#				test "$$SRC_DEPS" = "$$SRC_DEPS_OLD" && break; \
#			done; \
#			echo $$SRC $$SRC_DEPS >>$(DEPS); \
#		done; \
#	)

# include $(DEPS)
##################################################################################
# c++ files
#CXXFLAGS = -c -Wall
 
#SOURCE = $(wildcard *.cpp)
# HEADERS = $(wildcard *.h)

# %.o: %.cpp $(HEADERS)
#	$(CXX) $(CXXFLAGS) $< -o $@

# CXXOBJS = $(patsubst %.cpp, %.o, $(SOURCE)) 

# $(TARGET): $(OBJS)
#	$(CXX) $(OBJS) -lz -lssl -lcrypto -o $(TARGET)

##################################################################################

$(OUT): $(OBJS) $(CXXOBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

clean:
	$(RM) $(OUT) $(CONF) $(OBJS)

distclean: clean
	$(RM) tags $(DEPS)
	$(RM) -r gen
