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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "stm32l0xx_hal.h"
#include "sht41.h"
#include "ens160.h"
#include "drv8830.h"
#include "NEXTION.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef enum{
	buz_off,
	buz_on,
}BUZ_POWER_Typedef;

typedef enum{
	power_set_state,
	temp_set_state,
	alarm_set_state,
}LOWER_MENU_Typedef;

typedef enum{
	none,
	up_slide,
	down_slide,
}SLIDE_Typedef;

typedef enum{
	get_sht41,
	get_ens160,
}GET_SENSOR_Typedef;

typedef enum{
	a,
	b,
	c,
	d,
	e,
	f,
	g,
}SEGMENT_Typedef;

typedef enum{
	temp_display,
	alarm_display,
}DISPLAY_MENU_Typedef;

typedef enum{
	off,
	on,
}AIRCON_POWER_Typedef;

typedef enum{
	cooling,
	dehumidity,
	ventilation,
	heating,
	on_alarm,
	off_alarm,
}MODE_Typedef;

typedef enum{
	sht41_temp,
	sht41_hum,
	ens160_co2,
	max_addr,
}SENSOR_ADDR_Typedef;

typedef enum{
	buz_button,
	select_button,
	temp_button,
	wind_button,
	alarm_on_button,
	alarm_off_button,
	alarm_ok_button,
}BUTTON_ADDR_Typedef;

typedef struct{
	uint16_t x;
	uint16_t y;
	uint8_t touched;
}POS_Typedef;

typedef struct{
	uint16_t x0, y0;
	uint16_t x1, y1;
}AREA_Typedef;

typedef struct{
	AREA_Typedef area;
	uint16_t data;
	char* color;
}SENSOR_Typedef;

typedef struct{
	AREA_Typedef area;
	AREA_Typedef left_area, right_area;
	char* first_stirng;
	char* second_string;
	char* color;
}BUTTON_DATA_Typedef;

typedef struct{
	uint8_t on_time;
	uint8_t off_time;
	uint8_t alarm_on_f;
	uint8_t alarm_off_f;
}ALARM_Typedef;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
char bf[100];

uint8_t segment_data[11][7] = {
		{ 1, 1, 1, 1, 1, 1, 0 },  // means segment '0'
		{ 0, 1, 1, 0, 0, 0, 0 },  // means segment '1'
		{ 1, 1, 0, 1, 1, 0, 1 },  // means segment '2'
		{ 1, 1, 1, 1, 0, 0, 1 },  // means segment '3'
		{ 0, 1, 1, 0, 0, 1, 1 },  // means segment '4'
		{ 1, 0, 1, 1, 0, 1, 1 },  // means segment '5'
		{ 0, 0, 1, 1, 1, 1, 1 },  // means segment '6'
		{ 1, 1, 1, 0, 0, 0, 0 },  // means segment '7'
		{ 1, 1, 1, 1, 1, 1, 1 },  // means segment '8'
		{ 1, 1, 1, 0, 0, 1, 1 },  // means segment '9'
		{ 0, 0, 0, 0, 0, 0, 0 },  // means segment 'all off'
};

uint8_t motor_power[5] = { 0x0D, 0x19, 0x25, 0x32, 0x3F };

uint8_t buzM = 0;
uint8_t screen_update = 0;

POS_Typedef curXY = { 0, 0, 0 };
AIRCON_POWER_Typedef power = off;
SLIDE_Typedef slide_state = none;
LOWER_MENU_Typedef lower_menu = power_set_state;
BUZ_POWER_Typedef buz_state = on;

SENSOR_Typedef sensor[3] = {
		{ { 0,   30, 0   + 160, 30 + 30 }, 0, "RED"  },
		{ { 160, 30, 160 + 160, 30 + 30 }, 0, "BLUE" },
		{ { 320, 30, 320 + 160, 30 + 30 }, 0, "GRAY" },
};

