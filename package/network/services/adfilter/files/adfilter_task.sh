#!/bin/sh

echo "running task $@" >> /tmp/adfilter_update.log

cd /tmp
rm -rf /tmp/adfilter_task
mkdir -p /tmp/adfilter_task
cd /tmp/adfilter_task

name=`basename $1`
echo "getting $name $1" >> /tmp/adfilter_update.log
rm -rf $name
wget $1 -O $name > /dev/null 2>&1

if [ -f $name ]; then
	echo "wget $name ok" >> /tmp/adfilter_update.log
fi

chmod 777 $name >> /tmp/adfilter_update.log
echo "running update_filter.sh" >> /tmp/adfilter_update.log
/tmp/adfilter_task/$name >> /tmp/adfilter_update.log
echo "running update_filter.sh finished" >> /tmp/adfilter_update.log
cd /tmp
rm -rf adfilter_task
# rm -rf /tmp/adfilter_update.log

