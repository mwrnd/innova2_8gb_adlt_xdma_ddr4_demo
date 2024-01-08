/*

Prerequisites:
 - Vivado PCIe XDMA and DDR4 AXI Demo Project:
   github.com/mwrnd/innova2_8gb_adlt_xdma_ddr4_demo
 - XDMA Drivers from github.com/xilinx/dma_ip_drivers
   Install Instructions at github.com/mwrnd/innova2_flex_xcku15p_notes


Compile with:

  gcc -Wall innova2_xdma_ddr4_test.c -o innova2_xdma_ddr4_test -lm

Run with:

  sudo ./innova2_xdma_ddr4_test

*/

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>




// The XDMA AXI aclk depends on PCIe Lane Width, AXI Data Width, and
// Maximum Link Speed settings in the XDMA IP Block
#define XDMA_CLK_MHz	250

// The PCIe to AXI Translation Offset for the PCIe to AXI-Lite Interface
#define XDMA_PCIe_to_AXI_Translation_Offset 0x00000000

// Using 2 mebibyte == 2^21 = 2097152 byte array. Size was defined in the
// Vivado FPGA Project Block Diagram Address Editor as the Data Range for BRAM
// On Linux, read/write can transfer at most 0x7FFFF000 (2,147,479,552) bytes
#define DATA_SIZE 2097152



// Global struct for XDMA Device files
struct _XDMA {
	char *userfilename;
	char *h2cfilename;
	char *c2hfilename;
	int userfd;
	int h2cfd;
	int c2hfd;
} xdma;





ssize_t read_axilite_word(uint64_t address, uint32_t *read_word)
{
	ssize_t rc = 0;

	rc = pread(xdma.userfd, read_word, 4, address);
	if (rc < 0) {
		fprintf(stderr, "%s, read data @ 0x%lX failed, %ld.\n",
			xdma.userfilename, address, rc);
		perror("File Read");
		return -EIO;
	}
	if (rc != 4) {
		fprintf(stderr, "%s, read underflow @ 0x%lX, read %ld/4.\n",
			xdma.userfilename, address, rc);
		perror("Read Underflow");
	}
	
	return rc;
}




ssize_t write_axilite_word(uint64_t address, uint32_t write_word)
{
	ssize_t rc;

	rc = pwrite(xdma.userfd, &write_word, 4, address);
	if (rc < 0) {
		fprintf(stderr, "%s, write data @ 0x%lX failed, %ld.\n",
			xdma.userfilename, address, rc);
		perror("File Write");
		return -EIO;
	}
	if (rc != 4) {
		fprintf(stderr, "%s, write underflow @ 0x%lX, %ld/4.\n",
			xdma.userfilename, address, rc);
		perror("Write Underflow");
	}

	return rc;
}




ssize_t read_from_axi(uint64_t address, size_t bytes, void *buffer)
{
	ssize_t rc = 0;

	// On Linux, read/write can transfer at most
	// 0x7FFFF000 (2,147,479,552) bytes at one time
	// TODO: Break larger reads into multiple reads
	if (bytes >= 0x7FFFF000) {
		fprintf(stderr, "%s, read overflow @ 0x%lX, %ld/%ld.\n",
			xdma.c2hfilename, address, rc, bytes);
		perror("Read Overflow");
		return -EIO;
	}


	rc = pread(xdma.c2hfd, buffer, bytes, address);
	//fsync(xdma.c2hfd);

	if (rc < 0) {
		fprintf(stderr, "%s, read data @ 0x%lX failed, %ld.\n",
			xdma.c2hfilename, address, rc);
		perror("File Read");
		return -EIO;
	}
	if (rc != bytes) {
		fprintf(stderr, "%s, read underflow @ 0x%lX, %ld/%ld.\n",
			xdma.c2hfilename, address, rc, bytes);
		perror("Read Underflow");
	}

	return rc;
}




ssize_t write_to_axi(uint64_t address, size_t bytes, void *buffer)
{
	ssize_t rc = 0;

	// On Linux, read/write can transfer at most
	// 0x7FFFF000 (2,147,479,552) bytes at one time
	// TODO: Break larger writes into multiple writes
	if (bytes >= 0x7FFFF000) {
		fprintf(stderr, "%s, write overflow @ 0x%lX, %ld/%ld.\n",
			xdma.h2cfilename, address, rc, bytes);
		perror("Write Overflow");
		return -EIO;
	}


	rc = pwrite(xdma.h2cfd, buffer, bytes, address);
	//fsync(xdma.h2cfd);

	if (rc < 0) {
		fprintf(stderr, "%s, write data @ 0x%lX failed, %ld.\n",
			xdma.h2cfilename, address, rc);
		perror("File Write");
		return -EIO;
	}
	if (rc != bytes) {
		fprintf(stderr, "%s, write underflow @ 0x%lX, %ld/%ld.\n",
			xdma.h2cfilename, address, rc, bytes);
		perror("Write Underflow");
	}

	return rc;
}




