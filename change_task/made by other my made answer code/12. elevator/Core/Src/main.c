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
#include <time.h>
#include "stm32l0xx_hal.h"
#include "sht41.h"
#include "ens160.h"
#include "drv8830.h"
#include "NEXTION.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef enum{
	in_human,
	out_human,
}HUMAN_STATE_Typedef;

typedef enum{
	open,
	close
}DOOR_Typedef;

typedef enum{
	floor_1,
	floor_2,
	floor_3,
	floor_4,
	floor_5,
	floor_6,
	floor_7,
	floor_8,
	floor_9,
	floor_10,
	max_floor,
}FLOOR_Typedef;

typedef enum{
	sht41_temp,
	sht41_hum,
	ens160_co2,
	max_addr,
}SENSOR_ADDR_Typedef;

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
	uint16_t value;
	char* color;
}SENSOR_Typedef;

typedef struct{
	FLOOR_Typedef now_floor;
	FLOOR_Typedef goal_floor;

	HUMAN_STATE_Typedef human;

	uint8_t move_state;
	uint32_t move_tick;
	uint32_t floor_tick;
}ELEVATOR_Typedef;

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

uint8_t screen_update    = 0;
uint8_t call_state       = 0;
FLOOR_Typedef user_floor = floor_1;

POS_Typedef curXY = { 0, 0, 0 };
SENSOR_Typedef sensor[3] = {
		{ { 0,   0, 0   + 160, 30 }, 0, "RED" },
		{ { 160, 0, 160 + 160, 30 }, 0, "BLUE" },
		{ { 320, 0, 320 + 160, 30 }, 0, "GRAY" },
};

AREA_Typedef call_button_area = { 350, 150, 350 + 100, 150 + 40 };
AREA_Typedef keypad[12];

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

void BUZ_hz_set(uint16_t hz){
	TIM2->ARR = 1000000 / hz - 1;
	TIM2->CCR1 = TIM2->ARR / 2;
}

