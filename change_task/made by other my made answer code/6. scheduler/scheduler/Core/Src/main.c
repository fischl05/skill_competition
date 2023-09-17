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
	menu_basic,
	menu_detail,
}ADD_MENU_Typedef;

typedef enum{
	basic,
	detail,
	add,
}MENU_Typedef;

typedef enum{
	red,
	green,
	blue,
	yellow,
	purple,
	max_color,
}COLOR_Typedef;

typedef enum{
	meeting,
	biz_trip,
	holiday,
	lecture,
	work_out,
	max_title,
}TITLE_Typedef;

typedef enum{
	sun,
	mon,
	tue,
	wed,
	thu,
	fri,
	sat,
	max_week,
}WEEK_Typedef;

typedef enum{
	none,
	left_slide,
	right_slide,
}SLIDE_Typedef;

typedef struct{
	int8_t year;
	uint8_t month, day;
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
	TIME_Typedef time;
	TITLE_Typedef title[5];
	COLOR_Typedef color[5];
	uint8_t schedule_num;
}SCHEDULE_DATA_Typedef;

typedef struct{
	SCHEDULE_DATA_Typedef data[10];
	AREA_Typedef area[5];
	uint8_t schedule_num;
	uint8_t now_schedule;
}SCHEDULE_Typedef;

typedef struct{
	AREA_Typedef area;
	char* title_color;
	char* button_color;
	char* title;
}BUTTON_Typedef;

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

POS_Typedef curXY = { 0, 0, 0 };
MENU_Typedef menu = basic;
SCHEDULE_Typedef schedule;
BUTTON_Typedef button_data[2] = {
		{ { 140, 220, 140 + 80, 220 + 25 }, "WHITE", "GREEN", "add" },
		{ { 270, 220, 270 + 80, 220 + 25 }, "GREEN", "WHITE", "exit" },
};

AREA_Typedef check_box_area[2] = {
		{ 200, 73 + 20, 200 + 20, 73 + 20 + 20 },
		{ 370, 73 + 20, 370 + 20, 73 + 20 + 20 }
};

ADD_MENU_Typedef title_state = menu_basic;
ADD_MENU_Typedef color_state = menu_basic;

uint8_t last_day[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

char* schedule_title[max_title] = { "meeting", "Biz trip", "holiday", "lecture", "work out" };
char* schedule_color[max_color] = { "RED", "GREEN", "BLUE", "YELLOW", "RED|BLUE" };
char* color_string[max_color] = { "RED", "GREEN", "BLUE", "YELLOW", "PURPLE" };
char* day_string[max_week] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };

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

void BUZ(uint8_t state){
	if(state) HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
	else      HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
}

void nextion_inst_set(char* str){
	uint8_t end_cmd[3] = { 0xFF, 0xFF, 0xFF };
	HAL_UART_Transmit(&huart1, (uint8_t*)str, strlen(str), 100);
	HAL_UART_Transmit(&huart1, end_cmd, 3, 100);
}

void get_touch(POS_Typedef* buf){
	HAL_StatusTypeDef res = HAL_OK;
	uint8_t rx_data[8] = { 0, };

	nextion_inst_set("get tch0");
	res = HAL_UART_Receive(&huart1, rx_data, 8, 100);
	if(res == HAL_OK) { if(rx_data[0] == 0x71) { buf->x = rx_data[2] << 8 | rx_data[1]; } }

	nextion_inst_set("get tch1");
	res = HAL_UART_Receive(&huart1, rx_data, 8, 100);
	if(res == HAL_OK) { if(rx_data[0] == 0x71) { buf->y = rx_data[2] << 8 | rx_data[1]; } }

	if(buf->x > 0 && buf->y > 0) buf->touched = 1;
	else buf->touched = 0;
}

void get_slide(POS_Typedef* xy, SLIDE_Typedef* buf){
	static POS_Typedef befo_touch = { 0, 0, 0 };
	static POS_Typedef touched_xy = { 0, 0, 0 };

	if(befo_touch.touched != xy->touched){
		if(xy->touched == 0){
			if(touched_xy.x > befo_touch.x){
				if(touched_xy.x - befo_touch.x > 30) *buf = left_slide;
			}
			else if(befo_touch.x > touched_xy.x){
				if(befo_touch.x - touched_xy.x > 30) *buf = right_slide;
			}
		}
		else touched_xy = *xy;
	}
	else *buf = none;

	befo_touch = *xy;
}

uint8_t area_check(POS_Typedef* xy, AREA_Typedef* area){
	if(xy->x >= area->x0 && xy->x <= area->x1){
		if(xy->y >= area->y0 && xy->y <= area->y1){
			return 1;
		}
	}
	return 0;
}

