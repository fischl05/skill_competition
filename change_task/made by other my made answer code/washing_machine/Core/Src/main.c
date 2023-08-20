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
	none,
	up_slide,
	down_slide,
}SLIDE_Typedef;

typedef enum{
	menu,
	button,
	add,
	wash,
}MENU_Typedef;

typedef enum{
	black,
	blue,
	green,
	brown,
	red,
	yellow,
	gray,
}COLOR_Typedef;

typedef struct{
	uint16_t x;
	uint16_t y;
	uint8_t touched;
}POS;

typedef struct{
	uint16_t x0, y0;
	uint16_t x1, y1;
}AREA_Typedef;

typedef struct{
	uint8_t cnt;
	uint8_t state;
}PUSHED_Typedef;

typedef struct{
	uint16_t x;
	uint16_t y;
	uint8_t rad;
	uint16_t cnt;
	COLOR_Typedef color;
	PUSHED_Typedef check;
}BUTTON_Typedef;

typedef struct{
	uint16_t x, y;
	uint8_t weight;
	uint8_t selected;
	COLOR_Typedef color;
}laundry_Typedef;

typedef struct{
	AREA_Typedef area;
	uint8_t weight;
	uint8_t cnt;
	uint8_t laundry_num;
	laundry_Typedef* laundry[10];
}BASKET_Typedef;
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
char *door_state_text[2] = {"Unlocked Door", "Locked Door"};
char *car_state_text[4] = {"Waiting for car", "Preparing a car", "Arrival of a car", "Ride in a car"};
char post_state_text[10];

char* user_color[10] = { "BLACK", "BLUE", "GREEN", "BROWN", "RED", "YELLOW", "GRAY", "", "", ""  };

uint8_t firF = 0, Re = 0, buz_state = 0, door_lock = 0, door_open = 0, car_state = 0, post = 0;
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

uint32_t start_cnt = 0;
uint16_t buz_cnt = 0;
uint16_t wash_cnt = 0;
uint8_t buz_on = 0, wash_m = 0;

/* button color generate method is RGB565 method color code                   */
/* red 5-bit, green 6-bit, blue 5-bit                                         */
/* red color is (31,0,0), yellow color is (31,63,0), blue color is (0,0,31)   */
/* if you want changed colors brightness, You should be increase colors value */
BUTTON_Typedef button_data[3] = {
		{160,230,20,0,blue,{0,0}},
		{240,230,20,0,yellow,{0,0}},
		{320,230,20,0,red,{0,0}},
};

/* main display basket range is (LCD x range / 3, 200)        */
/* add laundry display basket range is (LCD x range / 4, 200) */
/* baskets laundry management with pointer array              */
BASKET_Typedef basket[3] = {
		{{0,0,160,200},0,0,0},
		{{161,0,320,200},0,0,0},
		{{321,0,479,200},0,0,0},
};

/* main display laundry`s x_range: 80, y_range: 40        */
/* add laundry display laundry`s x_range: 60, y_range: 40 */
laundry_Typedef laundry[10] = {
		{420,160,5,0,black},
		{361,160,5,0,red},    // first floor laundry

		{420,120,4,0,blue},
		{361,120,4,0,brown},  // second floor laundry

		{420,80,3,0,green},
		{361,80,3,0,green},   // third floor laundry

		{420,40,2,0,brown},
		{361,40,2,0,blue},    // fourth floor laundry

		{420,1,1,0,red},
		{361,1,1,0,black},    // last floor laundry
};

void BUZ(uint8_t state){
	if(state == 1) HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
	else HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
}

/* parameters is laundry pointer array`s address                        */
/* origin of qsort function`s function pointer parameter is const void* */
/* return value`s meaning 1: change that`s value 0: don`t change value  */
int weight_compare(const void* mem1, const void* mem2){
	laundry_Typedef** a = (laundry_Typedef**)mem1;
	laundry_Typedef** b = (laundry_Typedef**)mem2;

	if((*a)->weight < (*b)->weight) return 1;
	else if((*a)->weight == (*b)->weight){
		if((*a)->color > (*b)->color) return 1;
	}
	return 0;
}

