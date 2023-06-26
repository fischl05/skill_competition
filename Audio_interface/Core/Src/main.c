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
#include "ILI9341_GFX.h"
#include "ILI9341_STM32_Driver.h"
#include "fonts.h"
#include "FT6206.h"
#include "DS3231.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef struct{
	uint8_t year, month, day;
	uint8_t hour, min, sec;
}TIME;

typedef struct{
	uint16_t x, y;
}POS;

typedef enum{
	black, blue, green, red, yellow, white,
}COLOR_;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define RECT 50
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc;
DMA_HandleTypeDef hdma_adc;

DAC_HandleTypeDef hdac;

I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

/* USER CODE BEGIN PV */
const uint8_t lastDay[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
const uint16_t lcd_color[6] = { BLACK, BLUE, GREEN, RED, YELLOW, WHITE };

COLOR_ txt_color[17] = { black }, back_color[17] = { white };
COLOR_ set_tcolor = black, set_bcolor = white;
TS_POINT curXY;
TIME time = {23, 6, 22, 0, 0, 0};

uint8_t modeF, firF, buzM;
uint16_t buzC;
uint32_t adcV;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC_Init(void);
static void MX_DAC_Init(void);
static void MX_DMA_Init(void);
static void MX_SPI1_Init(void);
static void MX_I2C1_Init(void);
static void MX_NVIC_Init(void);
/* USER CODE BEGIN PFP */

__STATIC_INLINE void BUZ(uint8_t state){
	HAL_GPIO_WritePin(BUZ_GPIO_Port, BUZ_Pin, state);
}

__STATIC_INLINE void time_set(void){
	DS3231_set_date(time.day, time.month, time.year);
	DS3231_set_time(time.sec, time.min, time.hour);
}

__STATIC_INLINE void time_get(void){
	DS3231_get_date(&time.day, &time.month, &time.year);
	DS3231_get_time(&time.sec, &time.min, &time.hour);
}

__STATIC_INLINE uint8_t coor_check(uint16_t start_x, uint16_t end_x, uint16_t start_y, uint16_t end_y){
	return ((curXY.x >= start_x * 15 && curXY.x <= end_x * 15) && (curXY.y >= start_y * 19 && curXY.y <= end_y * 19));
}

__STATIC_INLINE void LCD_clear(COLOR_ color){
	ILI9341_FillScreen(lcd_color[color]);
}

__STATIC_INLINE void reset_value(void){
	modeF = firF = buzM = 0;
	LCD_clear(set_bcolor);
}

void LCD_putsXY(uint16_t x, uint16_t y, char* str, COLOR_ color, COLOR_ bg_color){
	ILI9341_DrawText(str, FONT, x * 15, y * 19, lcd_color[color], lcd_color[bg_color]);
}

void array_puts(POS* pos, char* title, char** arr, COLOR_* color, COLOR_* bg_color, uint8_t num){
	LCD_putsXY(0, 0, title, set_tcolor, set_bcolor);
	for(uint8_t i = 0 ; i < num ; i++)
		LCD_putsXY(pos[i].x, pos[i].y, arr[i], color[i], bg_color[i]);
}

void time_setting(void){
	uint8_t sel = 0;
	TIME set_time = { 23, 6, 22, 0, 0, 0 };

	POS pos[9] = {{5, 4}, {10, 4}, {14, 4}, {6, 5}, {10, 5}, {14, 5}, {7, 8}, {12, 8}, {17, 10}};
	char bf[6][20];

	LCD_clear(set_bcolor);
	while(1){
		if(!firF){
			firF = 1;
			LCD_putsXY(6, 1, "< Time Setting >", set_tcolor, set_bcolor);

			for(uint8_t i = 0 ; i < 9 ; i++){
				if(sel == i){
					txt_color[i] = set_bcolor;
					back_color[i] = set_tcolor;
				}
				else{
					txt_color[i] = set_tcolor;
					back_color[i] = set_bcolor;
				}
			}

			sprintf(bf[0], "Y:%04d", 2000 + set_time.year);
			sprintf(bf[1], "M:%02d", set_time.month);
			if(set_time.day > lastDay[set_time.month - 1]) set_time.day = lastDay[set_time.month - 1];
			sprintf(bf[2], "D:%02d", set_time.day);
			sprintf(bf[3], "H:%02d", set_time.hour);
			sprintf(bf[4], "m:%02d", set_time.min);
			sprintf(bf[5], "S:%02d", set_time.sec);

			char* array[9] = { bf[0], bf[1], bf[2], bf[3], bf[4], bf[5], "UP", "DOWN", "OK!" };
			array_puts(pos, "", array, txt_color, back_color, 9);
			LCD_putsXY(6, 1, "< Time Setting >", set_tcolor, set_bcolor);
		}

		if(touched()) firF = 0;

		if(curXY.x > 0 || curXY.y > 0){
			if(coor_check(17, 17 + strlen("OK!"), 10, 11)) break;
			else if(coor_check(5, 5 + strlen(bf[0]), 4, 5)) sel = 0;
			else if(coor_check(10, 10 + strlen(bf[1]), 4, 5)) sel = 1;
			else if(coor_check(14, 14 + strlen(bf[2]), 4, 5)) sel = 2;
			else if(coor_check(6, 7 + strlen(bf[3]), 5, 6)) sel = 3;
			else if(coor_check(10, 10 + strlen(bf[4]), 5, 6)) sel = 4;
			else if(coor_check(14, 14 + strlen(bf[5]), 5, 6)) sel = 5;
			else{
				if(coor_check(7, 7 + strlen("UP"), 7, 9)){
					if(sel == 0 && set_time.year < 99) set_time.year++;
					else if(sel == 1 && set_time.month < 12) set_time.month++;
					else if(sel == 2 && set_time.day < lastDay[set_time.month - 1]) set_time.day++;
					else if(sel == 3 && set_time.hour < 23) set_time.hour++;
					else if(sel == 4 && set_time.min < 59) set_time.min++;
					else if(sel == 5 && set_time.sec < 59) set_time.sec++;
				}
				else if(coor_check(12, 12 + strlen("DOWN"), 7, 9)){
					if(sel == 0 && set_time.year > 0) set_time.year--;
					else if(sel == 1 && set_time.month > 1) set_time.month--;
					else if(sel == 2 && set_time.day > 1) set_time.day--;
					else if(sel == 3 && set_time.hour > 0) set_time.hour--;
					else if(sel == 4 && set_time.min > 0) set_time.min--;
					else if(sel == 5 && set_time.sec > 0) set_time.sec--;
				}
			}
		}
	}
	buzM = 1;
	time = set_time;
	firF = 0;
	time_set();
	LCD_clear(set_bcolor);
}

void start(void){
	LCD_clear(set_bcolor);

	LCD_putsXY(3, 4, "< Skill Competition Task 3 >", set_tcolor, set_bcolor);
	LCD_putsXY(7, 6, "Audio Interface", set_tcolor, set_bcolor);
	HAL_Delay(2000);
	time_setting();
}

void main_menu(void){
	if(!firF){
		firF = 1;

		for(uint8_t i = 0 ; i < 17 ; i++){
			txt_color[i] = set_tcolor;
			back_color[i] = set_bcolor;
		}
	}
	time_get();
	char bf[20];
	POS pos[4] = {{0, 1}, {2, 4}, {2, 7}};
	sprintf(bf, "%04d.%02d.%02d %02d:%02d:%02d", 2000 + time.year, time.month, time.day, time.hour, time.min, time.sec);

	char* array[3] = { bf, "1. Sound modulation", "2. Color Setting" };
	array_puts(pos, ">Main", array, txt_color, back_color, 3);

	if(curXY.x > 0 || curXY.y > 0){
		if(coor_check(0, strlen(bf), 1, 1 + 1)) time_setting();
		else if(coor_check(2, 2 + strlen("1. Sound modulation"), 4, 4 + 1)){
			reset_value();
			modeF = 1;
		}
		else if(coor_check(2, 2 + strlen("2. Color Setting"), 7, 7 + 1)){
			reset_value();
			modeF = 2;
		}
	}
}

void modulation_mode(void){

}

void setting_mode(void){
	static uint8_t sel;
	static COLOR_ t_color = white, b_color = black;

	if(!firF){
		firF = 1;

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

	sprintf(bf[1], "%s   ", t_color == black ? "Black" : t_color == blue ? "Blue" : t_color == green ? "Green" : t_color == red ? "Red" : t_color == yellow ? "Yellow" : "White");
	sprintf(bf[2], "%s   ", b_color == black ? "Black" : b_color == blue ? "Blue" : b_color == green ? "Green" : b_color == red ? "Red" : b_color == yellow ? "Yellow" : "White");

	char* array[6] = { bf[0], "Text Color:", "Background Color:", bf[1], bf[2], "Back" };
	array_puts(pos, ">Setting", array, txt_color, back_color, 6);

	for(uint8_t i = 0 ; i < 6 ; i++)
		ILI9341_DrawFilledRectangleCoord(RECT * i, 190, RECT * (i + 1), 240, lcd_color[i]);

	if(curXY.x > 0 || curXY.y > 0){
		if(coor_check(15, 15 + strlen("Back"), 1, 1 + 1)){
			set_tcolor = t_color;
			set_bcolor = b_color;
			reset_value();
			buzM = 1;
		}
		else if(coor_check(0, strlen(bf[0]), 1, 1 + 1)) { firF = 0; time_setting(); }
		else if(coor_check(2, 2 + strlen("Text Color: Yellow"), 4, 4 + 1)) sel = 0;
		else if(coor_check(2, 2 + strlen("Background Color: Yellow"), 6, 6 + 1)) sel = 1;
		else if(curXY.y > 190){
			if(curXY.x < RECT) { if(sel == 0) t_color = black; else b_color = black; }
			else if(curXY.x < RECT * 2) { if(sel == 0) t_color = blue; else b_color = blue; }
			else if(curXY.x < RECT * 3) { if(sel == 0) t_color = green; else b_color = green; }
			else if(curXY.x < RECT * 4) { if(sel == 0) t_color = red; else b_color = red; }
			else if(curXY.x < RECT * 5) { if(sel == 0) t_color = yellow; else b_color = yellow; }
			else if(curXY.x < RECT * 6) { if(sel == 0) t_color = white; else b_color = white; }
		}
	}
}

void (*main_fuc[3])(void) = { main_menu, modulation_mode, setting_mode };

void HAL_SYSTICK_Callback(void){
	if(buzM) buzC++;

	if(buzM == 1){
		if(buzC < 500) BUZ(1);
		else{
			buzM = buzC = 0;
			BUZ(0);
		}
	}

	if(touched()) curXY = getPoint(0);
	else curXY = getPoint(1);
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
	MX_ADC_Init();
	MX_DAC_Init();
	MX_DMA_Init();
	MX_SPI1_Init();
	MX_I2C1_Init();

	/* Initialize interrupts */
	MX_NVIC_Init();
	/* USER CODE BEGIN 2 */
	HAL_DAC_Start(&hdac, DAC1_CHANNEL_1);
	HAL_ADC_Start_DMA(&hadc, &adcV, 1);
	ILI9341_Init();
	INIT_FT6206();
	FT6206_Begin(255);
	LCD_clear(set_bcolor);

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
	PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
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
	hadc.Init.ContinuousConvMode = ENABLE;
	hadc.Init.DiscontinuousConvMode = DISABLE;
	hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
	hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
	hadc.Init.DMAContinuousRequests = ENABLE;
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
 * @brief SPI1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI1_Init(void)
{

	/* USER CODE BEGIN SPI1_Init 0 */

	/* USER CODE END SPI1_Init 0 */

	/* USER CODE BEGIN SPI1_Init 1 */

	/* USER CODE END SPI1_Init 1 */
	/* SPI1 parameter configuration*/
	hspi1.Instance = SPI1;
	hspi1.Init.Mode = SPI_MODE_MASTER;
	hspi1.Init.Direction = SPI_DIRECTION_2LINES;
	hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi1.Init.NSS = SPI_NSS_SOFT;
	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
	hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi1.Init.CRCPolynomial = 7;
	if (HAL_SPI_Init(&hspi1) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN SPI1_Init 2 */

	/* USER CODE END SPI1_Init 2 */

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
	HAL_GPIO_WritePin(GPIOB, LCD_CS_Pin|LCD_DC_Pin|BUZ_Pin|LCD_RST_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pins : LCD_CS_Pin LCD_DC_Pin BUZ_Pin LCD_RST_Pin */
	GPIO_InitStruct.Pin = LCD_CS_Pin|LCD_DC_Pin|BUZ_Pin|LCD_RST_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
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