WEEK_Typedef day_cal(TIME_Typedef* now_time){
	uint8_t leaf = ((now_time->year % 4 == 0 && now_time->year % 100 != 0) || (now_time->year % 400 == 0));
	uint32_t sum = 0;
	WEEK_Typedef res = sun;


	for(uint16_t i = 1 ; i < now_time->year + 2000 ; i++){
		uint8_t leaf = ((i % 4 == 0 && i % 100 != 0) || (i % 400 == 0));

		if(leaf == 1) sum += 366;
		else sum += 365;
	}

	if(leaf == 1) last_day[1] = 29;
	else last_day[1] = 28;

	for(uint8_t i = 0 ; i < now_time->month - 1 ; i++){
		sum += last_day[i];
	}

	sum += now_time->day;

	res = sum % 7;
	return res;
}

uint8_t equal_time_check(TIME_Typedef* a, TIME_Typedef* b){
	if(a->year == b->year){
		if(a->month == b->month){
			if(a->day == b->day){
				return 1;
			}
		}
	}
	return 0;
}

void basic_screen(TIME_Typedef* data, TIME_Typedef* print_time){
	AREA_Typedef current_date_area = { 0, 0, 0, 0 };

	/* title draw */
	sprintf(bf, "xstr 0,0,480,32,0,WHITE,BLACK,0,1,1,\" %d. %d\"", print_time->year + 2000, print_time->month);
	nextion_inst_set(bf);

	print_time->day = 1;

	/* calender draw */
	if(menu == basic){
		for(uint8_t i = 0 ; i < 6 ; i++){
			for(WEEK_Typedef j = 0 ; j < max_week ; j++){
				AREA_Typedef area = { j * 68, i * 40 + 32, j == 6 ? 480 : (j + 1) * 68, (i + 1) * 40 + 32 };

				WEEK_Typedef print_time_day = day_cal(print_time);

				if(print_time_day == j && print_time->day <= last_day[print_time->month - 1]){
					sprintf(bf, "xstr %d,%d,%d,40,0,%s,WHITE,0,0,1,\"%d\"", area.x0 + 1, area.y0 + 1, j == 6 ? 72 : 68, print_time_day == sun ? "RED" : print_time_day == sat ? "BLUE" : "BLACK", print_time->day);
					nextion_inst_set(bf);

					if(equal_time_check(data, print_time)) current_date_area = area;

					for(uint8_t i = 0 ; i < schedule.schedule_num ; i++){
						if(equal_time_check(print_time, &schedule.data[i].time) && schedule.data[i].schedule_num > 0){
							sprintf(bf, "xstr %d,%d,%d,20,0,BLACK,%s,1,1,1,\"%s\"", area.x0, area.y0 + 20, j == 6 ? 72 : 68,
									schedule_color[schedule.data[i].color[0]],
									schedule_title[schedule.data[i].title[0]]);
							nextion_inst_set(bf);
							sprintf(bf, "draw %d,%d,%d,%d,BLACK", area.x0, area.y0 + 20, area.x1, area.y1);
							nextion_inst_set(bf);
						}
					}

					print_time->day++;
				}
				else{
					sprintf(bf, "fill %d,%d,%d,40,WHITE", area.x0, area.y0, j == 6 ? 72 : 68);
					nextion_inst_set(bf);
				}

				sprintf(bf, "draw %d,%d,%d,%d,BLACK", area.x0, area.y0, area.x1, area.y1);
				nextion_inst_set(bf);
			}
		}

		sprintf(bf, "draw %d,%d,%d,%d,RED", current_date_area.x0, current_date_area.y0, current_date_area.x1, current_date_area.y1);
		nextion_inst_set(bf);
	}
	else if(menu != basic){
		AREA_Typedef area = { 35, 52, 35 + 410, 52 + 198 };

		sprintf(bf, "fill %d,%d,%d,%d,WHITE", area.x0, area.y0, area.x1 - area.x0, area.y1 - area.y0);
		nextion_inst_set(bf);

		sprintf(bf, "xstr %d,%d,410,20,0,BLACK,WHITE,0,1,1,\" Date: %d, %d, %d, %s\"", area.x0, area.y0,
				schedule.data[schedule.now_schedule].time.year + 2000,
				schedule.data[schedule.now_schedule].time.month,
				schedule.data[schedule.now_schedule].time.day,
				day_string[day_cal(&schedule.data[schedule.now_schedule].time)]);
		nextion_inst_set(bf);

		sprintf(bf, "line %d,%d,%d,%d,BLACK", area.x0, area.y0 + 20, area.x1, area.y0 + 20);
		nextion_inst_set(bf);

		if(menu == detail){
			for(uint8_t i = 0 ; i < schedule.data[schedule.now_schedule].schedule_num ; i++){
				sprintf(bf, "xstr %d,%d,410,30,0,BLACK,WHITE,0,1,3,\"%s | %d-%d-%02d\"", area.x0 + 3, area.y0 + 20 + i * 30,
						schedule_title[schedule.data[schedule.now_schedule].title[i]],
						schedule.data[schedule.now_schedule].time.year + 2000,
						schedule.data[schedule.now_schedule].time.month,
						schedule.data[schedule.now_schedule].time.day);
				nextion_inst_set(bf);

				sprintf(bf, "line %d,%d,%d,%d,RED", schedule.area[i].x0,
						schedule.area[i].y0,
						schedule.area[i].x1,
						schedule.area[i].y1);
				nextion_inst_set(bf);

				sprintf(bf, "line %d,%d,%d,%d,RED", schedule.area[i].x0,
						schedule.area[i].y1,
						schedule.area[i].x1,
						schedule.area[i].y0);
				nextion_inst_set(bf);

				sprintf(bf, "draw %d,%d,%d,%d,BLACK", schedule.area[i].x0,
						schedule.area[i].y0,
						schedule.area[i].x1,
						schedule.area[i].y1);
				nextion_inst_set(bf);
			}

			button_data[0].title = "add";
			button_data[1].title = "exit";
		}
		else if(menu == add){
			nextion_inst_set("xstr 100,52+21,150,20,0,BLACK,WHITE,0,1,1,\"Title\"");
			nextion_inst_set("xstr 270,52+21,150,20,0,BLACK,WHITE,0,1,1,\"Color\"");

			sprintf(bf, "xstr %d,%d,100,20,0,BLACK,WHITE,1,1,1,\"%s\"", 100, 73 + 20,
					schedule_title[schedule.data[schedule.now_schedule].title[schedule.data[schedule.now_schedule].schedule_num]]);
			nextion_inst_set(bf);
			sprintf(bf, "draw %d,%d,%d,%d,BLACK", 100, 73 + 20, 100 + 100, 73 + 20 + 20);
			nextion_inst_set(bf);

			sprintf(bf, "xstr %d,%d,100,20,0,BLACK,%s,1,1,1,\"%s\"", 270, 73 + 20,
					schedule_color[schedule.data[schedule.now_schedule].color[schedule.data[schedule.now_schedule].schedule_num]],
					color_string[schedule.data[schedule.now_schedule].color[schedule.data[schedule.now_schedule].schedule_num]]);
			nextion_inst_set(bf);
			sprintf(bf, "draw %d,%d,%d,%d,BLACK", 270, 73 + 20, 270 + 100, 73 + 20 + 20);
			nextion_inst_set(bf);

			for(uint8_t i = 0 ; i < 2 ; i++){
				sprintf(bf, "fill %d,%d,20,20,GRAY", check_box_area[i].x0, check_box_area[i].y0);
				nextion_inst_set(bf);

				sprintf(bf, "line %d,%d,%d,%d,BLACK", check_box_area[i].x0, check_box_area[i].y0, (check_box_area[i].x0 + check_box_area[i].x1) / 2, check_box_area[i].y1);
				nextion_inst_set(bf);
				sprintf(bf, "line %d,%d,%d,%d,BLACK", (check_box_area[i].x0 + check_box_area[i].x1) / 2, check_box_area[i].y1, check_box_area[i].x1, check_box_area[i].y0);
				nextion_inst_set(bf);

				sprintf(bf, "draw %d,%d,%d,%d,BLACK", check_box_area[i].x0, check_box_area[i].y0, check_box_area[i].x1, check_box_area[i].y1);
				nextion_inst_set(bf);
			}

			if(title_state == menu_detail){
				for(TITLE_Typedef i = meeting ; i < max_title ; i++){
					sprintf(bf, "xstr %d,%d,100,20,0,BLACK,WHITE,1,1,1,\"%s\"", 100, 73 + 40 + (i * 20), schedule_title[i]);
					nextion_inst_set(bf);
					sprintf(bf, "draw %d,%d,%d,%d,BLACK", 100, 73 + 40 + (i * 20), 100 + 100, 73 + 40 + ((i + 1) * 20));
					nextion_inst_set(bf);
				}
			}

			if(color_state == menu_detail){
				for(COLOR_Typedef i = red ; i < max_color ; i++){
					sprintf(bf, "xstr %d,%d,100,20,0,BLACK,%s,1,1,1,\"%s\"", 270, 73 + 40 + (i * 20), schedule_color[i], color_string[i]);
					nextion_inst_set(bf);
					sprintf(bf, "draw %d,%d,%d,%d,BLACK", 270, 73 + 40 + (i * 20), 270 + 100, 73 + 40 + ((i + 1) * 20));
					nextion_inst_set(bf);
				}
			}

			button_data[0].title = "save";
			button_data[1].title = "cancel";
		}

		for(uint8_t i = 0 ; i < 2 ; i++){
			sprintf(bf, "xstr %d,%d,80,25,0,%s,%s,1,1,1,\"%s\"", button_data[i].area.x0, button_data[i].area.y0, button_data[i].title_color, button_data[i].button_color, button_data[i].title);
			nextion_inst_set(bf);
			sprintf(bf, "draw %d,%d,%d,%d,BLACK", button_data[i].area.x0, button_data[i].area.y0, button_data[i].area.x1, button_data[i].area.y1);
			nextion_inst_set(bf);
		}

		sprintf(bf, "draw %d,%d,%d,%d,BLACK", area.x0, area.y0, area.x1, area.y1);
		nextion_inst_set(bf);
	}
}