int color_compare(const void* mem1, const void* mem2){
	laundry_Typedef** a = (laundry_Typedef**)mem1;
	laundry_Typedef** b = (laundry_Typedef**)mem2;

	if((*a)->color > (*b)->color) return 1;
	else if((*a)->color == (*b)->color){
		if((*a)->weight < (*b)->weight) return 1;
	}
	return 0;
}

int (*compare_item[2])(const void* mem1, const void* mem2) = { weight_compare, color_compare };

void nextion_inst_set(char* str){
	uint8_t end_cmd[3] = { 0xFF, 0xFF, 0xFF };
	HAL_UART_Transmit(&huart1, (uint8_t*)str, strlen(str), 100);
	HAL_UART_Transmit(&huart1, end_cmd, 3, 100);
}

void get_touch(POS* buf){
	uint8_t rx_dat[8];

	nextion_inst_set("get tch0");
	HAL_UART_Receive(&huart1, rx_dat, 8, 100);
	if(rx_dat[0] == 0x71) buf->x = rx_dat[2] << 8 | rx_dat[1];

	nextion_inst_set("get tch1");
	HAL_UART_Receive(&huart1, rx_dat, 8, 100);
	if(rx_dat[0] == 0x71) buf->y = rx_dat[2] << 8 | rx_dat[1];

	if(buf->x > 0 && buf->y > 0) buf->touched = 1;
	else buf->touched = 0;
}

SLIDE_Typedef get_slide(POS* buf){
	static uint8_t befo_touched = 0;
	SLIDE_Typedef res = none;
	static POS befo_pos = { 0, 0, 0 }, cur_pos = { 0, 0, 0 };

	if(befo_touched != buf->touched){ // touched or release check
		if(befo_touched == 1){        // release
			if(cur_pos.y > befo_pos.y){
				if(cur_pos.y - befo_pos.y > 30) res = up_slide;
			}
			else if(befo_pos.y > cur_pos.y){
				if(befo_pos.y - cur_pos.y > 30) res = down_slide;
			}
		}
		else{                         // touched
			cur_pos.x = buf->x;
			cur_pos.y = buf->y;
		}
	}
	else res = none;                  // already sensing touch

	befo_touched = buf->touched;
	befo_pos = *buf;

	return res;
}

uint8_t area_check(POS* pos, AREA_Typedef* area){
	if(pos->x >= area->x0 && pos->x <= area->x1){
		if(pos->y >= area->y0 && pos->y <= area->y1){
			return 1;
		}
	}
	return 0;
}

void main_dis(void){
	HAL_Delay(100);
	nextion_inst_set("cls WHITE");
	nextion_inst_set("cls WHITE");
	nextion_inst_set("cls WHITE");
	nextion_inst_set("draw 0,0,479,271,BLACK");   // border

	nextion_inst_set("draw 0,0,159,200,BLACK");   // basket 1
	nextion_inst_set("xstr 1,1,158,30,0,BLACK,WHITE,1,1,1,\"Basket 1\"");

	nextion_inst_set("draw 160,0,319,200,BLACK"); // basket 2
	nextion_inst_set("xstr 161,1,158,30,0,BLACK,WHITE,1,1,1,\"Basket 2\"");

	nextion_inst_set("draw 320,0,479,200,BLACK"); // basket 3
	nextion_inst_set("xstr 321,1,158,30,0,BLACK,WHITE,1,1,1,\"Basket 3\"");

	for(uint8_t i = 0 ; i < 3 ; i++){
		for(uint8_t j = 0 ; j < basket[i].laundry_num ; j++){
			basket[i].laundry[j]->x = j % 2 == 0 ? 80 : 0;
			basket[i].laundry[j]->x += (i * 160);

			basket[i].laundry[j]->y = ((9 - j) / 2) * 40;

			sprintf(bf, "xstr %d,%d,79,39,0,WHITE,%s,1,1,1,\"%dkg\"", basket[i].laundry[j]->x, basket[i].laundry[j]->y, user_color[basket[i].laundry[j]->color], basket[i].laundry[j]->weight);
			nextion_inst_set(bf);
			sprintf(bf, "draw %d,%d,%d,%d,BLACK", basket[i].laundry[j]->x, basket[i].laundry[j]->y, basket[i].laundry[j]->x + 79, basket[i].laundry[j]->y + 39);
			nextion_inst_set(bf);
		}
	}
}

