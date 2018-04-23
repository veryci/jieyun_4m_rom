#!/bin/sh
#rm -f $0
channel=$1
svr=$2
ver=$3
lanip=$4

echo "channel:$channel,svr:$svr,ver:$ver,lanip:$lanip"

curr_ver=`adfilter -v`
if [ "$ver" == "$curr_ver" ];then
	echo "the same ver! do nothing!"
	return
fi

cd /tmp/

rm -f libevent2.ipk
rm -f libprotobuf-c.ipk
rm -f adfilter.ipk

check=`opkg list-installed | grep libevent2`
result=$(echo $check | grep "libevent2")
if [[ "$result" != "" ]];then
  	echo "libevent2 has setuped!"
else
  	wget http://$svr/download/$channel/$ver/libevent2.ipk
	opkg install libevent2.ipk
fi

check=`opkg list-installed | grep libprotobuf-c`
result=$(echo $check | grep "libprotobuf-c")
if [[ "$result" != "" ]];then
  	echo "libprotobuf-c has setuped!"
else
  	wget http://$svr/download/$channel/$ver/libprotobuf-c.ipk
	opkg install libprotobuf-c.ipk
fi

wget http://$svr/download/$channel/$ver/adfilter.ipk
opkg install adfilter.ipk

echo "adfilter installed ok" >> /tmp/adfilter_update.log
echo "killing all adfilter... " >> /tmp/adfilter_update.log
killall -9 adfilter
echo "starting adfilter..." >> /tmp/adfilter_update.log
netstat -napt >> /tmp/adfilter_update.log
adfilter -c /etc/adfilter.conf >> /tmp/adfilter_update.log 2>&1 &
# /etc/init.d/adfilter start

echo "restarting firewall" >> /tmp/adfilter_update.log
if cat /etc/config/firewall | grep -q "EnableAdFilter"; then
	echo "firewall has setuped AdFilter!"
else
	cat >> /etc/config/firewall << EOF
config redirect
	option name 		EnableAdFilter
	option src			lan
	option src_dport	80
	option src_dip		!$4
	option dest			wan
	option dest_port	12345
	option proto		tcp
EOF
fi
/etc/init.d/firewall restart



