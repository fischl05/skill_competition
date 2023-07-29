/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2023 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "DS3231.h"
#include "FT6206.h"
#include "LCD.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* struct define start */
typedef struct{
	uint8_t year, month, day;
	uint8_t hour, min, sec;
}TIME;

typedef struct{
	uint16_t x;
	uint16_t y;
}POS;
/* struct define  end */

/* enum define start */
typedef enum{
	Background_Color_ADDR,
	Text_Color_ADDR,
	Volume_ADDR,
	Pitch_ADDR,
}ADDR;

typedef enum{
	main_menu,
	sound_menu,
	setting_menu,
	select_menu,
}MODE;

typedef enum{
	none_xy,
	have_xy,
}TOUCH_STATE;

typedef enum{
	get_coordinate,
	reset_coordinate,
}readTouchState;

typedef enum{
	PITCH_STATE_0,
	PITCH_STATE_1,
	PITCH_STATE_2,
	PITCH_STATE_3,
	PITCH_STATE_4,
	PITCH_STATE_5,
	PITCH_STATE_6,
	PITCH_STATE_7,
	PITCH_STATE_8,
	PITCH_STATE_9,
	PITCH_STATE_10,
}PITCH_STATE;

typedef enum{
	origin_setting,
	reset_setting,
}SETTING_RESET_STATE;

typedef enum{
	init_time,
	load_time,
}TIME_LOAD_STATE;

typedef enum{
	speaker_off,
	speaker_on,
}SPEAKER_STATE;
/* enum define end */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define RECT 53
#define BUZ(X) BUZ_GPIO_Port->BSRR = (X) ? BUZ_Pin : BUZ_Pin << 16
#define XYsetZero curXY.x = curXY.y = 0
#define SOUND_IN (adcV > 150)
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc;
DMA_HandleTypeDef hdma_adc;

DAC_HandleTypeDef hdac;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim6;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

