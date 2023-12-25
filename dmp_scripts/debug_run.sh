#! /bin/bash

GEM5_PATH=".."
GEM5_ARCH="ARM"
GEM5_BIN="$GEM5_PATH/build/$GEM5_ARCH/gem5.opt"
SW_PATH="aarch-system-20220707"

CHECKPOINT="fs_as-caida_m5out"
DEBUG_FLAG_ARR=(
#    "TLB,TLBVerbose"
    "HWPrefetch,HWPrefetchQueue"
    "Cache,CacheVerbose,CachePort"
    "RequestSlot"
    "PacketQueue"
    "CoherentXBar"
    "DMP"
)

function gem5_run() {
    local RUN_LABEL
    IFS=',' read -r RUN_LABEL _ <<< $1;
    local RUN_LABEL="DEBUG_"$RUN_LABEL
    if [ -d  ${RUN_LABEL} ]; then
        rm -r $RUN_LABEL
    fi

    mkdir $RUN_LABEL
    cd $RUN_LABEL
    local LGEM5_PATH="../${GEM5_PATH}"
    local LGEM5_BIN="../${GEM5_BIN}"
    local LSW_PATH="../${SW_PATH}" 
    local LCHECKPOINT="../${CHECKPOINT}"

    $LGEM5_BIN --debug-flags=$1 \
        ${LGEM5_PATH}/configs/dmp_pf/fs.py \
        --num-cpus 1 \
        --cpu-clock 2.5GHz \
        --cpu-type O3_ARM_v7a_3 \
        --caches --l2cache --l3cache \
        --l1i_size 64kB --l1d_size 32kB --l2_size 256kB \
        --l1i_assoc 8 --l1d_assoc 8 --l2_assoc 16 --cacheline_size 64 \
        --l2_repl_policy LRURP \
        --l2-hwp-type DiffMatchingPrefetcher \
        --mem-type SimpleMemory --mem-size 8GB \
        --kernel=$LSW_PATH/binaries/vmlinux.arm64 \
        --bootloader=$LSW_PATH/binaries/boot.arm64 \
        --disk-image=$LSW_PATH/disks/ubuntu-18.04-arm64-docker.img \
        --script=spmv_csr.rcS \
        --restore-with-cpu O3_ARM_v7a_3 \
        --checkpoint-dir $LCHECKPOINT -r 1 \
        > ${RUN_LABEL}.log;
}

for label in ${DEBUG_FLAG_ARR[@]}; do
{
    gem5_run $label;
} &
done

wait


