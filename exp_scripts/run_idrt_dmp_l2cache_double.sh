#! /usr/bin/parallel --shebang-wrap -j 25 /bin/bash

# input parameter
BENCH=$1
MATRIX=$2
STR_DEG=$3
RAG_AHEAD=$4
RAG_DEG=$5
L2CACHE=$6
WARM_FLAG=$7
MODE=$8

# local parameter 
RUN_LABEL="exp_workspace_strideL1_dmpL2_lat-3-13/${BENCH}_${MATRIX}"

if [[ "$WARM_FLAG" == "warm" ]]; then
    RCS_FLAG="1"
    DIR_FLAG="warm_"
elif [[ "$WARM_FLAG" == "plain" ]]; then
    RCS_FLAG="0"
    DIR_FLAG=""
else
    echo "Invalid parameter: WARM_FLAG"
fi

OUT_DIR=$RUN_LABEL/SDG${STR_DEG}_RAH${RAG_AHEAD}_RDG${RAG_DEG}_L2Cache${L2CACHE}kB_Simple_BigPFQ_double_m5out

GEM5_PATH=".."
GEM5_ARCH="ARM"
GEM5_BIN="$GEM5_PATH/build/$GEM5_ARCH/gem5.opt"
SW_PATH="aarch-system-20220707"

function render_rcS() {
    :> $1/$2_$3_$4.rcS

    echo \
"#! /bin/sh

echo \"Bench starting\"
cd /home
./bench_elf_aftwarm/$2_csr.elf $4 ./mat_csr/$3_csr.mtx
echo \"Bench done.\"

/sbin/m5 exit 
" > $1/$2_$3_$4.rcS

}

if [[ ! -d  ${RUN_LABEL} ]]; then
    mkdir -p $RUN_LABEL
    #render_rcS $RUN_LABEL $BENCH $MATRIX "0";
fi

if [[ "$MODE" == "cpt" ]]; then
    render_rcS $RUN_LABEL $BENCH $MATRIX $RCS_FLAG;
    ${GEM5_BIN} --outdir=${RUN_LABEL}/cpt_${DIR_FLAG}m5out \
        ${GEM5_PATH}/configs/dmp_pf/fs_L2.py \
        --num-cpus 1 \
        --cpu-clock 2.5GHz \
        --cpu-type AtomicSimpleCPU \
        --tlb-size 65536 \
        --mem-type SimpleMemory --mem-size 8GB \
        --kernel=$SW_PATH/binaries/vmlinux.arm64 \
        --bootloader=$SW_PATH/binaries/boot.arm64 \
        --disk-image=$SW_PATH/disks/ubuntu-18.04-arm64-docker.img \
        --script=${RUN_LABEL}/${BENCH}_${MATRIX}_${RCS_FLAG}.rcS
elif [[ "$MODE" == "restore" ]]; then
    render_rcS $RUN_LABEL $BENCH $MATRIX $RCS_FLAG;
    $GEM5_BIN --outdir=$OUT_DIR \
        ${GEM5_PATH}/configs/dmp_pf/fs_L2.py \
        --num-cpus 1 \
        --cpu-clock 2.5GHz \
        --cpu-type O3_ARM_v7a_3 \
        --caches --l2cache \
        --l1i_size 64kB --l1d_size 32kB --l2_size ${L2CACHE}kB \
        --l1i_assoc 8 --l1d_assoc 8 --l2_assoc 16 --cacheline_size 64 \
        --l2_repl_policy LRURP \
        --l1d-hwp-type StridePrefetcher \
        --l2-hwp-type DiffMatchingPrefetcher \
        --dmp-init-bench $BENCH \
        --tlb-size 65536 \
        --stride-degree $STR_DEG \
        --dmp-range-ahead-dist $RAG_AHEAD \
        --dmp-indir-range $RAG_DEG \
        --mem-type SimpleMemory --mem-size 8GB \
        --kernel=$SW_PATH/binaries/vmlinux.arm64 \
        --bootloader=$SW_PATH/binaries/boot.arm64 \
        --disk-image=$SW_PATH/disks/ubuntu-18.04-arm64-docker.img \
        --script=${RUN_LABEL}/${BENCH}_${MATRIX}_${RCS_FLAG}.rcS \
        --restore-with-cpu O3_ARM_v7a_3 \
        --checkpoint-dir ${RUN_LABEL}/cpt_${DIR_FLAG}m5out -r 1 \
        #--maxinsts 10000000
else
    echo "Invalid parameter: MODE"
fi