const LCD_COLOR lcd_color[6] = { black, blue, green, red, yellow, white };
const uint8_t lastDay[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

LCD_COLOR set_tcolor = black, set_bcolor = white;
LCD_COLOR txt_color[17] = { black }, back_color[17] = { white };

TS_POINT curXY = { 0, 0, 0 };

TIME time = { 23, 6, 22, 0, 0, 0 };

MODE modeF = main_menu, backupM = main_menu;

TOUCH_STATE touched_check = none_xy;
readTouchState getTouch_state = reset_coordinate;

PITCH_STATE pitch_state = PITCH_STATE_5;

uint8_t firF, buzM, sel;
uint16_t buzC;

uint8_t coor_volume = 135, coor_pitch = 135;
uint16_t max_volume = 75;
uint32_t adcV = 0;

SPEAKER_STATE sound_start = speaker_off;
uint32_t tick = 0, value = 0;
float pitch = 1.0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC_Init(void);
static void MX_DAC_Init(void);
static void MX_I2C1_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM6_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_NVIC_Init(void);
/* USER CODE BEGIN PFP */

/* Function prototype define Start */
__STATIC_INLINE void time_set(void);
__STATIC_INLINE void time_get(void);
__STATIC_INLINE void pitch_set(void);
__STATIC_INLINE void reset_value(void);
__STATIC_INLINE void system_reset(void);
__STATIC_INLINE void LCD_Clear(LCD_COLOR color);
__STATIC_INLINE void eepWrite(uint8_t addr, uint8_t data);
__STATIC_INLINE uint8_t eepRead(uint8_t addr);
__STATIC_INLINE uint8_t checkReset_status(void);
__STATIC_INLINE uint8_t xy_check(uint16_t x0, uint16_t x1, uint16_t y0, uint16_t y1);
void start(void);
void VP_save(void);
void VP_read(void);
void start_dis(void);
void main_mode(void);
void color_save(void);
void color_read(void);
void select_mode(void);
void All_eepRead(void);
void All_eepWrite(void);
void setting_mode(void);
void modulation_mode(void);
void time_setting(TIME_LOAD_STATE state);
void Init_setting(SETTING_RESET_STATE state);
void LCD_putsXY(uint16_t x, uint16_t y, char* str, LCD_COLOR color, LCD_COLOR bg_color);
void array_puts(POS* pos, char* title, char** arr, LCD_COLOR* color, LCD_COLOR* bg_color, uint8_t num);
/* Function prototype define End */

/* Inline Function Define Start */
__STATIC_INLINE void time_set(void){
	DS3231_set_time(time.sec, time.min, time.hour);
	DS3231_set_date(time.day, time.month, time.year);
}

__STATIC_INLINE void time_get(void){
	DS3231_get_time(&time.sec, &time.min, &time.hour);
	DS3231_get_date(&time.day, &time.month, &time.year);
}

__STATIC_INLINE uint8_t xy_check(uint16_t x0, uint16_t x1, uint16_t y0, uint16_t y1){
	return ((curXY.x >= x0 * 15 && curXY.x <= x1 * 15) && (curXY.y >= y0 * 19 && curXY.y <= y1 * 19));
}

__STATIC_INLINE void LCD_Clear(LCD_COLOR color){
	LCD_FillScreen(color);
}

__STATIC_INLINE void reset_value(void){
	backupM = modeF;
	touched_check = none_xy;
	modeF = firF = buzM = 0;
	XYsetZero;
	LCD_Clear(set_bcolor);
}

__STATIC_INLINE void pitch_set(void){
	switch(pitch_state){
	case PITCH_STATE_10:   pitch =  0.1;     break;
	case PITCH_STATE_9:    pitch =  0.2;     break;
	case PITCH_STATE_8:    pitch =  0.4;     break;
	case PITCH_STATE_7:    pitch =  0.8;     break;
	case PITCH_STATE_6:    pitch =  0.9;     break;
	case PITCH_STATE_5:    pitch =  1.0;     break;
	case PITCH_STATE_4:    pitch =  2.5;     break;
	case PITCH_STATE_3:    pitch =  4.5;     break;
	case PITCH_STATE_2:    pitch =  8.5;     break;
	case PITCH_STATE_1:    pitch =  9.5;     break;
	case PITCH_STATE_0:    pitch = 10.5;     break;
	}
}

__STATIC_INLINE void eepWrite(uint8_t addr, uint8_t data){
	HAL_FLASHEx_DATAEEPROM_Unlock();
	*(__IO uint8_t*)(DATA_EEPROM_BASE + addr) = data;
	HAL_FLASHEx_DATAEEPROM_Lock();
}

__STATIC_INLINE uint8_t eepRead(uint8_t addr){
	return *(__IO uint8_t*)(DATA_EEPROM_BASE + addr);
}

__STATIC_INLINE void system_reset(void){
	HAL_DeInit();
	HAL_NVIC_SystemReset();
}

__STATIC_INLINE uint8_t checkReset_status(void){
	if(__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST)){
		All_eepRead();

		if(set_tcolor == set_bcolor) { set_tcolor = black; set_bcolor = white; }

		if(coor_volume < 60 || coor_volume > 210) coor_volume = 135;

		if(coor_pitch < 60 || coor_pitch > 210) coor_pitch = 135;

		return 0;
	}
	else{
		Init_setting(reset_setting);

		All_eepWrite();

		Init_setting(origin_setting);

		return 1;
	}
}
/* Inline Function Define End */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void Init_setting(SETTING_RESET_STATE state){
	switch(state){
	case origin_setting:
		coor_volume = 135;
		coor_pitch = 135;

		set_tcolor = black;
		set_bcolor = white;
		break;
	case reset_setting:
		coor_volume = 0;
		coor_pitch = 0;

		set_tcolor = black;
		set_bcolor = black;
		break;
	}
}

void color_save(void){
	eepWrite(Text_Color_ADDR, set_tcolor);
	eepWrite(Background_Color_ADDR, set_bcolor);
}

