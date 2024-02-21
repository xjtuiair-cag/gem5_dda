# DMP-VA-FS Tutorial

## Environment
+ Linux
+ GEM5 build env

## Artifacts
+ FS mode required binaries [AArch64](https://www.gem5.org/documentation/general_docs/fullsystem/guest_binaries)
    + [Linux Kernel with Bootloader](http://dist.gem5.org/dist/v22-0/arm/aarch-system-20220707.tar.bz2)
    + [Linux Disk Images](http://dist.gem5.org/dist/v22-0/arm/disks/ubuntu-18.04-arm64-docker.img.bz2)
+ executable workload, instrumented with gem5ops (eg. spmv_csr.elf)
+ test scripts (eg. spmv_csr.rcS)

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

Configurate rcS with your workload path. Example *spmv_csr.rcS*
```shell
#!/bin/sh

echo "SpMV Computing!"
cd /spmv
./spmv_csr.elf false ./as-caida_csr.mtx
echo "SpMV Done."

/sbin/m5 exit
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
    --dmp-init-bench spmv \
    --dmp-notify l1 \
    --mem-type SimpleMemory --mem-size 8GB \
    --kernel=kernel_path \
    --bootloader=bootloader_path \
    --disk-image=disk_path \
    --script=spmv_csr.rcS \

```

### Additional Options
| Name | Description | Example |
| --- | ------------ | ----- |
| --dmp-init-bench | benchmark name for PC hint | spmv |
| --dmp-notify | access cache level which trigger DMP | l1 |
| --tlb-size | DTLB size | 65536 |
| --stride-degree | degree for StridePrefetcher | 4 |
| --dmp-range-ahead-dist | prefetch ahead number of continuous target pc address | 0 |
| --dmp-indir-range | prefetch generating number from continuous index pc offset | 16 |

### Checkpoints

You can utilize checkpoint mechanism to speedup simulation. Keep *--tlb-size* consistent between cpt dumping and cpt restoring.