#!/bin/bash

DPDK_PATH=/home/haizhi/work/dpdk/
NIC_1=enp184s0f1
NIC_2=enp184s0f3
PCIE_ID_1=0000:b8:00.1
PCIE_ID_2=0000:b8:00.3
res=$(dpdk-devbind.py -s | grep -E "${NIC_1}|${NIC_2}")
if [ ! -z "${res}" ];then
	ifconfig ${NIC_1} down
	ifconfig ${NIC_2} down
fi

if [ ! -d /sys/module/igb_uio ];then
	modprobe uio
	insmod ${DPDK_PATH}/x86_64-native-linuxapp-gcc/kmod/igb_uio.ko 
fi
dpdk-devbind.py -b igb_uio ${PCIE_ID_1}
dpdk-devbind.py -b igb_uio ${PCIE_ID_2}