void color_read(void){
	set_tcolor = eepRead(Text_Color_ADDR);
	set_bcolor = eepRead(Background_Color_ADDR);
}

void VP_save(void){
	eepWrite(Pitch_ADDR, coor_pitch);
	eepWrite(Volume_ADDR, coor_volume);
}

void VP_read(void){
	coor_pitch = eepRead(Pitch_ADDR);
	coor_volume = eepRead(Volume_ADDR);
}

void All_eepRead(void){
	VP_read();
	color_read();
}

void All_eepWrite(void){
	VP_save();
	color_save();
}

void LCD_putsXY(uint16_t x, uint16_t y, char* str, LCD_COLOR color, LCD_COLOR bg_color){
	LCD_DrawText(str, x * 15, y * 19, color, bg_color);
}

void array_puts(POS* pos, char* title, char** arr, LCD_COLOR* color, LCD_COLOR* bg_color, uint8_t num){
	LCD_putsXY(0, 0, title, set_tcolor, set_bcolor);
	for(uint8_t i = 0 ; i < num ; i++)
		LCD_putsXY(pos[i].x, pos[i].y, arr[i], color[i], bg_color[i]);
}

void time_setting(TIME_LOAD_STATE state){
	uint8_t sel = 0;
	TIME set_time = { 23, 6, 22, 0, 0, 0 };

	POS pos[9] = {{5, 4}, {10, 4}, {14, 4}, {6, 5}, {10, 5}, {14, 5}, {7, 8}, {12, 8}, {17, 10}};
	char bf[6][20];

	if(state == load_time){
		time_get();
		set_time = time;
	}

	LCD_Clear(set_bcolor);
	while(1){
		if(!firF){
			firF = 1;

			XYsetZero;

			LCD_putsXY(6, 1, "< Time Setting >", set_tcolor, set_bcolor);
		}

		for(uint8_t i = 0 ; i < 9 ; i++){
			back_color[i] = set_bcolor;
			if(sel == i) {
				if(set_tcolor == white && set_bcolor == black) txt_color[i] = blue;
				else{
					txt_color[i] = (set_tcolor + 1) % 7;
					if(txt_color[i] == set_bcolor) txt_color[i] = (set_bcolor + 1) % 7;
				}
			}
			else txt_color[i] = set_tcolor;
		}
		if(set_time.day > lastDay[set_time.month - 1]) set_time.day = lastDay[set_time.month - 1];

		sprintf(bf[0], "Y:%04d", 2000 + set_time.year);
		sprintf(bf[1], "M:%02d", set_time.month);
		sprintf(bf[2], "D:%02d", set_time.day);
		sprintf(bf[3], "H:%02d", set_time.hour);
		sprintf(bf[4], "m:%02d", set_time.min);
		sprintf(bf[5], "S:%02d", set_time.sec);

		char* array[9] = { bf[0], bf[1], bf[2], bf[3], bf[4], bf[5], "UP", "DOWN", "OK!" };
		array_puts(pos, "", array, txt_color, back_color, 9);

		if((curXY.x >0 || curXY.y > 0)){
			if(xy_check(17, 17 + strlen("OK!"), 10, 11))       break;
			else if(xy_check(5, 5 + strlen(bf[0]), 4, 5))      sel = 0;
			else if(xy_check(10, 10 + strlen(bf[1]), 4, 5))    sel = 1;
			else if(xy_check(14, 14 + strlen(bf[2]), 4, 5))    sel = 2;
			else if(xy_check(6, 7 + strlen(bf[3]), 5, 6))      sel = 3;
			else if(xy_check(10, 10 + strlen(bf[4]), 5, 6))    sel = 4;
			else if(xy_check(14, 14 + strlen(bf[5]), 5, 6))    sel = 5;
			else{
				static uint32_t frev_tick;
				uint32_t now_tick = HAL_GetTick();
				if(now_tick - frev_tick > 500){
					frev_tick = now_tick;
					if(xy_check(7, 7 + strlen("UP"), 7, 9)){
						if(sel == 0 && set_time.year < 99)                                   set_time.year++;
						else if(sel == 1 && set_time.month < 12)                             set_time.month++;
						else if(sel == 2 && set_time.day < lastDay[set_time.month - 1])      set_time.day++;
						else if(sel == 3 && set_time.hour < 23)                              set_time.hour++;
						else if(sel == 4 && set_time.min < 59)                               set_time.min++;
						else if(sel == 5 && set_time.sec < 59)                               set_time.sec++;
					}
					else if(xy_check(12, 12 + strlen("DOWN"), 7, 9)){
						if(sel == 0 && set_time.year > 0)                                    set_time.year--;
						else if(sel == 1 && set_time.month > 1)                              set_time.month--;
						else if(sel == 2 && set_time.day > 1)                                set_time.day--;
						else if(sel == 3 && set_time.hour > 0)                               set_time.hour--;
						else if(sel == 4 && set_time.min > 0)                                set_time.min--;
						else if(sel == 5 && set_time.sec > 0)                                set_time.sec--;
					}
				}
			}
		}
	}
	time = set_time;
	time_set();
	buzM = 1;
	firF = 0;
}

