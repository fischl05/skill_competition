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
	none,
	up_slide,
	down_slide,
}SLIDE_Typedef;

typedef enum{
	basic_display,
	add_display,
	wash_display,
}HIGHER_MENU_Typedef;

typedef enum{
	weight_display,
	button_display,
}LOWER_MENU_Typedef;

typedef enum{
	black,
	blue,
	green,
	brown,
	red,
	yellow,
	max_color,
}COLOR_Typedef;

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
	COLOR_Typedef color;
	uint8_t weight;
	uint8_t selected;
}LAUNDRY_Typedef;

typedef struct{
	AREA_Typedef area;
	LAUNDRY_Typedef* laundry[10];
	uint8_t laundry_num;

	uint8_t weight;
	uint8_t cnt;
}BASKET_Typedef;

typedef struct{
	AREA_Typedef area;
	COLOR_Typedef color;
	uint8_t radius;
	uint8_t touched;
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
char* color_item[max_color] = { "BLACK", "BLUE", "GREEN", "BROWN", "RED", "YELLOW" };

uint8_t screen_update = 0;
uint8_t wash_basket   = 0;
uint32_t wash_tick    = 0;

POS_Typedef curXY = { 0, 0, 0 };
SLIDE_Typedef slide = none;

LAUNDRY_Typedef laundry[10] = {
		{ { 360, 160, 360 + 60, 160 + 40}, red,   5, 0 },
		{ { 420, 160, 420 + 60, 160 + 40}, black, 5, 0 },

		{ { 360, 120, 360 + 60, 120 + 40}, brown, 4, 0 },
		{ { 420, 120, 420 + 60, 120 + 40}, blue,  4, 0 },

		{ { 360, 80 , 360 + 60, 80  + 40}, green, 3, 0 },
		{ { 420, 80 , 420 + 60, 80  + 40}, green, 3, 0 },

		{ { 360, 40, 360 + 60, 40 + 40},   blue,  2, 0 },
		{ { 420, 40, 420 + 60, 40 + 40},   brown, 2, 0 },

		{ { 360, 0, 360 + 60, 0 + 40},     black, 1, 0 },
		{ { 420, 0, 420 + 60, 0 + 40},     red,   1, 0 },
};

BASKET_Typedef basket[3] = {
		{ { 0,   0, 160, 200 }, },
		{ { 160, 0, 320, 200 }, },
		{ { 320, 0, 480, 200 }, },
};

BUTTON_Typedef button_data[3] = {
		{ { 160 - 20, 230 - 20, 160 + 20, 230 + 20 }, blue,   20, 0 },
		{ { 240 - 20, 230 - 20, 240 + 20, 230 + 20 }, yellow, 20, 0 },
		{ { 320 - 20, 230 - 20, 320 + 20, 230 + 20 }, red,    20, 0 },
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

void BUZ(uint8_t state){
	if(state) HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
	else      HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
}

int weight_compare(const void* mem1, const void* mem2){
	LAUNDRY_Typedef** a = (LAUNDRY_Typedef**)mem1;
	LAUNDRY_Typedef** b = (LAUNDRY_Typedef**)mem2;

	if((*a)->weight < (*b)->weight) return 1;
	else return 0;
}

int color_compare(const void* mem1, const void* mem2){
	LAUNDRY_Typedef** a = (LAUNDRY_Typedef**)mem1;
	LAUNDRY_Typedef** b = (LAUNDRY_Typedef**)mem2;

	if((*a)->color > (*b)->color) return 1;
	else return 0;
}

int (*compare_item[2])(const void* mem1, const void* mem2) = { weight_compare, color_compare, };

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

void get_slide(POS_Typedef* buf, SLIDE_Typedef* slide){
	static POS_Typedef touch_xy = { 0, 0, 0 };
	static POS_Typedef befo_xy = { 0, 0, 0 };

	if(befo_xy.touched != buf->touched){
		if(buf->touched == 0){
			if(befo_xy.y > touch_xy.y){
				if(befo_xy.y - touch_xy.y > 30) { *slide = down_slide; }
			}
			else if(touch_xy.y > befo_xy.y){
				if(touch_xy.y - befo_xy.y > 30) { *slide = up_slide; }
			}
		}
		else touch_xy = *buf;
	}
	else *slide = none;

	befo_xy = *buf;
}

uint8_t area_check(POS_Typedef* xy, AREA_Typedef* area){
	if(xy->x >= area->x0 && xy->x <= area->x1){
		if(xy->y >= area->y0 && xy->y <= area->y1){
			screen_update = 0;
			return 1;
		}
	}
	return 0;
}

void laundry_draw(LAUNDRY_Typedef* data){
	sprintf(bf, "xstr %d,%d,60,40,0,WHITE,%s,1,1,1,\"%dkg\"", data->area.x0, data->area.y0, color_item[data->color], data->weight);
	nextion_inst_set(bf);
	sprintf(bf, "draw %d,%d,%d,%d,%s", data->area.x0, data->area.y0, data->area.x1, data->area.y1, data->selected == 1 ? "GRAY" : "BLACK");
	nextion_inst_set(bf);
}

void button_draw(BUTTON_Typedef* data){
	if(area_check(&curXY, &data->area)){
		uint16_t color = 0;

		color |= data->color == red || data->color == yellow ? 31 << 11 : 10 << 11;
		color |= data->color == yellow ? 62 << 5 : 40 << 5;
		color |= data->color == blue ? 31 : 20;

		sprintf(bf, "cirs %d,%d,%d,%d", data->area.x0 + 20, data->area.y0 + 20, data->radius, color);
		nextion_inst_set(bf);
	}
	else{
		sprintf(bf, "cirs %d,%d,%d,%s", data->area.x0 + 20, data->area.y0 + 20, data->radius, color_item[data->color]);
		nextion_inst_set(bf);
	}
	sprintf(bf, "cir %d,%d,%d,BLACK", data->area.x0 + 20, data->area.y0 + 20, data->radius);
	nextion_inst_set(bf);
}

void higher_menu_display(HIGHER_MENU_Typedef* data){
	if(*data == basic_display){
		for(uint8_t i = 0 ; i < 3 ; i++){
			sprintf(bf, "draw %d,%d,%d,%d,BLACK", basket[i].area.x0, basket[i].area.y0, basket[i].area.x1, basket[i].area.y1);
			nextion_inst_set(bf);

			sprintf(bf, "xstr %d,1,158,30,0,BLACK,WHITE,1,1,1,\"Basket %d\"", basket[i].area.x0 + 1, i + 1);
			nextion_inst_set(bf);

			for(uint8_t j = 0 ; j < basket[i].laundry_num ; j++){
				basket[i].laundry[j]->area.x0 = j % 2 * 80;
				basket[i].laundry[j]->area.x0 += i * 160;

				basket[i].laundry[j]->area.y0 = (9 - j) / 2 * 40;

				basket[i].laundry[j]->area.x1 = basket[i].laundry[j]->area.x0 + 80;
				basket[i].laundry[j]->area.y1 = basket[i].laundry[j]->area.y0 + 40;

				sprintf(bf, "xstr %d,%d,80,40,0,WHITE,%s,1,1,1,\"%dkg\"", basket[i].laundry[j]->area.x0, basket[i].laundry[j]->area.y0, color_item[basket[i].laundry[j]->color], basket[i].laundry[j]->weight);
				nextion_inst_set(bf);
				sprintf(bf, "draw %d,%d,%d,%d,BLACK", basket[i].laundry[j]->area.x0, basket[i].laundry[j]->area.y0, basket[i].laundry[j]->area.x1, basket[i].laundry[j]->area.y1);
				nextion_inst_set(bf);
			}
		}
	}
	else if(*data == add_display){
		for(uint8_t i = 0 ; i < 3 ; i++){
			sprintf(bf, "draw %d,0,%d,200,BLACK", i * 120, i * 120 + 120);
			nextion_inst_set(bf);

			sprintf(bf, "xstr %d,1,118,30,0,%s,WHITE,1,1,1,\"%d/15kg\"", i * 120 + 1, basket[i].weight > 15 ? "RED" : "BLACK", basket[i].weight);
			nextion_inst_set(bf);
		}
		nextion_inst_set("draw 360,0,360+120,200,BLACK");

		for(uint8_t i = 0 ; i < 3 ; i++){
			for(uint8_t j = 0 ; j < basket[i].laundry_num ; j++){
				basket[i].laundry[j]->area.x0 = j % 2 * 60;
				basket[i].laundry[j]->area.x0 += i * 120;

				basket[i].laundry[j]->area.y0 = (9 - j) / 2* 40;

				basket[i].laundry[j]->area.x1 = basket[i].laundry[j]->area.x0 + 60;
				basket[i].laundry[j]->area.y1 = basket[i].laundry[j]->area.y0 + 40;
			}
		}

		for(uint8_t i = 0 ; i < 10 ; i++) laundry_draw(&laundry[i]);
		for(uint8_t i = 0 ; i < 10 ; i++){
			if(laundry[i].selected == 1){
				sprintf(bf, "draw %d,%d,%d,%d,GRAY", laundry[i].area.x0, laundry[i].area.y0, laundry[i].area.x1, laundry[i].area.y1);
				nextion_inst_set(bf);
			}
		}
	}
	else{
		sprintf(bf, "xstr 0,1,158,30,0,BLACK,WHITE,1,1,1,\"Basket %d\"", wash_basket + 1);
		nextion_inst_set(bf);

		sprintf(bf, "xstr 270,1,200,30,0,BLACK,WHITE,2,1,1,\"Time: %02ds\"", basket[wash_basket].cnt);
		nextion_inst_set(bf);

		nextion_inst_set("cirs 320,110,60,BLUE");
		nextion_inst_set("cir 320,110,70,BLACK");

		nextion_inst_set("draw 0,0,160,200,BLACK");
		nextion_inst_set("draw 160,0,480,200,BLACK");

		for(uint8_t i = 0  ; i < basket[wash_basket].laundry_num ; i++){
			basket[wash_basket].laundry[i]->area.x0 = i % 2 * 80;
			basket[wash_basket].laundry[i]->area.y0 = (9 - i) / 2 * 40;

			basket[wash_basket].laundry[i]->area.x1 = basket[wash_basket].laundry[i]->area.x0 + 80;
			basket[wash_basket].laundry[i]->area.y1 = basket[wash_basket].laundry[i]->area.y0 + 40;

			sprintf(bf, "xstr %d,%d,80,40,0,WHITE,%s,1,1,1,\"%dkg\"", basket[wash_basket].laundry[i]->area.x0, basket[wash_basket].laundry[i]->area.y0, color_item[basket[wash_basket].laundry[i]->color], basket[wash_basket].laundry[i]->weight);
			nextion_inst_set(bf);
			sprintf(bf, "draw %d,%d,%d,%d,BLACK", basket[wash_basket].laundry[i]->area.x0, basket[wash_basket].laundry[i]->area.y0, basket[wash_basket].laundry[i]->area.x1, basket[wash_basket].laundry[i]->area.y1);
			nextion_inst_set(bf);
		}
	}
}

void lower_menu_display(LOWER_MENU_Typedef* data){
	if(*data == weight_display){
		for(uint8_t i = 0 ; i < 3 ; i++){
			sprintf(bf, "xstr %d,201,160,30,0,%s,WHITE,1,1,1,\"%d/15kg\"", basket[i].area.x0, basket[i].weight > 15 ? "RED" : "BLACK", basket[i].weight);
			nextion_inst_set(bf);
		}
	}
	else if(*data == button_display){
		for(uint8_t i = 0 ; i < 3 ; i++){
			button_draw(&button_data[i]);
		}
	}
}

void task_fuc(void){
	HIGHER_MENU_Typedef higher_menu = basic_display;
	LOWER_MENU_Typedef lower_menu   = weight_display;

	uint8_t befo_touch = 0;

	uint8_t  yellow_cnt = 0;
	uint32_t yellow_tick = 0;

	nextion_inst_set("cls WHITE");
	nextion_inst_set("cls WHITE");
	nextion_inst_set("cls WHITE");

	while(1){
		get_touch(&curXY);
		get_slide(&curXY, &slide);

		if(slide != none){
			screen_update = 0;

			if(slide == up_slide)        lower_menu = button_display;
			else if(slide == down_slide) lower_menu = weight_display;
		}

		if(screen_update == 0){
			screen_update = 1;

			nextion_inst_set("cls WHITE");

			higher_menu_display(&higher_menu);
			lower_menu_display(&lower_menu);
		}

		if(befo_touch != curXY.touched && curXY.touched == 1){
			if(higher_menu == add_display){
				if(curXY.x > 360){
					for(uint8_t i = 0 ; i < 10 ; i++){
						if(area_check(&curXY, &laundry[i].area)){
							laundry[i].selected = laundry[i].selected == 1 ? 0 : 1;
						}
					}
				}

				for(uint8_t i = 0 ; i < 3 ; i++){
					AREA_Typedef basket_area = { i * 120, 0, i * 120 + 120, 200 };
					if(area_check(&curXY, &basket_area)){
						for(uint8_t j = 0 ; j < 10 ; j++){
							if(laundry[j].selected == 1){
								basket[i].laundry[basket[i].laundry_num] = &laundry[j];

								laundry[j].area.x0 = basket[i].laundry_num % 2 * 60;
								laundry[j].area.x0 += i * 120;

								laundry[j].area.y0 = (9 - basket[i].laundry_num) / 2 * 40;

								laundry[j].area.x1 = laundry[j].area.x0 + 60;
								laundry[j].area.y1 = laundry[j].area.y0 + 40;

								laundry[j].selected = 0;

								basket[i].laundry_num++;
							}
						}
						basket[i].weight = 0;
						for(uint8_t j = 0 ; j < basket[i].laundry_num ; j++) basket[i].weight += basket[i].laundry[j]->weight;
						basket[i].cnt = basket[i].weight;
					}
				}
			}

			if(lower_menu == button_display){
				if(area_check(&curXY, &button_data[0].area))      higher_menu = higher_menu == add_display ? basic_display : higher_menu == basic_display ? add_display : wash_display;
				else if(area_check(&curXY, &button_data[1].area)) yellow_cnt = yellow_cnt < 2 ? yellow_cnt + 1 : yellow_cnt;
				else if(area_check(&curXY, &button_data[2].area)){
					setMotor(DRV8830_CW);
					higher_menu = wash_display;
					wash_basket = 0;
				}
			}
		}

		if(higher_menu == wash_display){
			if(wash_basket == 2 && basket[wash_basket].cnt == 0){
				screen_update = 0;

				setMotor(DRV8830_STOP);
				BUZ(1);
				HAL_Delay(500);
				BUZ(0);
				HAL_Delay(500);
				BUZ(1);
				HAL_Delay(500);
				BUZ(0);
				higher_menu = basic_display;

				for(uint8_t i = 0 ; i < 3 ; i++){
					if(basket[i].weight > 15) continue;

					for(uint8_t j = 0 ; j < basket[i].laundry_num ; j++){
						memset((LAUNDRY_Typedef*)basket[i].laundry[j], 0, sizeof(LAUNDRY_Typedef));
						basket[i].laundry[j]->area.x0 = 500;
						basket[i].laundry[j]->area.y0 = 500;
					}
					basket[i].cnt = 0;
					basket[i].laundry_num = 0;
					basket[i].weight = 0;
					memset((LAUNDRY_Typedef**)basket[i].laundry, 0, sizeof(LAUNDRY_Typedef*) * 10);
				}
			}

			if(basket[wash_basket].cnt > 15 || basket[wash_basket].cnt == 0) {
				if(basket[wash_basket].cnt == 0 && basket[wash_basket].weight > 0){
					setMotor(DRV8830_STOP);
					HAL_Delay(1000);
					wash_tick = HAL_GetTick();
					if(wash_basket < 2) { if(basket[wash_basket + 1].cnt > 0) screen_update = 0; }
					setMotor(DRV8830_CW);
				}
				wash_basket = wash_basket < 2 ? wash_basket+ 1 : wash_basket;
			}

			if(HAL_GetTick() - wash_tick >= 1000){
				screen_update = 0;
				wash_tick = HAL_GetTick();
				if(basket[wash_basket].cnt > 0) { basket[wash_basket].cnt--; setMotor(DRV8830_CW); }
			}
		}
		else wash_tick = HAL_GetTick();

		if(yellow_cnt > 0){
			if(HAL_GetTick() - yellow_tick > 500){
				for(uint8_t i = 0 ; i < 3 ; i++){
					if(basket[i].weight > 15) continue;
					qsort((LAUNDRY_Typedef**)&basket[i].laundry, basket[i].laundry_num, sizeof(LAUNDRY_Typedef*), compare_item[yellow_cnt - 1]);
				}
				screen_update = 0;
				yellow_cnt = 0;
			}
		}
		else yellow_tick = HAL_GetTick();

		if(befo_touch != curXY.touched) screen_update = 0;
		befo_touch = curXY.touched;
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

	nextion_inst_set("dp=0");
	nextion_inst_set("dp=0");
	nextion_inst_set("dp=0");

	nextion_inst_set("door_button.txt=\"Unlocked Door\"");
	nextion_inst_set("door_button.txt=\"Unlocked Door\"");
	nextion_inst_set("door_button.txt=\"Unlocked Door\"");

	nextion_inst_set("car_t.txt=\"Waiting for car\"");
	nextion_inst_set("car_t.txt=\"Waiting for car\"");
	nextion_inst_set("car_t.txt=\"Waiting for car\"");

	nextion_inst_set("post_t.txt=\"Empty\"");
	nextion_inst_set("post_t.txt=\"Empty\"");
	nextion_inst_set("post_t.txt=\"Empty\"");

	uint32_t tick = 0;

	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1)
	{
		if(SW(2)){
			if(HAL_GetTick() - tick > 3000){
				nextion_inst_set("cls WHITE");
				task_fuc();
			}
		}
		else tick = HAL_GetTick();
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

