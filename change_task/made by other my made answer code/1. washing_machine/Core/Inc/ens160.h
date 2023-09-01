#ifndef INC_ENS160_H_
#define INC_ENS160_H_

#include "main.h"

#define ENS160_DeviceAddress			(0xa4>>0)

#define ENS160_OpMode							0x10
#define ENS160_OpMode_Sleep				0x00
#define ENS160_OpMode_Idle				0x01
#define ENS160_OpMode_Run					0x02
#define ENS160_TempIn							0x13
#define ENS160_HumidityIn					0x15
#define ENS160_DeviceStatus				0x20
#define ENS160_DataTVOC						0x22
#define ENS160_DataECO2						0x24
#define ENS160_DataTemperature 		0x30
#define ENS160_DataHumidity				0x32

void initEns160();
void putTemperature(float temp);
void putHumidity(float humidity);
int	getCO2();
int getTemperature();
int getHumidity();

#endif /* INC_ENS160_H_ */