void* get_sensor(SENSOR_ADDR_Typedef data){
	if(data == sht41_temp || data == sht41_hum){
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

void free_reset(void** addr){
	free(*addr);
	*addr = NULL;
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

uint8_t area_check(POS_Typedef* xy, AREA_Typedef* area){
	if(xy->x >= area->x0 && xy->x <= area->x1){
		if(xy->y >= area->y0 && xy->y <= area->y1){
			return 1;
		}
	}
	return 0;
}

void main_dis(ELEVATOR_Typedef* data, uint8_t* door_x){
	/* get sensor value */
	SHT41_t*  sht41_value  = get_sensor(sht41_temp);
	uint16_t* ens160_value = get_sensor(ens160_co2);

	sensor[sht41_temp].value = (uint16_t)(sht41_value->temperature * 10.0f);
	sensor[sht41_hum].value  = (uint16_t)(sht41_value->humidity    * 10.0f);
	sensor[ens160_co2].value = *ens160_value;

	free_reset((void*)&sht41_value);
	free_reset((void*)&ens160_value);

	/* sensor values draw */
	for(SENSOR_ADDR_Typedef i = sht41_temp ; i < max_addr ; i++){
		if(i == sht41_temp)      sprintf(bf, "xstr %d,%d,160,30,0,WHITE,%s,1,1,1,\"%02d.%d%cC\"", sensor[i].area.x0, sensor[i].area.y0, sensor[i].color, sensor[i].value / 10, sensor[i].value % 10, 0xb0);
		else if(i == sht41_hum)  sprintf(bf, "xstr %d,%d,160,30,0,WHITE,%s,1,1,1,\"%02d.%d%%\"", sensor[i].area.x0, sensor[i].area.y0, sensor[i].color, sensor[i].value / 10, sensor[i].value % 10);
		else if(i == ens160_co2) sprintf(bf, "xstr %d,%d,160,30,0,WHITE,%s,1,1,1,\"%d\"", sensor[i].area.x0, sensor[i].area.y0, sensor[i].color, sensor[i].value);
		nextion_inst_set(bf);

		sprintf(bf, "draw %d,%d,%d,%d,BLACK", sensor[i].area.x0, sensor[i].area.y0, sensor[i].area.x1, sensor[i].area.y1);
		nextion_inst_set(bf);
	}

	/* floor draw */
	for(uint8_t i = 0 ; i < 10 ; i++){
		sprintf(bf, "fill 60,%d,45,15,WHITE", 100 + i * 15);
		nextion_inst_set(bf);

		sprintf(bf, "draw %d,%d,%d,%d,BLACK", 60, 100 + i * 15, 60 + 45, 100 + (i + 1) * 15);
		nextion_inst_set(bf);
	}

	/* floor unit draw */
	uint8_t cnt = 0;

	if(sensor[sht41_temp].value > 300)  cnt++;
	if(sensor[sht41_hum].value  > 700)  cnt++;
	if(sensor[ens160_co2].value > 1000) cnt++;

	sprintf(bf, "fill %d,%d,15,15,%s", 75, 100 + (9 - data->now_floor) * 15, cnt < 1 ? "GREEN" : cnt < 3 ? "YELLOW" : "RED");
	nextion_inst_set(bf);
	sprintf(bf, "draw %d,%d,%d,%d,BLACK", 75, 100 + (9 - data->now_floor) * 15, 75 + 15, 115 + (9 - data->now_floor) * 15);
	nextion_inst_set(bf);

	/* elevator draw */
	sprintf(bf, "xstr 190,60,100,30,0,RED,BLACK,1,1,1,\"%dF\"", data->now_floor + 1);
	nextion_inst_set(bf);

	if(data->move_state == 1){
		if(data->now_floor > data->goal_floor){
			nextion_inst_set("line 200,70,205,80,RED");
			nextion_inst_set("line 205,80,210,70,RED");
		}
		else if(data->goal_floor > data->now_floor){
			nextion_inst_set("line 200,80,205,70,RED");
			nextion_inst_set("line 205,70,210,80,RED");
		}
	}

	/* elevator door draw */
	nextion_inst_set("fill 160,95,160,170,GRAY");
	nextion_inst_set("draw 160,95,160+160,95+170,BLACK");

	nextion_inst_set("fill 160,95,160,30,WHITE");
	nextion_inst_set("draw 160,95,320,95+30,BLACK");

	/* left door */
	sprintf(bf, "fill 160,125,%d,140,WHITE", *door_x);
	nextion_inst_set(bf);
	sprintf(bf, "draw %d,%d,%d,%d,BLACK", 160, 125, 160 + *door_x, 125 + 140);
	nextion_inst_set(bf);

	/* right door */
	sprintf(bf, "fill %d,125,%d,140,WHITE", 240 + (80 - *door_x), *door_x);
	nextion_inst_set(bf);
	sprintf(bf, "draw %d,%d,%d,%d,BLACK", 240 + (80 - *door_x), 125, 320, 125 + 140);
	nextion_inst_set(bf);

	nextion_inst_set("fill 321,31,160,241,WHITE");

	if(data->human == out_human){
		sprintf(bf, "xstr 350,110,100,30,0,BLACK,WHITE,1,1,1,\"%dF\"", user_floor + 1);
		nextion_inst_set(bf);
		nextion_inst_set("draw 350,110,350+100,110+30,BLACK");

		sprintf(bf, "fill %d,%d,100,40,GRAY", call_button_area.x0, call_button_area.y0);
		nextion_inst_set(bf);
		sprintf(bf, "draw %d,%d,%d,%d,%s", call_button_area.x0, call_button_area.y0, call_button_area.x1, call_button_area.y1, call_state == 0 ? "BLACK" : "RED");
		nextion_inst_set(bf);

		sprintf(bf, "cir %d,%d,15,%s", call_button_area.x0 + 50, call_button_area.y0 + 20, call_state == 0 ? "BLACK" : "RED");
		nextion_inst_set(bf);
	}
	else{
		for(uint8_t i = 0 ; i < 10 ; i++){
			sprintf(bf, "xstr %d,%d,30,30,0,%s,GRAY,1,1,1,\"%d\"", keypad[i].x0, keypad[i].y0, (i == data->goal_floor && data->move_state == 1) ? "RED" : "BLACK", i + 1);
			nextion_inst_set(bf);

			sprintf(bf, "draw %d,%d,%d,%d,%s", keypad[i].x0, keypad[i].y0, keypad[i].x1, keypad[i].y1, (i == data->goal_floor && data->move_state == 1) ? "RED" : "BLACK");
			nextion_inst_set(bf);
		}
	}
}

void task_fuc(void){
	uint32_t sensor_tick = 0;

	DOOR_Typedef now_door_state = close;
	uint32_t door_tick = 0;
	uint8_t door_x = 80;

	uint32_t buz_tick = 0;
	uint8_t buzM = 0;

	uint8_t befo_touch = 0;

	ELEVATOR_Typedef elevator = { floor_1, elevator.now_floor, out_human, 0, 0, 0 };

	srand((unsigned)time(NULL));

	nextion_inst_set("cls WHITE");
	nextion_inst_set("cls WHITE");
	nextion_inst_set("cls WHITE");

	while(1){
		get_touch(&curXY);

		if(screen_update == 0){
			main_dis(&elevator, &door_x);

			screen_update = 1;
		}

		if(befo_touch != curXY.touched && curXY.touched == 1){
			AREA_Typedef elevator_area = { 160, 95, 320, 95 + 170 };

			if(elevator.human == out_human){
				if(area_check(&curXY, &call_button_area)){
					screen_update = 0;

					call_state = call_state == 0 ? 1 : call_state;

					elevator.goal_floor = user_floor;
					elevator.move_state = 1;
				}
			}
			else{
				for(FLOOR_Typedef i = 0 ; i < max_floor ; i++){
					if(area_check(&curXY, &keypad[i])){
						screen_update = 0;

						call_state = 1;

						elevator.move_state = 1;
						elevator.goal_floor = i;
						now_door_state = close;

						break;
					}
				}
			}

			if(area_check(&curXY, &elevator_area) && now_door_state == open){
				screen_update = 0;

				elevator.human = elevator.human == out_human ? in_human : out_human;
			}
		}

		if(HAL_GetTick() - sensor_tick >= 500){
			sensor_tick = HAL_GetTick();

			screen_update = 0;
		}

		if(elevator.human == out_human && call_state == 0 && now_door_state == close){
			if(elevator.move_state == 0){
				if(HAL_GetTick() - elevator.floor_tick >= 10000){
					elevator.floor_tick = HAL_GetTick();

					while(elevator.now_floor == elevator.goal_floor) elevator.goal_floor = rand() % max_floor;
					elevator.move_state = 1;
				}
			}
		}
		else elevator.floor_tick = HAL_GetTick();

		if(elevator.move_state == 1){
			if(HAL_GetTick() - elevator.move_tick >= 1000){
				elevator.move_tick = HAL_GetTick();

				screen_update = 0;

				if(door_x == 80) elevator.now_floor = elevator.now_floor > elevator.goal_floor ? elevator.now_floor - 1 : elevator.now_floor < elevator.goal_floor ? elevator.now_floor + 1 : elevator.now_floor;

				if(elevator.now_floor == elevator.goal_floor) {
					if(call_state == 1){
						buzM = 1;

						now_door_state = now_door_state == close ? open : close;
						call_state = 0;
						user_floor = elevator.now_floor;
					}
					elevator.move_state = 0;
				}
			}
		}
		else elevator.move_tick = HAL_GetTick();

		if(HAL_GetTick() - door_tick > 30){
			static uint8_t sensing_pir = 0;

			door_tick = HAL_GetTick();

			if(now_door_state == open && door_x > 0){
				screen_update = 0;
				door_x--;
				setMotor(DRV8830_CW);

				if((PIR_LEFT || PIR_RIGHT) && sensing_pir == 0){
					sensing_pir = 1;

					BUZ_hz_set(4000);

					setMotor(DRV8830_STOP);

					for(uint8_t i = 0 ; i < 5 ; i++){
						BUZ(1);
						HAL_Delay(50);
						BUZ(0);
						HAL_Delay(50);
					}

					now_door_state = close;
				}
			}
			else if(now_door_state == close && door_x < 80){
				screen_update = 0;
				door_x++;
				setMotor(DRV8830_CCW);

				if((PIR_LEFT || PIR_RIGHT) && sensing_pir == 0){
					sensing_pir = 1;

					BUZ_hz_set(4000);

					setMotor(DRV8830_STOP);

					for(uint8_t i = 0 ; i < 5 ; i++){
						BUZ(1);
						HAL_Delay(50);
						BUZ(0);
						HAL_Delay(50);
					}

					now_door_state = open;
				}
			}
			else { setMotor(DRV8830_STOP); sensing_pir = 0; }
		}

		if(buzM == 1){
			static uint16_t buz_hz = 600;

			BUZ(1);
			BUZ_hz_set(buz_hz);

			if(HAL_GetTick() - buz_tick >= 200){
				buz_tick = HAL_GetTick();

				buz_hz += 200;
			}

			if(buz_hz >= 1400) { BUZ(0); buzM = 0; buz_hz = 600; }
		}
		else buz_tick = HAL_GetTick();

		befo_touch = curXY.touched;

		/* coordinate display for task testing */
		/* sprintf(bf, "xstr 0,31,150,30,0,BLACK,WHITE,0,1,1,\"X: %d Y: %d\"", curXY.x, curXY.y); */
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

	LED1(0);
	LED2(0);
	LED3(0);

	setMotor(DRV8830_STOP);

	nextion_inst_set("baud=921600");
	nextion_inst_set("baud=921600");
	nextion_inst_set("baud=921600");
	HAL_Delay(50);
	USART1->CR1 &= (~USART_CR1_UE);
	USART1->BRR = 0x23;
	USART1->CR1 |= USART_CR1_UE;
	HAL_Delay(1000);

	for(uint8_t i = 0 ; i < 10 ; i++){
		keypad[i].x0 = (i / 5) * 70 + 350;
		keypad[i].y0 = (i % 5) * 40 + 60;

		keypad[i].x1 = keypad[i].x0 + 30;
		keypad[i].y1 = keypad[i].y0 + 30;
	}

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
