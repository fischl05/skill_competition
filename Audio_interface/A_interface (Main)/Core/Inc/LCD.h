#ifndef __LCD_H__
#define __LCD_H__

#include "stm32l052xx.h"
#include "main.h"

// LCD screen height 240
// LCD screen width  320

extern UART_HandleTypeDef huart1;

typedef enum{
	black, blue, green, red, yellow, white,
}LCD_COLOR;

//void LCD_Init(void);

void LCD_DrawColor(LCD_COLOR color);
void LCD_DrawColorBurst(LCD_COLOR color, uint32_t size);
void LCD_FillScreen(LCD_COLOR color);
void LCD_DrawPixel(uint16_t x,uint16_t y,LCD_COLOR color);
void LCD_DrawRectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, LCD_COLOR color);
void LCD_DrawHLine(uint16_t x, uint16_t y, uint16_t width, LCD_COLOR color);
void LCD_DrawVLine(uint16_t x, uint16_t y, uint16_t height, LCD_COLOR color);

void LCD_DrawHollowCircle(uint16_t X, uint16_t Y, uint16_t radius, LCD_COLOR color);
void LCD_DrawFilledCircle(uint16_t X, uint16_t Y, uint16_t radius, LCD_COLOR color);
void LCD_DrawHollowRectangleCoord(uint16_t X0, uint16_t Y0, uint16_t X1, uint16_t Y1, LCD_COLOR color);
void LCD_DrawFilledRectangleCoord(uint16_t X0, uint16_t Y0, uint16_t X1, uint16_t Y1, LCD_COLOR color);
void LCD_DrawChar(char ch , uint16_t X, uint16_t Y, LCD_COLOR color, LCD_COLOR bgcolor);
void LCD_DrawText(const char* str, uint16_t X, uint16_t Y, LCD_COLOR color, LCD_COLOR bgcolor);

#endif
