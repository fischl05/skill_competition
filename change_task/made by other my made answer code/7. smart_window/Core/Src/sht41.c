#include "sht41.h"

extern I2C_HandleTypeDef hi2c1;

SHT41_t	getTempSht41() {
	uint8_t txData = SHT41_MeasureHigh;
	uint8_t	rxData[6];
	SHT41_t result;
	uint16_t buf[2];

	HAL_I2C_Init(&hi2c1);
	HAL_StatusTypeDef status = HAL_ERROR;
	HAL_I2C_Master_Transmit(&hi2c1, SHT41_DeviceAddress, &txData, 1, 1);
	HAL_Delay(20);
	status = HAL_I2C_Master_Receive(&hi2c1, SHT41_DeviceAddress, rxData, 6, 10);
	buf[0] = rxData[0] << 8 | rxData[1];
	buf[1] = rxData[3] << 8 | rxData[4];
	result.temperature = -45.0f + (175.0f * ((float)buf[0] / 65535.0f));
	result.humidity = -6.0f + (125.0f * ((float)buf[1] / 65535.0f));
	return result;
}
