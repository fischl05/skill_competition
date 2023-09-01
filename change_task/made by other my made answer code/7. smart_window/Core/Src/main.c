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

/* for sensor getting value */
typedef enum{
	get_sht41,
	get_ens160,
}GET_SENSOR_Typedef;

/* sensor data address */
typedef enum{
	sht41_temp,
	sht41_hum,
	ens160_co2,
}SENSOR_ADR_Typedef;

/* door now state */
typedef enum{
	close,
	open,
}DOOR_OPEN_Typedef;

/* now time state */
typedef enum{
	day,
	night,
}DAY_Typedef;

/* button data address */
typedef enum{
	auto_button,
	time_button,
}BUTTON_SEL_Typedef;

/* touch coordinate & now touch state */
typedef struct{
	uint16_t x;
	uint16_t y;
	uint8_t touched;
}POS_Typedef;

/* each parts touch area */
typedef struct{
	uint16_t x0, y0;
	uint16_t x1, y1;
}AREA_Typedef;

/* for Now time display */
typedef struct{
	uint8_t hour;
	uint8_t min;
}TIME_Typedef;

/* Button data */
typedef struct{
	AREA_Typedef area;
	char* color;
	char* text;
}BUTTON_Typedef;

/* for sensor value display */
typedef struct{
	AREA_Typedef area;
	uint16_t value;
	char* color;
}SENSOR_Typedef;
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
TIME_Typedef time = { 0, 0 };
BUTTON_Typedef button[2] = {
		{ { 395, 30,  395 + 50, 30  + 30 }, "GREEN", "auto" },
		{ { 395, 100, 395 + 50, 100 + 30 }, "GREEN", "time" },
};

SENSOR_Typedef sensor[3] = {
		{ { 50,  20, 50  + 90, 20 + 30 }, 0, "RED",  },
		{ { 140, 20, 140 + 90, 20 + 30 }, 0, "BLUE", },
		{ { 230, 20, 230 + 90, 20 + 30 }, 0, "GRAY", },
};
AREA_Typedef window_area = { 40, 80, 330, 240 };
DOOR_OPEN_Typedef door_state = close;

uint8_t screen_update    = 0;
uint8_t door_move        = 0;
uint8_t auto_buz         = 0;
uint16_t door_coordinate = 140;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */

/* get sensor values */
void* get_sensor(GET_SENSOR_Typedef mem){
	/* return sht41_t address */
	if(mem == get_sht41){
		SHT41_t* buf = (SHT41_t*)malloc(sizeof(SHT41_t));
		*buf = getTempSht41();
		return buf;
	}
	/* return uint16_t address */
	else{
		uint16_t* buf = (uint16_t*)malloc(sizeof(uint16_t));
		*buf = getCO2();
		return buf;
	}
}

/* free and NULL reset */
void free_value(void** address){
	/* deallocate dynamic memory */
	free(*address);

	/* reset NULL */
	*address = NULL;
}

/* BUZZER on/off set */
void BUZ(uint8_t state){
	if(state == 1) HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
	else HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
}

/* buzzer frequency set */
void BUZ_hz_set(uint16_t hz){
	TIM2->ARR = 1000000 / hz;
	TIM2->CCR1 = TIM2->ARR / 2;
}

/* NEXTION instruction set */
void nextion_inst_set(char* str){
	uint8_t end_cmd[3] = { 0xFF, 0xFF, 0xFF };
	HAL_UART_Transmit(&huart1, (uint8_t*)str, strlen(str), 100);
	HAL_UART_Transmit(&huart1, end_cmd, 3, 100);
}

/* get touch coordinate */
void get_touch(POS_Typedef* buf){
	HAL_StatusTypeDef res = HAL_OK;
	uint8_t rx_data[8] = { 0, };

	/* x coordinate */
	nextion_inst_set("get tch0");
	res = HAL_UART_Receive(&huart1, rx_data, 8, 100);
	if(res == HAL_OK) { if(rx_data[0] == 0x71) { buf->x = rx_data[2] << 8 | rx_data[1]; } }

	/* y coordinate */
	nextion_inst_set("get tch1");
	res = HAL_UART_Receive(&huart1, rx_data, 8, 100);
	if(res == HAL_OK) { if(rx_data[0] == 0x71) { buf->y = rx_data[2] << 8 | rx_data[1]; } }

	/* touch state set */
	if(buf->x > 0 && buf->y > 0) buf->touched = 1;
	else buf->touched = 0;
}