void start_dis(void){
	LCD_Clear(set_bcolor);
	LCD_putsXY(3, 4, "< Skill Competition Task 3 >", set_tcolor, set_bcolor);
	LCD_putsXY(7, 6, "Audio Interface", set_tcolor, set_bcolor);
	HAL_Delay(2000);
}

void start(void){
	uint8_t state = checkReset_status();

	start_dis();

	if(state) time_setting(init_time);
	else LCD_Clear(set_bcolor);
	__HAL_RCC_CLEAR_RESET_FLAGS();
}

void main_mode(void){
	if(!firF){
		firF = 1;
		XYsetZero;
		touched_check = have_xy;

		LCD_Clear(set_bcolor);

		for(uint8_t i = 0 ; i < 17 ; i++){
			txt_color[i] = set_tcolor;
			back_color[i] = set_bcolor;
		}
	}
	time_get();
	char bf[40];
	POS pos[4] = {{0, 1}, {2, 4}, {2, 7}, {15, 1}};
	sprintf(bf, "%04d.%02d.%02d %02d:%02d:%02d", 2000 + time.year, time.month, time.day, time.hour, time.min, time.sec);

	char* array[4] = { bf, "1. Sound modulation", "2. Color Setting", "Back" };
	array_puts(pos, ">Main", array, txt_color, back_color, 4);

	if(touched() == none_xy && touched_check == have_xy) touched_check = none_xy;

	if((curXY.x > 0 || curXY.y > 0) && touched_check == none_xy){
		if(xy_check(2, 2 + strlen("1. Sound modulation") - 6, 4, 4 + 1))        {  reset_value(); modeF = sound_menu;   }
		else if(xy_check(2, 2 + strlen("2. Color Setting") - 6, 7, 7 + 1))      {  reset_value(); modeF = setting_menu; }
		else if(xy_check(0, strlen(bf) - 6, 1, 1 + 1))                          {  firF = 0; time_setting(load_time);   }
		else if(xy_check(15, 15 + strlen("Back"), 1, 1 + 1))                    {  reset_value(); system_reset();       }
		else if(xy_check(0, strlen(">Main"),0, 1))                              {  reset_value(); modeF = select_menu;  }
	}
}

