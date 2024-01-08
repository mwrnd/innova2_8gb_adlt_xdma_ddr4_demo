# Innova-2 Flex XCKU15P XDMA PCIe DDR4 GPIO Demo

This is a [Vivado 2023.2](https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/vivado-design-tools/2023-2.html) starter project for the [XCKU15P FPGA](https://www.xilinx.com/products/silicon-devices/fpga/kintex-ultrascale-plus.html) on the [Innova-2 Flex SmartNIC MNV303212A-ADLT](https://www.nvidia.com/en-us/networking/ethernet/innova-2-flex/). It is a superset of the [`innova2_xdma_demo`](https://github.com/mwrnd/innova2_xdma_demo) project that adds DDR4.

Refer to the [innova2_flex_xcku15p_notes](https://github.com/mwrnd/innova2_flex_xcku15p_notes/) project for instructions on setting up an Innova-2 system with all drivers including [Xilinx's PCIe XDMA Drivers](https://github.com/Xilinx/dma_ip_drivers).

Refer to [this tutorial](https://github.com/mwrnd/notes/tree/main/Vivado_XDMA_DDR4_Tutorial) for detailed instructions on generating a similar project from scratch.

There is also a [version for the 4GB `MNV303212A-ADIT`/`MNV303212A-ADAT` variant available](https://github.com/mwrnd/innova2_4gb_adit_xdma_ddr4_demo).




# Block Design

![Block Design Diagram](img/xdma_ddr4_Block_Diagram.png)




# AXI Addresses

| Block                      | Address (Hex) | Size  |
| -------------------------- |:-------------:| :---: |
| `M_AXI` `BRAM_CTRL_0`      |  0x80000000   |   2M  |
| `M_AXI` `GPIO_0`           |  0x70100000   |  64K  |
| `M_AXI` `GPIO_1`           |  0x70110000   |  64K  |
| `M_AXI` `GPIO_2`           |  0x70120000   |  64K  |
| `M_AXI` `GPIO_4`           |  0x70140000   |  64K  |
| `M_AXI` `DDR4_0`           | 0x200000000   |   8G  |
| `M_AXI` `DDR4_CTRL_0`      |  0x70200000   |   1M  |
| `M_AXI_LITE` `BRAM_CTRL_1` |  0x00080000   |   8K  |
| `M_AXI_LITE` `GPIO_3`      |  0x00090000   |  64K  |

![AXI Addresses](img/xdma_adlt_ddr4_demo_Addresses.png)




# Table of Contents

 * [Program the Design into the XCKU15P Configuration Memory](#program-the-design-into-the-xcku15p-configuration-memory)
 * [Testing the Design](#testing-the-design)
    * [lspci](#lspci)
    * [AXI BRAM Communication](#axi-bram-communication)
    * [AXI GPIO Control](#axi-gpio-control)
    * [DDR4 Communication and Throughput](#ddr4-communication-and-throughput)
       * [Test DDR4 Correct Data Retention](#test-ddr4-correct-data-retention)
       * [DDR4 Communication Error](#ddr4-communication-error)
    * [Custom Software for Accessing AXI Blocks](#custom-software-for-accessing-axi-blocks)
 * [Recreating the Design in Vivado](#recreating-the-design-in-vivado)
 * [Block Design Customization Options](#block-design-customization-options)
    * [XDMA](#xdma)
    * [DDR4](#ddr4)




## Program the Design into the XCKU15P Configuration Memory

Refer to the [`innova2_flex_xcku15p_notes`](https://github.com/mwrnd/innova2_flex_xcku15p_notes) project's instructions on [Loading a User Image](https://github.com/mwrnd/innova2_flex_xcku15p_notes/#loading-a-user-image). Binary Memory Configuration Bitstream Files are included in this project's [Releases](https://github.com/mwrnd/innova2_8gb_adlt_xdma_ddr4_demo/releases).

```
wget https://github.com/mwrnd/innova2_8gb_adlt_xdma_ddr4_demo/releases/download/v0.2/innova2_8gb_adlt_xdma_ddr4_demo_bitstream.zip
unzip innova2_8gb_adlt_xdma_ddr4_demo_bitstream.zip
md5sum *bin
echo fd2fe52b344f46725ca083e4a108b6f8 should be md5sum of innova2_8gb_adlt_xdma_ddr4_demo_primary.bin
echo cfe2edd5c91cb6d7f41d00969b0041be should be md5sum of innova2_8gb_adlt_xdma_ddr4_demo_secondary.bin
echo 90055e5b1a28b98dea2f6703b68040fd should be md5sum of xdma_wrapper.bit
```




## Testing the Design

### lspci

After programming the bitstream and rebooting, the design should show up as `Memory controller: Xilinx Corporation Device 9038`. It shows up at PCIe Bus Address `03:00` for me.
```
lspci -d 10ee:
```

![lspci Xilinx Devices](img/lspci_d_10ee_Xilinx_Devices.png)

The following [lspci](https://manpages.ubuntu.com/manpages/jammy/man8/lspci.8.html) commands list all Mellanox and Xilinx devices and show their relation.
```
lspci -nn | grep "Mellanox\|Xilinx"
lspci -tv | grep "0000\|Mellanox\|Xilinx"
```

![lspci Xilinx and Mellanox Devices](img/lspci_view_of_innova2_FPGA.jpg)

The FPGA is attached to a PCIe Bridge (`02:08.0`), as are the two Ethernet Controllers (`02:10.0`).
```
01:00.0 PCI bridge [0604]: Mellanox Technologies MT28800 Family [ConnectX-5 PCIe Bridge] [15b3:1974]
02:08.0 PCI bridge [0604]: Mellanox Technologies MT28800 Family [ConnectX-5 PCIe Bridge] [15b3:1974]
02:10.0 PCI bridge [0604]: Mellanox Technologies MT28800 Family [ConnectX-5 PCIe Bridge] [15b3:1974]
03:00.0 Memory controller [0580]: Xilinx Corporation Device [10ee:9038]
04:00.0 Ethernet controller [0200]: Mellanox Technologies MT27800 Family [ConnectX-5] [15b3:1017]
04:00.1 Ethernet controller [0200]: Mellanox Technologies MT27800 Family [ConnectX-5] [15b3:1017]

-[0]-+-00.0 Intel Corporation Device 3e0f
     +-1d.0-[01-04]----00.0-[02-04]--+-08.0-[03]----00.0 Xilinx Corporation Device 9038
     |                               \-10.0-[04]--+-00.0 Mellanox Technologies MT27800 Family [ConnectX-5]
     |                                            \-00.1 Mellanox Technologies MT27800 Family [ConnectX-5]
```

The current PCIe Link status is useful. Note this is the FPGA to ConnectX-5 PCIe Bridge link.
```
sudo lspci -nnvd 10ee:  ;  sudo lspci -nnvvd 10ee: | grep Lnk
```

![lspci Link Status](img/lspci_Lnk.png)

`dmesg | grep -i xdma` provides details on how Xilinx's PCIe XDMA driver has loaded.

![dmesg xdma](img/dmesg_xdma.jpg)




### AXI BRAM Communication

The [XDMA driver from dma_ip_drivers](https://github.com/Xilinx/dma_ip_drivers/tree/master/XDMA/linux-kernel) creates [character device files](https://en.wikipedia.org/wiki/Device_file#Character_devices) for access to an AXI Bus. For DMA transfers to **M_AXI** blocks, `/dev/xdma0_h2c_0` is Write-Only and `/dev/xdma0_c2h_0` is Read-Only. To read from an AXI Block at address `0x12345600` you would read from address `0x12345600` of the `/dev/xdma0_c2h_0` (Card-to-Host) file. To write you would write to the appropriate address of the `/dev/xdma0_h2c_0` (Host-to-Card) file. For single word (32-Bit) register-like reads and writes to **M_AXI_LITE** blocks, `/dev/xdma0_user` is Read-Write.

The commands below generate 2MB of random data, then send it to a URAM Block (`0x80000000`) in the XCKU15P, then read it back and confirm the data is identical.
```Shell
cd dma_ip_drivers/XDMA/linux-kernel/tools/
dd if=/dev/urandom bs=256 count=8192 of=TEST
sudo ./dma_to_device   --verbose --device /dev/xdma0_h2c_0 --address 0x80000000 --size 2097152  -f    TEST
sudo ./dma_from_device --verbose --device /dev/xdma0_c2h_0 --address 0x80000000 --size 2097152 --file RECV
md5sum TEST RECV
```

![XDMA BRAM Test](img/XDMA_Tools_BRAM_Test.png)




### AXI GPIO Control

![User LED](img/User_LEDs_on_Innova2.jpg)

The design includes an [AXI GPIO](https://docs.xilinx.com/v/u/3.0-English/ds744_axi_gpio) block to control Pin *B6*, the **D18** LED on the back of the Innova-2. The LED control is inverted on the board so the design includes a signal inverter. The LED can be turned on by writing a `0x01` to the `GPIO_DATA` Register. Only a single bit is enabled in the port so excess bit writes are ignored. No direction control writes are necessary as the port is set up for output-only (the `GPIO_TRI` Direction Control Register is fixed at `0xffffffff`).

![AXI GPIO](img/AXI_GPIO.png)

The LED GPIO Block is connected to the **M_AXI_LITE** port so access to it is via 32-bit=1-word reads and writes to the **/dev/xdma0_user** file using the `reg_rw` utility from `dma_ip_drivers`. The commands below should turn on then turn off the *D18* LED in between reads of the GPIO register.
```Shell
cd dma_ip_drivers/XDMA/linux-kernel/tools/
sudo ./reg_rw /dev/xdma0_user 0x90000 w
sudo ./reg_rw /dev/xdma0_user 0x90000 w 0x0001
sudo ./reg_rw /dev/xdma0_user 0x90000 w
sudo ./reg_rw /dev/xdma0_user 0x90000 w 0x0000
sudo ./reg_rw /dev/xdma0_user 0x90000 w
```

![XDMA GPIO Testing](img/XDMA_AXI-Lite_Access.png)




### DDR4 Communication and Throughput

The [DDR4 Controller](https://docs.xilinx.com/v/u/en-US/pg150-ultrascale-memory-ip) prevents data reads from uninitialized memory. DDR4 must first be written before it can be read.

Your system must have enough free memory to test DDR4 DMA transfers. Run `free -m` to determine how much RAM you have available and keep the amount of data to transfer below that. The commands below generate 512MB of random data then transfer it to and from the Innova-2. The address of the DDR4 is `0x200000000` as noted earlier.

The [`dd`](https://manpages.ubuntu.com/manpages/trusty/man1/dd.1.html) command is used to generate a file (`of=DATA`) from pseudo-random data ([`if=/dev/urandom`](https://manpages.ubuntu.com/manpages/focal/en/man4/random.4.html)). The value for Block Size (`bs`) will be multiplied by the value for `count` to produce the size in bytes of the output file. For example, `8192*65536=536870912=0x20000000=512MiB`. Use a block size (`bs=`) that is a multiple of your drive's block size. `df .` informs you on which drive your current directory is located. `dumpe2fs` will tell you the drive's block size.

```Shell
df .
sudo dumpe2fs /dev/sda3 | grep "Block size"
```

![Determine SSD or Hard Drive Block Size](img/df_dumpe2fs_Determine_Block_Size.png)

To test the full 8GB of memory you can increment the address by the data size enough times that all `8Gib = 8589934592 = 0x200000000` has been tested.

If you have 8GB+ of free memory space, generate 8GB of random data with the `dd` command options `bs=8192 count=1048576` and test the DDR4 in one go.

`dd` uses decimal numbers. Convert from hexadecimal using [`printf`](https://manpages.ubuntu.com/manpages/trusty/man1/printf.1.html).

If checksums do not match, [`vbindiff DATA RECV`](https://manpages.ubuntu.com/manpages/jammy/man1/vbindiff.1.html) can be used to determine differences between the sent and received data and the failing address locations.

Note that data is loaded from your system drive into memory then sent to the Innova-2 DDR4 over PCIe DMA. Likewise it is loaded from the Innova-2's DDR4 into system RAM, then onto disk. The wall time of these functions can therefore be significantly longer than the DMA Memory-to-Memory over PCIe transfer time.
```Shell
free -m
dd if=/dev/urandom bs=8192 count=65536 of=DATA
printf "%ld\n" 0x200000000
sudo dd if=DATA of=/dev/xdma0_h2c_0 count=1 bs=536870912 seek=8589934592 oflag=seek_bytes
sudo dd if=/dev/xdma0_c2h_0 of=RECV count=1 bs=536870912 skip=8589934592 iflag=skip_bytes
md5sum DATA RECV
```

![DDR4 dd Random Data Test](img/DDR4_dd_Test_With_Random_Data.png)

The tools from `dma_ip_drivers` can also be used to run the same test:
```
cd dma_ip_drivers/XDMA/linux-kernel/tools  
free -m
dd if=/dev/urandom bs=8192 count=65536 of=DATA
sudo ./dma_to_device   --verbose --device /dev/xdma0_h2c_0 --address 0x200000000 --size 536870912 -f     DATA
sudo ./dma_from_device --verbose --device /dev/xdma0_c2h_0 --address 0x200000000 --size 536870912 --file RECV
md5sum DATA RECV
```

![DDR4 XDMA Tools Random Data Test](img/DDR4_XDMA_Tools_Test_With_Random_Data.png)




#### Test DDR4 Correct Data Retention


Test the first `1GB = 1073741824 bytes` of the DDR4 memory space using a binary all-zeros file.
```
cd dma_ip_drivers/XDMA/linux-kernel/tools/
dd if=/dev/zero of=DATA bs=8192 count=131072
printf "%ld\n" 0x200000000
sudo dd if=DATA of=/dev/xdma0_h2c_0 count=1 bs=1073741824 seek=8589934592 oflag=seek_bytes
sudo dd if=/dev/xdma0_c2h_0 of=RECV count=1 bs=1073741824 skip=8589934592 iflag=skip_bytes
md5sum DATA RECV
```

![Test DDR4 With All-Zeros File](img/DDR4_dd_Test_With_All-Zeros_Data.png)

Test the first `1GB = 1073741824 bytes` of the DDR4 memory space using a binary [all-ones file](https://stackoverflow.com/questions/10905062/how-do-i-get-an-equivalent-of-dev-one-in-linux).
```
cd dma_ip_drivers/XDMA/linux-kernel/tools/
tr '\0' '\377' </dev/zero | dd of=DATA bs=8192 count=131072 iflag=fullblock
printf "%ld\n" 0x200000000
sudo dd if=DATA of=/dev/xdma0_h2c_0 count=1 bs=1073741824 seek=8589934592 oflag=seek_bytes
sudo dd if=/dev/xdma0_c2h_0 of=RECV count=1 bs=1073741824 skip=8589934592 iflag=skip_bytes
md5sum DATA RECV
```

![Test DDR4 With All-Ones File](img/DDR4_dd_Test_With_All-Ones_Data.png)




#### DDR4 Communication Error

If you attempt to send data to the DDR4 address but get `write file: Unknown error 512` it means DDR4 did not initialize properly or the AXI bus has encountered an error and stalled. Reboot and try again. If that fails, proceed to the [Innova-2 DDR4 Troubleshooting](https://github.com/mwrnd/innova2_ddr4_troubleshooting) project.
```Shell
sudo ./dma_to_device --verbose --device /dev/xdma0_h2c_0 --address 0x0 --size 8192 -f TEST
```

![Error 512](img/XDMA_DDR4_Communication_Failure_Error_512.png)




### Custom Software for Accessing AXI Blocks

[`pread`/`pwrite`](https://manpages.ubuntu.com/manpages/jammy/en/man2/pread.2.html) combine [`lseek`](https://manpages.ubuntu.com/manpages/jammy/en/man2/lseek.2.html) and [`read`/`write`](https://manpages.ubuntu.com/manpages/jammy/en/man2/read.2.html). Note the Linux Kernel has a [write limit](https://manpages.ubuntu.com/manpages/focal/en/man2/write.2.html) of `0x7FFFF000=2147479552` bytes per call.
```C
#include <unistd.h>

ssize_t pread(int fd, void *buf, size_t count, off_t offset);
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);
```

[innova2_xdma_ddr4_test.c](innova2_xdma_ddr4_test.c) is a simple program that demonstrates XDMA communication in [C](https://en.wikipedia.org/wiki/C_(programming_language)) using [`pread` and `pwrite`](https://manpages.ubuntu.com/manpages/jammy/en/man2/pread.2.html) to communicate with AXI Blocks.

```
gcc -Wall innova2_xdma_ddr4_test.c -o innova2_xdma_ddr4_test -lm
sudo ./innova2_xdma_ddr4_test
```

![innova2_xdma_test.c Run](img/innova2_xdma_ddr4_test_Run.png)




## Recreating the Design in Vivado

Run the `source` command from the main Vivado **2023.2** window.

```
cd innova2_8gb_adlt_xdma_ddr4_demo
dir
source innova2_8gb_adlt_xdma_ddr4_demo.tcl
```

![Source Project Files](img/Vivado_source_Project_Tcl.png)

Click on *Generate Bitstream*.

![Generate Bitstream](img/Vivado_Generate_Bitstream.png)

Once the Bitstream is generated, run **Write Memory Configuration File**, select **bin**, **mt25qu512_x1_x2_x4_x8**, **SPIx8**, **Load bitstream files**, and a location and name for the output binary files. The bitstream will end up in the `innova2_8gb_adlt_xdma_ddr4_demo/innova2_8gb_adlt_xdma_ddr4_demo.runs/impl_1` directory as `xdma_wrapper.bit`. Vivado will add the `_primary.bin` and `_secondary.bin` extensions as the Innova-2 uses dual MT25QU512 FLASH ICs in x8 for high speed programming.

![Write Memory Configuration File](img/Vivado_Write_Memory_Configuration_File.png)

Proceed to [Loading a User Image](https://github.com/mwrnd/innova2_flex_xcku15p_notes/#loading-a-user-image)




### Resource Utilization

Design run details:

![Design Run Output](img/xdma_ddr4_demo_Design_Run.png)

Resource Utilization Chart:

![Resource Utilization Chart](img/xdma_adlt_ddr4_demo_Resource_Utilization.png)




## Block Design Customization Options

### XDMA

The Innova-2's XCKU15P is wired for **x8** PCIe at *PCIe Block Location:* **X0Y2**. It is capable of **8.0 GT/s** Link Speed.

![XDMA Basic Customizations](img/XDMA_Customization_Options-Basic.png)

For this design I set the PCIe *Base Class* to **Memory Controller** and the *Sub-Class* to **Other**.

![XDMA PCIe ID Customizations](img/XDMA_Customization_Options-PCIe_ID.png)

I disable the **Configuration Management Interface**.

![XDMA Misc Customizations](img/XDMA_Customization_Options-Misc.png)




### DDR4

The DDR4 Memory Part is selected as `MT40A1G16WBU-083E` which is compatible with the [MT40A1G16KNR-075](https://media-www.micron.com/-/media/client/global/documents/products/data-sheet/dram/ddr4/ddr4_16gb_x16_1cs_twindie.pdf) x16 Twin Die ICs with [`D9WFR` FBGA Code](https://www.micron.com/support/tools-and-utilities/fbga?fbga=D9WFR#pnlFBGA) on the Innova-2.

![DDR4 Memory Part](img/DDR4_Settings-Memory_Part.png)

The DDR4 is configured for a Memory Speed of **833**ps = 1200MHz = 2400 MT/s Transfer Rate. The DDR4 reference clock is **9996**ps = 100.04MHz. [CAS Latency](https://en.wikipedia.org/wiki/CAS_latency) is set to **16** and CAS Write Latency is set to **12**.

![DDR4 Basic Configuration](img/DDR4_Settings-Basic.png)

*Data Mask and DBI* is set to **NO DM NO DBI** which automatically enables ECC on a 72-Bit interface.

![When is ECC Enabled](img/DDR4_72-Bit_When_Is_ECC_Enabled.png)

The *Arbitration Scheme* is set to **RD PRI REG** under AXI Options.

![DDR4 AXI Configuration](img/DDR4_Customization_Options-AXI_Options.png)




