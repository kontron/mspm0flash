/*
 * Copyright (c) 2024 Kontron Europe GmbH
 *
 * Author: Heiko Thiery <heiko.thiery@kontron.com>
 * Created: May 18, 2024
 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/select.h>

#include "bsl.h"
#include "common.h"

extern int verbosity;

static void dump_data(char *prefix, uint8_t *buf, int len)
{
	if (!verbosity) {
		return;
	}

	printf("%s\n", prefix);
	for (int i=0; i<len; i++) {
		printf("0x%02x ", buf[i]);
		if ((i+1)%16 == 0) {
			printf("\n");
		}
	}
	printf("\n");
}

static int i2c_write_read(int fd, uint8_t addr, uint8_t *tx, uint32_t write_len,
		uint8_t *rx, uint32_t read_len)
{
	struct i2c_msg message;
	struct i2c_rdwr_ioctl_data packets;

	if (ioctl(fd, I2C_SLAVE_FORCE, addr) < 0) {
		int error_code = errno;
		printf("ioctl(I2C_SLAVE) failed and returned errno %s \n",
				strerror(error_code));
	}

	memset(&message, 0, sizeof(message));
	memset(&packets, 0, sizeof(packets));

	/* setup write message */
	message.addr = addr;
	message.flags = 0;
	message.len = write_len;
	message.buf = tx;

	packets.msgs = &message;
	packets.nmsgs = 1;

	if (ioctl(fd, I2C_RDWR, &packets) < 0) {
		int error_code = errno;
		printf("%s: ioctl(I2C_RDWR) write failed and returned errno %s \n",
				__func__, strerror(error_code));
		return 1;
	}

	memset(&message, 0, sizeof(message));
	memset(&packets, 0, sizeof(packets));

	/* setup read message */
	message.addr = addr;
	message.flags = I2C_M_RD;
	message.len = read_len;
	message.buf = rx;

	packets.msgs = &message;
	packets.nmsgs = 1;

	if (ioctl(fd, I2C_RDWR, &packets) < 0) {
		int error_code = errno;
		printf("%s: ioctl(I2C_RDWR) read failed and returned errno %s \n",
				__func__, strerror(error_code));
		return 1;
	}

	return 0;
}

static int uart_write_read(int fd, uint8_t *tx, uint32_t write_len,
		uint8_t *rx, uint32_t read_len)
{
	struct timeval tv;
	int rc;
	uint32_t idx = 0;
	int cnt;
	fd_set fds;
	long timeout_ms = 500;

	rc = write(fd, tx, write_len);
	assert(rc != -1);

	while (idx < read_len) {
		int n;
		FD_ZERO(&fds);
		FD_SET(fd, &fds);

		tv.tv_sec = timeout_ms / 1000;
		tv.tv_usec = (timeout_ms % 1000) * 1000;

		n = select(fd+1, &fds, NULL, NULL, &tv);
		assert(n >= -1 && n <= 1);

		if (n == -1) {
			perror("select() failed");
			return 1;
		} else if (n == 0) {
			/* timeout */
			DEBUG(0, "timeout\n");
			return EIO;
		} else if (n == 1) {
			cnt = read(fd, rx + idx, read_len - idx);
			assert(cnt > 0);
			idx += cnt;
			DEBUG(2, "received %d bytes\n", cnt);
		} else {
			perror("should not happen");
			return 1;
		}
	}

	return 0;
}

static int bsl_write_read(struct bsl_intf *intf,
		uint8_t *tx, uint32_t write_len, uint8_t *rx, uint32_t read_len)
{
	int rc = -1;

	switch (intf->type) {
		case INTERFACE_TYPE_I2C:
			rc = i2c_write_read(intf->fd, intf->i2c_address,
						tx, write_len, rx, read_len);
			break;
		case INTERFACE_TYPE_UART:
			rc = uart_write_read(intf->fd, tx, write_len, rx, read_len);
			break;
	}

	return rc;
}

#define POLY 0xEDB88320
uint32_t crc32(uint8_t *buf, int len)
{
	uint32_t crc = 0xFFFFFFFF;
	uint32_t mask;

	for (int i=0; i<len; i++) {
		crc = crc ^ buf[i];

		for (int j=0; j<8; j++) {
			mask = -(crc & 1);
			crc = (crc >> 1) ^ (POLY & mask);
		}
	}
	return crc;
}