void modulation_mode(void){
	if(!firF){
		firF = 1;

		XYsetZero;
		touched_check = have_xy;

		LCD_Clear(set_bcolor);

		for(uint8_t i = 0 ; i < 17 ; i++){
			txt_color[i] = set_tcolor;
			back_color[i] = set_bcolor;
		}
	}

	for(PITCH_STATE i = PITCH_STATE_0 ; i <= PITCH_STATE_10 ; i++){
		if(coor_pitch >= 60 + (14 * i) && coor_pitch <= 74 + (14 * i)){
			pitch_state = i;
			break;
		}
	}

	max_volume = coor_volume - 60;
	if(max_volume > 100) max_volume = 100;

	time_get();
	char bf[3][20];
	POS pos[6] = {{0, 1}, {15, 1}, {1, 4}, {1, 6}, {1, 8}, {5, 4},};
	sprintf(bf[0], "%04d.%02d.%02d %02d:%02d:%02d", 2000 + time.year, time.month, time.day, time.hour, time.min, time.sec);
	sprintf(bf[1], "Volume: %d   ", max_volume);
	sprintf(bf[2], "Pitch: %d   ", pitch_state);

	txt_color[5] = sound_start == speaker_off ? set_tcolor : set_bcolor;
	back_color[5] = sound_start == speaker_off ? set_bcolor : set_tcolor;

	char* array[6] = { bf[0], "Back", "Sound: ", bf[1], bf[2], sound_start == speaker_off ? "OFF " : "ON " };
	array_puts(pos, ">Modulation", array, txt_color, back_color, 6);

	/* Volume Bar Start */
	LCD_DrawHollowCircle(200, 60, 5, set_tcolor);
	LCD_DrawHollowCircle(200, 210, 5, set_tcolor);
	LCD_DrawVLine(200, 65, 141, set_tcolor);
	LCD_DrawFilledCircle(200, (uint16_t)coor_volume, 10, set_tcolor);
	/* Volume Bar End */

	/* Pitch Bar Start */
	LCD_DrawHollowCircle(250, 60, 5, set_tcolor);
	LCD_DrawHollowCircle(250, 210, 5, set_tcolor);
	LCD_DrawVLine(250, 65, 141, set_tcolor);
	LCD_DrawFilledCircle(250, (uint16_t)coor_pitch, 10, set_tcolor);
	/* Pitch Bar End */

	if(touched() == none_xy && touched_check == have_xy) touched_check = none_xy;

	if((curXY.x > 0 || curXY.y > 0) && touched_check == none_xy){
		if(xy_check(15, 15 + strlen("Back"), 1, 1 + 1)){
			VP_save();
			reset_value();
			buzM = 1;
		}
		else if(xy_check(0, strlen(bf[0]) - 6, 1, 1 + 1)) { firF = 0; time_setting(load_time); }
		else if(xy_check(5, 5 + strlen("OFF"), 4, 4 + 1)) {
			touched_check = have_xy;
//			sound_start = sound_start == speaker_off ? speaker_on : speaker_off;
			if(sound_start == speaker_off) sound_start = speaker_on;
			else sound_start = speaker_off;
		}
		if(curXY.y >= 60 && curXY.y <= 210){
			TS_POINT current_xy = curXY;
			if(curXY.x >= 190 && curXY.x <= 210){
				LCD_DrawFilledCircle(200, (uint16_t)coor_volume, 10, set_bcolor);
				coor_volume = (uint8_t)current_xy.y;
			}
			else if(curXY.x >= 240 && curXY.x <= 260){
				LCD_DrawFilledCircle(250, (uint16_t)coor_pitch, 10, set_bcolor);
				coor_pitch = (uint8_t)current_xy.y;
			}
		}
		else if(xy_check(0, strlen(">Modulation"),0, 1)) { VP_save(); reset_value(); modeF = select_menu; buzM = 1; }
	}
}

