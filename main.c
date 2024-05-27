/*
 * Copyright (c) 2024 Kontron Europe GmbH
 *
 * Author: Heiko Thiery <heiko.thiery@kontron.com>
 * Created: May 18, 2024
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bsl.h"
#include "common.h"
#include "script.h"


#ifndef VERSION
  #define VERSION "unrel"
#endif

#define DEFAULT_I2C_ADDR 0x48
char *o_device = NULL;
uint8_t o_i2c_address = DEFAULT_I2C_ADDR;

bool o_info = false;
bool o_erase = false;
bool o_no_script = false;
bool o_program = false;
char *o_fw_file;

int verbosity = 0;


static void error(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf(": %s\n", strerror(errno));
}


static int load_fw_image(const char *filename, uint8_t **_buf, size_t *_len)
{
	int fd;
	int rc = 0;
	ssize_t ret;
	off_t len, ret2;
	size_t len_pad;
	uint8_t *buf;

	assert(_buf);
	assert(_len);

	DEBUG(0, "opening %s\n", filename);
	if ((fd = open(filename, O_RDONLY)) == -1)
	{
		error("open(%s) failed", filename);
		rc = errno;
		goto err;
	}

	/* determine file length */
	len = lseek(fd, 0, SEEK_END);
	assert(len != (off_t)-1);
	ret2 = lseek(fd, 0, SEEK_SET);
	assert(ret2 != (off_t)-1);

	DEBUG(0, "image_size=%ju\n", (intmax_t)len);

	if (len == 0) {
		printf("ERROR: empty file specified\n");
		rc = EIO;
		goto err_close;
	}

	/* pad len to 4k boundary */
	len_pad = (len + 4095) & (~0xfff);

	DEBUG(1, "image_size_padded=%zu\n", len_pad);

	buf = malloc(len_pad);
	assert(buf);
	memset(buf, 0xff, len_pad);

	ret = read(fd, buf, len);
	if (ret == -1) {
		error("read(%s) failed", filename);
		free(buf);
		rc = errno;
		goto err_close;
	}

	if (ret != len) {
		printf("ERROR: truncated read\n");
		free(buf);
		rc = EIO;
		goto err_close;
	}

	*_len = len;
	*_buf = buf;

err_close:
	close(fd);
err:

	return rc;
}


static void usage(char* self)
{
    printf(
"Usage: %s [options] <CMD> <fw-bin-file>\n"
"\n"
"  Flash and verify firmware binary to a TI MSPM0L microcontroller.\n"
"\n"
"  Options:\n"
"  -a I2C address,         Using given I2C_ADDRESS for communication\n"
"                          (default 0x48)\n"
"\n"
"  -d DEVICE,              Using given DEVICE for communication.\n"
"\n"
"  -n  --no-script         Do not execute init/exit script.\n"
"\n"
"  -v  --verbose           Increase verbosity, can be set multiple times.\n"
"\n"
"  -V, --version           Display program version and exit.\n"
"\n"
"  -h, --help              Display this help and exit.\n"
"\n"
"  CMD:\n"
"    prog <fw-bin-file>   Program the firmware data.\n"
"    info                 Display the device info.\n"
"    erase                Erase the full flash.\n"
"\n",
        self);
}


int cmd_erase(int fd, uint8_t i2c_address)
{
	if (bsl_connect(fd, i2c_address) != 0) {
		printf("ERROR: connect\n");
		return -1;
	}

	if (bsl_unlock_bootloader(fd, i2c_address) != 0) {
		printf("ERROR: unlock device\n");
		return -1;
	}

	if (bsl_mass_erase(fd, i2c_address) != 0) {
		printf("ERROR: mass erase device\n");
		return -1;
	}

	return 0;
}


int cmd_info(int fd, uint8_t i2c_address)
{
	struct device_info info;

	if (bsl_connect(fd, i2c_address) != 0) {
		printf("ERROR: connect\n");
		return -1;
	}

	if (bsl_get_device_info(fd, i2c_address, &info) != 0) {
		printf("ERROR: Get Device info\n");
		return -1;
	}

	printf("CMD interpreter version:    0x%04x\n", info.version);
	printf("Build ID:                   0x%04x\n", info.build_id);
	printf("Application Version::       0x%08x\n", info.app_version);
	printf("Plug-in interface Version:  0x%04x\n", info.interface_version);
	printf("BSL max buffer size:        0x%04x\n", info.bsl_max_buffer_size);
	printf("BSL buffer start address:   0x%08x\n", info.bsl_buffer_start);
	printf("BCR configuration ID:       0x%08x\n", info.bcr_config_id);
	printf("BSL configuration ID:       0x%08x\n", info.bsl_config_id);

	return 0;
}


