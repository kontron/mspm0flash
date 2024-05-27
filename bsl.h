/*
 * Copyright (c) 2024 Kontron Europe GmbH
 *
 * Author: Heiko Thiery <heiko.thiery@kontron.com>
 * Created: May 18, 2024
 */

#ifndef __I2C_H__
#define __I2C_H__

uint32_t crc32(uint8_t *buf, int len);

int bsl_connect(int fd, uint8_t i2c_address);

int bsl_unlock_bootloader(int fd, uint8_t i2c_address);


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

int bsl_get_device_info(int fd, uint8_t i2c_address, struct device_info *info);

int bsl_mass_erase(int fd, uint8_t i2c_address);

int bsl_readback_data(int fd, uint8_t i2c_address,
		uint32_t start, uint32_t count);

int bsl_program_data(int fd, uint8_t i2c_address,
		uint32_t address, uint8_t *data, size_t len);

int bsl_verification(int fd, uint8_t i2c_address,
		uint32_t address, uint32_t len, uint32_t *crc);

#endif /* #ifndef __I2C_H__ */