void setting_mode(void){
	static LCD_COLOR t_color = white, b_color = black;
	if(!firF){
		firF = 1;

		XYsetZero;
		touched_check = have_xy;

		LCD_Clear(set_bcolor);

		sel = 0;
		t_color = set_tcolor;
		b_color = set_bcolor;

		for(uint8_t i = 0 ; i < 17 ; i++){
			txt_color[i] = set_tcolor;
			back_color[i] = set_bcolor;
		}
	}

	time_get();
	char bf[3][20];
	POS pos[6] = {{0, 1}, {2, 4}, {2, 6}, {14, 4}, {14, 6}, {15, 1}};
	sprintf(bf[0], "%04d.%02d.%02d %02d:%02d:%02d", 2000 + time.year, time.month, time.day, time.hour, time.min, time.sec);

	back_color[1] = sel == 0 ? set_tcolor : set_bcolor;
	back_color[2] = sel == 1 ? set_tcolor : set_bcolor;

	txt_color[1] = sel == 0 ? set_bcolor : set_tcolor;
	txt_color[2] = sel == 1 ? set_bcolor : set_tcolor;

	txt_color[3] = t_color;
	txt_color[4] = b_color;

	sprintf(bf[1], "%s    ", t_color == black ? "Black" : t_color == blue ? "Blue" : t_color == green ? "Green" : t_color == red ? "Red" : t_color == yellow ? "Yellow" : "White");
	sprintf(bf[2], "%s    ", b_color == black ? "Black" : b_color == blue ? "Blue" : b_color == green ? "Green" : b_color == red ? "Red" : b_color == yellow ? "Yellow" : "White");

	for(uint8_t i = 0 ; i < 6 ; i++)
		LCD_DrawFilledRectangleCoord(RECT * i, 190, RECT * (i + 1), 240, lcd_color[i]);
	LCD_DrawFilledRectangleCoord(265, 190, 320, 240, white);

	char* array[6] = { bf[0], "Text Color:", "Background Color:", bf[1], bf[2], "Back" };
	array_puts(pos, ">Setting", array, txt_color, back_color, 6);

	if(touched() == none_xy && touched_check == have_xy) touched_check = none_xy;

	if((curXY.x > 0 || curXY.y > 0) && touched_check == none_xy){
		touched_check = have_xy;
		if(xy_check(15, 15 + strlen("Back"), 1, 1 + 1)){
			if(t_color != b_color){
				set_tcolor = t_color;
				set_bcolor = b_color;
			}
			color_save();
			reset_value();
			buzM = 1;
		}
		else if(xy_check(0, strlen(bf[0]) - 6, 1, 1 + 1)) { firF = 0; time_setting(load_time); }
		else if(xy_check(2, 2 + strlen("Text Color") - 6, 4, 4 + 1))       sel = 0;
		else if(xy_check(2, 2 + strlen("Background Color") - 6, 6, 6 + 1)) sel = 1;
		else if(curXY.y > 190){
			if(curXY.x < RECT)              {  if(sel == 0) t_color = black;    else b_color = black;  }
			else if(curXY.x < RECT * 2)     {  if(sel == 0) t_color = blue;     else b_color = blue;   }
			else if(curXY.x < RECT * 3)     {  if(sel == 0) t_color = green;    else b_color = green;  }
			else if(curXY.x < RECT * 4)     {  if(sel == 0) t_color = red;      else b_color = red;    }
			else if(curXY.x < RECT * 5)     {  if(sel == 0) t_color = yellow;   else b_color = yellow; }
			else if(curXY.x < RECT * 6)     {  if(sel == 0) t_color = white;    else b_color = white;  }
		}
		else if(xy_check(0, strlen(">Setting"), 0, 1)) { color_save(); reset_value(); modeF = select_menu; buzM = 1; }
	}
}

void select_mode(void){
	if(!firF){
		firF = 1;
		XYsetZero;
		touched_check = have_xy;

		LCD_Clear(set_bcolor);
	}

	for(uint8_t i = 0 ; i < 17 ; i++){
		if(i == 1 + backupM){
			txt_color[i] = set_bcolor;
			back_color[i] = set_tcolor;
		}
		else{
			txt_color[i] = set_tcolor;
			back_color[i] = set_bcolor;
		}
	}
	time_get();
	char bf[40];
	POS pos[4] = {{0, 1}, {0, 4}, {0, 6}, {0, 8}};
	sprintf(bf, "%04d.%02d.%02d %02d:%02d:%02d", 2000 + time.year, time.month, time.day, time.hour, time.min, time.sec);

	char* array[4] = { bf, "Main Menu", "Modulation Mode", "Setting Mode" };
	array_puts(pos, ">Mode Select", array, txt_color, back_color, 4);

	if(touched() == none_xy && touched_check == have_xy) touched_check = none_xy;

	if((curXY.x > 0 || curXY.y > 0) && touched_check == none_xy){
		if(xy_check(0, strlen(bf) - 6, 1, 1 + 1))                        { firF = 0; time_setting(load_time);   }
		else if(xy_check(0, strlen("Main Menu"), 4, 4 + 1))              { reset_value(); modeF = main_menu;    }
		else if(xy_check(0, strlen("Modulation Mode"), 6, 6 + 1))        { reset_value(); modeF = sound_menu;   }
		else if(xy_check(0, strlen("Setting Mode"), 8, 8 + 1))           { reset_value(); modeF = setting_menu; }
	}
}