void kg_draw(void){
	for(uint8_t i = 0 ; i < 3 ; i++){
		sprintf(bf, "xstr %d,201,159,100,0,%s,WHITE,1,0,1,\"%d/15kg\"", basket[i].area.x0, basket[i].weight > 15 ? "RED" : "BLACK", basket[i].weight);
		nextion_inst_set(bf);
	}
}

void button_draw(BUTTON_Typedef* buf){
	if(buf->check.state == 0){
		sprintf(bf, "cir %d,%d,%d,%s", buf->x, buf->y, buf->rad + 1, user_color[gray]);
		nextion_inst_set(bf);
		sprintf(bf, "cirs %d,%d,%d,%s", buf->x, buf->y, buf->rad, user_color[buf->color]);
		nextion_inst_set(bf);
	}
	else{
		// RGB565 method color code generate
		uint16_t color = 0;
		color |= (buf->color == red || buf->color == yellow) ? 31 << 11 : 10 << 11;
		color |= buf->color == yellow ? 62 << 5 : 40 << 5;
		color |= buf->color == blue ? 31 : 20;

		sprintf(bf, "cir %d,%d,%d,%s", buf->x, buf->y, buf->rad + 1, user_color[gray]);
		nextion_inst_set(bf);
		sprintf(bf, "cirs %d,%d,%d,%d", buf->x, buf->y, buf->rad, color);
		nextion_inst_set(bf);
	}
}

void button_check(BUTTON_Typedef* buf, POS* curXY){
	if((buf->x - buf->rad) <= curXY->x && (buf->x + buf->rad) >= curXY->x){
		if((buf->y - buf->rad) <= curXY->y && (buf->y + buf->rad) >= curXY->y){
			if(buf->check.state == 0) { buf->check.cnt++; buf->check.state = 1; }
		}
		else buf->check.state = 0;
	}
	else buf->check.state = 0;
}

void laundry_display(laundry_Typedef* item){
	sprintf(bf, "xstr %d,%d,59,39,0,WHITE,%s,1,1,1,\"%dkg\"", item->x, item->y, user_color[item->color],item->weight);
	nextion_inst_set(bf);
	sprintf(bf, "draw %d,%d,%d,%d,%s", item->x, item->y, item->x + 59, item->y + 39, item->selected ? "GRAY" : "BLACK");
	nextion_inst_set(bf);
}

void add_laundry_dis(void){
	nextion_inst_set("cls WHITE");
	nextion_inst_set("draw 0,0,479,271,BLACK");

	nextion_inst_set("draw 0,0,119,200,BLACK");
	sprintf(bf, "xstr 1,1,118,30,0,%s,WHITE,1,1,1,\"%d/15kg\"", basket[0].weight > 15 ? "RED" : "BLACK", basket[0].weight);
	nextion_inst_set(bf);

	nextion_inst_set("draw 120,0,239,200,BLACK");
	sprintf(bf, "xstr 121,1,118,30,0,%s,WHITE,1,1,1,\"%d/15kg\"", basket[1].weight > 15 ? "RED" : "BLACK", basket[1].weight);
	nextion_inst_set(bf);

	nextion_inst_set("draw 240,0,359,200,BLACK");
	sprintf(bf, "xstr 241,1,118,30,0,%s,WHITE,1,1,1,\"%d/15kg\"", basket[2].weight > 15 ? "RED" : "BLACK", basket[2].weight);
	nextion_inst_set(bf);

	nextion_inst_set("draw 360,0,479,200,BLACK");

	for(uint8_t i = 0 ; i < 3 ; i++){
		for(uint8_t j = 0 ; j < basket[i].laundry_num ; j++){
			basket[i].laundry[j]->x = j % 2 == 0 ? 60 : 0;
			basket[i].laundry[j]->x += (i * 120);

			basket[i].laundry[j]->y = ((9 - j) / 2) * 40;
		}
	}

	for(uint8_t i = 0 ; i < 10 ; i++){
		laundry[i].selected = 0;
		laundry_display(&laundry[i]);
	}
}

