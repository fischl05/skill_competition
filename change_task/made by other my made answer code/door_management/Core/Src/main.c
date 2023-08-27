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
	out,
	in,
}HUMAN_STATE_Typedef;

typedef enum{
	none,
	enter,
	edit,
}INPUT_STATE_Typedef;

typedef enum{
	edit_button,
	out_button,
	del_button,
	max_button,
}BUTTON_ITEM_Typedef;

typedef struct{
	uint8_t min;
	uint8_t sec;
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
	HUMAN_STATE_Typedef state;
	TIME_Typedef time;
	uint8_t select;
	uint8_t save_state;
}LOG_Data_Typedef;

typedef struct{
	LOG_Data_Typedef data[6];
	AREA_Typedef area[6];
	uint8_t cnt;
}LOG_Typedef;

typedef struct{
	char data;
	AREA_Typedef area;
}KEYPAD_Typedef;

typedef struct{
	char save_password[11];
	char input_password[11];
	uint8_t input_num;
}PASSWORD_Typedef;

typedef struct{
	AREA_Typedef area;
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

/* keypad string */
char* keypad_string = "123456789*0#";

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

uint8_t screen_update = 0;

TIME_Typedef time = { 3, 0 };
KEYPAD_Typedef keypad[12];
INPUT_STATE_Typedef input_state = none;
POS_Typedef curXY = { 0, 0, 0 };
HUMAN_STATE_Typedef human = out;
PASSWORD_Typedef password = { { 0, }, { 0, }, 0 };
LOG_Typedef log_data = { 0, };

/* button coordinate & text */
BUTTON_Typedef button_data[3] = {
		{ { 260, 225, 260 + 30, 225 + 30 }, "edit" },
		{ { 345, 225, 345 + 30, 225 + 30 }, "out" },
		{ { 430, 225, 430 + 30, 225 + 30 }, "del" },
};

/* BUZ frequency set */
void BUZ_tone(uint16_t hz){
	/* TIME2 Auto Reload Register = 32000000 / 32 */
	TIM2->ARR = 1000000 / hz - 1;
	TIM2->CCR1 = TIM2->ARR / 2;
}

/* BUZ on/off set */
void BUZ(uint8_t state){
	if(state) HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
	else HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
}

/* Nextion LCD instruction set */
void nextion_inst_set(char* str){
	uint8_t end_cmd[3] = { 0xFF, 0xFF, 0xFF };
	HAL_UART_Transmit(&huart1, (uint8_t*)str, strlen(str), 100);
	HAL_UART_Transmit(&huart1, end_cmd, 3, 100);
}

/* get touch coordinate */
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

/* human state circle draw */
void circle_draw(POS_Typedef* draw_pos){
	/* color circle draw */
	sprintf(bf, "cirs %d,%d,5,%s", draw_pos->x, draw_pos->y, human == out ? "BLUE" : "RED");
	nextion_inst_set(bf);
	/* gray circle draw */
	sprintf(bf, "cir %d,%d,6,GRAY", draw_pos->x, draw_pos->y);
	nextion_inst_set(bf);
}

/* area touch check */
uint8_t area_check(POS_Typedef* curXY, AREA_Typedef* area){
	if(curXY->x >= area->x0 && curXY->x < area->x1){
		if(curXY->y >= area->y0 && curXY->y <= area->y1){
			return 1;
		}
	}
	return 0;
}

/* keypad coordinate and character initialize */
void keypad_init(void){
	for(uint8_t i = 0 ; i < 4 ; i++){
		for(uint8_t j = 0 ; j < 3 ; j++){
			/* 55, 105, 155 */
			/* 90, 135, 180, 225 */
			keypad[j + i * 3].area.x0 = 55 + (j * 50);
			keypad[j + i * 3].area.y0 = 90 + (i * 45);

			/* area is x0 and y0 + 30 */
			keypad[j + i * 3].area.x1 = keypad[j + i * 3].area.x0 + 30;
			keypad[j + i * 3].area.y1 = keypad[j + i * 3].area.y0 + 30;

			/* character reset */
			keypad[j + i * 3].data = keypad_string[j + i * 3];
		}
	}
}

/* keypad draw */
void keypad_draw(void){
	for(uint8_t i = 0 ; i < 12 ; i++){
		/* keypad text draw */
		sprintf(bf, "xstr %d,%d,30,30,0,GREEN,BLACK,1,1,1,\"%c\"", keypad[i].area.x0, keypad[i].area.y0, keypad[i].data);
		nextion_inst_set(bf);

		/* keypad outline draw */
		sprintf(bf, "draw %d,%d,%d,%d,GREEN", keypad[i].area.x0, keypad[i].area.y0, keypad[i].area.x1, keypad[i].area.y1);
		nextion_inst_set(bf);
	}
}

/* menu button draw */
void button_draw(BUTTON_Typedef* data){
	/* button text draw */
	sprintf(bf, "xstr %d,%d,30,30,0,BLACK,GRAY,1,1,1,\"%s\"", data->area.x0, data->area.y0, data->title);
	nextion_inst_set(bf);

	/* button outline draw */
	sprintf(bf, "draw %d,%d,%d,%d,BLACK", data->area.x0, data->area.y0, data->area.x1, data->area.y1);
	nextion_inst_set(bf);
}

/* main screen display */
void basic_screen(HUMAN_STATE_Typedef state, char* password, char* title){
	/* display clear */
	nextion_inst_set("fill 0,0,240,272,BLACK");
	nextion_inst_set("fill 241,0,240,272,WHITE");

	/* title draw */
	sprintf(bf, "xstr 0,0,240,30,0,GREEN,BLACK,1,1,1,\"%s\"", title);
	nextion_inst_set(bf);

	/* log title */
	nextion_inst_set("xstr 241,0,50,30,0,BLACK,WHITE,1,1,3,\"Log\"");

	/* time display */
	sprintf(bf, "xstr 300,0,175,30,0,BLACK,WHITE,2,1,1,\"%d:%02d\"", time.min, time.sec);
	nextion_inst_set(bf);

	/* password '*' display */
	if(!SW(1)){
		char buf[11] = { 0, };
		for(uint8_t i = 0 ; i < strlen(password) ; i++) buf[i] = '*';
		sprintf(bf, "xstr 30,40,180,30,0,GREEN,BLACK,1,1,1,\"%s\"", buf);
	}
	/* password display */
	else sprintf(bf, "xstr 30,40,180,30,0,GREEN,BLACK,1,1,1,\"%s\"", password);
	nextion_inst_set(bf);
	nextion_inst_set("draw 30,40,30+180,40+30,GREEN");

	/* human state display */
	POS_Typedef draw_coordinate = { 240+50, 15, 0 };
	circle_draw(&draw_coordinate);

	/* button draw */
	for(BUTTON_ITEM_Typedef i = edit_button ; i < max_button ; i++)
		button_draw(&button_data[i]);

	/* display cutting line draw */
	nextion_inst_set("line 0,30,240,30,GREEN");
	nextion_inst_set("line 241,30,480,30,BLACK");
	nextion_inst_set("line 241,210,480,210,BLACK");

	/* keypad draw */
	keypad_draw();
}

/* log data save & sorting */
void log_save(TIME_Typedef* time, HUMAN_STATE_Typedef* state){
	/* log data number (MAX:6) */
	if(log_data.cnt < 6) log_data.cnt++;

	/* log data shift */
	for(uint8_t i = 5 ; i > 0 ; i--){
		log_data.data[i] = log_data.data[i - 1];
	}

	/* data save */
	log_data.data[0].state = *state;
	log_data.data[0].time  = *time;
	log_data.data[0].save_state = 1;
}

/* log data print */
void log_print(LOG_Typedef* data){
	/* loop as number of log data */
	for(uint8_t i = 0 ; i < data->cnt ; i++){
		/* circle draw */
		sprintf(bf, "cirs %d,%d,5,%s", 240+15, 45 + i * 30, data->data[i].state == in ? "RED" : "BLUE");
		nextion_inst_set(bf);
		sprintf(bf, "cir %d,%d,6,GRAY", 240+15, 45 + i * 30);
		nextion_inst_set(bf);

		/* time display */
		sprintf(bf, "xstr 240+30,30+%d,100,30,0,BLACK,WHITE,0,1,1,\"%d:%02d %s\"", i * 30, data->data[i].time.min, data->data[i].time.sec, data->data[i].state == in ? "in" : "out");
		nextion_inst_set(bf);

		/* check line draw */
		sprintf(bf, "line 460,%d,465,%d,%s", 40 + i * 30, 50 + i * 30, data->data[i].select == 1 ? "RED" : "WHITE");
		nextion_inst_set(bf);
		sprintf(bf, "line 465,%d,470,%d,%s", 50 + i * 30, 40 + i * 30, data->data[i].select == 1 ? "RED" : "WHITE");
		nextion_inst_set(bf);

		/* check box draw */
		sprintf(bf, "draw %d,%d,%d,%d,BLACK", data->area[i].x0, data->area[i].y0, data->area[i].x1, data->area[i].y1);
		nextion_inst_set(bf);

		/* menu outline draw */
		sprintf(bf, "draw 240,30+%d,480,60+%d,BLACK", i * 30, i * 30);
		nextion_inst_set(bf);
	}
}

/* log data sort */
void log_sort(LOG_Typedef* data){
	for(uint8_t i = 0 ; i < data->cnt ; /*i++*/){
		if(data->data[i].select == 1){
			/* memory move & last data clear */
			memmove((LOG_Data_Typedef*)&data->data[i], (LOG_Data_Typedef*)&data->data[i + 1], (data->cnt - i) * sizeof(LOG_Data_Typedef));
			memset((LOG_Data_Typedef*)&data->data[data->cnt - 1], 0, sizeof(LOG_Data_Typedef));
			data->cnt--;
		}
		else i++;
	}
}

/* task main function */
void task_fuc(void){
	uint8_t sw1_state = 0;
	uint8_t befo_sw1 = 0;
	uint8_t befo_touch = 0;

	while(1){
		get_touch(&curXY);
		sw1_state = SW(1);

		if(screen_update == 0) {
			char* title = "";
			/* title text set */
			if(input_state == enter)     title = "Enter Password";
			else if(input_state == edit) title = "Edit Password";

			/* password text set */
			if(input_state == none) basic_screen(human, "", title);
			else if(human == out)   basic_screen(human, password.input_password, title);
			else                    basic_screen(human, password.save_password, title);

			/* log data draw */
			log_print(&log_data);

			screen_update = 1;
		}

		/* touch function */
		if(befo_touch != curXY.touched && curXY.touched == 1){
			for(uint8_t i = 0 ; i < 12 ; i++){
				/* keypad select */
				if(input_state == none) break;
				if(area_check(&curXY, &keypad[i].area)){
					if(i == 11){
						if(input_state == enter) memset(password.input_password, 0, 11);
						else if(input_state == edit) memset(password.save_password, 0, 11);
						password.input_num = 0;
					}
					else if(i == 9){
						/* human state: out */
						if(human == out){
							uint8_t input_len = strlen(password.input_password);
							uint8_t save_len = strlen(password.save_password);
							uint8_t i = 0;

							/* input number > save number */
							if(input_len >= save_len){
								for(i = 0; i < save_len ; i++){
									if(password.input_password[input_len - save_len + i] != password.save_password[i]) break;
								}
							}

							/* input password == save password */
							if(i == save_len){
								human = in;
								password.input_num = 0;
								input_state = none;
								BUZ(1);
								BUZ_tone(1000);
								HAL_Delay(300);
								BUZ_tone(1100);
								HAL_Delay(300);
								BUZ_tone(1200);
								HAL_Delay(500);
								BUZ(0);
								setMotor(DRV8830_CW);
								HAL_Delay(5000);
								setMotor(DRV8830_STOP);
								log_save(&time, &human);
							}
							/* input number != save number */
							else{
								memset(password.input_password, 0, 11);
								password.input_num = 0;
								BUZ_tone(4000);
								BUZ(1);
								HAL_Delay(100);
								BUZ(0);
								HAL_Delay(100);
								BUZ(1);
								HAL_Delay(100);
								BUZ(0);
							}
						}
						/* human state: in */
						else{
							if(password.input_num < 4){
								screen_update = 0;

								memset(password.save_password, 0, 11);
								password.input_num = 0;

								BUZ_tone(4000);
								BUZ(1);
								HAL_Delay(100);
								BUZ(0);
								HAL_Delay(100);
								BUZ(1);
								HAL_Delay(100);
								BUZ(0);
							}
							else{
								screen_update = 0;
								password.input_num = 0;
								input_state = none;

								BUZ_tone(4000);
								BUZ(1);
								HAL_Delay(100);
								BUZ(0);
							}
						}
					}
					else{
						/* password input */
						if(password.input_num < 10){
							if(human == out && input_state == enter) password.input_password[password.input_num++] = keypad[i].data;
							else password.save_password[password.input_num++] = keypad[i].data;
						}
					}
					/* if touch pad touched loop stop */
					screen_update = 0;
					break;
				}
			}

			/* edit button */
			if(area_check(&curXY, &button_data[edit_button].area)){
				if(human == in && input_state == none){
					input_state = edit;
					memset(password.save_password, 0, 11);
					screen_update = 0;
				}
			}
			/* out button */
			else if(area_check(&curXY, &button_data[out_button].area)){
				if(human == in){
					screen_update = 0;
					human = out;
					input_state = none;
					memset(password.input_password, 0, 11);
					log_save(&time, &human);
				}
			}
			/* delete button */
			else if(area_check(&curXY, &button_data[del_button].area)){
				screen_update = 0;
				log_sort(&log_data);
			}

			/* log data check box control */
			if(log_data.cnt > 0){
				for(uint8_t i = 0 ; i < log_data.cnt ; i++){
					if(area_check(&curXY, &log_data.area[i])){
						screen_update = 0;
						log_data.data[i].select ^= 1;
						break;
					}
				}
			}
		}

		/* password input mode */
		if(human == out && input_state == none){
			if(PIR_LEFT || PIR_RIGHT) {
				input_state = enter;
				screen_update = 0;
			}
		}

		/* rising | falling button check */
		if(sw1_state != befo_sw1){
			screen_update = 0;
		}

		befo_touch = curXY.touched;
		befo_sw1 = sw1_state;

		/* coordinate display for task check */
		/* sprintf(bf, "xstr 0,0,150,30,0,GREEN,BLACK,0,1,1,\"X: %d Y: %d\"", curXY.x, curXY.y); */
		/* nextion_inst_set(bf); */
	}
}

/* time count */
void HAL_SYSTICK_Callback(void){
	static uint16_t cnt = 0;

	cnt++;
	if(cnt >= 1000){
		screen_update = 0;

		cnt = 0;
		time.sec++;
		if(time.sec > 59){
			time.sec = 0;
			time.min++;
			if(time.min > 59) time.min = 0;
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

	nextion_inst_set("baud=921600");
	nextion_inst_set("baud=921600");
	nextion_inst_set("baud=921600");
	HAL_Delay(50);
	USART1->CR1 &= (~USART_CR1_UE);
	USART1->BRR = 0x23;
	USART1->CR1 |= USART_CR1_UE;
	HAL_Delay(1000);

	keypad_init();
	for(uint8_t i = 0 ; i < 4 ; i++)
		password.save_password[i] = '0';

	for(uint8_t i = 0 ; i < 6 ; i++){
		log_data.area[i].x0 = 460;
		log_data.area[i].y0 = 40 + i * 30;

		log_data.area[i].x1 = log_data.area[i].x0 + 10;
		log_data.area[i].y1 = log_data.area[i].y0 + 10;
	}

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

