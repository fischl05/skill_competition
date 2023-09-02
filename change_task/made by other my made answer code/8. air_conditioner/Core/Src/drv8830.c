#include "drv8830.h"

extern I2C_HandleTypeDef hi2c1;

uint8_t initDrv8830() {
	uint8_t txData = 0x80;
	// clear fault
	return HAL_I2C_Mem_Write(&hi2c1, DRV8830_DeviceAddress, DRV8830_FAULT, 1, &txData, 1, 5);
}

// VSET[7:2] voltage 0x00~0x3F
// D1, D0 direction
uint8_t setMotor(int direction) {
	uint8_t txData;
	uint8_t	result;
	txData = 0x1f << 2 | direction;
	int status;
	status = HAL_I2C_Mem_Write(&hi2c1, DRV8830_DeviceAddress, DRV8830_CONTROL, 1, &txData, 1, 5);
	status = HAL_I2C_Mem_Read(&hi2c1, DRV8830_DeviceAddress, DRV8830_FAULT, 1, &result, 1, 5);
	return result;
}