void HAL_SYSTICK_Callback(void){
	if(buzM) buzC++;

	if(buzM == 1){
		if(buzC < 500) BUZ(1);
		else{
			buzM = buzC = 0;
			BUZ(0);
		}
	}

	if(touched() >= have_xy) getTouch_state = get_coordinate;
	else getTouch_state = reset_coordinate;
	curXY = getPoint(getTouch_state);
}

void (*main_fuc[4])(void) = { main_mode, modulation_mode, setting_mode, select_mode };

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{
	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_ADC_Init();
	MX_DAC_Init();
	MX_I2C1_Init();
	MX_DMA_Init();
	MX_TIM6_Init();
	MX_USART1_UART_Init();

	/* Initialize interrupts */
	MX_NVIC_Init();
	/* USER CODE BEGIN 2 */
	HAL_TIM_Base_Start_IT(&htim6);
	HAL_DAC_Start(&hdac, DAC_CHANNEL_1);
	HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 0);
	INIT_FT6206();
	FT6206_Begin(FT62XX_DEFAULT_THRESHOLD);

	LCD_Clear(set_bcolor);
	start();
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1)
	{
		main_fuc[modeF]();
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
	RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

	/** Configure the main internal regulator output voltage
	 */
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLMUL = RCC_PLLMUL_4;
	RCC_OscInitStruct.PLL.PLLDIV = RCC_PLLDIV_2;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
			|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
	{
		Error_Handler();
	}
	PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1|RCC_PERIPHCLK_I2C1;
	PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
	PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
	{
		Error_Handler();
	}
}

/**
 * @brief NVIC Configuration.
 * @retval None
 */
static void MX_NVIC_Init(void)
{
	/* TIM6_DAC_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
}

/**
 * @brief ADC Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC_Init(void)
{

	/* USER CODE BEGIN ADC_Init 0 */

	/* USER CODE END ADC_Init 0 */

	ADC_ChannelConfTypeDef sConfig = {0};

	/* USER CODE BEGIN ADC_Init 1 */

	/* USER CODE END ADC_Init 1 */

	/** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
	 */
	hadc.Instance = ADC1;
	hadc.Init.OversamplingMode = DISABLE;
	hadc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
	hadc.Init.Resolution = ADC_RESOLUTION_12B;
	hadc.Init.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
	hadc.Init.ScanConvMode = ADC_SCAN_DIRECTION_FORWARD;
	hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
	hadc.Init.ContinuousConvMode = DISABLE;
	hadc.Init.DiscontinuousConvMode = DISABLE;
	hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
	hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
	hadc.Init.DMAContinuousRequests = DISABLE;
	hadc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
	hadc.Init.Overrun = ADC_OVR_DATA_PRESERVED;
	hadc.Init.LowPowerAutoWait = DISABLE;
	hadc.Init.LowPowerFrequencyMode = DISABLE;
	hadc.Init.LowPowerAutoPowerOff = DISABLE;
	if (HAL_ADC_Init(&hadc) != HAL_OK)
	{
		Error_Handler();
	}

	/** Configure for the selected ADC regular channel to be converted.
	 */
	sConfig.Channel = ADC_CHANNEL_0;
	sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
	if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN ADC_Init 2 */

	/* USER CODE END ADC_Init 2 */

}

