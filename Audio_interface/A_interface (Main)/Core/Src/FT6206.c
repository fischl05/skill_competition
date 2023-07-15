#include "FT6206.h"

static TS_POINT coor; // User coordinate
static uint8_t touches;
static uint16_t touchX[2], touchY[2], touchID[2];

static uint8_t readRegister8(uint8_t reg){
	uint8_t data;
	HAL_I2C_Master_Transmit(&hi2c1, FT62XX_ADDR, &reg, 1, 10);
	HAL_I2C_Master_Receive(&hi2c1, FT62XX_ADDR, &data, 1, 10);
	return data;
}

static void writeRegister8(uint8_t reg, uint8_t val){
	HAL_I2C_Master_Transmit(&hi2c1, FT62XX_ADDR, &reg, 1, 10);
	HAL_I2C_Master_Transmit(&hi2c1, FT62XX_ADDR, &val, 1, 10);
}

bool FT6206_Begin(uint8_t thresh){
	writeRegister8(FT62XX_REG_THRESHHOLD, thresh);

	if(readRegister8(FT62XX_REG_VENDID) != FT62XX_VENDID)
		return false;
	uint8_t id = readRegister8(FT62XX_REG_CHIPID);
	if((id != FT6206_CHIPID) && (id != FT6236_CHIPID) && (id != FT6236U_CHIPID))
		return false;

	return true;
}

void readData(void){
	uint8_t i2cdat[16];
	uint8_t data = 0;

	HAL_I2C_Master_Transmit(&hi2c1, FT62XX_ADDR, &data, 1, 10);
	HAL_I2C_Master_Receive(&hi2c1, FT62XX_ADDR, i2cdat, 16, 10);

	touches = i2cdat[0x02];
	if(touches > 2 || touches == 0)
		touches = 0;

	for(uint8_t i = 0 ; i < 2 ; i++){
	    touchX[i] = i2cdat[0x03 + i * 6] & 0x0F;
	    touchX[i] <<= 8;
	    touchX[i] |= i2cdat[0x04 + i * 6];

	    touchY[i] = i2cdat[0x05 + i * 6] & 0x0F;
	    touchY[i] <<= 8;
	    touchY[i] |= i2cdat[0x06 + i * 6];

	    touchID[i] = i2cdat[0x05 + i * 6] >> 4;
	}
}

void INIT_FT6206(void) {
	touches = 0;
}

uint8_t touched(void){
	uint8_t n = readRegister8(FT62XX_REG_NUMTOUCHES);
	if(n > 2)
		n = 0;
	return n;
}

TS_POINT getPoint(uint8_t n) {
	readData();
	if ((touches == 0) || (n >= 1))
		TS_POINT_set(0, 0, 0);
	else
		TS_POINT_set(320 - touchY[n], touchX[n], 1);
	return coor;
}

void TS_POINT_clear(void) {
	coor.x = coor.y = coor.z = 0;
}

void TS_POINT_set(int16_t _x, int16_t _y, int16_t _z) {
	coor.x = _x;
	coor.y = _y;
	coor.z = _z;
}
