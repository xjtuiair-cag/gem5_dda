#! /usr/bin/parallel --shebang-wrap /bin/bash

GEM5_PATH=".."
GEM5_ARCH="ARM"
GEM5_BIN="$GEM5_PATH/build/$GEM5_ARCH/gem5.opt"
SW_PATH="aarch-system-20220707"

BENCH=$1
MATRIX=$2
RUN_LABEL="${BENCH}_${MATRIX}"

STR_DEG=$3
MODE=$4

OUT_DIR=$RUN_LABEL/stride_DG${STR_DEG}_m5out

function render_rcS() {
    :> $1/$2_$3_$4.rcS

    echo \
"#! /bin/sh

echo \"Bench starting\"
cd /home
./bench_elf/$2_csr.elf $4 ./mat_csr/$3_csr.mtx
echo \"Bench done.\"

/sbin/m5 exit 
" > $1/$2_$3_$4.rcS

}

if [[ ! -d  ${RUN_LABEL} ]]; then
    mkdir $RUN_LABEL
    render_rcS $RUN_LABEL $BENCH $MATRIX "1";
fi

if [ $MODE == "cpt" ]; then
    ${GEM5_BIN} --outdir=$RUN_LABEL/cpt_m5out \
        ${GEM5_PATH}/configs/dmp_pf/fs.py \
        --num-cpus 1 \
        --cpu-clock 2.5GHz \
        --cpu-type AtomicSimpleCPU \
        --mem-type SimpleMemory --mem-size 8GB \
        --kernel=$SW_PATH/binaries/vmlinux.arm64 \
        --bootloader=$SW_PATH/binaries/boot.arm64 \
        --disk-image=$SW_PATH/disks/ubuntu-18.04-arm64-docker.img \
        --script=${RUN_LABEL}/${BENCH}_${MATRIX}_1.rcS
else
    #render_rcS $RUN_LABEL $BENCH $MATRIX "1";
    $GEM5_BIN --outdir=$OUT_DIR \
        ${GEM5_PATH}/configs/dmp_pf/fs.py \
        --num-cpus 1 \
        --cpu-clock 2.5GHz \
        --cpu-type O3_ARM_v7a_3 \
        --caches --l2cache --l3cache \
        --l1i_size 64kB --l1d_size 32kB --l2_size 256kB \
        --l1i_assoc 8 --l1d_assoc 8 --l2_assoc 16 --cacheline_size 64 \
        --l2_repl_policy LRURP \
        --l2-hwp-type StridePrefetcher \
        --stride-degree $STR_DEG \
        --mem-type SimpleMemory --mem-size 8GB \
        --kernel=$SW_PATH/binaries/vmlinux.arm64 \
        --bootloader=$SW_PATH/binaries/boot.arm64 \
        --disk-image=$SW_PATH/disks/ubuntu-18.04-arm64-docker.img \
        --script=${RUN_LABEL}/${BENCH}_${MATRIX}_1.rcS \
        --restore-with-cpu O3_ARM_v7a_3 \
        --checkpoint-dir $RUN_LABEL/cpt_m5out -r 1
fi
