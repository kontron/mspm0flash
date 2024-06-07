/*
 * Copyright (c) 2024 Kontron Europe GmbH
 *
 * Author: Heiko Thiery <heiko.thiery@kontron.com>
 * Created: May 18, 2024
 */

#ifndef __BSL_H__
#define __BSL_H__

#define BSL_CMD_HEADER 0x80
#define BSL_HEADER_SIZE 3
#define BSL_CRC_SIZE 4

#define BSL_TX_LEN (BSL_HEADER_SIZE + (tx[1] | (tx[2] << 8)) + BSL_CRC_SIZE)

#define BSL_CMD_CONNECTION 0x12
#define BSL_CMD_MASS_ERASE 0x15
#define BSL_CMD_GET_DEVICE_INFO 0x19
#define BSL_CMD_PROGRAM_DATA 0x20
#define BSL_CMD_UNLOCK_BL 0x21
#define BSL_CMD_STANDALONE_VERIFICATION 0x26
#define BSL_CMD_MEMORY_READ_BACK 0x29
#define BSL_CMD_START_APPLICATION 0x40
#define BSL_CMD_CHANGE_BAUDRATE 0x52

#define BSL_ACK 0x00
#define BSL_ERROR_HEADER_INCORRECT 0x51
#define BSL_ERROR_CHECKSUM_INCORRECT 0x52
#define BSL_ERROR_PACKET_SIZE_ZERO 0x53
#define BSL_ERROR_PACKET_SIZE_TOO_BIG 0x54
#define BSL_ERROR_UNKNOWN_ERROR 0x55
#define BSL_ERROR_UNKNOWN_BAUD_RATE 0x56

#define BSL_CORE_RSP_MEMORY_READ_BACK 0x30
#define BSL_CORE_RSP_GET_DEVICE_INFO 0x31
#define BSL_CORE_RSP_STANDALONE_VERIFICATION 0x32
#define BSL_CORE_RSP_DETAILED_ERROR 0x3A
#define BSL_CORE_RSP_MESSAGE 0x3B

#define BSL_CORE_MSG_OPERATION_SUCCESSFUL 0x00
#define BSL_CORE_MSG_BSL_LOCKED_ERROR 0x01
#define BSL_CORE_MSG_BSL_PASSWORD_ERROR 0x02
#define BSL_CORE_MSG_MULTIPLE_BSL_PASSWORD_ERROR 0x03
#define BSL_CORE_MSG_UNKNOWN_COMMAND 0x04
#define BSL_CORE_MSG_INVALID_MEMORY_RAMGE 0x05
#define BSL_CORE_MSG_INVALID_COMMAND 0x06
#define BSL_CORE_MSG_FACTORY_RESET_DISABLED 0x07
#define BSL_CORE_MSG_FACTORY_RESET_PASSWORD_ERROR 0x08
#define BSL_CORE_MSG_READ_OUT_ERROR 0x09
#define BSL_CORE_MSG_INVALID_ADDRESS 0x0a
#define BSL_CORE_MSG_INVALID_LENGTH 0x0b

#define BSL_UART_B4800 1
#define BSL_UART_B9600 2
#define BSL_UART_B19200 3
#define BSL_UART_B38400 4
#define BSL_UART_B57600 5
#define BSL_UART_B115200 6
#define BSL_UART_B1000000 7

enum {
	INTERFACE_TYPE_INVALID = 0,
	INTERFACE_TYPE_UART,
	INTERFACE_TYPE_I2C
};

struct bsl_intf {
	int fd;
	uint8_t i2c_address;
	uint32_t baudrate;
	int type;
};

struct bsl_device_info {
	uint16_t version;
	uint16_t build_id;
	uint32_t app_version;
	uint16_t interface_version;
	uint16_t bsl_max_buffer_size;
	uint32_t bsl_buffer_start;
	uint32_t bcr_config_id;
	uint32_t bsl_config_id;
};

uint32_t crc32(uint8_t *buf, int len);

int bsl_connect(struct bsl_intf *intf);

int bsl_start_application(struct bsl_intf *intf);

int bsl_unlock_bootloader(struct bsl_intf *intf);

int bsl_get_device_info(struct bsl_intf *intf, struct bsl_device_info *info);

int bsl_mass_erase(struct bsl_intf *intf);

int bsl_readback_data(struct bsl_intf *intf,
		uint32_t start, uint32_t count);

#define BSL_PROGGRAM_DATA_MAX_LEN 256
int bsl_program_data(struct bsl_intf *intf,
		uint32_t address, uint8_t *data, size_t len);

int bsl_verification(struct bsl_intf *intf,
		uint32_t address, uint32_t len, uint32_t *crc);

int bsl_change_baudrate(struct bsl_intf *intf, uint8_t baudrate);

#endif /* #ifndef __BSL_H__ */
