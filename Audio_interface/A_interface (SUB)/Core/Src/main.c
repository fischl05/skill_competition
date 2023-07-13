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
#include "fonts.h"
#include "ILI9341_GFX.h"
#include "ILI9341_STM32_Driver.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum{
	black, blue, green, red, yellow, white,
}LCD_COLOR;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
 SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
uint8_t RxData;
uint8_t Txdata = 1;
uint8_t uartDat[10];
uint16_t COLOR[6] = { BLACK, BLUE, GREEN, RED, YELLOW, WHITE };
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_NVIC_Init(void);
/* USER CODE BEGIN PFP */

__STATIC_INLINE void reset_check(void){
	if(__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST)){
		__HAL_RCC_CLEAR_RESET_FLAGS();
		HAL_DeInit();
		HAL_NVIC_SystemReset();
	}
}

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	if(huart->Instance == huart1.Instance){
		if(RxData == 0) return;
		HAL_UART_Transmit(&huart1, &Txdata, 1, 10);
		if(RxData == 1){
			HAL_UART_Receive(&huart1, uartDat, 1, 10);
			ILI9341_DrawColor(COLOR[uartDat[0]]);
		}
		if(RxData == 2){
			uint32_t size = 0;
			HAL_UART_Receive(&huart1, uartDat, 5, 10);
			for(uint8_t i = 0 ; i < 4 ; i++){
				size <<= 8;
				size |= uartDat[4 - i];
			}
			ILI9341_DrawColorBurst(COLOR[uartDat[0]], size);
		}
		if(RxData == 3){
			HAL_UART_Receive(&huart1, uartDat, 1, 10);
			ILI9341_FillScreen(COLOR[uartDat[0]]);
		}
		if(RxData == 4){
			uint16_t x = 0, y = 0;
			HAL_UART_Receive(&huart1, uartDat, 5, 10);
			x = uartDat[1];
			x <<= 8;
			x |= uartDat[0];
			y = uartDat[3];
			y <<= 8;
			y |= uartDat[2];
			ILI9341_DrawPixel(x, y, COLOR[uartDat[4]]);
		}
		if(RxData == 5){
			uint16_t x = 0, y = 0, width = 0, height = 0;
			HAL_UART_Receive(&huart1, uartDat, 9, 10);
			x = uartDat[1];
			x <<= 8;
			x |= uartDat[0];
			y = uartDat[3];
			y <<= 8;
			y |= uartDat[2];
			width = uartDat[5];
			width <<= 8;
			width |= uartDat[4];
			height = uartDat[7];
			height <<= 8;
			height |= uartDat[6];
			ILI9341_DrawRectangle(x, y, width, height, COLOR[uartDat[8]]);
		}
		if(RxData == 6){
			uint16_t x = 0, y = 0, width = 0;
			HAL_UART_Receive(&huart1, uartDat, 7, 10);
			x = uartDat[1];
			x <<= 8;
			x |= uartDat[0];
			y = uartDat[3];
			y <<= 8;
			y |= uartDat[2];
			width = uartDat[5];
			width <<= 8;
			width |= uartDat[4];
			ILI9341_DrawHLine(x, y, width, COLOR[uartDat[6]]);
		}
		if(RxData == 7){
			uint16_t x = 0, y = 0, height = 0;
			HAL_UART_Receive(&huart1, uartDat, 7, 10);
			x = uartDat[1];
			x <<= 8;
			x |= uartDat[0];
			y = uartDat[3];
			y <<= 8;
			y |= uartDat[2];
			height = uartDat[5];
			height <<= 8;
			height |= uartDat[4];
			ILI9341_DrawVLine(x, y, height, COLOR[uartDat[6]]);
		}
		if(RxData == 8){
			uint16_t x = 0, y = 0, radius = 0;
			HAL_UART_Receive(&huart1, uartDat, 7, 10);
			x = uartDat[1];
			x <<= 8;
			x |= uartDat[0];
			y = uartDat[3];
			y <<= 8;
			y |= uartDat[2];
			radius = uartDat[5];
			radius <<= 8;
			radius |= uartDat[4];
			ILI9341_DrawHollowCircle(x, y, radius, COLOR[uartDat[6]]);
		}
		if(RxData == 9){
			uint16_t x = 0, y = 0, radius = 0;
			HAL_UART_Receive(&huart1, uartDat, 7, 10);
			x = uartDat[1];
			x <<= 8;
			x |= uartDat[0];
			y = uartDat[3];
			y <<= 8;
			y |= uartDat[2];
			radius = uartDat[5];
			radius <<= 8;
			radius |= uartDat[4];
			ILI9341_DrawFilledCircle(x, y, radius, COLOR[uartDat[6]]);
		}
		if(RxData == 10){
			uint16_t x0, y0, x1, y1;
			HAL_UART_Receive(&huart1, uartDat, 9, 10);
			x0 = uartDat[1];
			x0 <<= 8;
			x0 |= uartDat[0];
			y0 = uartDat[3];
			y0 <<= 8;
			y0 |= uartDat[2];
			x1 = uartDat[5];
			x1 <<= 8;
			x1 |= uartDat[4];
			y1 = uartDat[7];
			y1 <<= 8;
			y1 |= uartDat[6];
			ILI9341_DrawHollowRectangleCoord(x0, y0, x1, y1, COLOR[uartDat[8]]);
		}
		if(RxData == 11){
			uint16_t x0, y0, x1, y1;
			HAL_UART_Receive(&huart1, uartDat, 9, 10);
			x0 = uartDat[1];
			x0 <<= 8;
			x0 |= uartDat[0];
			y0 = uartDat[3];
			y0 <<= 8;
			y0 |= uartDat[2];
			x1 = uartDat[5];
			x1 <<= 8;
			x1 |= uartDat[4];
			y1 = uartDat[7];
			y1 <<= 8;
			y1 |= uartDat[6];
			ILI9341_DrawFilledRectangleCoord(x0, y0, x1, y1, COLOR[uartDat[8]]);
		}
		if(RxData == 12){
			char ch;
			uint16_t x = 0, y = 0;
			HAL_UART_Receive(&huart1, uartDat, 7, 10);
			ch = (char)uartDat[0];
			x = uartDat[2];
			x <<= 8;
			x |= uartDat[1];
			y = uartDat[4];
			y <<= 8;
			y |= uartDat[3];
			ILI9341_DrawChar(ch, FONT, x, y, COLOR[uartDat[5]], COLOR[uartDat[6]]);
		}
		if(RxData == 13){
			char str[40] = { 0 };
			uint8_t cnt = 0;
			uint16_t x = 0, y = 0;

			while(1){
				char data;
				HAL_UART_Receive(&huart1, (uint8_t*)&data, 1, 10);

				if(data == '\0'){
					str[cnt] = '\0';
					break;
				}
				else str[cnt++] = data;
			}
			HAL_UART_Receive(&huart1, uartDat, 6, 10);
			x = uartDat[1];
			x <<= 8;
			x |= uartDat[0];
			y = uartDat[3];
			y <<= 8;
			y |= uartDat[2];
			ILI9341_DrawText(str, FONT, x, y, COLOR[uartDat[4]], COLOR[uartDat[5]]);
		}
		if(RxData == 14){
			ILI9341_Init();
		}
//		HAL_UART_Receive_IT(&huart1, &RxData, 1);
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
  MX_SPI1_Init();
  MX_USART1_UART_Init();

  /* Initialize interrupts */
  MX_NVIC_Init();
  /* USER CODE BEGIN 2 */
  ILI9341_Init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1)
	{
//		reset_check();
		HAL_UART_Receive_IT(&huart1, &RxData, 1);
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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
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
  /* USART1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
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
  HAL_GPIO_WritePin(GPIOB, SPI_DC_Pin|LCD_CS_Pin|LCD_RST_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : SPI_DC_Pin LCD_CS_Pin LCD_RST_Pin */
  GPIO_InitStruct.Pin = SPI_DC_Pin|LCD_CS_Pin|LCD_RST_Pin;
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