/* check for area touch */
uint8_t area_check(POS_Typedef* xy, AREA_Typedef* area){
	if(xy->x >= area->x0 && xy->x <= area->x1){
		if(xy->y >= area->y0 && xy->y <= area->y1){
			return 1;
		}
	}
	return 0;
}

/* auto with menu button draw */
void button_draw(BUTTON_Typedef* data){
	/* button draw */
	sprintf(bf, "xstr %d,%d,50,30,0,BLACK,%s,1,1,1,\"%s\"", data->area.x0, data->area.y0, data->color, data->text);
	nextion_inst_set(bf);

	/* button outline draw */
	sprintf(bf, "draw %d,%d,%d,%d,BLACK", data->area.x0, data->area.y0, data->area.x1, data->area.y1);
	nextion_inst_set(bf);
}

/* draw sensor value */
void draw_sensor(void* buf, uint8_t i){
	SENSOR_Typedef* data = (SENSOR_Typedef*)buf;
	if(i == 0){
		/* temperature dis */
		sprintf(bf, "xstr %d,%d,90,30,0,BLACK,%s,1,1,1,\"%d.%d%cC\"", data->area.x0, data->area.y0, data->color, data->value / 10, data->value % 10, 0xb0);
	}
	else if(i == 1){
		/* humidity dis */
		sprintf(bf, "xstr %d,%d,90,30,0,BLACK,%s,1,1,1,\"%02d.%d%%\"", data->area.x0, data->area.y0, data->color, data->value / 10, data->value % 10);
	}
	else{
		/* co2 dis */
		sprintf(bf, "xstr %d,%d,90,30,0,BLACK,%s,1,1,1,\"%d\"", data->area.x0, data->area.y0, data->color, data->value);
	}
	nextion_inst_set(bf);

	/* value outline draw */
	sprintf(bf, "draw %d,%d,%d,%d,BLACK", data->area.x0, data->area.y0, data->area.x1, data->area.y1);
	nextion_inst_set(bf);
}

/* button with sensor draw */
void button_display(uint8_t* rad){
	/* circle color */
	if(*rad > 0) sprintf(bf, "cirs 420,220,%d,GREEN", *rad); // green circle draw ( variable radius  )
	else         sprintf(bf, "cirs 420,220,25,WHITE");       // white circle draw ( fix radius at 25 )
	nextion_inst_set(bf);

	/* circle outline */
	nextion_inst_set("cir 420,220,25,BLACK");

	/* value draw */
	SHT41_t* sht41_vluae   = get_sensor(get_sht41);
	uint16_t* ens160_value = get_sensor(get_ens160);

	sensor[sht41_temp].value = (uint16_t)(sht41_vluae->temperature * 10.0f); // getting sht41 temperature
	sensor[sht41_hum].value  = (uint16_t)(sht41_vluae->humidity    * 10.0f); // getting sht41 humidity
	sensor[ens160_co2].value = *ens160_value;                                // getting ens160 co2

	/* dynamic memory free and NULL reset */
	free_value((void*)&sht41_vluae);
	free_value((void*)&ens160_value);

	/* sensor menu draw */
	for(uint8_t i = 0 ; i < 3 ; i++)
		draw_sensor(&sensor[i], i);

	/* button draw */
	for(uint8_t i = 0 ; i < 2 ; i++)
		button_draw(&button[i]);
}

