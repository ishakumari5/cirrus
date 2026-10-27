# Qualcomm Snapdragon 632 / MSM8953 (Xiaomi Redmi 7 - onclite) EDK2 Port

## 1. Hardware Specifications

| Component | Specification |
|---|---|
| **SoC** | Qualcomm Snapdragon 632 (SDM632 / MSM8953 family) |
| **CPU Architecture** | ARMv8-A 64-bit (8x Kryo 250 / Cortex-A53 cores up to 1.80 GHz) |
| **GPU** | Qualcomm Adreno 506 (Vulkan 1.0, OpenGL ES 3.1) |
| **System RAM** | 3 GiB LPDDR3 (dual 32-bit channels at 933 MHz); 2 GiB / 4 GiB variants supported |
| **Display Panel** | HD+ (720 x 1520), Novatek NT36672A or Synaptics DSX (32bpp XRGB8888, stride 2880 bytes) |
| **Display Controller** | Qualcomm Mobile Display Subsystem (MDSS) / Mobile Display Processor 5 (MDP5) |
| **Interrupt Controller** | ARM Generic Interrupt Controller v2 (GICv2) |
| **Storage** | eMMC 5.1 (wrp0 partitions) |
| **Watchdog** | Qualcomm APCS Hardware Watchdog (WDT0) |

---

## 2. Complete Physical Memory Map (3 GiB DDR)

The Snapdragon 632 / MSM8953 divides physical DDR memory into two discrete banks:
- **Bank 0 (Low RAM)**: `0x40000000 - 0x80000000` (1 GiB total)
- **Bank 1 (High RAM)**: `0x80000000 - 0x100000000` (2 GiB total)

### Memory Map Layout

| Physical Address Range | Size | HOB Type | Cache / Arm Attributes | Description |
|---|---|---|---|---|
| `0x40000000 - 0x43400000` | 52 MiB | `SYS_MEM` / `Conv` | `WRITE_BACK_XN` | Conventional System Memory (Bank 0) |
| `0x43400000 - 0x43600000` | 2 MiB | `MEM_RES` / `Reserv` | `WRITE_BACK_XN` | ABOOT Tags & Flattened Device Tree (`tags_addr`) |
| `0x43600000 - 0x7FFFFFFF` | 970 MiB | `SYS_MEM` / `Conv` | `WRITE_BACK_XN` | Conventional System Memory (Bank 0) |
| `0x80000000 - 0x84000000` | 64 MiB | `SYS_MEM` / `Conv` | `WRITE_BACK_XN` | HLOS Low Conventional Memory |
| `0x84000000 - 0x8F600000` | 182 MiB | `MEM_RES` / `Reserv` | `UNCACHED_UNBUFFERED_XN` | PIL Subsystems (Modem, ADSP, WCNSS, Venus, SMEM) |
| `0x8F600000 - 0x90000000` | 10 MiB | `MEM_RES` / `Reserv` | `WRITE_BACK_XN` | ABOOT / lk2nd Execution & Scratch Region |
| `0x90000000 - 0x90800000` | 8 MiB | `MEM_RES` / `Reserv` | `WRITE_THROUGH_XN` | FrameBuffer Continuous Splash (MDP5 Scanout 720x1520) |
| `0x90800000 - 0x9C000000` | 184 MiB | `SYS_MEM` / `Conv` | `WRITE_BACK_XN` | Conventional System Memory |
| `0x9C000000 - 0x9E000000` | 32 MiB | `SYS_MEM` / `BsData` | `WRITE_BACK` | UEFI Firmware Device (FD) Image Base |
| `0x9E000000 - 0x9FF8C000` | ~31.5 MiB | `SYS_MEM` / `Conv` | `WRITE_BACK_XN` | Conventional System Memory |
| `0x9FF8C000 - 0x9FF8D000` | 4 KiB | `SYS_MEM` / `BsData` | `WRITE_BACK` | CPU Exception Vectors |
| `0x9FF8D000 - 0x9FF90000` | 12 KiB | `SYS_MEM` / `BsData` | `WRITE_BACK_XN` | MMU Level-1 / Level-2 Translation Tables |
| `0x9FF90000 - 0x9FFD0000` | 256 KiB | `SYS_MEM` / `BsData` | `WRITE_BACK_XN` | Primary UEFI Execution Stack |
| `0x9FFD0000 - 0x9FFF7000` | 156 KiB | `MEM_RES` / `Reserv` | `WRITE_BACK_XN` | Shared Console Cursor & UART Buffers |
| `0x9FFF7000 - 0x9FFFF000` | 32 KiB | `MEM_RES` / `Reserv` | `WRITE_BACK_XN` | Early Log Buffer |
| `0x9FFFF000 - 0xA0000000` | 4 KiB | `MEM_RES` / `Reserv` | `WRITE_BACK_XN` | Platform Info Block |
| `0xA0000000 - 0xBE000000` | 480 MiB | `SYS_MEM` / `Conv` | `WRITE_BACK` | **DXE Heap** (Executable Memory Pool for DXE Drivers) |
| `0xBE000000 - 0xF2000000` | 832 MiB | `SYS_MEM` / `Conv` | `WRITE_BACK_XN` | Conventional System Memory (Bank 1) |
| `0xF2000000 - 0xFFFFFFFF` | 224 MiB | `MEM_RES` / `Reserv` | `UNCACHED_UNBUFFERED_XN` | Qualcomm Secure Execution Environment (QSEE / TrustZone) |

