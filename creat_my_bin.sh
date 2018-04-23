#!/bin/bash

/root/zyg/openwrt-sdk/staging_dir/host/bin/mksquashfs4 /root/zyg/openwrt-sdk/build_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/root-ramips /root/zyg/openwrt-sdk/build_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/linux-ramips_mt7628/root.squashfs -nopad -noappend -root-owned -comp xz -Xpreset 9 -Xe -Xlc 0 -Xlp 2 -Xpb 2  -b 256k -p '/dev d 755 0 0' -p '/dev/console c 600 0 0 5 1' -processors 1

#dd if=/root/zyg/openwrt-sdk/build_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/linux-ramips_mt7628/root.squashfs of=/root/zyg/openwrt-sdk/bin/ramips/openwrt-ramips-mt7628-root.squashfs bs=128k conv=sync

cat /root/zyg/openwrt-sdk/build_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/linux-ramips_mt7628/vmlinux-mt7628.uImage /root/zyg/openwrt-sdk/build_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/linux-ramips_mt7628/root.squashfs > bin/test_4m.bin

cp -rf bin/test_4m.bin /home/lzj/openwrt/7628/jcroute/jy