static void add_crc(uint8_t *data, int len)
{
	int core_data_len;
	uint32_t crc;

	assert(len > 3);

	core_data_len = data[1] | data[2] << 8;

	crc = crc32(&data[3], core_data_len);

	//assert(len > (3 + core_data_len + 3));

	data[3 + core_data_len + 0] = (crc >> 0) & 0xff;
	data[3 + core_data_len + 1] = (crc >> 8) & 0xff;
	data[3 + core_data_len + 2] = (crc >> 16) & 0xff;
	data[3 + core_data_len + 3] = (crc >> 24) & 0xff;
}

static int check_bsl_acknowledgement(uint8_t ack)
{
	if (ack != BSL_ACK) {
		switch (ack) {
			case BSL_ERROR_HEADER_INCORRECT:
				printf("BSL_ERROR_HEADER_INCORRECT\n");
				break;
			case BSL_ERROR_CHECKSUM_INCORRECT:
				printf("BSL_ERROR_CHECKSUM_INCORRECT\n");
				break;
			case BSL_ERROR_PACKET_SIZE_ZERO:
				printf("BSL_ERROR_PACKET_SIZE_ZERO\n");
				break;
			case BSL_ERROR_PACKET_SIZE_TOO_BIG:
				printf("BSL_ERROR_PACKET_SIZE_TOO_BIGn\n");
				break;
			case BSL_ERROR_UNKNOWN_ERROR:
				printf("BSL_ERROR_UNKNOWN_ERROR\n");
				break;
			case BSL_ERROR_UNKNOWN_BAUD_RATE:
				printf("BSL_ERROR_UNKNOWN_BAUD_RATE\n");
				break;
			default:
				printf("ERROR: acknowledge 0x%02x\n", ack);
				break;
		}
		return 1;
	}
	return 0;
}

static int check_bsl_response(uint8_t *buffer, int len)
{
	if (len == 0) {
		return 1;
	}

	if ( check_bsl_acknowledgement(buffer[0]) ) {
		return 1;
	}

	/* BSL Core Message Header */
	if (buffer[1] != 0x08) {
		printf("invalid response header\n");
		return 1;
	}

	if (buffer[4] == BSL_CORE_RSP_MESSAGE
			&& buffer[5] != BSL_CORE_MSG_OPERATION_SUCCESSFUL) {
		switch (buffer[5]) {
			case BSL_CORE_MSG_BSL_LOCKED_ERROR:
				printf("Incorrect password sent to unlock bootloader\n");
				return 1;
			case BSL_CORE_MSG_BSL_PASSWORD_ERROR:
				return 1;
			case BSL_CORE_MSG_MULTIPLE_BSL_PASSWORD_ERROR:
				return 1;
			case BSL_CORE_MSG_UNKNOWN_COMMAND:
				printf("Unknown command\n");
				return 1;
			case BSL_CORE_MSG_INVALID_MEMORY_RAMGE:
				printf("The given memory range is invalid\n");
				return 1;
			case BSL_CORE_MSG_INVALID_COMMAND:
				return 1;
			case BSL_CORE_MSG_FACTORY_RESET_DISABLED:
				printf("Factory reset is disabled in the BCR configuration\n");
				return 1;
			case BSL_CORE_MSG_FACTORY_RESET_PASSWORD_ERROR:
				printf("Incorrect/no password sent with factory reset CMD\n");
				return 1;
			case BSL_CORE_MSG_READ_OUT_ERROR:
				printf("Read out is disabled in BCR configuration\n");
				return 1;
			case BSL_CORE_MSG_INVALID_ADDRESS:
				printf("Start address or data length is not 8-byte aligned\n");
				return 1;
			case BSL_CORE_MSG_INVALID_LENGTH:
				printf("Data size is less than 1KB\n");
				return 1;
		}
	}
	return 0;
}

int bsl_connect(struct bsl_intf *intf)
{
	int rc;
	uint8_t tx[32];
	uint8_t rx[64];

	tx[0] = BSL_CMD_HEADER;
	tx[1] = 1;
	tx[2] = 0;
	tx[3] = BSL_CMD_CONNECTION;
	add_crc(tx, sizeof(tx));

	memset(rx, 0, sizeof(rx));

	dump_data("TX:", tx, BSL_TX_LEN);
	rc = bsl_write_read(intf, tx, BSL_TX_LEN, rx, 1);
	if (rc) {
		return rc;
	}
	dump_data("RX:", rx, 1);

	if ((rc = check_bsl_acknowledgement(rx[0])) != 0) {
		return rc;
	}

	return 0;
}

