#ifndef INC_NEXTION_H_
#define INC_NEXTION_H_

#include "main.h"

typedef struct{
	uint16_t x;
	uint16_t y;
}coordinate;

void NEXTION_puts(char *ID, char *string);
uint8_t NEXTION_Put_Value_Etc(char *ID, char *attribute, char *value);
void NEXTION_Change_Page(char *Page);
uint8_t NEXTION_Get_Touch(uint8_t Page);
uint8_t NEXTION_Get_Button_Value(uint8_t ID);
uint8_t NEXTION_Get_Page(uint8_t Page);
void nextion_inst_set(char* inst);
coordinate nextion_get_pos(void);

#endif /* INC_NEXTION_H_ */
