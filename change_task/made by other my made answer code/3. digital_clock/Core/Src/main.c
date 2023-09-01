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
#include <string.h>
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
	main_menu,
	timer_menu,
	stop_watch_menu,
}MODE_Typedef;

typedef enum{
	none,
	hour_input,
	min_input,
}TIME_INPUT_Typedef;

typedef enum{
	a, // 7-Segment 'a'
	b, // 7-Segment 'b'
	c, // 7-Segment 'c'
	d, // 7-Segment 'd'
	e, // 7-Segment 'e'
	f, // 7-Segment 'f'
	g, // 7-Segment 'g'
}SEGMENT_Typedef;

typedef enum{
	timer_button,
	watch_button,
	timer_start_button,
	stop_watch_reset_button,
	stop_watch_start_button,
}BUTTON_Typedef;

typedef struct{
	uint8_t time1; // left time number
	uint8_t time2; // right time number
}TIME_Typedef;

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
	char pad_num;
	AREA_Typedef area;
}KEYPAD_Typedef;

typedef struct{
	TIME_Typedef time;
	TIME_Typedef buf_time;
	uint8_t state;
	uint32_t tick;
}TIMER_MODE_Typedef;

typedef struct{
	AREA_Typedef area;
	char* title;
}BUTTON_DATA_Typedef;

typedef struct{
	AREA_Typedef area;
	char* color;
	char* name;
}BUTTON_DRAW_Typedef;

typedef struct{
	uint8_t laps;
	TIME_Typedef time;
	TIME_Typedef total;
}LOG_Typedef;

typedef struct{
	uint8_t state;
	uint32_t tick;
	TIME_Typedef time;
	LOG_Typedef log[3];
}STOP_WATCH_Typedef;
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

MODE_Typedef mode = main_menu;
TIME_INPUT_Typedef input_state = none;
POS_Typedef curXY = { 0, 0, 0 };
TIME_Typedef time = { 12, 0 }, set_time ={ 10, 10 };

LOG_Typedef log_data[3] = {
		{ 0, { 0, 0 }, { 0, 0 } },
		{ 0, { 0, 0 }, { 0, 0 } },
		{ 0, { 0, 0 }, { 0, 0 } },
};

TIMER_MODE_Typedef timer_mode = {
		{ 0, 0 }, { 0, 0 }, 0, 0,
};

STOP_WATCH_Typedef stop_watch_mode = {
		0, 0, { 0, 0 }, { { 0, { 0, 0 }, { 0, 0 } }, { 0, { 0, 0 }, { 0, 0 } }, { 0, { 0, 0 }, { 0, 0 } } }
};

BUTTON_DRAW_Typedef button_data[5] = {
		{ { 25, 30, 25 + 100, 30 + 30 },    "BLACK", "Timer"      },
		{ { 25, 80, 25 + 100, 80 + 30 },    "BLACK", "Stop Watch" },
		{ { 270, 230, 270 + 95, 230 + 30 }, "BLUE",  "Start",     },
		{ { 190, 230, 190 + 95, 230 + 30 }, "GRAY",  "Reset",     },
		{ { 345, 230, 345 + 95, 230 + 30 }, "BLUE",  "Start",     },
};

KEYPAD_Typedef keypad[10] = {
		{ '0', { 165, 150, 165 + 40, 150 + 40 }, },
		{ '1', { 225, 150, 225 + 40, 150 + 40 }, },
		{ '2', { 285, 150, 285 + 40, 150 + 40 }, },
		{ '3', { 345, 150, 345 + 40, 150 + 40 }, },
		{ '4', { 405, 150, 405 + 40, 150 + 40 }, },
		{ '5', { 165, 210, 165 + 40, 210 + 40 }, },
		{ '6', { 225, 210, 225 + 40, 210 + 40 }, },
		{ '7', { 285, 210, 285 + 40, 210 + 40 }, },
		{ '8', { 345, 210, 345 + 40, 210 + 40 }, },
		{ '9', { 405, 210, 405 + 40, 210 + 40 }, },
};

