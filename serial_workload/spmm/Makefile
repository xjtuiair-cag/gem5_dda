.PHONY: all clean

CROSS_COMPILE = aarch64-linux-gnu-

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

GEM5_PATH = ../../../gem5_dram

# CFLAGS       = -I../../include
CFLAGS       = -I${GEM5_PATH}/include -g -O2
# CFLAGS       = -I${GEM5_PATH}/include -g -Ofast 
CFLAGS      += -march=armv8-a+nosimd+nosve
ODFLAGS     = -S
# ODFLAGS      = -Sd
# ODFLAGS      = -DSxtd
LDFLAGS      = -static 
LDFLAGS     +=-L${GEM5_PATH}/util/m5/build/arm64/out -lm5


spmm.elf : spmm.cc mat_csr.cc 
	$(CXX) -o $@ spmm.cc mat_csr.cc $(CFLAGS) $(LDFLAGS) 

spmm.asm: spmm.elf
	$(OBJDUMP) $(ODFLAGS) spmm.elf > spmm.asm

all: spmm.asm

clean:
	rm -f *.o *.elf *.asm