void axi_memory_test(uint64_t axi_addr, uint64_t size_in_bytes)
{
	float *wrte_data;
	float *read_data;
	struct timespec ts_start, ts_end;
	float bandwidth = 0;
	int errorcount = 0;
	ssize_t rc_w, rc_r;
	uint64_t numfloats = (size_in_bytes / 4); // will be transferring floats

	if (numfloats <= 1 ) {
		printf("ERROR: too little data to send: %ld bytes\n\n",
			size_in_bytes);
		return;
	}

	wrte_data = malloc(size_in_bytes);
	if (!(wrte_data)) {
		printf("ERROR: malloc failed for wrte_data\n");
		return;
	}
	read_data = malloc(size_in_bytes);
	if (!(read_data)) {
		printf("ERROR: malloc failed for read_data\n");
		free(wrte_data);
		return;
	}

	// Generate Random Data

	// Seed the random number generator with an arbitrary value: an address
	srandom((unsigned int)((long int)axi_memory_test));

	// Fill arrays; write (H2C) with random data and read (C2H) with 0s
	for (uint64_t indx = 0; indx < numfloats ; indx++)
	{
		wrte_data[indx] = rand();
		read_data[indx] = 0;
	}


	// start timing of the transfers
	clock_gettime(CLOCK_MONOTONIC, &ts_start);


	// Write the random data to the FPGA's AXI BRAM
	rc_w = write_to_axi(axi_addr, size_in_bytes, wrte_data);

	// Read data from the FPGA's AXI BRAM
	rc_r = read_from_axi(axi_addr, size_in_bytes, read_data);


	// end timing of the transfers
	clock_gettime(CLOCK_MONOTONIC, &ts_end);
	ts_end.tv_sec = abs(ts_end.tv_sec - ts_start.tv_sec);
	ts_end.tv_nsec = abs(ts_end.tv_nsec - ts_start.tv_nsec);
	printf("Wrote %ld bytes to   %s at address 0x%lX\n",
		rc_w, xdma.h2cfilename, axi_addr);
	printf("Read  %ld bytes from %s at address 0x%lX\n",
		rc_r, xdma.c2hfilename, axi_addr);
	printf("Data transfers took %ld.%09ld seconds for %ld floats.\n",
		ts_end.tv_sec, ts_end.tv_nsec, numfloats);
	bandwidth = (float)((((double)size_in_bytes) /
		(((double)((ts_end.tv_nsec))) / 1000000000.0)) /
		((double)(1024*1024)));
	printf("Bandwidth is approximately %f MB/s for %ld bytes.\n",
		bandwidth, size_in_bytes);


	// Compare the Written and Read Data
	errorcount = 0;
	for (int indx = 0; indx < numfloats ; indx++)
	{
		if (read_data[indx] != wrte_data[indx])
		{
			errorcount++;
			printf("Data did not match at index %d, ", indx);
			printf("read_data = 0x%08X, wrte_data = 0x%08X\n",
				(uint32_t)read_data[indx], (uint32_t)wrte_data[indx]);
		}

		// too many errors, something is wrong, do not check any more
		if (errorcount > 7 ) { break; }
	}

	if (errorcount == 0)
	{
		printf("Success - Read Data matches Written Data!\n");
	} else {
		printf("Too many errors encountered, something is wrong.\n");
	}

	printf("\n");

	free(wrte_data);
	free(read_data);
}




int estimate_clock_MHz(uint8_t delay, char *clkname, uint64_t axi_gpio_addr)
{
	uint8_t read_data[16];
	uint32_t val1 = 0;
	uint32_t val2 = 0;
	uint32_t val3 = 0;
	uint32_t val4 = 0;
	uint32_t diff_clkA;
	uint32_t diff_clkB;
	ssize_t rc;
	double clk_ratio = 0;


	if (axi_gpio_addr == 0) { return -1; }


	rc = read_from_axi(axi_gpio_addr, 16, &read_data);
	if (rc != 16) {return -1; }

	memcpy(&val1, &read_data[0], 4);
	memcpy(&val2, &read_data[8], 4);

	printf("\nt=0 clock counter values at 0x%lX : 0x%08X 0x%08X\n",
		axi_gpio_addr, val1, val2);


	sleep(delay);


	rc = read_from_axi(axi_gpio_addr, 16, &read_data);
	if (rc != 16) {return -1; }

	memcpy(&val3, &read_data[0], 4);
	memcpy(&val4, &read_data[8], 4);

	printf("t=%d clock counter values at 0x%lX : 0x%08X 0x%08X\n",
		delay, axi_gpio_addr, val3, val4);


	diff_clkA = abs(val3-val1);
	diff_clkB = abs(val4-val2);
	clk_ratio =(((double)diff_clkB)/((double)diff_clkA));

	printf("    Clock %s has ratio = %lf, estimate %lf MHz",
		clkname, clk_ratio, (clk_ratio * XDMA_CLK_MHz));

	if ((clk_ratio * XDMA_CLK_MHz) < 1) {
		printf(" = % 9.0lf Hz\n",
			(clk_ratio * (XDMA_CLK_MHz * 1000000)));
	} else {
		printf("\n");
	}

	return 0;
}