uint8_t big_num_font[11][7] = {
		{ 1, 1, 1, 1, 1, 1, 0 },   // meaning 7-segment '0'
		{ 0, 1, 1, 0, 0, 0, 0 },   // meaning 7-segment '1'
		{ 1, 1, 0, 1, 1, 0, 1 },   // meaning 7-segment '2'
		{ 1, 1, 1, 1, 0, 0, 1 },   // meaning 7-segment '3'
		{ 0, 1, 1, 0, 0, 1, 1 },   // meaning 7-segment '4'
		{ 1, 0, 1, 1, 0, 1, 1 },   // meaning 7-segment '5'
		{ 0, 0, 1, 1, 1, 1, 1 },   // meaning 7-segment '6'
		{ 1, 1, 1, 0, 0, 0, 0 },   // meaning 7-segment '7'
		{ 1, 1, 1, 1, 1, 1, 1 },   // meaning 7-segment '8'
		{ 1, 1, 1, 0, 0, 1, 1 },   // meaning 7-segment '9'
		{ 0, 0, 0, 0, 0, 0, 0 },   // all off
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

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void BUZ(uint8_t x){
	if(x) HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
	else  HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
}
void nextion_inst_set(char* str){
	uint8_t end_cmd[3] = { 0xFF, 0xFF, 0xFF };
	HAL_UART_Transmit(&huart1, (uint8_t*)str, strlen(str), 100);
	HAL_UART_Transmit(&huart1, end_cmd, 3, 100);
}

void get_touch(POS_Typedef* buf){
	HAL_StatusTypeDef res = HAL_OK;
	uint8_t rx_dat[8] = { 0, };

	nextion_inst_set("get tch0");
	HAL_UART_Receive(&huart1, rx_dat, 8, 100);
	if(res == HAL_OK) { if(rx_dat[0] == 0x71) buf->x = rx_dat[2] << 8 | rx_dat[1]; }

	nextion_inst_set("get tch1");
	HAL_UART_Receive(&huart1, rx_dat, 8, 100);
	if(res == HAL_OK) { if(rx_dat[0] == 0x71) buf->y = rx_dat[2] << 8 | rx_dat[1]; }

	if(buf->x > 0 && buf->y > 0) buf->touched = 1;
	else buf->touched = 0;
}

uint8_t area_check(POS_Typedef* pos, AREA_Typedef* area){
	if(pos->x >= area->x0 && pos->x <= area->x1){
		if(pos->y >= area->y0 && pos->y <= area->y1){
			return 1;
		}
	}
	return 0;
}

uint8_t keypad_read(void){
	for(uint8_t i = 0 ; i < 10 ; i++){
		if(area_check(&curXY, &keypad[i].area)){
			return i + 1;
		}
	}
	return 0;
}

void number_print(POS_Typedef* print_xy, uint8_t* data){
	/* 'a' box draw */
	sprintf(bf, "fill %d,%d,30,10,%s", print_xy->x + 10, print_xy->y, data[a] == 1 ? "BLACK" : "WHITE");
	nextion_inst_set(bf);
	sprintf(bf, "draw %d,%d,%d,%d,BLACK", print_xy->x + 10, print_xy->y, print_xy->x + 10 + 30, print_xy->y + 10);
	nextion_inst_set(bf);

	/* 'b' box draw */
	sprintf(bf, "fill %d,%d,10,30,%s", print_xy->x + 40, print_xy->y + 10, data[b] == 1 ? "BLACK" : "WHITE");
	nextion_inst_set(bf);
	sprintf(bf, "draw %d,%d,%d,%d,BLACK", print_xy->x + 40, print_xy->y + 10, print_xy->x + 40 + 10, print_xy->y + 10 + 30);
	nextion_inst_set(bf);

	/* 'c' box draw */
	sprintf(bf, "fill %d,%d,10,30,%s", print_xy->x + 40, print_xy->y + 50, data[c] == 1 ? "BLACK" : "WHITE");
	nextion_inst_set(bf);
	sprintf(bf, "draw %d,%d,%d,%d,BLACK", print_xy->x + 40, print_xy->y + 50, print_xy->x + 40 + 10, print_xy->y + 50 + 30);
	nextion_inst_set(bf);

	/* 'd' box draw */
	sprintf(bf, "fill %d,%d,30,10,%s", print_xy->x + 10, print_xy->y + 80, data[d] == 1 ? "BLACK" : "WHITE");
	nextion_inst_set(bf);
	sprintf(bf, "draw %d,%d,%d,%d,BLACK", print_xy->x + 10, print_xy->y + 80, print_xy->x + 40, print_xy->y + 80 + 10);
	nextion_inst_set(bf);

	/* 'e' box draw */
	sprintf(bf, "fill %d,%d,10,30,%s", print_xy->x, print_xy->y + 50, data[e] == 1 ? "BLACK" : "WHITE");
	nextion_inst_set(bf);
	sprintf(bf, "draw %d,%d,%d,%d,BLACK", print_xy->x, print_xy->y + 50, print_xy->x + 10, print_xy->y + 50 + 30);
	nextion_inst_set(bf);

	/* 'f' box draw */
	sprintf(bf, "fill %d,%d,10,30,%s", print_xy->x, print_xy->y + 10, data[f] == 1 ? "BLACK" : "WHITE");
	nextion_inst_set(bf);
	sprintf(bf, "draw %d,%d,%d,%d,BLACK", print_xy->x, print_xy->y + 10, print_xy->x + 10, print_xy->y + 10 + 30);
	nextion_inst_set(bf);

	/* 'g' box draw */
	sprintf(bf, "fill %d,%d,30,10,%s", print_xy->x + 10, print_xy->y + 40, data[g] == 1 ? "BLACK" : "WHITE");
	nextion_inst_set(bf);
	sprintf(bf, "draw %d,%d,%d,%d,BLACK", print_xy->x + 10, print_xy->y + 40, print_xy->x + 10 + 30, print_xy->y + 40 + 10);
	nextion_inst_set(bf);
}

void main_dis(TIME_Typedef* display_time, uint8_t* dot_state){
	uint8_t i = 0 ;

	while(i++ < 3){
		/* button draw */
		for(uint8_t i = 0 ; i < 2 ; i++){
			sprintf(bf, "xstr %d,%d,100,30,0,WHITE,%s,1,1,1,\"%s\"", button_data[i].area.x0, button_data[i].area.y0, button_data[i].color, button_data[i].name);
			nextion_inst_set(bf);

			sprintf(bf, "draw %d,%d,%d,%d,BLACK", button_data[i].area.x0, button_data[i].area.y0, button_data[i].area.x1, button_data[i].area.y1);
			nextion_inst_set(bf);
		}

		/* Time display */
		POS_Typedef print_xy = { 180, 25, 0 };

		sprintf(bf, "draw %d,%d,%d,%d,BLACK", print_xy.x - 20, print_xy.y - 5, print_xy.x + 270, print_xy.y + 95);
		nextion_inst_set(bf);

		number_print(&print_xy, big_num_font[input_state == hour_input ? set_time.time1 : (display_time->time1 / 10) > 0 ? display_time->time1 / 10 : 10]);  // time1 left number
		number_print(&print_xy, big_num_font[input_state == hour_input ? set_time.time1 : (display_time->time1 / 10) > 0 ? display_time->time1 / 10 : 10]);
		number_print(&print_xy, big_num_font[input_state == hour_input ? set_time.time1 : (display_time->time1 / 10) > 0 ? display_time->time1 / 10 : 10]);

		print_xy.x +=60;

		number_print(&print_xy, big_num_font[input_state == hour_input ? set_time.time2 : display_time->time1 % 10]);  // time1 right number
		number_print(&print_xy, big_num_font[input_state == hour_input ? set_time.time2 : display_time->time1 % 10]);
		number_print(&print_xy, big_num_font[input_state == hour_input ? set_time.time2 : display_time->time1 % 10]);

		print_xy.x += 80;

		number_print(&print_xy, big_num_font[input_state == min_input ? set_time.time1 : display_time->time2 / 10]);  // time2 left number
		number_print(&print_xy, big_num_font[input_state == min_input ? set_time.time1 : display_time->time2 / 10]);
		number_print(&print_xy, big_num_font[input_state == min_input ? set_time.time1 : display_time->time2 / 10]);

		print_xy.x += 60;

		number_print(&print_xy, big_num_font[input_state == min_input ? set_time.time2 : display_time->time2 % 10]);  // time2 right number
		number_print(&print_xy, big_num_font[input_state == min_input ? set_time.time2 : display_time->time2 % 10]);
		number_print(&print_xy, big_num_font[input_state == min_input ? set_time.time2 : display_time->time2 % 10]);

		/* dot point draw */
		sprintf(bf, "fill 300,45,10,10,%s", *dot_state == 0 ? "WHITE" : "BLACK"); // first dot point
		nextion_inst_set(bf);
		sprintf(bf, "draw 300,45,300+10,45+10,BLACK");
		nextion_inst_set(bf);

		sprintf(bf, "fill 300,85,10,10,%s", *dot_state == 0 ? "WHITE" : "BLACK"); // second dot point
		nextion_inst_set(bf);
		sprintf(bf, "draw 300,85,300+10,85+10,BLACK");
		nextion_inst_set(bf);
	}
}

void button_draw(AREA_Typedef* print_xy, char* color, char* name){
	/* circle draw */
	sprintf(bf, "cirs %d,%d,15,%s", print_xy->x0, print_xy->y0 + 15, color);
	nextion_inst_set(bf);
	sprintf(bf, "cirs %d,%d,15,%s", print_xy->x0 + 80, print_xy->y0 + 15, color);
	nextion_inst_set(bf);

	/* rectangle draw */
	sprintf(bf, "xstr %d,%d,80,30,0,WHITE,%s,1,1,1,\"%s\"", print_xy->x0, print_xy->y0, color, name);
	nextion_inst_set(bf);
}

void swap(LOG_Typedef* a, LOG_Typedef* b){
	LOG_Typedef temp = *a;
	*a = *b;
	*b = temp;
}

void log_save(STOP_WATCH_Typedef* buf){
	static uint8_t cnt = 0;

	for(uint8_t i = 3 ; i > 0 ; i--){
		swap(&stop_watch_mode.log[i], &stop_watch_mode.log[i - 1]);
		if(cnt == 3) stop_watch_mode.log[i].laps--;
	}

	if(cnt < 3) cnt++;
	buf->log[0].laps = cnt;

	buf->log[0].total = buf->time;
	buf->log[0].time.time1 = buf->log[0].total.time1 - buf->log[1].total.time1;
	if(buf->log[1].total.time2 > buf->log[0].total.time2){
		buf->log[0].time.time1--;
		buf->log[0].time.time2 = (60 + buf->log[0].total.time2) - buf->log[1].total.time2;
	}
	else{
		buf->log[0].time.time2 = buf->log[0].total.time2 - buf->log[1].total.time2;
	}
}

void task_fuc(void){
	uint8_t screen_update = 0;
	uint8_t dot_state = 0;

	uint8_t befo_touch = curXY.touched;

	uint32_t flash_dot = 0;
	uint32_t sw1_tick = 0;
	uint32_t time_tick = 0;

	main_dis(&time, &dot_state);
	main_dis(&time, &dot_state);
	main_dis(&time, &dot_state);

	AREA_Typedef hour_area = { 180, 25, 180 + 110, 25 + 90 };
	AREA_Typedef min_area  = { 320, 25, 320 + 110, 25 + 90 };

	while(1){
		get_touch(&curXY);

		/* screen update */
		if(screen_update == 0) {
			if(mode == timer_menu) button_data[timer_button].color = "GRAY";
			else button_data[timer_button].color = "BLACK";

			if(mode == stop_watch_menu) button_data[watch_button].color = "GRAY";
			else button_data[watch_button].color = "BLACK";

			if(mode == main_menu)            main_dis(&time, &dot_state);
			else if(mode == timer_menu)      main_dis(&timer_mode.time, &dot_state);
			else if(mode == stop_watch_menu) main_dis(&stop_watch_mode.time, &dot_state);
			screen_update = 1;

			nextion_inst_set("fill 155,130,320,280,WHITE");

			if(mode == timer_menu && input_state == none)
				button_draw(&button_data[timer_start_button].area, button_data[timer_start_button].color, button_data[timer_start_button].name);
			else if(mode == stop_watch_menu && input_state == none){
				button_draw(&button_data[stop_watch_reset_button].area, button_data[stop_watch_reset_button].color, button_data[stop_watch_reset_button].name);
				button_draw(&button_data[stop_watch_start_button].area, button_data[stop_watch_start_button].color, button_data[stop_watch_start_button].name);

				nextion_inst_set("xstr 160,130,100,30,0,BLACK,WHITE,1,1,1,\"Laps\"");
				nextion_inst_set("xstr 260,130,100,30,0,BLACK,WHITE,1,1,1,\"Time\"");
				nextion_inst_set("xstr 360,130,100,30,0,BLACK,WHITE,1,1,1,\"Total\"");
				nextion_inst_set("line 160,160,450,160,BLACK");

				for(uint8_t i = 0 ; i < 3 ; i++){
					if(stop_watch_mode.log[i].laps == 0) break;

					sprintf(bf, "xstr 160,%d,100,17,0,BLACK,WHITE,1,1,1,\"%d\"", 161 + (i * 17), stop_watch_mode.log[i].laps);
					nextion_inst_set(bf);

					sprintf(bf, "xstr 260,%d,100,17,0,BLACK,WHITE,1,1,1,\"%d:%02d\"", 161 + (i * 17), stop_watch_mode.log[i].time.time1, stop_watch_mode.log[i].time.time2);
					nextion_inst_set(bf);

					sprintf(bf, "xstr 360,%d,100,17,0,BLACK,WHITE,1,1,1,\"%d:%02d\"", 161 + (i * 17), stop_watch_mode.log[i].total.time1, stop_watch_mode.log[i].total.time2);
					nextion_inst_set(bf);
				}
			}

			/* keypad draw */
			if(input_state != none){
				nextion_inst_set("draw 155,140,455,260,BLACK");
				for(uint8_t i = 0 ; i < 10 ; i++){
					sprintf(bf, "xstr %d,%d,40,40,0,BLACK,GRAY,1,1,1,\"%c\"", keypad[i].area.x0, keypad[i].area.y0, keypad[i].pad_num);
					nextion_inst_set(bf);
				}
			}
		}

		/* Switch control */
		if(SW(1)){
			if(HAL_GetTick() - sw1_tick > 1000){
				sw1_tick = HAL_GetTick();

				time.time2++;
				if(time.time2 >= 60){
					time.time2 = 0;
					time.time1++;
					if(time.time1 >= 24) time.time1 = 0;
				}

				screen_update = 0;
			}
		}
		else sw1_tick = HAL_GetTick();

		/* dot point state reverse */
		if(HAL_GetTick() - flash_dot > 1000){
			flash_dot = HAL_GetTick();
			dot_state ^= 1;
			screen_update = 0;
		}

		/* hour input mode */
		if(area_check(&curXY, &hour_area) && input_state != hour_input && mode != stop_watch_menu){
			input_state = hour_input;

			set_time.time1 = set_time.time2 = 10;

			screen_update = 0;
		}
		/* minute input mode */
		else if(area_check(&curXY, &min_area) && input_state != min_input && mode != stop_watch_menu){
			input_state = min_input;

			set_time.time1 = set_time.time2 = 10;

			screen_update = 0;
		}
		/* timer mode */
		else if(area_check(&curXY, &button_data[timer_button].area) && befo_touch != curXY.touched && curXY.touched == 1){
			mode = mode == timer_menu ? main_menu : timer_menu;
			screen_update = 0;
		}
		/* stop watch mode */
		else if(area_check(&curXY, &button_data[watch_button].area) && befo_touch != curXY.touched && curXY.touched == 1){
			mode = mode == stop_watch_menu ? main_menu : stop_watch_menu;
			screen_update = 0;
		}

		if(mode == timer_menu){
			if(area_check(&curXY, &button_data[timer_start_button].area) && befo_touch != curXY.touched && curXY.touched == 1){
				timer_mode.state ^= 1;

				if(timer_mode.state == 0 && timer_mode.time.time1 == 0 && timer_mode.time.time2 == 0) timer_mode.time = timer_mode.buf_time;
				else timer_mode.buf_time = timer_mode.time;

				screen_update = 0;
			}

			if(timer_mode.state == 0){
				button_data[timer_start_button].color = "BLUE";
				button_data[timer_start_button].name  = "Start";
			}
			else{
				button_data[timer_start_button].color = "RED";
				button_data[timer_start_button].name  = "Stop";
			}
		}
		else if(mode == stop_watch_menu){
			if(area_check(&curXY, &button_data[stop_watch_reset_button].area) && befo_touch != curXY.touched && curXY.touched == 1){
				screen_update = 0;
				if(stop_watch_mode.state == 0){
					memset((LOG_Typedef*)stop_watch_mode.log, 0, sizeof(LOG_Typedef) * 3);
					stop_watch_mode.time.time1 = stop_watch_mode.time.time2 = 0;
				}
				else{
					log_save(&stop_watch_mode);
					screen_update = 0;
				}
			}
			else if(area_check(&curXY, &button_data[stop_watch_start_button].area) && befo_touch != curXY.touched && curXY.touched == 1){
				screen_update = 0;
				stop_watch_mode.state ^= 1;
			}

			if(stop_watch_mode.state == 0){
				button_data[stop_watch_reset_button].name = "Reset";

				button_data[stop_watch_start_button].color = "BLUE";
				button_data[stop_watch_start_button].name = "Start";
			}
			else{
				button_data[stop_watch_reset_button].name = "Record";

				button_data[stop_watch_start_button].color = "RED";
				button_data[stop_watch_start_button].name = "Stop";
			}
		}

		/* keypad control */
		if(input_state != none){
			uint8_t touched_pad = keypad_read();

			if(touched_pad && befo_touch != curXY.touched && curXY.touched == 1){
				screen_update = 0;

				if(set_time.time2 == 10) set_time.time2 = keypad[touched_pad - 1].pad_num - '0';
				else{
					set_time.time1 = set_time.time2;
					set_time.time2 = keypad[touched_pad - 1].pad_num - '0';
					if(input_state == hour_input){
						if(set_time.time1 * 10 + set_time.time2 > ((mode == main_menu) ? 23 : 59)) { set_time.time1 = set_time.time2 = 10; }
						else{
							input_state = none;
							if(mode == main_menu)       { time.time1 = set_time.time1 * 10 + set_time.time2; }
							else if(mode == timer_menu) { timer_mode.time.time1 = set_time.time1 * 10 + set_time.time2; }
						}
					}
					else if(input_state == min_input){
						if(set_time.time1 * 10 + set_time.time2 > 59) { set_time.time1 = set_time.time2 = 10; }
						else{
							input_state = none;
							if(mode == main_menu)       { time.time2 = set_time.time1 * 10 + set_time.time2; }
							else if(mode == timer_menu) { timer_mode.time.time2 = set_time.time1 * 10 + set_time.time2; }
						}
					}
				}
			}
		}

		befo_touch = curXY.touched;

		if(HAL_GetTick() - time_tick >= 60000){
			time_tick = HAL_GetTick();

			time.time2++;
			if(time.time2 > 59){
				time.time2 = 0;
				time.time1++;
				if(time.time1 > 23) time.time1 = 0;
			}
		}

		if(timer_mode.state == 1){
			if(timer_mode.time.time1 > 0 || timer_mode.time.time2 > 0){
				if(HAL_GetTick() - timer_mode.tick >= 1000){
					timer_mode.tick = HAL_GetTick();
					if(timer_mode.time.time2 == 0){
						timer_mode.time.time2 = 60;
						timer_mode.time.time1--;
					}
					timer_mode.time.time2--;
					screen_update = 0;
				}
			}
			else{
				static uint8_t state = 0;
				if(HAL_GetTick() - timer_mode.tick >= 500){
					timer_mode.tick = HAL_GetTick();

					state ^= 1;
					BUZ(state);

					setMotor(DRV8830_CW);
				}
			}
		}
		else { timer_mode.tick = HAL_GetTick(); setMotor(DRV8830_STOP); BUZ(0); }

		if(stop_watch_mode.state == 1){
			if(HAL_GetTick() - stop_watch_mode.tick > 1000){
				stop_watch_mode.tick = HAL_GetTick();

				screen_update = 0;

				stop_watch_mode.time.time2++;
				if(stop_watch_mode.time.time2 > 59){
					stop_watch_mode.time.time2 = 0;
					stop_watch_mode.time.time1++;
				}
			}
		}
		else stop_watch_mode.tick = HAL_GetTick();

		/* coordinate display for check this code */
		/* sprintf(bf, "xstr 1,1,150,30,0,BLACK,WHITE,0,1,1,\"X: %d Y: %d\"", curXY.x, curXY.y); */
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

	LED(1, 0);
	LED(2, 0);
	LED(3, 0);

	setMotor(DRV8830_STOP);

	/* change baudrate */
	nextion_inst_set("baud=921600");
	nextion_inst_set("baud=921600");
	nextion_inst_set("baud=921600");
	HAL_Delay(50);
	USART1->CR1 &= (~USART_CR1_UE);
	USART1->BRR = 0x23;
	USART1->CR1 |= USART_CR1_UE;
	HAL_Delay(1000);

	nextion_inst_set("cls WHITE");
	nextion_inst_set("cls WHITE");
	nextion_inst_set("cls WHITE");

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

