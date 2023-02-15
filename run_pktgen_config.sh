#/bin/bash

PKTGEN_PATH=/home/haizhi/downland/Pktgen-DPDK
BIN_APP=${PKTGEN_PATH}/app/x86_64-native-linuxapp-gcc/pktgen

LUA_SCRIPT=${PKTGEN_PATH}/test/udp_seq.lua 

PCIE_1=0000:b8:00.1
PCIE_2=0000:b8:00.3


${BIN_APP} -l 3-7 --proc-type primary --log-level 7 --file-prefix pktgen -w ${PCIE_1} -w ${PCIE_2} --socket-mem 2048 -- -v -T -P -j -m "[4:5].0,[6:7].1" -f ${LUA_SCRIPT} 