void window_display(uint16_t* y){
	DAY_Typedef day_state = night;
	uint16_t time_sum = 0;
	uint16_t display_time = 0;

	/* night state */
	if(time.hour < 7) time_sum = (time.hour + 17) * 60 + time.min;

	/* day state */
	else              time_sum = (time.hour - 7) * 60 + time.min;

	/* 24hour = 1440 minute */
	if(time_sum < 720) day_state = day;

	display_time = time_sum % 720;

	/* basic x coordinate = 75 */
	uint16_t sun_x = (display_time * 30) / 100 + 75;
	uint16_t sun_y;
	/* max y coordinate 135 */
	/* time < 360: rising sun, time > 360: falling sun */
	if(display_time < 360) { sun_y = 135 - (display_time * 5 / 100); }
	else                   { sun_y = display_time * 5 / 100 + 100; }

	/* time draw */
	sprintf(bf, "xstr 30,51,100,29,0,BLACK,WHITE,0,1,1,\"%02d:%02d\"", time.hour, time.min);
	nextion_inst_set(bf);

	/* window outside */
	nextion_inst_set("fill 30,80,300,160,BLACK");

	/* background sky draw */
	uint16_t color = day_state == day ? 52831 : 6250;
	sprintf(bf, "fill 40,90,280,140,%d", color);
	nextion_inst_set(bf);

	/* sun draw */
	sprintf(bf, "cirs %d,%d,10,%s", sun_x, sun_y, day_state == day ? "RED" : "YELLOW");
	nextion_inst_set(bf);

	/* sun outline draw */
	sprintf(bf, "cir %d,%d,11,BLACK", sun_x, sun_y);
	nextion_inst_set(bf);

	/* window inside */
	sprintf(bf, "fill 40,90,%d,140,GRAY", *y);
	nextion_inst_set(bf);

	sprintf(bf, "fill %d,90,%d,140,GRAY", 180 + (140 - *y), *y);
	nextion_inst_set(bf);

	/* window inside outline */
	sprintf(bf, "draw %d,90,%d,230,BLACK", 40, *y + 40);
	nextion_inst_set(bf);

	sprintf(bf, "draw %d,90,%d,230,BLACK", 180 + (140 - *y), 320);
	nextion_inst_set(bf);
}

/* auto mode function */
void auto_mode(uint8_t* mode_f){
	/* auto mode start */
	if(*mode_f == 1){
		/* if humidity > 70 -> door open */
		if(sensor[ens160_co2].value > 1000){
			if(door_state == close && door_move == 0) { door_move = 1; auto_buz = 1; }
		}
		/* if co2 > 1000 door door open */
		else if(sensor[sht41_hum].value > 700){
			if(door_state == close && door_move == 0) { door_move = 1; auto_buz = 1; }
		}
		/* if temperature < 300 door control */
		else if(sensor[sht41_temp].value <= 300){
			if(time.hour < 10){
				if(door_state == open && door_move == 0)  { door_move = 1; auto_buz = 1; }
			}
			else if(time.hour < 11){
				if(door_state == close && door_move == 0) { door_move = 1; auto_buz = 1; }
			}
			else if(time.hour < 13){
				if(door_state == open && door_move == 0)  { door_move = 1; auto_buz = 1; }
			}
			else if(time.hour < 14){
				if(door_state == close && door_move == 0) { door_move = 1; auto_buz = 1; }
			}
			else if(time.hour < 16){
				if(door_state == open && door_move == 0)  { door_move = 1; auto_buz = 1; }
			}
			else if(time.hour < 17){
				if(door_state == close && door_move == 0) { door_move = 1; auto_buz = 1; }
			}
			else if(time.hour < 24){
				if(door_state == open && door_move == 0)  { door_move = 1; auto_buz = 1; }
			}
		}
	}
}