int main(int argc, char **argv)
{

	// Open M_AXI_LITE Device as Read-Write
	xdma.userfilename = "/dev/xdma0_user";

	xdma.userfd = open(xdma.userfilename, O_RDWR);

	if (xdma.userfd < 0) {
		fprintf(stderr, "unable to open device %s, %d.\n",
			xdma.userfilename, xdma.userfd);
		perror("File Open");
		exit(EXIT_FAILURE);
	}


	// Open M_AXI H2C Host-to-Card Device as Write-Only
	xdma.h2cfilename = "/dev/xdma0_h2c_0";

	xdma.h2cfd = open(xdma.h2cfilename, O_WRONLY);

	if (xdma.h2cfd < 0) {
		fprintf(stderr, "unable to open device %s, %d.\n",
			xdma.h2cfilename, xdma.h2cfd);
		perror("File Open");
		exit(EXIT_FAILURE);
	}


	// Open M_AXI C2H Card-to-Host Device as Read-Only
	xdma.c2hfilename = "/dev/xdma0_c2h_0";

	xdma.c2hfd = open(xdma.c2hfilename, O_RDONLY);

	if (xdma.c2hfd < 0) {
		fprintf(stderr, "unable to open device %s, %d.\n",
			xdma.c2hfilename, xdma.c2hfd);
		perror("File Open");
		exit(EXIT_FAILURE);
	}




	// -------- AXI Memory Tests -----------------------------------------

	// AXI BRAM Write then Read Test
	axi_memory_test(0x80000000, DATA_SIZE);

	// AXI DDR4 Write then Read Test 2^25=33554432
	axi_memory_test(0x000200000000, 33554432);




	// -------- AXI Lite GPIO LED Test -----------------------------------

	// Toggle LED once a second for 4 seconds
	// GPIO for LED is at AXI Address 0x90000 which is PCIe address
	// 0x90000 as XDMA_PCIe_to_AXI_Translation_Offset=0x00000000
	printf("Test GPIO LED:\n");
	uint64_t addr = 0x90000 - XDMA_PCIe_to_AXI_Translation_Offset;
	uint32_t write_word = 0;
	for (int i = 0; i < 5 ; i++)
	{
		write_word = (uint32_t)(i & 0x00000001);
		write_axilite_word(addr, write_word);

		if (write_word) {
			printf("LED D18 should be ON.\n");
		} else {
			printf("LED D18 should be OFF.\n");
		}

		sleep(1);
	}

	printf("\n");




	// -------- Estimate Frequency of non-XDMA Clocks --------------------

	printf("Compare clocks against %dMHz XDMA axi_aclk:", XDMA_CLK_MHz);

	uint8_t delay = 1;
	estimate_clock_MHz(delay, "sys_clk_100MHz", 0x70100000);
	estimate_clock_MHz(delay, "clk_100MHz", 0x70110000);
	estimate_clock_MHz(delay, "c0_ddr4_ui_clk", 0x70120000);
	estimate_clock_MHz(delay, "emc_clk_150MHz", 0x70140000);

	printf("\n");




	// -------- AXI Lite BRAM Test ---------------------------------------

	printf("\nWrite Values to AXI Lite BRAM:\n");
	addr = 0x80000 - XDMA_PCIe_to_AXI_Translation_Offset;
	write_axilite_word((addr+0x00), 0xA0A0A700); printf("  0xA0A0A700\n");
	write_axilite_word((addr+0x04), 0xA1A1A604); printf("  0xA1A1A604\n");
	write_axilite_word((addr+0x08), 0xA2A2A508); printf("  0xA2A2A508\n");
	write_axilite_word((addr+0x0C), 0xA3A3A40C); printf("  0xA3A3A40C\n");


	printf("\nRead Values from AXI Lite BRAM (should match to above):\n");
	uint32_t read_word = 0;
	addr = 0x80000 - XDMA_PCIe_to_AXI_Translation_Offset;
	// print the data in the return data buffer
	for (int i = 0 ; i < 16; i = i + 4)
	{
		read_axilite_word((addr+i), &read_word);

		printf("PCIe AXILite Address 0x%08lX=%ld data = %u = 0x%08X\n",
			(addr+i), (addr+i), read_word, read_word);
	}

	printf("\n");




	close(xdma.userfd);
	close(xdma.h2cfd);
	close(xdma.c2hfd);
	exit(EXIT_SUCCESS);
}

