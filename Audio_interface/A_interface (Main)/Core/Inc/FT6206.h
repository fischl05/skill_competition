#ifndef __FT6206_H__
#define __FT6206_H__

#include <stdint.h>
#include <stdbool.h>
#include <main.h>

#define FT62XX_ADDR 0x71           //!< I2C address
#define FT62XX_G_FT5201ID 0xA8     //!< FocalTech's panel ID
#define FT62XX_REG_NUMTOUCHES 0x02 //!< Number of touch points

#define FT62XX_NUM_X 0x33 //!< Touch X position
#define FT62XX_NUM_Y 0x34 //!< Touch Y position

#define FT62XX_REG_MODE 0x00        //!< Device mode, either WORKING or FACTORY
#define FT62XX_REG_CALIBRATE 0x02   //!< Calibrate mode
#define FT62XX_REG_WORKMODE 0x00    //!< Work mode
#define FT62XX_REG_FACTORYMODE 0x40 //!< Factory mode
#define FT62XX_REG_THRESHHOLD 0x80  //!< Threshold for touch detection
#define FT62XX_REG_POINTRATE 0x88   //!< Point rate
#define FT62XX_REG_FIRMVERS 0xA6    //!< Firmware version
#define FT62XX_REG_CHIPID 0xA3      //!< Chip selecting
#define FT62XX_REG_VENDID 0xA8      //!< FocalTech's panel ID

#define FT62XX_VENDID 0x11  //!< FocalTech's panel ID
#define FT6206_CHIPID 0x06  //!< Chip selecting
#define FT6236_CHIPID 0x36  //!< Chip selecting
#define FT6236U_CHIPID 0x64 //!< Chip selecting

// calibrated for Adafruit 2.8" ctp screen
#define FT62XX_DEFAULT_THRESHOLD 255 //!< Default threshold for touch detection

extern I2C_HandleTypeDef hi2c1;

typedef struct {
	int16_t x; // X coordinate
	int16_t y; // Y coordinate
	int16_t z; // Z coordinate (often used for pressure)
}TS_POINT;

// coordinate function
void TS_POINT_clear(void);
void TS_POINT_set(int16_t _x, int16_t _y, int16_t _z);

// FT6206 function
void INIT_FT6206(void);
bool FT6206_Begin(uint8_t thresh);
TS_POINT getPoint(uint8_t n);
uint8_t touched(void);

#endif