int bsl_get_device_info(struct bsl_intf *intf, struct bsl_device_info *info)
{
	int rc;
	uint8_t tx[32];
	uint8_t rx[64];

	tx[0] = BSL_CMD_HEADER;
	tx[1] = 1;
	tx[2] = 0;
	tx[3] = BSL_CMD_GET_DEVICE_INFO;
	add_crc(tx, sizeof(tx));

	memset(rx, 0, sizeof(rx));

	dump_data("TX:", tx, BSL_TX_LEN);
	rc = bsl_write_read(intf, tx, BSL_TX_LEN, rx, 33);
	if (rc) {
		return rc;
	}
	dump_data("RX:", rx, 33);

	if ((rc = check_bsl_acknowledgement(rx[0])) != 0) {
		return rc;
	}

	info->version = rx[5] | rx[6] << 8;
	info->build_id = rx[7] | rx[8] << 8;
	info->app_version = rx[9] | rx[10] << 8 | rx[11] << 16 | rx[12] << 24;
	info->interface_version = rx[13] | rx[14] << 8;
	info->bsl_max_buffer_size = rx[15] | rx[16] << 8;
	info->bsl_buffer_start = rx[17] | rx[18] << 8 | rx[19] << 16 | rx[20] << 24;
	info->bcr_config_id = rx[21] | rx[22] << 8 | rx[23] << 16 | rx[24] << 24;
	info->bsl_config_id = rx[25] | rx[26] << 8 | rx[27] << 16 | rx[28] << 24;

	return rc;
}

int bsl_unlock_bootloader(struct bsl_intf *intf)
{
	int rc;
	uint8_t tx[64];
	uint8_t rx[64];

	memset(rx, 0, sizeof(rx));

	tx[0] = BSL_CMD_HEADER;
	tx[1] = 33;
	tx[2] = 0;
	tx[3] = BSL_CMD_UNLOCK_BL;
	memset(&tx[4], 0xff, 32);
	add_crc(tx, sizeof(tx));

	dump_data("TX:", tx, BSL_TX_LEN);
	rc = bsl_write_read(intf, tx, BSL_TX_LEN, rx, 10);
	if (rc) {
		return rc;
	}
	dump_data("RX:", rx, 10);

	if ((rc = check_bsl_acknowledgement(rx[0])) != 0) {
		return rc;
	}

	return 0;
}

int bsl_mass_erase(struct bsl_intf *intf)
{
	int rc;
	uint8_t tx[64];
	uint8_t rx[64];

	memset(rx, 0, sizeof(rx));

	tx[0] = BSL_CMD_HEADER;
	tx[1] = 1;
	tx[2] = 0;
	tx[3] = BSL_CMD_MASS_ERASE;
	add_crc(tx, sizeof(tx));

	dump_data("TX:", tx, BSL_TX_LEN);
	rc = bsl_write_read(intf, tx, BSL_TX_LEN, rx, 10);
	if (rc) {
		return rc;
	}
	dump_data("RX:", rx, 10);

	if ((rc = check_bsl_response(rx, 10)) != 0) {
		return rc;
	}

	return 0;
}

int bsl_readback_data(struct bsl_intf *intf,
		uint32_t start, uint32_t count)
{
	int rc;
	uint8_t tx[32];
	uint8_t rx[32];

	memset(rx, 0, sizeof(rx));

	tx[0] = BSL_CMD_HEADER;
	tx[1] = 9;
	tx[2] = 0;
	tx[3] = BSL_CMD_MEMORY_READ_BACK;
	tx[4] = (start >> 0) & 0xff;
	tx[5] = (start >> 8) & 0xff;
	tx[6] = (start >> 16) & 0xff;
	tx[7] = (start >> 24) & 0xff;
	tx[8] = (count>> 0) & 0xff;
	tx[9] = (count >> 8) & 0xff;
	tx[10] = (count >> 16) & 0xff;
	tx[11] = (count >> 24) & 0xff;
	add_crc(tx, sizeof(tx));

	dump_data("TX:", tx, BSL_TX_LEN);
	rc = bsl_write_read(intf, tx, BSL_TX_LEN, rx, 9 + count);
	if (rc) {
		return rc;
	}
	dump_data("RX:", rx, 9 + count);


	if ((rc = check_bsl_response(rx, 10)) != 0) {
		return rc;
	}

	return rc;
}

