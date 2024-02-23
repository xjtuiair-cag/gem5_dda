#GEM5="/home/zhongpei/event_trigger_repro/gem5-cc813c"
# LLVM paths. Note: you probably need to update these.
#LLVM_DIR=
#CLANG=$LLVM_DIR/build/bin/clang
#SYSROOT=/usr/aarch64-linux-gnu

CC=gcc

$CC -O3 npj2epb.c main.c generator.c genzipf.c perf_counters.c cpu_mapping.c parallel_radix_join.c -lpthread -lm -fPIC -o hj2

$CC -O3 npj8epb.c main.c generator.c genzipf.c perf_counters.c cpu_mapping.c parallel_radix_join.c -lpthread -lm -fPIC -o hj8




