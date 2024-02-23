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
CFLAGS       = -I${GEM5_PATH}/include -g -O2
# CFLAGS       = -I${GEM5_PATH}/include -g -Ofast 
CFLAGS      += -march=armv8-a+nosimd+nosve
ODFLAGS     = -S
# ODFLAGS      = -Sd
# ODFLAGS      = -DSxtd
LDFLAGS      = -static 
LDFLAGS     +=-L${GEM5_PATH}/util/m5/build/arm64/out -lm5

bin/tc.o: src/tc.cpp 
	$(CXX) $(CFLAGS) -c -o $@ $<

bin/tc.elf: bin/tc.o
	$(CXX) -o $@ $< $(LDFLAGS)

bin/tc.asm: bin/tc.elf
	$(OBJDUMP) $(ODFLAGS) bin/tc.elf > bin/tc.asm

bin/tc_m5.o: src/tc_m5.cpp 
	$(CXX) $(CFLAGS) -c -o $@ $<

bin/tc_m5.elf: bin/tc_m5.o
	$(CXX) -o $@ $< $(LDFLAGS)

bin/tc_m5.asm: bin/tc_m5.elf
	$(OBJDUMP) $(ODFLAGS) bin/tc_m5.elf > bin/tc_m5.asm
	
clean:
	rm -f bin/*.o bin/*.elf bin/*.asm