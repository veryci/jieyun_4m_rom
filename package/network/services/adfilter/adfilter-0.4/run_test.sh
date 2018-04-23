#! /bin/bash

for k in $(seq 1 1000)
do
	curl --noproxy www.qq.com www.qq.com
done
