#include "NEXTION.h"
#include <string.h>
#include <stdio.h>

extern UART_HandleTypeDef huart1;

uint8_t cmd_end[3]={0xFF, 0xFF, 0xFF};

void NEXTION_puts(char *ID, char *string){
	char buf_str[100];
	int len = sprintf(buf_str, "%s.txt=\"%s\"", ID, string);
	HAL_UART_Transmit(&huart1, (uint8_t*)buf_str, len, 100);
	HAL_UART_Transmit(&huart1, cmd_end, 3, 100);
}

uint8_t NEXTION_Put_Value_Etc(char *ID, char *attribute, char *value){
	char buf_str[50];
	int len = sprintf(buf_str, "%s.%s=%s", ID, attribute, value);
	HAL_UART_Transmit(&huart1, (uint8_t*)buf_str, len, 100);
	HAL_UART_Transmit(&huart1, cmd_end, 3, 100);

	return 0;
}

void NEXTION_Change_Page(char *Page){
	char buf_str[50];
	int len = sprintf(buf_str, "page %s", Page);
	HAL_UART_Transmit(&huart1, (uint8_t*)buf_str, len, 100);
	HAL_UART_Transmit(&huart1, cmd_end, 3, 100);
}

uint8_t NEXTION_Get_Touch(uint8_t Page){
	uint8_t Rx_Data[5];
	HAL_UART_Receive(&huart1, Rx_Data, 5, 200);
	if(Rx_Data[1] == Page + 48) return Rx_Data[2];
	return 0;
}

uint8_t NEXTION_Get_Page(uint8_t Page){
	uint8_t Rx_Data[5];
	HAL_UART_Receive(&huart1, Rx_Data, 5, 200);
	if(Rx_Data[1] == Page + 48) return 1;
	return 0;
}

void nextion_inst_set(char* inst){
	HAL_UART_Transmit(&huart1, (uint8_t*)inst, strlen(inst), HAL_MAX_DELAY);
	HAL_UART_Transmit(&huart1, cmd_end, 3, HAL_MAX_DELAY);
}

coordinate nextion_get_pos(void){
	HAL_StatusTypeDef res = HAL_OK;
	uint8_t Rx_Data[9];
	coordinate pos = { 0, 0 };
	res = HAL_UART_Receive(&huart1, Rx_Data, 9, HAL_MAX_DELAY);
	if(res != HAL_OK) return pos;
	if(Rx_Data[0] != 0x67) return pos;
	if(Rx_Data[5] != 0x01) return pos;
	pos.x = Rx_Data[1] << 8 | Rx_Data[2];
	pos.y = Rx_Data[3] << 8 | Rx_Data[4];
	return pos;
}

void nextion_dummy_put(void){
	HAL_UART_Transmit(&huart1, cmd_end, 3, 10);
}