void task_fuc(void){
	POS_Typedef befo_touch = { 0, 0, 0 };
	SLIDE_Typedef slide = none;

	TIME_Typedef time = { 23, 9, 16 };
	TIME_Typedef print_time = { 23, 9, 1 };
	TIME_Typedef schedule_time = time;

	uint8_t screen_update = 0;

	uint8_t sw[2] = { SW(1), SW(2) };
	uint8_t befo_sw[2] = { sw[0], sw[1] };

	nextion_inst_set("cls WHITE");
	nextion_inst_set("cls WHITE");
	nextion_inst_set("cls WHITE");

	while(1){
		get_touch(&curXY);
		get_slide(&curXY, &slide);

		sw[0] = SW(1);
		sw[1] = SW(2);

		if(screen_update == 0){
			basic_screen(&time, &print_time);

			screen_update = 1;
		}

		/* slide function */
		if(slide != none && menu == basic){
			screen_update = 0;

			BUZ(1);
			HAL_Delay(100);
			BUZ(0);

			if(slide == left_slide){
				print_time.month++;
				if(print_time.month > 12){
					print_time.month = 1;
					print_time.year++;
				}
			}
			else if(slide == right_slide){
				print_time.month--;
				if(print_time.month == 0){
					print_time.month = 12;
					print_time.year--;
				}
			}
		}

		/* touch function */
		if(befo_touch.touched != curXY.touched){
			screen_update = 0;

			if(curXY.touched == 1){
				if(menu == basic && slide == none){
					TIME_Typedef touch_time = { print_time.year, print_time.month, 1 };

					for(uint8_t i = 0 ; i < 6 ; i++){
						for(WEEK_Typedef j = 0 ; j < max_week ; j++){
							AREA_Typedef area = { j * 68, i * 40 + 32, j == 6 ? 480 : (j + 1) * 68, (i + 1) * 40 + 32 };

							WEEK_Typedef touch_time_day = day_cal(&touch_time);

							if(touch_time_day == j && touch_time.day <= last_day[touch_time.month - 1]){
								if(area_check(&curXY, &area)){
									schedule_time = touch_time;

									menu = detail;

									for(uint8_t i = 0 ; i < schedule.schedule_num ; i++){
										schedule.now_schedule = i;
										if(equal_time_check(&schedule.data[i].time, &schedule_time)) {
											schedule.schedule_num--;
											break;
										}
									}

									if(schedule.data[schedule.now_schedule].time.year == 0) schedule.data[schedule.now_schedule].time = schedule_time;
									schedule.schedule_num++;

									BUZ(1);
									HAL_Delay(100);
									BUZ(0);
								}
								touch_time.day++;
							}
						}
					}
				}
				else if(menu == detail){
					if(area_check(&curXY, &button_data[0].area)){
						menu = add;
					}
					else if(area_check(&curXY, &button_data[1].area)){
						menu = basic;
					}
					else{
						for(uint8_t i = 0 ; i < 5 ; i++){
							if(area_check(&curXY, &schedule.area[i])){
								memmove((COLOR_Typedef*)&schedule.data[schedule.now_schedule].color[i],
										(COLOR_Typedef*)&schedule.data[schedule.now_schedule].color[i + 1],
										sizeof(COLOR_Typedef) * schedule.data[schedule.now_schedule].schedule_num - i);
								memmove((TITLE_Typedef*)&schedule.data[schedule.now_schedule].title[i],
										(TITLE_Typedef*)&schedule.data[schedule.now_schedule].title[i + 1],
										sizeof(TITLE_Typedef) * schedule.data[schedule.now_schedule].schedule_num - i);
								schedule.data[schedule.now_schedule].color[schedule.data[schedule.now_schedule].schedule_num - 1] = red;
								schedule.data[schedule.now_schedule].title[schedule.data[schedule.now_schedule].schedule_num - 1] = meeting;
								schedule.data[schedule.now_schedule].schedule_num--;
							}
						}
					}
				}
				else if(menu == add){
					if(area_check(&curXY, &button_data[0].area)){
						if(schedule.data[schedule.now_schedule].schedule_num == 5){
							BUZ(1);
							HAL_Delay(100);
							BUZ(0);
							HAL_Delay(100);
							BUZ(1);
							HAL_Delay(100);
							BUZ(0);
						}

						schedule.data[schedule.now_schedule].schedule_num = schedule.data[schedule.now_schedule].schedule_num < 5 ? schedule.data[schedule.now_schedule].schedule_num + 1 : schedule.data[schedule.now_schedule].schedule_num;

						menu = detail;
					}
					else if(area_check(&curXY, &button_data[1].area)){
						schedule.data[schedule.now_schedule].title[schedule.data[schedule.now_schedule].schedule_num] = meeting;
						schedule.data[schedule.now_schedule].color[schedule.data[schedule.now_schedule].schedule_num] = red;

						menu = detail;
					}
					else if(area_check(&curXY, &check_box_area[0])){
						title_state = title_state == menu_basic ? menu_detail : menu_basic;
					}
					else if(area_check(&curXY, &check_box_area[1])){
						color_state = color_state == menu_basic ? menu_detail : menu_basic;
					}

					if(title_state == menu_detail){
						for(TITLE_Typedef i = meeting ; i < max_title ; i++){
							AREA_Typedef area = { 100, 73 + 40 + (i * 20), 100 + 100, 73 + 40 + ((i + 1) * 20) };

							if(area_check(&curXY, &area)){
								schedule.data[schedule.now_schedule].title[schedule.data[schedule.now_schedule].schedule_num] = i;
								title_state = menu_basic;
								break;
							}
						}
					}
					if(color_state == menu_detail){
						for(COLOR_Typedef i = red ; i < max_color ; i++){
							AREA_Typedef area = { 270, 73 + 40 + (i * 20), 270 + 100, 73 + 40 + ((i + 1) * 20) };

							if(area_check(&curXY, &area)){
								schedule.data[schedule.now_schedule].color[schedule.data[schedule.now_schedule].schedule_num] = i;
								color_state = menu_basic;
								break;
							}
						}
					}
				}
			}
		}

		/* button function */
		if(sw[0] != befo_sw[0] && sw[0] == 1){
			screen_update = 0;

			time.day++;
			if(time.day > last_day[time.month - 1]){
				time.day = 1;
				time.month++;
				if(time.month > 12){
					time.month = 1;
					time.year++;
				}
			}
		}

		if(sw[1] != befo_sw[1] && sw[1] == 1){
			screen_update = 0;

			time.day--;
			if(time.day == 0){
				time.month--;
				if(time.month == 0){
					time.month = 12;
					time.year--;
				}
			time.day = last_day[time.month - 1];
			}
		}

		befo_sw[0] = sw[0];
		befo_sw[1] = sw[1];
		befo_touch = curXY;

		/* touch coordinate display for task test */
		/* sprintf(bf, "xstr 300,30,250,30,0,BLACK,WHITE,0,1,1,\"X: %d Y: %d\"", curXY.x, curXY.y); */
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

	setMotor(DRV8830_STOP);

	nextion_inst_set("baud=921600");
	nextion_inst_set("baud=921600");
	nextion_inst_set("baud=921600");
	HAL_Delay(50);
	USART1->CR1 &= (~USART_CR1_UE);
	USART1->BRR = 0x23;
	USART1->CR1 |= USART_CR1_UE;
	HAL_Delay(1000);

		for(uint8_t i = 0 ; i < 5 ; i++){
			schedule.area[i].x0 = 420;
			schedule.area[i].y0 = 52 + 25 + (i * 30);

			schedule.area[i].x1 = schedule.area[i].x0 + 20;
			schedule.area[i].y1 = schedule.area[i].y0 + 20;
		}

	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1)
	{
		task_fuc();
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