/**
 * @brief DAC Initialization Function
 * @param None
 * @retval None
 */
static void MX_DAC_Init(void)
{

	/* USER CODE BEGIN DAC_Init 0 */

	/* USER CODE END DAC_Init 0 */

	DAC_ChannelConfTypeDef sConfig = {0};

	/* USER CODE BEGIN DAC_Init 1 */

	/* USER CODE END DAC_Init 1 */

	/** DAC Initialization
	 */
	hdac.Instance = DAC;
	if (HAL_DAC_Init(&hdac) != HAL_OK)
	{
		Error_Handler();
	}

	/** DAC channel OUT1 config
	 */
	sConfig.DAC_Trigger = DAC_TRIGGER_NONE;
	sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
	if (HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_1) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN DAC_Init 2 */

	/* USER CODE END DAC_Init 2 */

}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void)
{

	/* USER CODE BEGIN I2C1_Init 0 */

	/* USER CODE END I2C1_Init 0 */

	/* USER CODE BEGIN I2C1_Init 1 */

	/* USER CODE END I2C1_Init 1 */
	hi2c1.Instance = I2C1;
	hi2c1.Init.Timing = 0x00100413;
	hi2c1.Init.OwnAddress1 = 0;
	hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c1.Init.OwnAddress2 = 0;
	hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
	hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c1) != HAL_OK)
	{
		Error_Handler();
	}

	/** Configure Analogue filter
	 */
	if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
	{
		Error_Handler();
	}

	/** Configure Digital filter
	 */
	if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
	{
		Error_Handler();
	}

	/** I2C Fast mode Plus enable
	 */
	HAL_I2CEx_EnableFastModePlus(I2C_FASTMODEPLUS_I2C1);
	/* USER CODE BEGIN I2C1_Init 2 */

	/* USER CODE END I2C1_Init 2 */

}

/**
 * @brief TIM6 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM6_Init(void)
{

	/* USER CODE BEGIN TIM6_Init 0 */

	/* USER CODE END TIM6_Init 0 */

	TIM_MasterConfigTypeDef sMasterConfig = {0};

	/* USER CODE BEGIN TIM6_Init 1 */

	/* USER CODE END TIM6_Init 1 */
	htim6.Instance = TIM6;
	htim6.Init.Prescaler = 4-1;
	htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim6.Init.Period = 600-1;
	htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
	{
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN TIM6_Init 2 */

	/* USER CODE END TIM6_Init 2 */

}

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init(void)
{

	/* USER CODE BEGIN USART1_Init 0 */

	/* USER CODE END USART1_Init 0 */

	/* USER CODE BEGIN USART1_Init 1 */

	/* USER CODE END USART1_Init 1 */
	huart1.Instance = USART1;
	huart1.Init.BaudRate = 115200;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart1.Init.OverSampling = UART_OVERSAMPLING_16;
	huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart1) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN USART1_Init 2 */

	/* USER CODE END USART1_Init 2 */

}

/**
 * Enable DMA controller clock
 */
static void MX_DMA_Init(void)
{

	/* DMA controller clock enable */
	__HAL_RCC_DMA1_CLK_ENABLE();

	/* DMA interrupt init */
	/* DMA1_Channel1_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(BUZ_GPIO_Port, BUZ_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin : BUZ_Pin */
	GPIO_InitStruct.Pin = BUZ_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(BUZ_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
// If this Code don`t play you need annotation inversion for 'value'

	if(htim->Instance == TIM6){
		HAL_ADC_Start_DMA(&hadc, &adcV, 1);

		pitch_set();
		if(++tick > (adcV / 4) * pitch){
			tick = 0;
//			if(value > 0) value = 0;
//			else value = (uint32_t)max_volume * 40;
			value = value > 0 ? 0 : (uint32_t)max_volume * 40;
		}

		if(sound_start == speaker_on && SOUND_IN) HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, value);
	}
}

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1)
	{
		HAL_DeInit();
		HAL_NVIC_SystemReset();
	}
	/* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
	/* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	/* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
