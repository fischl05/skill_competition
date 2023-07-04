#include "LCD.H"
#include "main.h"
#include <string.h>

static uint8_t num;
static uint8_t rxData;

static uint8_t Test_Rx(uint8_t num){
	rxData = 0;
	while(rxData == 0) {
		HAL_UART_Transmit(&huart1, &num, 1, 10);
		HAL_UART_Receive(&huart1, &rxData, 1, 10);
	}
	return rxData;
}

static void Coordinate_Tx(uint16_t x, uint16_t y){
	for(uint8_t i = 0 ; i < 2 ; i++){
		HAL_UART_Transmit(&huart1, (uint8_t*)&x, 1, 10);
		x >>= 8;
	}
	for(uint8_t i = 0 ; i < 2 ; i++){
		HAL_UART_Transmit(&huart1, (uint8_t*)&y, 1, 10);
		y >>= 8;
	}
}

void LCD_DrawColor(LCD_COLOR color){
	num = 1;
	if(Test_Rx(num)){
		HAL_UART_Transmit(&huart1, &color, 1, 10);
	}
	else return;
}

void LCD_DrawColorBurst(LCD_COLOR color, uint32_t size){
	num = 2;
	if(Test_Rx(num)){
		HAL_UART_Transmit(&huart1, &color, 1, 10);
		for(uint8_t i = 0 ; i < 4 ; i++){
			HAL_UART_Transmit(&huart1, (uint8_t*)&size, 1, 10);
			size >>= 8;
		}
	}
	else return;
}

void LCD_FillScreen(LCD_COLOR color){
	num = 3;
	if(Test_Rx(num)) {
		HAL_UART_Transmit(&huart1, &color, 1, 10);
	}
	else return;
}

void LCD_DrawPixel(uint16_t x,uint16_t y,LCD_COLOR color){
	num = 4;
	if(Test_Rx(num)){
		Coordinate_Tx(x, y);
		HAL_UART_Transmit(&huart1, &color, 1, 10);
	}
	else return;
}

void LCD_DrawRectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, LCD_COLOR color){
	num = 5;
	if(Test_Rx(num)){
		Coordinate_Tx(x, y);
		for(uint8_t i = 0 ; i < 2 ; i++){
			HAL_UART_Transmit(&huart1, (uint8_t*)&width, 1, 10);
			width >>= 8;
		}
		for(uint8_t i = 0 ; i < 2 ; i++){
			HAL_UART_Transmit(&huart1, (uint8_t*)&height, 1, 10);
			height >>= 8;
		}
		HAL_UART_Transmit(&huart1, &color, 1, 10);
	}
	else return;
}

void LCD_DrawHLine(uint16_t x, uint16_t y, uint16_t width, LCD_COLOR color){
	num = 6;
	if(Test_Rx(num)){
		Coordinate_Tx(x, y);
		for(uint8_t i = 0 ; i < 2 ; i++){
			HAL_UART_Transmit(&huart1, (uint8_t*)&width, 1, 10);
			width >>= 8;
		}
		HAL_UART_Transmit(&huart1, &color, 1, 10);
	}
	else return;
}

void LCD_DrawVLine(uint16_t x, uint16_t y, uint16_t height, LCD_COLOR color){
	num = 7;
	if(Test_Rx(num)){
		Coordinate_Tx(x, y);
		for(uint8_t i = 0 ; i < 2 ; i++){
			HAL_UART_Transmit(&huart1, (uint8_t*)&height, 1, 10);
			height >>= 8;
		}
		HAL_UART_Transmit(&huart1, &color, 1, 10);
	}
	else return;
}

void LCD_DrawHollowCircle(uint16_t X, uint16_t Y, uint16_t radius, LCD_COLOR color){
	num = 8;
	if(Test_Rx(num)){
		Coordinate_Tx(X, Y);
		for(uint8_t i = 0 ; i < 2 ; i++){
			HAL_UART_Transmit(&huart1, (uint8_t*)&radius, 1, 10);
			radius >>= 8;
		}
		HAL_UART_Transmit(&huart1, &color, 1, 10);
	}
	else return;
}

void LCD_DrawFilledCircle(uint16_t X, uint16_t Y, uint16_t radius, LCD_COLOR color){
	num = 9;
	if(Test_Rx(num)){
		Coordinate_Tx(X, Y);
		for(uint8_t i = 0 ; i < 2 ; i++){
			HAL_UART_Transmit(&huart1, (uint8_t*)&radius, 1, 10);
			radius >>= 8;
		}
		HAL_UART_Transmit(&huart1, &color, 1, 10);
	}
	else return;
}

void LCD_DrawHollowRectangleCoord(uint16_t X0, uint16_t Y0, uint16_t X1, uint16_t Y1, LCD_COLOR color){
	num = 10;
	if(Test_Rx(num)){
		Coordinate_Tx(X0, Y0);
		Coordinate_Tx(X1, Y1);
		HAL_UART_Transmit(&huart1, &color, 1, 10);
	}
	else return;
}

void LCD_DrawFilledRectangleCoord(uint16_t X0, uint16_t Y0, uint16_t X1, uint16_t Y1, LCD_COLOR color){
	num = 11;
	if(Test_Rx(num)){
		Coordinate_Tx(X0, Y0);
		Coordinate_Tx(X1, Y1);
		HAL_UART_Transmit(&huart1, &color, 1, 10);
	}
	else return;
}

void LCD_DrawChar(char ch , uint16_t X, uint16_t Y, LCD_COLOR color, LCD_COLOR bgcolor){
	num = 12;
	if(Test_Rx(num)){
		HAL_UART_Transmit(&huart1, (uint8_t*)&ch, 1, 10);
		Coordinate_Tx(X, Y);
		HAL_UART_Transmit(&huart1, &color, 1, 10);
		HAL_UART_Transmit(&huart1, &bgcolor, 1, 10);
	}
	return;
}

void LCD_DrawText(const char* str, uint16_t X, uint16_t Y, LCD_COLOR color, LCD_COLOR bgcolor){
	num = 13;
	if(Test_Rx(num)){
		uint8_t len = strlen(str);
		for(uint8_t i = 0 ; i <= len ; i++)
			HAL_UART_Transmit(&huart1, (uint8_t*)&str[i], 1, 10);
		Coordinate_Tx(X, Y);
		HAL_UART_Transmit(&huart1, &color, 1, 10);
		HAL_UART_Transmit(&huart1, &bgcolor, 1, 10);
	}
}
