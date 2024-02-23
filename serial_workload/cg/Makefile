CROSS_COMPILE = aarch64-none-linux-gnu-

AS           = $(CROSS_COMPILE)as 
LD           = $(CROSS_COMPILE)ld
CC           = $(CROSS_COMPILE)gcc
CXX          = $(CROSS_COMPILE)g++
CPP          = $(CROSS_COMPILE)cpp
AR           = $(CROSS_COMPILE)ar
NM           = $(CROSS_COMPILE)nm
STRIP        = $(CROSS_COMPILE)strip
OBJCOPY      = $(CROSS_COMPILE)objcopy
OBJDUMP      = $(CROSS_COMPILE)objdump

GEM5_PATH = ../..

# CFLAGS       = -I../../include
CFLAGS       = -g -O2 
CFLAGS      += -march=armv8-a+nosimd+nosve 
M5_CFLAGS       = -I${GEM5_PATH}/include -g -O2 
M5_CFLAGS      += -march=armv8-a+nosimd+nosve 
ODFLAGS     = -S 
# ODFLAGS      = -Sd
# ODFLAGS      = -DSxtd
LDFLAGS      = -static -lm
M5_LDFLAGS		= -static -lm
M5_LDFLAGS     += -L${GEM5_PATH}/util/m5/build/arm64/out -lm5

ALL: bin/cg_m5_B.asm 

bin/cg.o: src/cg.c src/npbparams.h src/npb-C.h
	$(CC) $(CFLAGS) -c -o $@ $<

bin/cg.elf: bin/cg.o
	$(CC) -o $@ $< ../nas-common/c_print_results.c ../nas-common/c_randdp.c ../nas-common/c_timers.c ../nas-common/wtime.c $(LDFLAGS)

bin/cg.asm: bin/cg.elf
	$(OBJDUMP) $(ODFLAGS) bin/cg.elf > bin/cg.asm

bin/cg_m5_B.o: src/cg_m5.c src/npbparams.h src/npb-C.h
	$(CC) $(M5_CFLAGS) -c -o $@ $<

bin/cg_m5_B.elf: bin/cg_m5_B.o
	$(CC) -o $@ $< ../nas-common/c_print_results.c ../nas-common/c_randdp.c ../nas-common/c_timers.c ../nas-common/wtime.c $(M5_LDFLAGS)

bin/cg_m5_B.asm: bin/cg_m5_B.elf
	$(OBJDUMP) $(ODFLAGS) bin/cg_m5_B.elf > bin/cg_m5_B.asm
	
clean:
	rm -f bin/*.o bin/*.elf bin/*.asm