#define BSL_PROGRAM_TX_BUFFER_LEN BSL_PROGGRAM_DATA_MAX_LEN + 12
int bsl_program_data(struct bsl_intf *intf,
		uint32_t address, uint8_t *data, size_t len)
{
	int rc;
	uint8_t tx[BSL_PROGRAM_TX_BUFFER_LEN];
	uint8_t rx[32];

	memset(rx, 0, sizeof(rx));

	tx[0] = BSL_CMD_HEADER;
	tx[1] = (5 + len) & 0xff;
	tx[2] = ((5 + len) >> 8) & 0xff;
	tx[3] = BSL_CMD_PROGRAM_DATA;
	tx[4] = (address>> 0) & 0xff;
	tx[5] = (address >> 8) & 0xff;
	tx[6] = (address >> 16) & 0xff;
	tx[7] = (address >> 24) & 0xff;
	memcpy(tx+8, data, len);
	add_crc(tx, sizeof(tx));

	dump_data("TX:", tx, BSL_TX_LEN);
	rc = bsl_write_read(intf, tx, BSL_TX_LEN, rx, 10);
	if (rc) {
		return rc;
	}
	dump_data("RX:", rx, 10);

	if ((rc = check_bsl_response(rx, 10)) != 0) {
		return rc;
	}

	return 0;
}

int bsl_verification(struct bsl_intf *intf,
		uint32_t address, uint32_t len, uint32_t *crc)
{
	int rc;
	uint8_t tx[512];
	uint8_t rx[32];

	memset(rx, 0, sizeof(rx));

	tx[0] = BSL_CMD_HEADER;
	tx[1] = 9;
	tx[2] = 0;
	tx[3] = BSL_CMD_STANDALONE_VERIFICATION;
	tx[4] = (address>> 0) & 0xff;
	tx[5] = (address >> 8) & 0xff;
	tx[6] = (address >> 16) & 0xff;
	tx[7] = (address >> 24) & 0xff;
	tx[8] = (len >> 0) & 0xff;
	tx[9] = (len >> 8) & 0xff;
	tx[10] = (len >> 16) & 0xff;
	tx[11] = (len >> 24) & 0xff;
	add_crc(tx, sizeof(tx));

	dump_data("TX:", tx, BSL_TX_LEN);
	rc = bsl_write_read(intf, tx, BSL_TX_LEN, rx, 13);
	if (rc) {
		return rc;
	}
	dump_data("RX:", rx, 13);

	if ((rc = check_bsl_response(rx, 10)) != 0) {
		return rc;
	}

	*crc = rx[5] | rx[6] << 8 | rx[7] << 16 | rx[8] << 24;

	return 0;
}

int bsl_start_application(struct bsl_intf *intf)
{
	int rc;
	uint8_t tx[64];
	uint8_t rx[64];

	memset(rx, 0, sizeof(rx));

	tx[0] = BSL_CMD_HEADER;
	tx[1] = 1;
	tx[2] = 0;
	tx[3] = BSL_CMD_START_APPLICATION;
	add_crc(tx, sizeof(tx));

	dump_data("TX:", tx, BSL_TX_LEN);
	rc = bsl_write_read(intf, tx, BSL_TX_LEN, rx, 1);
	if (rc) {
		return rc;
	}
	dump_data("RX:", rx, 10);

	if ((rc = check_bsl_acknowledgement(rx[0])) != 0) {
		return rc;
	}

	return 0;
}

int bsl_change_baudrate(struct bsl_intf *intf, uint8_t baudrate)
{
	int rc;
	uint8_t tx[64];
	uint8_t rx[64];

	memset(rx, 0, sizeof(rx));

	tx[0] = BSL_CMD_HEADER;
	tx[1] = 2;
	tx[2] = 0;
	tx[3] = BSL_CMD_CHANGE_BAUDRATE;
	tx[4] = baudrate;
	add_crc(tx, sizeof(tx));

	dump_data("TX:", tx, BSL_TX_LEN);
	rc = bsl_write_read(intf, tx, BSL_TX_LEN, rx, 1);
	if (rc) {
		return rc;
	}
	dump_data("RX:", rx, 1);

	if ((rc = check_bsl_acknowledgement(rx[0])) != 0) {
		return rc;
	}

	return 0;
}