---

## 3. Peripheral & MMIO Register Map

| Hardware Block | Physical Base Address | Size | Description |
|---|---|---|---|
| **Security Control** | `0x00058000` | 64 KiB | Qualcomm Security Control registers / fuse blowing |
| **MPM2 Control** | `0x004A0000` | 64 KiB | MSM Power Manager v2 control registers |
| **TLMM CSR** | `0x01000000` | 3 MiB | Top-Level Mode Multiplexer (GPIO configuration) |
| **GCC Clock Control** | `0x01800000` | 704 KiB | Global Clock Controller registers |
| **MDSS / MDP5** | `0x01A00000` | 2 MiB | Mobile Display Subsystem base |
| **MDP_CTL_0** | `0x01A02000` | 4 KiB | MDP5 Control Path 0 (`CTL_START`=0x00, `CTL_FLUSH`=0x18) |
| **MDP_VP_0_VIG_0** | `0x01A05000` | 4 KiB | Video / Graphics Pipe 0 |
| **MDP_VP_0_RGB_0** | `0x01A15000` | 4 KiB | RGB Pipe 0 (Primary display scanout pipe) |
| **MDP_VP_0_DMA_0** | `0x01A25000` | 4 KiB | Direct Memory Access Pipe 0 |
| **PMIC ARB SPMI** | `0x02000000` | 10 MiB | SPMI PMIC Arbiter interface |
| **Peripheral SS** | `0x07800000` | 2 MiB | Core peripheral subsystem |
| **MSM SDC1 (eMMC)** | `0x07824000` | 4 KiB | SDHC/eMMC Controller 1 |
| **MSM SDC2 (SD Card)** | `0x07864000` | 4 KiB | SDHC/SD Card Controller 2 |
| **IMEM Base** | `0x08600000` | 64 KiB | Internal Shared SRAM |
| **APCS Subsystem** | `0x0B000000` | 1 MiB | Application Processor Core Subsystem base |
| **GICv2 Distributor** | `0x0B000000` | 4 KiB | ARM GICv2 Interrupt Distributor (`GICD_*`) |
| **GICv2 CPU Interface** | `0x0B002000` | 4 KiB | ARM GICv2 CPU Interface (`GICC_*`) |
| **APCS Watchdog Base** | `0x0B017000` | 4 KiB | WDT0 Hardware Watchdog Controller |

### APCS Watchdog Register Offsets
- **Pet Trigger Register**: `0x0B017000` (Write 1)
- **WDT0_RST (Reset)**: `0x0B017004` (Write 1 to pet the watchdog timer)
- **WDT0_EN (Enable)**: `0x0B017008` (Write 0 to disable, 1 to enable)
- **WDT0_BARK_TIME**: `0x0B017010` (Set to `0xFFFFFFFF` for maximum bark timeout)
- **WDT0_BITE_TIME**: `0x0B017014` (Set to `0xFFFFFFFF` for maximum bite timeout ~36 hours)

> [!IMPORTANT]
> The APCS hardware watchdog must be continuously petted and disabled throughout PrePi, FrameBuffer console rendering, and `CpuDeadLoop` to prevent spontaneous hardware reboots while debugging or running UEFI applications.

### MDP5 Hardware Pipe Configuration
On MSM8953, only three physical MDP pipes exist: `VIG_0` (`0x01A05000`), `RGB_0` (`0x01A15000`), and `DMA_0` (`0x01A25000`). Accessing non-existent `_1` pipes causes an immediate external abort (AXI bus DECERR). 
The display scanout registers:
- `PIPE_SSPP_SRC0_ADDR` (`+0x14`): Framebuffer base address (`0x90000000`)
- `PIPE_SSPP_SRC_FORMAT` (`+0x30`): `0x000236FF` (32bpp XRGB8888)
- `PIPE_SSPP_SRC_UNPACK_PATTERN` (`+0x34`): `0x03020001` (XRGB byte order)
- `PIPE_SSPP_SRC_YSTRIDE` (`+0x24`): `2880` bytes (720 pixels * 4 bytes/pixel)
- `PIPE_SSPP_SRC_IMG_SIZE` (`+0x04`): `(1520 << 16) | 720`
- `CTL_FLUSH` (`0x01A02018`): `0x00022849` commits shadow registers to scanout on next VSYNC.

