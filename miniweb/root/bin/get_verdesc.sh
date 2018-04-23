#! /bin/sh

STATUS=$(uci get onekeyupgrade.config.retState)
ERRCODE=$(uci get onekeyupgrade.config.ErrorCode)


if [ $STATUS -eq 1 -a $ERRCODE -eq 0 ];then
	url=$(uci get onekeyupgrade.config.newurl)
	baseurl=$(echo $url|sed 's/\(.*\)\/.*$/\1/')
	verdesc="${baseurl}/verdesc"
	if [ ! -z "$url" ];then
		rm /tmp/verdesc &>/dev/null
		wget -T 30 ${verdesc} -P /tmp
		if [ $? -ne 0 ];then
			touch "/tmp/verdesc" &>/dev/null
		fi
	fi
else
	touch "/tmp/verdesc" &>/dev/null
fi
