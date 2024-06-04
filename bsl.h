/*
 * Copyright (c) 2024 Kontron Europe GmbH
 *
 * Author: Heiko Thiery <heiko.thiery@kontron.com>
 * Created: May 18, 2024
 */

#ifndef __BSL_H__
#define __BSL_H__

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

uint32_t crc32(uint8_t *buf, int len);

int bsl_connect(struct bsl_intf *intf);

int bsl_start_application(struct bsl_intf *intf);

int bsl_unlock_bootloader(struct bsl_intf *intf);


struct device_info {
	uint16_t version;
	uint16_t build_id;
	uint32_t app_version;
	uint16_t interface_version;
	uint16_t bsl_max_buffer_size;
	uint32_t bsl_buffer_start;
	uint32_t bcr_config_id;
	uint32_t bsl_config_id;
};

int bsl_get_device_info(struct bsl_intf *intf, struct device_info *info);

int bsl_mass_erase(struct bsl_intf *intf);

int bsl_readback_data(struct bsl_intf *intf,
		uint32_t start, uint32_t count);

#define BSL_PROGGRAM_DATA_MAX_LEN 256
int bsl_program_data(struct bsl_intf *intf,
		uint32_t address, uint8_t *data, size_t len);

int bsl_verification(struct bsl_intf *intf,
		uint32_t address, uint32_t len, uint32_t *crc);


#define BSL_UART_B4800 1
#define BSL_UART_B9600 2
#define BSL_UART_B19200 3
#define BSL_UART_B38400 4
#define BSL_UART_B57600 5
#define BSL_UART_B115200 6
#define BSL_UART_B1000000 7

int bsl_change_baudrate(struct bsl_intf *intf, uint8_t baudrate);

#endif /* #ifndef __BSL_H__ */
