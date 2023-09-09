#include "ens160.h"

extern I2C_HandleTypeDef hi2c1;

void initEns160() {
	uint8_t txData = ENS160_OpMode_Run;
	int status = HAL_I2C_Mem_Write(&hi2c1, ENS160_DeviceAddress, ENS160_OpMode, 1, &txData, 1, 10);
}

void putTemperature(float temp) {
	uint8_t txData[2];
	uint16_t writeValue;
	writeValue = (temp + 273.15f) * 64;	// temperature in Kelvin * 64
	txData[0]= writeValue;
	txData[1]= writeValue >> 8;
	int status = HAL_I2C_Mem_Write(&hi2c1, ENS160_DeviceAddress, ENS160_TempIn, 1, txData, 2, 10);
}

void putHumidity(float humidity) {
	uint8_t txData[2];
	uint16_t writeValue;
	writeValue = humidity * 512;	// temperature in Kelvin * 64
	txData[0]= writeValue;
	txData[1]= writeValue >> 8;
	int status = HAL_I2C_Mem_Write(&hi2c1, ENS160_DeviceAddress, ENS160_HumidityIn, 1, txData, 2, 10);
}

int	getCO2() {
	HAL_I2C_Init(&hi2c1);
	uint8_t rxData[2];
	int status = HAL_I2C_Mem_Read(&hi2c1, ENS160_DeviceAddress, ENS160_DataECO2, 1, rxData, 2, 10);
	return rxData[1] << 8 | rxData[0];
}

int getTemperature() {
	uint8_t rxData[2];
	int status = HAL_I2C_Mem_Read(&hi2c1, ENS160_DeviceAddress, ENS160_DataTemperature, 1, rxData, 2, 10);
	return rxData[1] << 8 | rxData[0];
}

int getHumidity() {
	uint8_t rxData[2];
	int status = HAL_I2C_Mem_Read(&hi2c1, ENS160_DeviceAddress, ENS160_DataHumidity, 1, rxData, 2, 10);
	return rxData[1] << 8 | rxData[0];
}
