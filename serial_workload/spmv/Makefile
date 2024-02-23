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

SRC_DIR := src
BUILD_DIR := build
GEM5_PATH := ../../..

#CROSS_STATIC_LIB := /usr/lib/gcc-cross/aarch64-linux-gnu/11/libgomp.a
#FS_ROOT := 

SRC_FILES := $(wildcard $(SRC_DIR)/*.c)
#SRC_FILES := $(SRC_DIR)/spmv_csr_omp.c
OBJ_FILES := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC_FILES))
ELF_FILES := $(patsubst $(BUILD_DIR)/%.o,$(BUILD_DIR)/%.elf,$(OBJ_FILES))
ASM_FILES := $(patsubst $(BUILD_DIR)/%.elf,$(BUILD_DIR)/%.asm,$(ELF_FILES))

#CFLAGS       += -I${FS_ROOT}/include -nostdinc
CFLAGS      += -DGEM5 -I${GEM5_PATH}/include
CFLAGS 		+= -g -O2 
#CFLAGS 		+=-fopenmp 
#CFLAGS 		 += --sysroot=$(FS_ROOT)
CFLAGS      += -march=armv8-a+nosimd+nosve
ODFLAGS     = -S
# ODFLAGS      = -Sd
# ODFLAGS      = -DSxtd
LDFLAGS     += -L${GEM5_PATH}/util/m5/build/arm64/out -lm5
LDFLAGS     += -static 
#LDFLAGS 	+= -fopenmp
#LDFLAGS 	+= -L${FS_ROOT}/lib -L${FS_ROOT}/lib/aarch64-linux-gnu 
#LDFLAGS 	+= --sysroot=$(FS_ROOT)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.elf: $(BUILD_DIR)/%.o
	$(CC) -o $@ $< $(LDFLAGS) 

$(BUILD_DIR)/%.asm: $(BUILD_DIR)/%.elf
	$(OBJDUMP) $(ODFLAGS) $< > $@ 

.PHONY: clean pre_build all
.PRECIOUS: $(ELF_FILES)
clean:
	rm -rf build 

pre_build:
	mkdir -p build

all: pre_build $(ASM_FILES)