/* button x range: 80 y range: 40 */
/* if button use only one text use struct member first_string only */
BUTTON_DATA_Typedef button[7] = {
		/* power set button */
		{ { 50,  205, 50  + 80,  205 + 40 }, { /*      not use      */ }, { /*      not use      */ }, "Buzz",      "on/off", "65529" },
		{ { 355, 205, 355 + 80,  205 + 40 }, { /*      not use      */ }, { /*      not use      */ }, "Select",    "mode",   "65529" },

		/* temperature set button */
		{ { 20,  205, 20  + 140, 205 + 40 }, { 25,  210, 25  + 10, 240 }, { 145, 210, 145 + 10, 240 }, "Temp",      "",       "65529" },
		{ { 320, 205, 320 + 140, 205 + 40 }, { 325, 210, 325 + 10, 240 }, { 445, 210, 445 + 10, 240 }, "Wind",      "",       "65529" },

		/* alarm set button */
		{ { 30,  205, 30  + 100, 205 + 40 }, { /*      not use      */ }, { /*      not use      */ }, "On alarm",  "",       "65529" },
		{ { 190, 205, 190 + 100, 205 + 40 }, { /*      not use      */ }, { /*      not use      */ }, "Off alarm", "",       "65529" },
		{ { 350, 205, 350 + 100, 205 + 40 }, { /*      not use      */ }, { /*      not use      */ }, "OK/Cancel", "",       "65529" },
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ----------------------------------------------*/
/* USER CODE BEGIN 0 */

/* if you motor run control you set drv8830 memory */
void motor_set(uint8_t voltage, uint8_t control){
	uint8_t set_command = voltage << 2 | control;
	HAL_I2C_Mem_Write(&hi2c1, DRV8830_DeviceAddress, DRV8830_CONTROL, 1, &set_command, 1, 100);
}

void motor_drive(uint8_t* state, uint8_t* power){
	if(*state == 1) motor_set(motor_power[*power], DRV8830_CW);
	else            motor_set(motor_power[*power], DRV8830_STOP);
}

/* BUZZER run function */
void BUZ(BUZ_POWER_Typedef state){
	if(state == buz_on) HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
	else                HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
}

/* return as memory have a values */
void* get_sensor(GET_SENSOR_Typedef mem){
	if(mem == get_sht41){
		SHT41_t* buf = (SHT41_t*)malloc(sizeof(SHT41_t));
		*buf = getTempSht41();
		return buf;
	}
	else{
		uint16_t* buf = (uint16_t*)malloc(sizeof(uint16_t));
		*buf = getCO2();
		return buf;
	}
}

/* sensor values memory free and NULL reset */
void free_addr(void** address){
	free(*address);
	*address = NULL;
}

/* nextion instruction set */
void nextion_inst_set(char* str){
	uint8_t end_cmd[3] = { 0xFF, 0xFF, 0xFF };
	HAL_UART_Transmit(&huart1, (uint8_t*)str, strlen(str), 100);
	HAL_UART_Transmit(&huart1, end_cmd, 3, 100);
}

/* get nextion LCD touch coordinate */
void get_touch(POS_Typedef* buf){
	HAL_StatusTypeDef res = HAL_OK;
	uint8_t rx_dat[8] = { 0, };

	nextion_inst_set("get tch0");
	res = HAL_UART_Receive(&huart1, rx_dat, 8, 100);
	if(res == HAL_OK) { if(rx_dat[0] == 0x71) { buf->x = rx_dat[2] << 8 | rx_dat[1]; } }

	nextion_inst_set("get tch1");
	res = HAL_UART_Receive(&huart1, rx_dat, 8, 100);
	if(res == HAL_OK) { if(rx_dat[0] == 0x71) { buf->y = rx_dat[2] << 8 | rx_dat[1]; } }

	if(buf->x > 0 && buf->y > 0) buf->touched = 1;
	else buf->touched = 0;
}

/* slide state set */
void get_slide(SLIDE_Typedef* buf, POS_Typedef* xy){
	static POS_Typedef befo_touch       = { 0, 0, 0 }; // previous touch coordinate
	static POS_Typedef touch_coordinate = { 0, 0, 0 }; // save first touch coordinate

	if(befo_touch.touched != xy->touched){
		/* if release touch this function run */
		if(xy->touched == 0){
			if(touch_coordinate.y > befo_touch.y)       *buf = (touch_coordinate.y - befo_touch.y > 30) ? up_slide   : none;
			else if(befo_touch.y  > touch_coordinate.y) *buf = (befo_touch.y - touch_coordinate.y > 30) ? down_slide : none;
		}
		/* if touch this function run */
		else touch_coordinate = *xy;
	}
	else *buf  = none;
	befo_touch = *xy;
}

/* area touch check */
uint8_t area_check(POS_Typedef* curXY, AREA_Typedef* area){
	if(curXY->x >= area->x0 - 10 && curXY->x <= area->x1 + 10){
		if(curXY->y >= area->y0 - 10 && curXY->y <= area->y1 + 10){
			screen_update = (screen_update == 1) ? 0 : screen_update;
			buzM = (buzM == 0) ? 1 : buzM;
			return 1;
		}
	}
	return 0;
}

/* segment draw function */
void segment_draw(POS_Typedef* xy, uint8_t* data){
	/* segment 'a' draw */
	sprintf(bf, "fill %d,%d,30,10,%s", xy->x + 10, xy->y, data[a] == 1 ? "BLACK" : "WHITE");
	nextion_inst_set(bf);

	/* segment 'b' draw */
	sprintf(bf, "fill %d,%d,10,30,%s", xy->x + 40, xy->y + 10, data[b] == 1 ? "BLACK" : "WHITE");
	nextion_inst_set(bf);

	/* segment 'c' draw */
	sprintf(bf, "fill %d,%d,10,30,%s", xy->x + 40, xy->y + 50, data[c] == 1 ? "BLACK" : "WHITE");
	nextion_inst_set(bf);

	/* segment 'd' draw */
	sprintf(bf, "fill %d,%d,30,10,%s", xy->x + 10, xy->y + 80, data[d] == 1 ? "BLACK" : "WHITE");
	nextion_inst_set(bf);

	/* segment 'e' draw */
	sprintf(bf, "fill %d,%d,10,30,%s", xy->x, xy->y + 50, data[e] == 1 ? "BLACK" : "WHITE");
	nextion_inst_set(bf);

	/* segment 'f' draw */
	sprintf(bf, "fill %d,%d,10,30,%s", xy->x, xy->y + 10, data[f] == 1 ? "BLACK" : "WHITE");
	nextion_inst_set(bf);

	/* segment 'g' draw */
	sprintf(bf, "fill %d,%d,30,10,%s", xy->x + 10, xy->y + 40, data[g] == 1 ? "BLACK" : "WHITE");
	nextion_inst_set(bf);
}

/* segment draw all in one */
void value_put(uint8_t* data, DISPLAY_MENU_Typedef menu, POS_Typedef* xy){
	POS_Typedef buf = *xy;

	if(*data / 10 > 0) segment_draw(&buf, (uint8_t*)&segment_data[*data / 10]);
	else               segment_draw(&buf, (uint8_t*)&segment_data[10]);
	buf.x += 60;
	segment_draw(&buf, (uint8_t*)&segment_data[*data % 10]);

	buf.x += 60;
	if(menu == temp_display) sprintf(bf, "xstr %d,%d,50,90,0,BLACK,WHITE,0,0,1,\"%cC\"", buf.x, buf.y, 0xb0);
	else                     sprintf(bf, "xstr %d,%d,50,90,0,BLACK,WHITE,0,3,1,\"s later\"", buf.x, buf.y);
	nextion_inst_set(bf);
}

/* basic y range 20, 20++ */
/* member to member distance is 20pixcel */
void wind_power_draw(uint8_t* data){
	/* sky blue 22271 */

	/* 1. figure */
	sprintf(bf, "fill 320,170-20,10,20,%s", *data >= 0 ? "22271" : "WHITE");
	nextion_inst_set(bf);
	sprintf(bf, "cirs 325,170-20,5,%s",     *data >= 0 ? "22271" : "WHITE");
	nextion_inst_set(bf);

	/* 2. figure */
	sprintf(bf, "fill 340,170-30,10,30,%s", *data >= 1 ? "22271" : "WHITE");
	nextion_inst_set(bf);
	sprintf(bf, "cirs 345,170-30,5,%s",     *data >= 1 ? "22271" : "WHITE");
	nextion_inst_set(bf);

	/* 3. figure */
	sprintf(bf, "fill 360,170-40,10,40,%s", *data >= 2 ? "22271" : "WHITE");
	nextion_inst_set(bf);
	sprintf(bf, "cirs 365,170-40,5,%s",     *data >= 2 ? "22271" : "WHITE");
	nextion_inst_set(bf);

	/* 4. figure */
	sprintf(bf, "fill 380,170-50,10,50,%s", *data >= 3 ? "22271" : "WHITE");
	nextion_inst_set(bf);
	sprintf(bf, "cirs 385,170-50,5,%s",     *data >= 3 ? "22271" : "WHITE");
	nextion_inst_set(bf);

	/* 5. figure */
	sprintf(bf, "fill 400,170-60,10,60,%s", *data >= 4 ? "22271" : "WHITE");
	nextion_inst_set(bf);
	sprintf(bf, "cirs 405,170-60,5,%s",     *data >= 4 ? "22271" : "WHITE");
	nextion_inst_set(bf);
}

/* power switch draw */
void power_switch_draw(void){
	nextion_inst_set("fill 180,180,130,92,65093");

	nextion_inst_set("cir 245,226,20,BLACK");

	nextion_inst_set("fill 245-10,226-30,20,30,65093");
	nextion_inst_set("fill 245-5,226-25,10,25,BLACK");

	nextion_inst_set("draw 180,180,180+130,272,BLACK");
}

/* button draw merge (lower menu button`s draw) */
void button_draw(BUTTON_DATA_Typedef* data,	LOWER_MENU_Typedef state){
	/* have two floor button draw */
	if(state == power_set_state){
		sprintf(bf, "xstr %d,%d,80,20,0,BLACK,%s,1,1,1,\"%s\"", data->area.x0, data->area.y0, data->color, data->first_stirng);
		nextion_inst_set(bf);

		sprintf(bf, "xstr %d,%d,80,20,0,BLACK,%s,1,1,1,\"%s\"", data->area.x0, data->area.y0 + 20, data->color, data->second_string);
		nextion_inst_set(bf);
	}
	/* arrow switch draw */
	else if(state == temp_set_state){
		/* text draw */
		sprintf(bf, "xstr %d,%d,140,40,0,BLACK,%s,1,1,1,\"%s\"", data->area.x0, data->area.y0, data->color, data->first_stirng);
		nextion_inst_set(bf);

		/* left arrow draw */
		sprintf(bf, "line %d,%d,%d,%d,BLACK", data->left_area.x0, (data->left_area.y0 + data->left_area.y1) / 2, data->left_area.x1, data->left_area.y0);
		nextion_inst_set(bf);

		sprintf(bf, "line %d,%d,%d,%d,BLACK", data->left_area.x0, (data->left_area.y0 + data->left_area.y1) / 2, data->left_area.x1, data->left_area.y1);
		nextion_inst_set(bf);

		sprintf(bf, "line %d,%d,%d,%d,BLACK", data->left_area.x1, data->left_area.y0, data->left_area.x1, data->left_area.y1);
		nextion_inst_set(bf);

		/* right arrow draw */
		sprintf(bf, "line %d,%d,%d,%d,BLACK", data->right_area.x0, data->right_area.y0, data->right_area.x1, (data->right_area.y0 + data->right_area.y1) / 2);
		nextion_inst_set(bf);

		sprintf(bf, "line %d,%d,%d,%d,BLACK", data->right_area.x0, data->right_area.y1, data->right_area.x1, (data->right_area.y0 + data->right_area.y1) / 2);
		nextion_inst_set(bf);

		sprintf(bf, "line %d,%d,%d,%d,BLACK", data->right_area.x0, data->right_area.y0, data->right_area.x0, data->right_area.y1);
		nextion_inst_set(bf);
	}
	/* basic button draw */
	else if(state == alarm_set_state){
		sprintf(bf, "xstr %d,%d,100,40,0,BLACK,%s,1,1,1,\"%s\"", data->area.x0, data->area.y0, data->color, data->first_stirng);
		nextion_inst_set(bf);
	}

	sprintf(bf, "draw %d,%d,%d,%d,BLACK", data->area.x0, data->area.y0, data->area.x1, data->area.y1);
	nextion_inst_set(bf);
}

/* power state button */
void lower_power(void){
	power_switch_draw();
	button_draw(&button[buz_button],    lower_menu);
	button_draw(&button[select_button], lower_menu);
}

/* arrow state button */
void lower_temp(void){
	button_draw(&button[temp_button], lower_menu);
	button_draw(&button[wind_button], lower_menu);
}

/* basic state button */
void lower_alarm(void){
	button_draw(&button[alarm_on_button],  lower_menu);
	button_draw(&button[alarm_off_button], lower_menu);
	button_draw(&button[alarm_ok_button],  lower_menu);
}

/* button function merge */
void (*lower_menu_fuc[3])(void) = { lower_power, lower_temp, lower_alarm };

/* merge all display function */
void basic_screen(MODE_Typedef* alarm, MODE_Typedef* color, uint8_t* data, uint8_t* wind_data){
	/* mode title display */
	sprintf(bf, "xstr 0,0,120,30,0,%s,WHITE,1,1,1,\"cooling\"",       *color == cooling     ? "RED" : "BLACK");
	nextion_inst_set(bf);

	sprintf(bf, "xstr 120,0,120,30,0,%s,WHITE,1,1,1,\"Dehumidify\"",  *color == dehumidity  ? "RED" : "BLACK");
	nextion_inst_set(bf);

	sprintf(bf, "xstr 240,0,120,30,0,%s,WHITE,1,1,1,\"ventilation\"", *color == ventilation ? "RED" : "BLACK");
	nextion_inst_set(bf);

	sprintf(bf, "xstr 360,0,120,30,0,%s,WHITE,1,1,1,\"heating\"",     *color == heating     ? "RED" : "BLACK");
	nextion_inst_set(bf);

	if(power == off) nextion_inst_set("fill 0,0,480,30,WHITE");

	/* get sensor value */
	SHT41_t*  sht41_value  = get_sensor(get_sht41);
	uint16_t* ens160_value = get_sensor(get_ens160);

	sensor[sht41_temp].data = (uint16_t)sht41_value->temperature;
	sensor[sht41_hum].data  = (uint16_t)sht41_value->humidity;
	sensor[ens160_co2].data = *ens160_value;

	free_addr((void*)&sht41_value);
	free_addr((void*)&ens160_value);

	/* sensor value display */
	for(SENSOR_ADDR_Typedef i = sht41_temp ; i < max_addr ; i++){
		if(i == sht41_temp){
			/* sensor value draw (temperature) */
			if(power == on) sprintf(bf, "xstr %d,%d,160,30,0,BLACK,%s,1,1,1,\"%d%cC\"", sensor[i].area.x0, sensor[i].area.y0, sensor[i].color, sensor[i].data, 0xb0);
			else            sprintf(bf, "fill %d,%d,160,30,%s", sensor[i].area.x0, sensor[i].area.y0, sensor[i].color);
		}
		/* sensor value draw (humidity) */
		else if(i == sht41_hum){
			if(power == on) sprintf(bf, "xstr %d,%d,160,30,0,BLACK,%s,1,1,1,\"%d%%\"", sensor[i].area.x0, sensor[i].area.y0, sensor[i].color, sensor[i].data);
			else            sprintf(bf, "fill %d,%d,160,30,%s", sensor[i].area.x0, sensor[i].area.y0, sensor[i].color);
		}
		/* sensor value draw (co2) */
		else if(i == ens160_co2){
			if(power == on) sprintf(bf, "xstr %d,%d,160,30,0,BLACK,%s,1,1,1,\"%dppm\"", sensor[i].area.x0, sensor[i].area.y0, sensor[i].color, sensor[i].data);
			else            sprintf(bf, "fill %d,%d,160,30,%s", sensor[i].area.x0, sensor[i].area.y0, sensor[i].color);
		}
		nextion_inst_set(bf);

		/* sensor outline draw */
		sprintf(bf, "draw %d,%d,%d,%d,BLACK", sensor[i].area.x0, sensor[i].area.y0, sensor[i].area.x1, sensor[i].area.y1);
		nextion_inst_set(bf);
	}

	/* middle menu draw */
	POS_Typedef draw_pos = { 150, 80, 0 };
	if(power == on) {
		if(*alarm != ventilation) value_put(data, *alarm == on_alarm || *alarm == off_alarm ? alarm_display : temp_display, &draw_pos);
		else                      nextion_inst_set("fill 150,61,169,118,WHITE");
		wind_power_draw(wind_data);
	}
	else if(*alarm == on_alarm || *alarm == off_alarm) value_put(data, alarm_display, &draw_pos);
	else nextion_inst_set("fill 0,61,480,118,WHITE");

	/* left middle menu */
	if(power == on || *alarm == on_alarm || *alarm == off_alarm){
		nextion_inst_set("xstr 0,60,90,40,0,WHITE,BLACK,1,1,1,\"desired\"");
		nextion_inst_set("draw 0,60,90,60+40,BLACK");

		sprintf(bf, "xstr 0,100,90,40,0,%s,%s,1,1,1,\"on\"", *alarm == on_alarm ? "WHITE" : "BLACK", *alarm == on_alarm ? "BLACK" : "WHITE");
		nextion_inst_set(bf);
		nextion_inst_set("draw 0,100,90,100+40,BLACK");

		sprintf(bf, "xstr 0,140,90,40,0,%s,%s,1,1,1,\"off\"", *alarm == off_alarm ? "WHITE" : "BLACK", *alarm == off_alarm ? "BLACK" : "WHITE");
		nextion_inst_set(bf);
		nextion_inst_set("draw 0,140,90,140+40,BLACK");
	}

	/* lower menu draw */
	nextion_inst_set("fill 0,180,480,92,WHITE");
	nextion_inst_set("line 0,180,480,180,BLACK");
	lower_menu_fuc[lower_menu]();
}

/* all task function */
/* all display function is merge in this function */
void task_fuc(void){
	uint32_t tick       = 0;
	uint32_t buz_tick   = 0;
	uint32_t alarm_tick = 0;

	MODE_Typedef mode      = cooling;
	MODE_Typedef befo_mode = mode;

	uint8_t befo_touch = 0;

	uint8_t set_temp   = 18;
	uint8_t wind_power = 0, motorM = 0;

	AREA_Typedef power_area  = { 180, 180, 180+130, 180+92 };
	ALARM_Typedef alarm_time = { 1, 1, 0, 0 };

	/* LCD clear (color: WHITE) */
	nextion_inst_set("cls WHITE");
	nextion_inst_set("cls WHITE");
	nextion_inst_set("cls WHITE");

	while(1){
		/* get touch coordinate */
		get_touch(&curXY);

		/* get slide state */
		get_slide(&slide_state, &curXY);

		if(mode != on_alarm && mode != off_alarm) befo_mode = mode;

		/* screen update */
		if(screen_update == 0){
			screen_update = 1;

			basic_screen(&mode, &befo_mode, mode == on_alarm ? &alarm_time.on_time : mode == off_alarm ? &alarm_time.off_time : &set_temp, &wind_power);
		}

		/* when this function run if you touched LCD */
		if(curXY.touched != befo_touch && curXY.touched == 1){
			/* touch function for each menu */

			/* buzzer on/off, power on/off, mode change button */
			if(lower_menu == power_set_state){
				if(area_check(&curXY, &power_area)){
					power = power == off ? on : off;
					mode = cooling;
					wind_power = 0;
				}
				else if(area_check(&curXY, &button[buz_button].area)) buz_state = buz_state == buz_on ? buz_off : buz_on;
				else if(area_check(&curXY, &button[select_button].area)){
					mode = mode == heating ? 0 : mode + 1;
					wind_power = (mode == ventilation && wind_power != 1) ? 1 : wind_power;
				}
			}
			/* adjust desired temperature value, adjust wind power button */
			else if(lower_menu == temp_set_state){
				/* temp button check */
				if(area_check(&curXY, &button[temp_button].left_area))       { set_temp = (set_temp > 18) ? set_temp - 1 : set_temp; }
				else if(area_check(&curXY, &button[temp_button].right_area)) { set_temp = (set_temp < 30) ? set_temp + 1 : set_temp; }

				/* wind button check */
				if(area_check(&curXY, &button[wind_button].left_area))       { wind_power = (wind_power > 0) ? wind_power - 1 : wind_power; }
				else if(area_check(&curXY, &button[wind_button].right_area)) { wind_power = (wind_power < 4) ? wind_power + 1 : wind_power; }
			}
			/* adjust power on reservation time, adjust power off reservation, save setting button */
			else if(lower_menu == alarm_set_state){
				if(area_check(&curXY, &button[alarm_on_button].area)){
					if(mode != on_alarm) {
						if(alarm_time.alarm_on_f == 0)  { alarm_time.alarm_on_f  = 1; alarm_time.on_time = 1; }
						mode = on_alarm;
					}
					else alarm_time.on_time = alarm_time.on_time < 20 ? alarm_time.on_time + 1 : 1;
				}
				else if(area_check(&curXY, &button[alarm_off_button].area)){
					if(mode != off_alarm) {
						if(alarm_time.alarm_off_f == 0) { alarm_time.alarm_off_f = 1; alarm_time.off_time = 1; }
						mode = off_alarm;
					}
					else alarm_time.off_time = alarm_time.off_time < 20 ? alarm_time.off_time + 1 : 1;
				}
				else if(area_check(&curXY, &button[alarm_ok_button].area)) mode = befo_mode;
			}
		}

		/* slide function */
		if(slide_state != none){
			screen_update = 0;

			if(slide_state == up_slide)        lower_menu = (lower_menu == alarm_set_state) ? lower_menu = power_set_state : lower_menu + 1;
			else if(slide_state == down_slide) lower_menu = (lower_menu == power_set_state) ? lower_menu = alarm_set_state : lower_menu - 1;
		}


		/* run motor function */
		if(power == on){
			if(mode == cooling)          motorM = sensor[sht41_temp].data > set_temp ? 1 : 0;
			else if(mode == dehumidity)  motorM = sensor[sht41_hum].data >= 70 ? 1 : 0;
			else if(mode == ventilation) motorM = motorM == 0 ? 1 : motorM;
			else if(mode == heating)     motorM = sensor[sht41_temp].data < set_temp ? 1 : 0;
		}
		else if(motorM == 1) motorM = 0;

		motor_drive(&motorM, &wind_power);

		/* function that use time */
		if(HAL_GetTick() - tick >= 500){
			tick = HAL_GetTick();
			screen_update = 0;
		}

		if(buzM == 1){
			if(HAL_GetTick() - buz_tick < 100){
				BUZ(buz_state);
			}
			else { buzM = 0; buz_tick = HAL_GetTick(); BUZ(0); }
		}
		else buz_tick = HAL_GetTick();

		if(mode != on_alarm && mode != off_alarm && (alarm_time.alarm_on_f == 1 || alarm_time.alarm_off_f == 1)){
			if(HAL_GetTick() - alarm_tick > 1000){
				alarm_tick = HAL_GetTick();

				alarm_time.on_time  = alarm_time.on_time  > 0 ? alarm_time.on_time  - 1 : alarm_time.on_time;
				alarm_time.off_time = alarm_time.off_time > 0 ? alarm_time.off_time - 1 : alarm_time.off_time;

				if(alarm_time.on_time == 0 && alarm_time.alarm_on_f == 1)        { power = on;  alarm_time.alarm_on_f  = 0; screen_update = 0; }
				else if(alarm_time.off_time == 0 && alarm_time.alarm_off_f == 1) { power = off; alarm_time.alarm_off_f = 0; screen_update = 0; }
			}
		}
		else alarm_tick = HAL_GetTick();

		befo_touch = curXY.touched;

		/* touch coordinate and slide state display for test */
		/* sprintf(bf, "xstr 0,240,150,30,0,BLACK,WHITE,0,1,1,\"X:%d Y:%d\"", curXY.x, curXY.y); */
		/* nextion_inst_set(bf); */
	}
}
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
	MX_USART1_UART_Init();
	MX_TIM2_Init();
	MX_I2C1_Init();
	/* USER CODE BEGIN 2 */
	initEns160();
	initDrv8830();

	LED(1,0);
	LED(2,0);
	LED(3,0);

	nextion_inst_set("baud=921600");
	nextion_inst_set("baud=921600");
	nextion_inst_set("baud=921600");
	HAL_Delay(50);
	USART1->CR1 &= (~USART_CR1_UE);
	USART1->BRR = 0x23;
	USART1->CR1 |= USART_CR1_UE;
	HAL_Delay(1000);

	setMotor(DRV8830_STOP);
	task_fuc();
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1)
	{

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
	hi2c1.Init.Timing = 0x00300F38;
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
	/* USER CODE BEGIN I2C1_Init 2 */

	/* USER CODE END I2C1_Init 2 */

}

