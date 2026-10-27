# lk2nd Port for Xiaomi Redmi 7 (onclite / sdm632)

This repository contains the `lk2nd` secondary bootloader port for the **Xiaomi Redmi 7** (`onclite` / `onc`), powered by the Qualcomm Snapdragon 632 (MSM8953 family) Application Processor.

`lk2nd` is an open-source Little Kernel (LK) implementation acting as a second-stage bootloader. It provides a standard Android boot image interface, device-tree manipulation and overlay merging, clean hardware initialization, and a 64-bit execution handoff mechanism (`scm_elexec_call`) for downstream/mainline Linux and UEFI firmware environments.

---

## 1. Hardware Overview

| Specification | Details |
|---|---|
| **SoC** | Qualcomm Snapdragon 632 (SDM632 / MSM8953pro) |
| **CPU** | 8x Kryo 250 (ARM Cortex-A53 up to 1.80 GHz) |
| **Architecture** | ARMv8-A (AArch32 execution in stock aboot / lk2nd) |
| **GPU** | Adreno 506 |
| **RAM** | 2 GiB / 3 GiB / 4 GiB LPDDR3 dual-channel |
| **Display Panel** | HD+ (720x1520), Novatek NT36672A or Synaptics DSX |
| **Storage** | eMMC 5.1 (wrp0 partitions) |
| **Foundry ID** | Foundry ID 2 (Samsung 14nm FinFET) |

---

## 2. Key Port Features & Enhancements

### 2.1. Exact DTB Matching & Foundry ID 2
Stock bootloaders on MSM8953 devices query the SoC ID, Chip Variant, Subtype, and Foundry ID from hardware fuses via SMEM. Redmi 7 (`onclite`) uses Foundry ID 2:
- Fixed DTB exact matching logic in `dev_tree.c` to properly score and accept DTBs matching Foundry ID 2.
- Implemented automatic fallback to embedded panel DTBO overlay if the storage DTBO partition is unavailable.

### 2.2. Dynamic DTB / DTBO Overlay Merging (`libufdt`)
- Integrated dynamic device tree overlay applying via `libufdt`.
- Detects the attached display panel and touch controller (Novatek NT36672A vs Synaptics DSX).
- Merges panel and touchscreen overlay nodes dynamically into the base device tree before kernel execution.
- If the eMMC `dtbo` partition cannot be read, falls back cleanly to the embedded `onclite_panel_dtbo_data`.
- Replaced monolithic, static pre-merged DTB blob with dynamic appending (`dev_tree_appended()`), resolving audio DAI probe deferrals (`-517`) and eliminating the 32.5-second `audioserver` restart loop on Android 15.

### 2.3. Partition Budget & Compression
- The target boot partition budget for `lk2nd` is strictly **512 KiB**.
- Dynamic DTBO loading and DTB compression keeps the total `lk2nd.img` binary size at ~454 KiB, safely under the 512 KiB threshold.

### 2.4. Android 15 & Keymaster Compatibility
- Set `mkbootimg` OS version headers to `15.0.0` (patch level `2024-09`), resolving Keymaster `super_key` decryption failures.
- Normalizes kernel command-line arguments, cleanly stripping `androidboot.dtb_idx` and `androidboot.dtbo_idx` to prevent conflict with userspace init.

### 2.5. Storage & I/O Optimization
- Excluded internal eMMC `wrp0` hardware partitions from ext2/block filesystem scanning in `lk2nd_scan_devices()`, eliminating unnecessary pre-kernel boot latency.

---

## 3. Physical Memory Map (3 GiB DDR)

The Qualcomm Snapdragon 632 / MSM8953 architecture splits LPDDR3 physical address space into two discrete hardware banks: Bank 0 (Low RAM) and Bank 1 (High RAM).

| Physical Address Range | Size | Allocation / Description |
|---|---|---|
| `0x40000000 - 0x43400000` | 52 MiB | Low RAM Bank 0 (Conventional System Memory) |
| `0x43400000 - 0x43600000` | 2 MiB | ABOOT Tags & Flattened Device Tree (`tags_addr`) |
| `0x43600000 - 0x7FFFFFFF` | 970 MiB | Low RAM Bank 0 (Free System Memory) |
| `0x80000000 - 0x84000000` | 64 MiB | High RAM Bank 1 (HLOS Low Memory) |
| `0x84000000 - 0x8F600000` | 182 MiB | PIL Reserved Subsystems (Modem, ADSP, WCNSS, Venus, SMEM) |
| `0x8F600000 - 0x90000000` | 10 MiB | ABOOT / lk2nd Execution & Scratch Region |
| `0x90000000 - 0x90800000` | 8 MiB | Continuous Splash FrameBuffer (MDP5 Scanout, 720x1520x4) |
| `0x90800000 - 0x9C000000` | 184 MiB | Free System Memory |
| `0x9C000000 - 0x9E000000` | 32 MiB | UEFI Firmware Device (FD) Image Base |
| `0x9E000000 - 0x9FF8C000` | ~31.5 MiB | Free System Memory |
| `0x9FF8C000 - 0x9FF8D000` | 4 KiB | UEFI Exception Vectors |
| `0x9FF8D000 - 0x9FF90000` | 12 KiB | MMU Level-1 / Level-2 Page Tables |
| `0x9FF90000 - 0x9FFD0000` | 256 KiB | UEFI Execution Stack |
| `0x9FFD0000 - 0x9FFFF000` | ~188 KiB | Debug / FrameBuffer Console Buffers |
| `0xA0000000 - 0xBE000000` | 480 MiB | DXE Core Heap (Executable Memory Pool) |
| `0xBE000000 - 0xF2000000` | 832 MiB | High Free Conventional RAM (Bank 1) |
| `0xF2000000 - 0xFFFFFFFF` | 224 MiB | Qualcomm Secure Execution Environment (QSEE / TrustZone) |

---

## 4. Handoff to 64-Bit BootShim / UEFI

The primary stock bootloader executes in **AArch32** (ARMv7 32-bit) mode. When loading a 64-bit kernel or UEFI firmware device:

1. **SCM EL-Execution Switch (`scm_elexec_call`)**:
   `lk2nd` prepares the 64-bit entry address and invokes the Qualcomm Secure Channel Manager (SCM) service via `smc #0`. The TrustZone firmware (QSEE) reconfigures execution state registers and transitions the primary core to **AArch64 Non-Secure EL1**.

2. **BootShim Entry**:
   Control transfers to `BootShim.S` loaded at `0x80080000` or `0x9C000000`. BootShim validates its load location, initializes initial CPU registers, passes the FDT pointer in `x0`, paints early framebuffer diagnostic checkpoints, and branches directly into `_ModuleEntryPoint` of UEFI PrePi.

3. **PrePi & DXE Dispatch**:
   PrePi initializes caches, sets up the primary 64-bit stack at `0x9FF90000`, configures the MMU, decompresses `FVMAIN` using LZMA, and jumps directly to `DxeMain`.

---

## 5. Building and Flashing

### Requirements
- ARM 32-bit cross toolchain (`arm-none-eabi-` or `arm-linux-gnueabihf-`)
- Device Tree Compiler (`dtc`)
- Python 3

### Build Command
```bash
make TOOLCHAIN_PREFIX=arm-none-eabi- lk2nd-msm8953
```

The resulting bootable image is located at `build-lk2nd-msm8953/lk2nd.img`.

### Fastboot Flash
```bash
fastboot flash boot build-lk2nd-msm8953/lk2nd.img
fastboot reboot
```