void wash_dis(BASKET_Typedef* now_basket, uint8_t* basket_num){
	nextion_inst_set("fill 0,0,160,200,WHITE");
	nextion_inst_set("draw 0,0,160,200,BLACK");
	nextion_inst_set("draw 161,0,479,200,BLACK");
	nextion_inst_set("cir 320,110,70,BLACK");
	nextion_inst_set("cirs 320,110,60,BLUE");

	sprintf(bf, "xstr 1,1,159,30,0,BLACK,WHITE,1,1,1,\"Basket %d\"", *basket_num + 1);
	nextion_inst_set(bf);

	sprintf(bf, "xstr 162,1,300,30,0,BLACK,WHITE,2,1,1,\"Time: %02ds\"", now_basket->cnt);
	nextion_inst_set(bf);

	for(uint8_t i = 0 ; i < now_basket->laundry_num ; i++){
		now_basket->laundry[i]->x = i % 2 == 0 ? 80 : 0;
		now_basket->laundry[i]->y = ((9 - i) / 2) * 40;

		sprintf(bf, "xstr %d,%d,79,39,0,WHITE,%s,1,1,1,\"%dkg\"", now_basket->laundry[i]->x, now_basket->laundry[i]->y, user_color[now_basket->laundry[i]->color], now_basket->laundry[i]->weight);
		nextion_inst_set(bf);
		sprintf(bf, "draw %d,%d,%d,%d,BLACK", now_basket->laundry[i]->x, now_basket->laundry[i]->y, now_basket->laundry[i]->x + 79, now_basket->laundry[i]->y + 39);
		nextion_inst_set(bf);
	}
}