/**
 * @brief TIM2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM2_Init(void)
{

	/* USER CODE BEGIN TIM2_Init 0 */

	/* USER CODE END TIM2_Init 0 */

	TIM_MasterConfigTypeDef sMasterConfig = {0};
	TIM_OC_InitTypeDef sConfigOC = {0};

	/* USER CODE BEGIN TIM2_Init 1 */

	/* USER CODE END TIM2_Init 1 */
	htim2.Instance = TIM2;
	htim2.Init.Prescaler = 32-1;
	htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim2.Init.Period = 250-1;
	htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
	{
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
	{
		Error_Handler();
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 125-1;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN TIM2_Init 2 */

	/* USER CODE END TIM2_Init 2 */
	HAL_TIM_MspPostInit(&htim2);

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
	huart1.Init.BaudRate = 9600;
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
	HAL_GPIO_WritePin(GPIOA, LED1_Pin|LED2_Pin|LED3_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pins : SW1_Pin SW2_Pin SW3_Pin */
	GPIO_InitStruct.Pin = SW1_Pin|SW2_Pin|SW3_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pins : LED1_Pin LED2_Pin LED3_Pin */
	GPIO_InitStruct.Pin = LED1_Pin|LED2_Pin|LED3_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pins : LEFT_Pin RIGHT_Pin */
	GPIO_InitStruct.Pin = LEFT_Pin|RIGHT_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

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
