# DMP-VA-FS Tutorial

## Environment
+ Linux
+ GEM5 build env

## Artifacts
+ FS mode required binaries [AArch64](https://www.gem5.org/documentation/general_docs/fullsystem/guest_binaries)
    + [Linux Kernel with Bootloader](http://dist.gem5.org/dist/v22-0/arm/aarch-system-20220707.tar.bz2)
    + [Linux Disk Images](http://dist.gem5.org/dist/v22-0/arm/disks/ubuntu-18.04-arm64-docker.img.bz2)
+ executable workload, instrumented with gem5ops
+ test scripts (eg. \*.rcS)

## Configuration
+ unzip aarch-system-20220707.tar.bz2
    + path to *vmlinux.arm64* as **kernel_path**
    + path to *boot.arm64* as **bootloader_path**
+ unzip ubuntu-18.04-arm-docker.img.bz2
    + path to this img as **disk_path**

Mount disk img as loop dev.
```shell
sudo losetup -P /dev/loopX disk_path
sudo mount /dev/loopXpY disk_mount_point_as_you_like
```

Copy workload into it.
```shell
sudo cp workload disk_mount_point_as_you_like/your_workload_path/
```
Example:
```
sudo cp spmv_csr.elf as-caida_csr.mtx disk_mount_point_as_you_like/spmv/
```

Config rcS with your workload path. Example *spmv_csr.rcS*
```shell
#!/bin/sh

echo "SpMV Computing!"
cd /spmv
./spmv_csr.elf false ./as-caida_csr.mtx
echo "SpMV Done."

/sbin/m5 exit
```

### [TEST ONLY] src modified
Add potential index PC and targe PC to get matched in `src/mem/cache/prefetch/diff_matching.cc::28-39`. Instantiated and validated.

Example:
```C++
    indexDataDeltaTable.emplace_back(0x400a28, 0, iddt_diff_num);
    targetAddrDeltaTable.emplace_back(0x400a34, 0, tadt_diff_num);
    indexDataDeltaTable[0].validate();
    targetAddrDeltaTable[0].validate();
```

## Run

+ Compile GEM5 with scons:
```shell
scons build/ARM/gem5.opt -j(nproc)
```

+ run gem5. Example:
```shell

build/ARM/gem5.opt \
    configs/dmp_pf/fs.py \
    --num-cpus 1 \
    --cpu-clock 2.5GHz \
    --cpu-type O3_ARM_v7a_3 \
    --caches --l2cache --l3cache \
    --l1i_size 64kB --l1d_size 32kB --l2_size 256kB \
    --l1i_assoc 8 --l1d_assoc 8 --l2_assoc 16 --cacheline_size 64 \
    --l2_repl_policy LRURP \
    --l2-hwp-type DiffMatchingPrefetcher \
    --mem-type SimpleMemory --mem-size 8GB \
    --kernel=kernel_path \
    --bootloader=bootloader_path \
    --disk-image=disk_path \
    --script=your_workload.rcS \

```