---

## 4. Chainloading Mechanism

```
+-------------------------------------------------------------------+
|               lk2nd (Little Kernel Secondary Loader)              |
|   - Runs in AArch32 mode at 0x8F600000                            |
|   - Parses boot.img, merges panel DTBO via libufdt                |
|   - Prepares tags and FDT pointer at 0x43400000 / 0x80000100      |
+-------------------------------------------------------------------+
                                  │
                                  ▼ scm_elexec_call (TrustZone SMC)
+-------------------------------------------------------------------+
|                     BootShim (tools/BootShim.S)                   |
|   - Transitions CPU into AArch64 Non-Secure EL1                   |
|   - Checkpoint 1 (Blue bar): BootShim entry verified              |
|   - Relocates UEFI FD image if needed                             |
|   - Checkpoint 2 (Green bar): Relocation complete                 |
|   - Branches to PrePi _ModuleEntryPoint (x0 = FDT address)        |
+-------------------------------------------------------------------+
                                  │
                                  ▼ Branch
+-------------------------------------------------------------------+
|               PrePi (Silicon/Qualcomm/QcomPkg/PrePi)              |
|   - ModuleEntryPoint.S:                                           |
|       * Checkpoint 3 (Yellow bar): PrePi reached                  |
|       * Sets up primary stack at 0x9FF90000                       |
|       * Enables data and instruction caches (SCTLR_EL1)           |
|       * Checkpoint 4 (Magenta bar): Stack & Caches active         |
|   - Pi.c:                                                         |
|       * Pets & disables APCS watchdog (0x0B017000)                |
|       * Configures MDP5 pipe to 32bpp XRGB8888 (720x1520)         |
|       * Status Indicator (Rows 0-6 Green): PrePi live             |
|       * Initializes FrameBuffer console & FrameBufferSerialPort   |
|       * Parses HOB memory map and initializes MMU translation     |
|       * Checkpoint 6 (Rows 4-8 Orange): MMU & HOBs ready          |
|       * LZMA decompression of FVMAIN                              |
|       * Checkpoint 7 (Rows 8-12 White): FV decompressed           |
+-------------------------------------------------------------------+
                                  │
                                  ▼ LoadDxeCoreFromFv()
+-------------------------------------------------------------------+
|                      DXE Core (DxeMain.c)                         |
|   - Executes in DXE Heap at 0xA0000000 (executable WRITE_BACK)    |
|   - Initializes DXE Architectural Protocols (GICv2, Timers, MMU)  |
|   - Dispatches drivers according to Apriori list                  |
|   - Loads FrameBuffer GraphicsOutputProtocol (GOP) & SimpleInit   |
+-------------------------------------------------------------------+
```

### Visual Diagnostic Framebuffer Indicators
During the transition from `lk2nd` to `DXE Core`, horizontal color bars are drawn directly to the top scanout rows of the framebuffer (`0x90000000`) before any display or console drivers are active:
1. **Blue (40 rows at row 0)**: `BootShim` entry from `lk2nd`.
2. **Green (40 rows at row 40)**: `BootShim` relocation finished, preparing to branch into PrePi.
3. **Yellow (40 rows at row 80)**: `PrePi` entry (`_ModuleEntryPoint`) in AArch64 EL1.
4. **Magenta (40 rows at row 120)**: Primary stack initialized and data/instruction caches enabled.
5. **PrePi Live Green (Rows 0-6)**: `PlatformInitialize()` completed, watchdog disabled, MDP5 configured.
6. **Orange (Rows 4-8)**: Platform HOBs & MMU translation initialized, LZMA decompression of `FVMAIN` starting.
7. **White (Rows 8-12)**: `FVMAIN` decompression completed, jumping to `DxeCore`.

---

## 5. Building and Booting

### Prerequisites
- Ubuntu 20.04+ or Debian 11+
- Cross-compiler: `aarch64-linux-gnu-gcc`
- Python 3, `dtc`, `uuid-dev`, `build-essential`

### Building UEFI
```bash
./build.sh --device onclite --release RELEASE
```
The resulting bootable image is generated at:
`Build/onclite/RELEASE_GCC5/boot.img`

### Booting via Fastboot
```bash
fastboot boot Build/onclite/RELEASE_GCC5/boot.img
```