void task_fuc(void){
	POS curXY = { 0, 0 };
	SLIDE_Typedef slide = none;
	MENU_Typedef mode_f = menu;
	uint8_t button_state = 0;

	uint8_t blue_check = 0, red_check = 0;

	nextion_inst_set("cls WHITE");
	nextion_inst_set("cls WHITE");
	nextion_inst_set("cls WHITE");

	main_dis();
	main_dis();
	main_dis();

	while(1){
		get_touch(&curXY);
		slide = get_slide(&curXY);

		if(slide == up_slide) { button_state = 1; nextion_inst_set("fill 0,201,479,100,WHITE"); }
		else if(slide == down_slide) { button_state = 0; nextion_inst_set("fill 0,201,479,100,WHITE"); }

		if(mode_f == menu){
			if(button_data[0].check.state == 1 && blue_check == 0){
				blue_check = 1;
				mode_f = add;
				add_laundry_dis();
			}
			else if(button_data[0].check.state == 0) blue_check = 0;

			if(button_data[2].check.state == 1 && red_check == 0){
				red_check = 1;
				mode_f = wash;
				wash_m = 1;
				nextion_inst_set("cls WHITE");

				wash_dis(&basket[0], &wash_m);
				wash_dis(&basket[0], &wash_m);
				wash_dis(&basket[0], &wash_m);
			}
			else if(button_data[2].check.state == 0) red_check = 0;

			static uint8_t yellow_check = 0;
			static uint32_t tick = 0;
			if(button_data[1].check.state == 1 && yellow_check == 0){
				yellow_check = 1;
				if(button_data[1].check.cnt == 1) tick = HAL_GetTick();
			}
			else if(button_data[1].check.state == 0) yellow_check = 0;

			if(button_data[1].check.cnt >= 1){
				if(HAL_GetTick() - tick > 500){
					for(uint8_t i = 0 ; i < 3 ; i++){
						if(basket[i].weight > 15) continue;
						qsort((laundry_Typedef**) &basket[i].laundry, basket[i].laundry_num, sizeof(laundry_Typedef*), compare_item[button_data[1].check.cnt - 1]);
					}
					button_data[1].check.cnt = 0;
					main_dis();
				}
			}
		}
		else if(mode_f == add){
			static uint8_t touch_check = 0;

			if(curXY.touched == 1 && touch_check == 0){
				static uint8_t cnt = 0;

				if(curXY.x >= 360){
					for(uint8_t i = 0 ; i < 10 ; i++){
						AREA_Typedef check = { laundry[i].x, laundry[i].y, laundry[i].x + 59, laundry[i].y + 39 };
						if(area_check(&curXY, &check) == 1) {
							if(laundry[i].selected == 0) { laundry[i].selected = 1; cnt++; }
							else if(laundry[i].selected == 1) { laundry[i].selected = 0; cnt--; }
							laundry_display(&laundry[i]);
							touch_check = 1;
						}
					}
				}
				else if(cnt > 0){
					for(uint8_t i = 0 ; i < 3 ; i++){
						AREA_Typedef area = { 120 * i, 0, 120 * (i + 1), 200 };
						if(area_check(&curXY, &area) == 1){
							for(uint8_t j = 0 ; j < 10 ; j++){
								if(laundry[j].selected == 1){
									basket[i].laundry[basket[i].laundry_num] = &laundry[j];

									laundry[j].x = (basket[i].laundry_num % 2) == 0 ? 60 : 1;
									laundry[j].x += (i * 120);

									laundry[j].y = ((9 - basket[i].laundry_num) / 2) * 40;

									laundry[j].selected = 0;
									cnt--;

									basket[i].laundry_num++;
									basket[i].weight = 0;
									for(uint8_t k = 0 ; k < basket[i].laundry_num ; k++)
										basket[i].weight += basket[i].laundry[k]->weight;
									basket[i].cnt = basket[i].weight;
								}
							}
							add_laundry_dis();
						}
					}
				}
			}
			else if(curXY.touched == 0) touch_check = 0;

			if(button_data[0].check.state == 1 && blue_check == 0){
				blue_check = 1;
				main_dis();
				mode_f = menu;
			}
			else if(button_data[0].check.state == 0) blue_check = 0;
		}
		else if(mode_f == wash){
			static uint8_t now_basket = 0;

			if(now_basket < 3) setMotor(DRV8830_CW);

			while(basket[now_basket].cnt == 0 || basket[now_basket].cnt > 15){
				now_basket++;
				if(now_basket >= 3) break;
			}

			wash_dis(&basket[now_basket], &now_basket);
			wash_dis(&basket[now_basket], &now_basket);
			wash_dis(&basket[now_basket], &now_basket);

			if(now_basket >= 3){
				for(uint8_t i = 0 ; i < 3 ; i++){
					for(uint8_t j = 0 ; j < basket[i].laundry_num ; j++){
						basket[i].laundry[j]->x = 500;
						basket[i].laundry[j]->y = 500;
						basket[i].laundry[j] = NULL;
					}
					basket[i].cnt = 0;
					basket[i].laundry_num = 0;
					basket[i].weight = 0;
				}

				main_dis();
				main_dis();
				main_dis();

				wash_m = 0;
				now_basket = 0;
				mode_f = menu;
				buz_on = 1;
				setMotor(DRV8830_STOP);
			}
			else if(wash_cnt >= 1000){
				wash_cnt = 0;
				basket[now_basket].cnt--;

				wash_dis(&basket[now_basket], &now_basket);
				wash_dis(&basket[now_basket], &now_basket);
				wash_dis(&basket[now_basket], &now_basket);

				if(basket[now_basket].cnt == 0) {
					now_basket++;
					wash_m = 0;
					setMotor(DRV8830_STOP);
					HAL_Delay(1000);
					wash_m = 1;
				}
			}
		}

		if(button_state == 1){
			for(uint8_t i = 0 ; i < 3 ; i++){
				button_check(&button_data[i], &curXY);
				button_draw(&button_data[i]);
			}
		}
		else kg_draw();
	}
}

void HAL_SYSTICK_Callback(void){
	if(buz_on) buz_cnt++;
	if(wash_m) wash_cnt++;

	if(buz_on == 1){
		if(buz_cnt < 250) BUZ(1);
		else if(buz_cnt < 500) BUZ(0);
		else if(buz_cnt < 750) BUZ(1);
		else{
			buz_on = buz_cnt = 0;
			BUZ(0);
		}
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

	LED1(0);
	LED2(0);
	LED3(0);

	setMotor(DRV8830_STOP);

	nextion_inst_set("door_button.txt=\"Unlocked Door\"");
	nextion_inst_set("car_t.txt=\"Waiting for car\"");
	nextion_inst_set("post_t.txt=\"Empty\"");

	nextion_inst_set("baud=921600");
	nextion_inst_set("baud=921600");
	nextion_inst_set("baud=921600");
	HAL_Delay(50);
	USART1->CR1 &= (~USART_CR1_UE);
	USART1->BRR = 0x23;
	USART1->CR1 |= USART_CR1_UE;
	HAL_Delay(50);
	HAL_Delay(1000);

	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1)
	{

		if(SW2){
			if(HAL_GetTick() - start_cnt > 3000) task_fuc();
		}
		else start_cnt = HAL_GetTick();
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