/* main task function */
void task_fuc(void){
	nextion_inst_set("cls WHITE");        // clear screen
	nextion_inst_set("cls WHITE");
	nextion_inst_set("cls WHITE");

	uint8_t rad = 0;                      // reset button radius
	uint8_t auto_f = 0;                   // auto mode state
	uint16_t befo_touch = curXY.touched;  // state at before touch

	/* checking tick, using with hal_gettick */
	uint32_t time_tick = 0;
	uint32_t door_tick = 0;
	uint32_t buz_tick = 0;

	AREA_Typedef area = { 420 - 25, 220 - 25, 420 + 25, 220 + 25 }; // reset button area

	while(1){
		/* get touch coordinate */
		get_touch(&curXY);

		/* screen update */
		if(screen_update == 0){
			/* auto button color set */
			button[auto_button].color = auto_f == 0 ? "GREEN" : "64517";

			/* window display */
			window_display(&door_coordinate);

			/* button display */
			button_display(&rad);

			screen_update = 1;
		}

		/* if touched */
		if(curXY.touched == 1){
			/* if touched reset button */
			if(area_check(&curXY, &area)){
				static uint32_t tick;
				if(HAL_GetTick() - tick > 10){
					tick = HAL_GetTick();

					/* circle radius plus */
					if(rad < 25) { rad++; screen_update = 0; }

					/* reset menu ( time: reset, auto mode: off ) */
					else{
						BUZ_hz_set(4000);
						BUZ(1);
						HAL_Delay(100);
						BUZ(0);
						HAL_Delay(100);
						BUZ(1);
						HAL_Delay(100);
						BUZ(0);
						door_state = open;
						door_move = 1;
						time.hour = time.min = 0;
						rad = 0;
						screen_update = 0;
						auto_f = 0;
					}
				}
			}
			/* time button touch */
			else if(area_check(&curXY, &button[time_button].area) && curXY.touched != befo_touch){
				screen_update = 0;
				if(time.hour >= 7 && time.hour <= 18) time.hour = 19;
				else time.hour = 7;
				time.min = 0;
			}
			/* auto button touch */
			else if(area_check(&curXY, &button[auto_button].area) && curXY.touched != befo_touch){
				screen_update = 0;
				auto_f ^= 1;
			}
			/* window open/close move */
			else if(area_check(&curXY, &window_area) && curXY.touched != befo_touch){
				if(door_move == 0 && auto_f == 0) door_move = 1;
			}
		}

		/* auto mode run */
		auto_mode(&auto_f);

		/* time count */
		if(HAL_GetTick() - time_tick >= 1000){
			screen_update = 0;
			time_tick = HAL_GetTick();

			/* minute += 10 */
			if(SW(1)){
				time.min += 10;
				if(time.min > 60){
					time.min -= 60;
					time.hour++;
					if(time.hour > 23) time.hour = 0;
				}
			}
			/* hour += 1 */
			else if(SW(2)){
				time.hour++;
				if(time.hour > 23) time.hour = 0;
			}

			/* sum of basic time */
			time.min++;
			if(time.min > 59){
				time.min = 0;
				time.hour++;
				if(time.hour > 23) time.hour = 0;
			}
		}

		/* door count */
		if(HAL_GetTick() - door_tick >= 20){
			door_tick = HAL_GetTick();

			if(door_move == 1){
				window_display(&door_coordinate);

				/* door: close -> open state */
				if(door_state == close){
					if(door_coordinate > 10) door_coordinate--;
					else { door_move = 0; door_state = open; }
				}
				/* door: open -> close state */
				else if(door_state == open){
					if(door_coordinate < 140) door_coordinate++;
					else { door_move = 0; door_state = close; }
				}
			}
		}

		/* buzzer count */
		if(HAL_GetTick() - buz_tick >= 100){
			buz_tick = HAL_GetTick();
			static uint8_t buz_cnt = 0;

			if(auto_buz == 1){
				BUZ(1);
				buz_cnt++;

				if(buz_cnt < 2)      BUZ_hz_set(1000);
				else if(buz_cnt < 4) BUZ_hz_set(800);
				else if(buz_cnt < 7) BUZ_hz_set(500);
				else { buz_cnt = auto_buz = 0; BUZ(0); }
			}
		}

		/* motor run */
		if(door_move == 1){
			if(door_state == close) setMotor(DRV8830_CW);
			else setMotor(DRV8830_CCW);
		}
		else setMotor(DRV8830_STOP);

		/* touch check */
		if(curXY.touched != befo_touch) { screen_update = 0; rad = 0; }

		/* touch state save */
		befo_touch = curXY.touched;

		/* for test */
		/* sprintf(bf, "xstr 0,0,150,30,0,BLACK,WHITE,0,0,1,\"X: %d Y: %d %d\"", curXY.x, curXY.y, curXY.touched); */
		/* nextion_inst_set(bf); */
	}
}

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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

	/*Configure GPIO pin : FAULTN_Pin */
	GPIO_InitStruct.Pin = FAULTN_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(FAULTN_GPIO_Port, &GPIO_InitStruct);

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
