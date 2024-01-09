#! /bin/bash

GEM5_PATH=".."
GEM5_ARCH="ARM"
GEM5_BIN="$GEM5_PATH/build/$GEM5_ARCH/gem5.opt"
SW_PATH="aarch-system-20220707"

CHECKPOINT="fs_as-caida_m5out"

${GEM5_BIN} \
    ${GEM5_PATH}/configs/dmp_pf/fs.py \
    --num-cpus 1 \
    --cpu-clock 2.5GHz \
    --cpu-type O3_ARM_v7a_3 \
    --caches --l2cache --l3cache \
    --l1i_size 64kB --l1d_size 32kB --l2_size 256kB \
    --l1i_assoc 8 --l1d_assoc 8 --l2_assoc 16 --cacheline_size 64 \
    --l2_repl_policy LRURP \
    --l2-hwp-type DiffMatchingPrefetcher \
    --mem-type SimpleMemory --mem-size 8GB \
    --kernel=$SW_PATH/binaries/vmlinux.arm64 \
    --bootloader=$SW_PATH/binaries/boot.arm64 \
    --disk-image=$SW_PATH/disks/ubuntu-18.04-arm64-docker.img \
    --script=spmv_csr.rcS \
    --restore-with-cpu O3_ARM_v7a_3 \
    --checkpoint-dir $CHECKPOINT -r 1

mv m5out tune_dmp_m5out

${GEM5_BIN} \
    ${GEM5_PATH}/configs/dmp_pf/fs.py \
    --num-cpus 1 \
    --cpu-clock 2.5GHz \
    --cpu-type O3_ARM_v7a_3 \
    --caches --l2cache --l3cache \
    --l1i_size 64kB --l1d_size 32kB --l2_size 256kB \
    --l1i_assoc 8 --l1d_assoc 8 --l2_assoc 16 --cacheline_size 64 \
    --l2_repl_policy LRURP \
    --l2-hwp-type StridePrefetcher \
    --mem-type SimpleMemory --mem-size 8GB \
    --kernel=$SW_PATH/binaries/vmlinux.arm64 \
    --bootloader=$SW_PATH/binaries/boot.arm64 \
    --disk-image=$SW_PATH/disks/ubuntu-18.04-arm64-docker.img \
    --script=spmv_csr.rcS \
    --restore-with-cpu O3_ARM_v7a_3 \
    --checkpoint-dir $CHECKPOINT -r 1

mv m5out tune_stride_m5out

${GEM5_BIN} \
    ${GEM5_PATH}/configs/dmp_pf/fs.py \
    --num-cpus 1 \
    --cpu-clock 2.5GHz \
    --cpu-type O3_ARM_v7a_3 \
    --caches --l2cache --l3cache \
    --l1i_size 64kB --l1d_size 32kB --l2_size 256kB \
    --l1i_assoc 8 --l1d_assoc 8 --l2_assoc 16 --cacheline_size 64 \
    --l2_repl_policy LRURP \
    --mem-type SimpleMemory --mem-size 8GB \
    --kernel=$SW_PATH/binaries/vmlinux.arm64 \
    --bootloader=$SW_PATH/binaries/boot.arm64 \
    --disk-image=$SW_PATH/disks/ubuntu-18.04-arm64-docker.img \
    --script=spmv_csr.rcS \
    --restore-with-cpu O3_ARM_v7a_3 \
    --checkpoint-dir $CHECKPOINT -r 1

mv m5out tune_nopf_m5out
