#ifndef INC_DRV8830_H_
#define INC_DRV8830_H_

#include "main.h"

#define DRV8830_DeviceAddress		(0xc0>>0)

#define DRV8830_CONTROL					0x00
#define DRV8830_FAULT						0x01

#define DRV8830_STOP						0x00
#define DRV8830_CW							0x01
#define DRV8830_CCW							0x02
#define DRV8830_BREAK						0x03

uint8_t initDrv8830();
uint8_t setMotor(int direction);

#endif /* INC_DRV8830_H_ */