int cmd_prog(int fd, uint8_t i2c_address, char *filename)
{
	int rc = 0;
	size_t write_len;
	uint8_t *p;
	uint8_t *fw_buf = NULL;
    size_t total_len = 0;
	size_t len;
	uint32_t address;
	uint32_t pad_len;
	uint32_t crc_file;
	uint32_t crc_bsl;

	if (load_fw_image(filename, &fw_buf, &total_len) != 0) {
		return -1;
	}

	if (bsl_connect(fd, i2c_address) != 0) {
		printf("ERROR: connect\n");
		goto out_free;
	}

	printf("UNLOCK .. ");
	if (bsl_unlock_bootloader(fd, i2c_address) != 0) {
		printf("ERROR: unlock device\n");
		goto out_free;
	}
	printf("OK\n");

	printf("ERASE .. ");
	if (bsl_mass_erase(fd, i2c_address) != 0) {
		printf("ERROR: mass erase device\n");
		goto out_free;
	}
	printf("OK\n");


	p = fw_buf;
	address = 0;
	len = total_len;
	printf("FLASH ..");
	fflush(stdout);
	while (len > 0) {
		if (len > 256) {
			write_len = 256;
		} else {
			write_len = len;
		}

		if (bsl_program_data(fd, i2c_address, address, p, write_len) != 0) {
			printf("ERROR: program data\n");
			goto out_free;
		}

		usleep(100);
		printf(".");
		fflush(stdout);

		p = p+write_len;
		len -= write_len;
		address += write_len;
	}
	printf(" OK\n");

	printf("VERIFY .. ");
	/* BSL only supports calculating 1k blocks */
	pad_len = (total_len+1023) & (~0x3ff);

	if (bsl_verification(fd, i2c_address, 0, pad_len, &crc_bsl) != 0) {
		printf("ERROR: bsl_verification\n");
		goto out_free;
	}

	crc_file = crc32(fw_buf, pad_len);

	if (crc_file != crc_bsl) {
		printf("FAIL\n");
		rc = -1;
		goto out_free;
	}
	printf("OK\n");

out_free:
	free(fw_buf);

	return rc;
}


static void version()
{
	printf("%s\n", VERSION);
}


static struct option bsl_options[] = {
 	{ "address",    required_argument,  NULL,   'a'},
 	{ "device",     required_argument,  NULL,   'a'},
	{ "no-script",  no_argument,        NULL,   'n'},
 	{ "version",    no_argument,        NULL,   'V'},
 	{ "verbose",    no_argument,        NULL,   'v'},
 	{ "help",       no_argument,        NULL,   'h'},
 	{ 0, 0, 0, 0 }
};


int main(int argc, char **argv)
{
	int rc = -1;
	int opt;
	int fd;

	while ((opt = getopt_long(argc, argv, "a:d:hnvV",
			bsl_options, NULL))!= -1) {
		switch (opt) {
			case 'a':
				o_i2c_address = atoi(optarg);
				break;
			case 'd':
				o_device = optarg;
				break;
			case 'h':
				usage(argv[0]);
				exit(0);
				break;
			case 'n':
				o_no_script = true;
				break;
			case 'V':
				version();
				exit(0);
				break;
			case 'v':
				verbosity++;
				break;
			default:
				fprintf(stderr, "Usage: %s [-ilw] [file...]\n", argv[0]);
				usage(argv[0]);
				exit(EXIT_FAILURE);
        }
    }

	if ((argc-optind) < 1) {
		usage(argv[0]);
		printf("ERROR: CMD is missing\n");
		exit(1);
	}

	if (!strcmp(argv[optind], "info")) {
		o_info = true;
	} else if (!strcmp(argv[optind], "erase")) {
		o_erase = true;
	} else if (!strncmp(argv[optind], "prog", 4)) {
		o_program = true;
		if ((argc - optind) < 2) {
			usage(argv[0]);
			printf("ERROR: fw-bin-file is missing\n");
			exit(1);
		}
		o_fw_file = argv[optind+1];
	} else {
		usage(argv[0]);
			printf("ERROR: unsupported CMD %s\n", argv[optind]);
		exit(1);
	}

	if ((fd = open(o_device, O_RDWR)) < 0) {
		printf("ERROR: cannot open device %s\n", o_device);
		return -1;
	}

	if (!o_no_script) {
		rc = script_init();
		if (rc) {
			printf("ERROR: script init\n");
			goto out_close;
		}
	}

	if (o_erase) {
		rc = cmd_erase(fd, o_i2c_address);
	} else if (o_info) {
		rc = cmd_info(fd, o_i2c_address);
	} else if (o_program) {
		rc = cmd_prog(fd, o_i2c_address, o_fw_file);
	}

	if (!o_no_script) {
		script_exit();
	}

out_close:
	close(fd);

	return rc;
}
