#ifndef INC_SHT41_H_
#define INC_SHT41_H_

#include "main.h"

#define SHT41_DeviceAddress		(0x88>>0)

#define SHT41_MeasureHigh			0xfd
#define SHT41_MeasureMedium		0xf6
#define SHT41_MeasureLow			0xe0
#define SHT41_ReadSerial			0x89
#define SHT41_SoftReset				0x94
#define SHT41_ActivateHeater	0x39

typedef struct {
	float temperature;
	float humidity;
} SHT41_t;

SHT41_t	getTempSht41();

#endif /* INC_SHT41_H_ */
