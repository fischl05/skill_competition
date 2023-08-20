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
 * Made by 2023.8.20 KMS
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

enum { led1, led2, led3, LED_MAX };

typedef enum{
	temp,
	humidity,
	co2,
	brightness,
	motor,
	buzzer,
	MAX_ITEM
}ITEM_Typedef;

typedef enum{
	temp_addr       =  0,
	humidity_addr   =  2,
	co2_addr        =  4,
	brightness_addr =  6,
	motor_addr      =  8,
	buzzer_addr     = 10,
	MAX_ADDR        =  6,
}ADDR_Typedef;

typedef enum{
	off,
	on,
}BUTTON_STATE_Typedef;

typedef struct{
	int16_t x;         // x coordinate
	int16_t y;         // y coordinate
	uint8_t touched;   // current touch state
}POS_Typedef;

typedef struct{
	int16_t x0, y0;    // left, up coordinate
	int16_t x1, y1;    // right, down coordinate
}AREA_Typedef;

typedef struct{
	AREA_Typedef area; // menu touch area
	uint16_t color;    // menu background color
	uint16_t value;    // for menu display value
	char* title;       // menu title
}MENU_Typedef;

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
char* motor_string = "0.5V >";

uint8_t screen_update = 0;
uint8_t buzM = 0, motorM = 0;
uint16_t buz_cnt = 0, buz_on_time = 0, motor_cnt = 0;

uint8_t motor_voltage[10]       = { 0x06, 0x0D, 0x13, 0x19, 0x1F, 0x25, 0x2C, 0x32, 0x38, 0x3F };
uint16_t origin_y_coordinate[6] = { 45, 125, 205, 285, 365, 445 };

POS_Typedef curXY = { 0, 0, 0 };

AREA_Typedef bar_area = { 453, 50, 478, 80 };
AREA_Typedef button_area = { 370, 16, 370+55, 20 };

/* menu`s data array */
MENU_Typedef menu_data[MAX_ITEM] = {
		{ { 3, 45,  3 + 474, 45  + 60 }, 31<<11,                  0, "TEMP"     }, /*  TEMP menu data        */
		{ { 3, 125, 3 + 474, 125 + 60 }, 31,                      0, "HUMIDITY" }, /*  HUMIDITY menu data    */
		{ { 3, 205, 3 + 474, 205 + 60 }, 16<<11 | 32 << 5 | 16,   0, "CO2"      }, /*  CO2 menu data         */
		{ { 3, 285, 3 + 474, 285 + 60 }, 30<<11 | 50 << 5,      100, "BRIGHT"   }, /*  BRIGHTNESS menu data  */
		{ { 3, 365, 3 + 474, 365 + 60 }, 20<<11 | 20,             0, "MOTOR"    }, /*  MOTOR menu data       */
		{ { 3, 445, 3 + 474, 445 + 60 }, 45<<5,                   0, "BUZZER"   }, /*  BUZZER menu data      */
};

BUTTON_STATE_Typedef button_state = off;

SHT41_t* sht41_value = NULL;
uint16_t* ens160_value = NULL;

ADDR_Typedef addr_array[MAX_ADDR] = { temp_addr, humidity_addr, co2_addr, brightness_addr, motor_addr, buzzer_addr };
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

/* get value at sensor and that value return */
void* get_sensor(ITEM_Typedef* want_data){
	if(*want_data == temp || *want_data == humidity){
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

/* If using get_sensor() should be use this function after using values */
void free_reset(void** reset_data){
	free(*reset_data);
	(*reset_data) = NULL;
}

/* buzzer on or off state setting */
void BUZ(uint8_t state){
	if(state == 1) HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
	else HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
}

/* buzzer frequency set                    */
/* adjusting timer2`s auto reload register */
void BUZ_hz_set(uint16_t* hz){
	uint16_t hz_dat = (*hz * 100) / 10 + 10;
	TIM2->ARR  = 1000000 / hz_dat - 1;
	TIM2->CCR1 = TIM2->ARR / 2;
}

/* save at eeprom with uint16_t          */
/* uint16_t divide to two uint8_t format */
void eeprom_save(ADDR_Typedef address, uint16_t* data){
	HAL_FLASHEx_DATAEEPROM_Unlock();
	*(__IO uint8_t*)(DATA_EEPROM_BASE + address) = (uint8_t)(*data);
	*(__IO uint8_t*)(DATA_EEPROM_BASE + address + 1) = (uint8_t)(*data >> 8);
	HAL_FLASHEx_DATAEEPROM_Lock();
}

/* load uint16_t at eeprom           */
/* divided uint16_t merge and return */
uint16_t eeprom_read(ADDR_Typedef address){
	uint16_t return_dat = 0;
	return_dat |= *(__IO uint8_t*)(DATA_EEPROM_BASE + address);
	return_dat |= *(__IO uint8_t*)(DATA_EEPROM_BASE + address + 1) << 8;
	return return_dat;
}

/* Nextion instruction set */
void nextion_inst_set(char* str){
	uint8_t end_cmd[3] = { 0xFF, 0xFF, 0xFF };
	HAL_UART_Transmit(&huart1, (uint8_t*)str, strlen(str), 10);
	HAL_UART_Transmit(&huart1, end_cmd, 3, 10);
}

/* request Nextion LCD current touch coordinate transmit & receive */
void get_touch(POS_Typedef* buf){
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

/* current coordinate & menu area equality check */
uint8_t area_check(POS_Typedef* pos, AREA_Typedef* area){
	if(pos->x >= area->x0 && pos->x <= area->x1){
		if(pos->y >= area->y0 && pos->y <= area->y1){
			return 1;
		}
	}
	return 0;
}

/* menu drag bar coordinate set & touch area set */
void bar_coordinate_set(int16_t* bar_y){
	if(curXY.y >= 55 && curXY.y <= 255){
		*bar_y = curXY.y;
		bar_area.y0 = *bar_y - 15;
		bar_area.y1 = *bar_y + 15;
	}
}

/* Adjust coordinate for displaying menu */
void menu_coordinate_set(AREA_Typedef* area, uint16_t *first_y){
	if(curXY.y >= 55 && curXY.y <= 255){
		area->y0 = *first_y - (uint16_t)((float)(curXY.y - 55) * 1.2);
		area->y1 = area->y0 + 60;
	}
}

/* Motor drive and set motor drive voltage */
void motor_set(uint8_t dir, uint8_t* voltage){
	uint8_t tx_dat = *voltage << 2 | dir;
	HAL_I2C_Mem_Write(&hi2c1, DRV8830_DeviceAddress, DRV8830_CONTROL, 1, &tx_dat, 1, 100);
}

/* this function parameter is character double pointer */
/* (*str) <- reference to character pointer`s value    */
void motor_string_set(char** str, uint16_t* value){
	switch(*value){
	case  0: (*str) =   "0.5V >"; break;
	case  1: (*str) = "< 1.0V >"; break;
	case  2: (*str) = "< 1.5V >"; break;
	case  3: (*str) = "< 2.0V >"; break;
	case  4: (*str) = "< 2.5V >"; break;
	case  5: (*str) = "< 3.0V >"; break;
	case  6: (*str) = "< 3.5V >"; break;
	case  7: (*str) = "< 4.0V >"; break;
	case  8: (*str) = "< 4.5V >"; break;
	case  9: (*str) = "< 5.0V";   break;
	default:                      break; // run when input data is error data
	}
}

/* draw button using the button state */
void button_draw(BUTTON_STATE_Typedef* power_state){
	/* off button draw */
	if(*power_state == off){
		nextion_inst_set("cirs 370,17,10,33823");   // 33823 <- sky blue color
		nextion_inst_set("cirs 370+50,17,10,33823");
		nextion_inst_set("xstr 370,7,50,20,0,WHITE,33823,1,1,1,\"OFF\"");
		nextion_inst_set("cirs 370+50,16,8,WHITE");
	}
	/* on button draw */
	else if(*power_state == on){
		nextion_inst_set("cirs 370,17,10,RED");
		nextion_inst_set("cirs 370+50,17,10,RED");
		nextion_inst_set("xstr 370,7,50,20,0,WHITE,RED,1,1,1,\"ON\"");
		nextion_inst_set("cirs 370,16,8,WHITE");
	}
}

/* This function merge the all screen display function at Nextion LCD */
/* All the screen display function merge at this function and using   */
void task_dis(int16_t* curY){
	/* LCD clear */
	nextion_inst_set("cls WHITE");

	/* menu draw */
	for(ITEM_Typedef i = temp ; i < MAX_ITEM ; i++){
		/* values display only inside LCD */
		if(menu_data[i].area.y1 < 0)   continue;
		if(menu_data[i].area.y0 > 270) continue;

		/* menu background and title display */
		uint16_t y_coordinate_buf = menu_data[i].area.y0 + 21;
		sprintf(bf, "fill %d,%d,474,60,%d", menu_data[i].area.x0, menu_data[i].area.y0, menu_data[i].color);
		nextion_inst_set(bf);
		sprintf(bf, "draw %d,%d,%d,%d,BLACK", menu_data[i].area.x0, menu_data[i].area.y0, menu_data[i].area.x1, menu_data[i].area.y1);
		nextion_inst_set(bf);

		sprintf(bf, "xstr 10,%d,80,17,0,WHITE,BLACK,0,1,3,\"%s\"", y_coordinate_buf, menu_data[i].title);
		nextion_inst_set(bf);

		/* each menu`s value display */
		if(i == temp){
			sprintf(bf, "xstr 3,%d,3+474,17,0,WHITE,BLACK,1,1,3,\"%02d.%d / 28\"", y_coordinate_buf, menu_data[temp].value / 10, menu_data[temp].value % 10);
			nextion_inst_set(bf);
		}
		else if(i == humidity){
			sprintf(bf, "xstr 3,%d,3+474,17,0,WHITE,BLACK,1,1,3,\"%02d.%d%% / 070\"", y_coordinate_buf, menu_data[humidity].value / 10, menu_data[humidity].value % 10);
			nextion_inst_set(bf);
		}
		else if(i == co2){
			sprintf(bf, "xstr 3,%d,3+474,17,0,WHITE,BLACK,1,1,3,\"%04d / 0700\"", y_coordinate_buf, menu_data[co2].value);
			nextion_inst_set(bf);
		}
		else if(i == brightness){
			/* drag bar shape draw */
			sprintf(bf, "fill 130,%d,220,20,GRAY", menu_data[brightness].area.y0 + 20);
			nextion_inst_set(bf);
			sprintf(bf, "draw %d,%d,%d,%d,BLACK", 130, menu_data[brightness].area.y0 + 20, 140 + 210, menu_data[brightness].area.y0 + 40);
			nextion_inst_set(bf);

			/* drag bar cursor draw */
			sprintf(bf, "fill %d,%d,20,20,WHITE", 140 + menu_data[brightness].value * 2 - 10, menu_data[brightness].area.y0 + 20);
			nextion_inst_set(bf);
			sprintf(bf, "draw %d,%d,%d,%d,BLACK", 140 + menu_data[brightness].value * 2 - 10, menu_data[brightness].area.y0 + 20, 140 + menu_data[brightness].value * 2 + 10, menu_data[brightness].area.y0 + 40);
			nextion_inst_set(bf);
			sprintf(bf, "line %d,%d,%d,%d,BLACK", 140 + menu_data[brightness].value * 2, menu_data[brightness].area.y0 + 23, 140 + menu_data[brightness].value * 2, menu_data[brightness].area.y0 + 37);
			nextion_inst_set(bf);
		}
		else if(i == motor){
			sprintf(bf, "xstr 3,%d,3+474,17,0,WHITE,BLACK,1,1,3,\"%s\"", y_coordinate_buf, motor_string);
			nextion_inst_set(bf);
		}
		else if(i == buzzer){
			/* drag bar shape draw */
			sprintf(bf, "fill 130,%d,220,20,GRAY", menu_data[buzzer].area.y0 + 20);
			nextion_inst_set(bf);
			sprintf(bf, "draw %d,%d,%d,%d,BLACK", 130, menu_data[buzzer].area.y0 + 20, 140 + 210, menu_data[buzzer].area.y0 + 40);
			nextion_inst_set(bf);

			/* drag bar cursor draw */
			sprintf(bf, "fill %d,%d,20,20,WHITE", 140 + menu_data[buzzer].value * 2 - 10, menu_data[buzzer].area.y0 + 20);
			nextion_inst_set(bf);
			sprintf(bf, "draw %d,%d,%d,%d,BLACK", 140 + menu_data[buzzer].value * 2 - 10, menu_data[buzzer].area.y0 + 20, 140 + menu_data[buzzer].value * 2 + 10, menu_data[buzzer].area.y0 + 40);
			nextion_inst_set(bf);
			sprintf(bf, "line %d,%d,%d,%d,BLACK", 140 + menu_data[buzzer].value * 2, menu_data[buzzer].area.y0 + 23, 140 + menu_data[buzzer].value * 2, menu_data[buzzer].area.y0 + 37);
			nextion_inst_set(bf);
		}
	}

	/* title draw */
	sprintf(bf, "xstr 0,0,480,35,0,WHITE,BLACK,1,1,1,\"Setting\"");
	nextion_inst_set(bf);

	/* button draw */
	button_draw(&button_state);

	/* menu drag bar draw */
	uint16_t color = 4<<11 | 4<<6 | 4; // 8452 <- more dark than general gray color
	sprintf(bf, "fill %d,%d,15,30,%d", bar_area.x0, *curY-15, color);
	nextion_inst_set(bf);
}

void task_fuc(void){
	int16_t bar_y = 55;
	uint8_t befo_touched = curXY.touched;
	uint32_t tick = 0;
	uint32_t auto_tick = 0;

	uint8_t led_state[LED_MAX] = { 0, 0, 0 };

	uint8_t sign_f = 0;
	uint8_t sign_check = 0;

	nextion_inst_set("thdra=0");
	nextion_inst_set("thdra=0");
	nextion_inst_set("thdra=0");

	sprintf(bf, "dim=%d", menu_data[brightness].value);
	nextion_inst_set(bf);
	nextion_inst_set(bf);
	nextion_inst_set(bf);

	task_dis(&curXY.y);
	task_dis(&curXY.y);
	task_dis(&curXY.y);

	motor_string_set(&motor_string, &menu_data[motor].value);

	while(1){
		/* read touch coordinate & current touch state */
		/* if not sign mode getting touch coordinate   */
		if(sign_f == 0) get_touch(&curXY);

		/* display component coordinate set */
		if(area_check(&curXY, &bar_area) == 1 || SW(1) || SW(3) || sign_f == 2) {
			screen_update = 0;

			if(SW(1))       { bar_y = 55;  curXY.y = 55;             }
			if(SW(3))       { bar_y = 255; curXY.y = 255;            }
			if(sign_f == 2) { bar_y = 55;  curXY.y = 55; sign_f = 0; }

			bar_coordinate_set(&bar_y);
			for(ITEM_Typedef i = temp ; i < MAX_ITEM ; i++)
				menu_coordinate_set(&menu_data[i].area, &origin_y_coordinate[i]);
		}
		/* auto sensing button read */
		else if(area_check(&curXY, &button_area) == 1 && befo_touched != curXY.touched){
			if(curXY.touched == 1){
				screen_update = 0;
				if(button_state == off) button_state = on;
				else button_state = off;
			}
		}
		/* display component`s touch read & action */
		else{
			/* except brightness and buzzer menu other menu control function */
			ITEM_Typedef i;
			static ITEM_Typedef befo_item = MAX_ITEM;
			for(i = temp ; i < MAX_ITEM ; i++){
				if(i == brightness || i == buzzer) continue;
				else if(area_check(&curXY, &menu_data[i].area)){
					if(curXY.touched == 1 && HAL_GetTick() - tick > 500){
						screen_update = 0;
						tick = HAL_GetTick(); befo_item = i;
						switch(i){
						case temp:
							sht41_value = (SHT41_t*)get_sensor(&i);
							menu_data[temp].value = (uint16_t)(sht41_value->temperature * 10);
							free_reset((void*)&sht41_value);
							break;
						case humidity:
							sht41_value = (SHT41_t*)get_sensor(&i);
							menu_data[humidity].value = (uint16_t)(sht41_value->humidity * 10);
							free_reset((void*)&sht41_value);
							break;
						case co2:
							ens160_value = (uint16_t*)get_sensor(&i);
							menu_data[co2].value = *ens160_value;
							free_reset((void*)&ens160_value);
							break;
						case motor:
							menu_data[motor].value = (menu_data[motor].value < 9) ? menu_data[motor].value + 1 : 0;
							motor_string_set(&motor_string, &menu_data[motor].value);
							break;
						default: break; // run when input data is error data
						}
					}
					break;
				}
			}
			if(befo_item == motor && motorM == 0){
				if(befo_touched != curXY.touched && curXY.touched == 0) { motorM = 1; befo_item = MAX_ITEM; }
			}
			else if(i == MAX_ITEM || curXY.touched == 0) tick = HAL_GetTick();

			/* brightness and buzzer menu drag bar control function */
			AREA_Typedef maximum_area = { 140, 0, 140 + 200, 0 };

			/* brightness bar & LCD brightness set */
			AREA_Typedef brightness_var_area = { 130 + menu_data[brightness].value * 2, menu_data[brightness].area.y0 + 20, 150 + menu_data[brightness].value * 2, menu_data[brightness].area.y0 + 40 };
			maximum_area.y0 = menu_data[brightness].area.y0 + 20; maximum_area.y1 = menu_data[brightness].area.y0 + 40;
			if(area_check(&curXY, &brightness_var_area) == 1 && area_check(&curXY, &maximum_area) == 1){
				screen_update = 0;
				menu_data[brightness].value = (curXY.x - 140) / 2;
				sprintf(bf, "dim=%d", menu_data[brightness].value);
				nextion_inst_set(bf);
			}

			/* buzzer bar & BUZZER frequency set */
			static uint8_t buzzer_check = 0;
			AREA_Typedef buzzer_var_area = { 130 + menu_data[buzzer].value * 2, menu_data[buzzer].area.y0 + 20, 150 + menu_data[buzzer].value * 2, menu_data[buzzer].area.y0 + 40 };
			maximum_area.y0 = menu_data[buzzer].area.y0 + 20; maximum_area.y1 = menu_data[buzzer].area.y0 + 40;
			if(area_check(&curXY, &buzzer_var_area) == 1 && area_check(&curXY, &maximum_area) == 1){
				buzzer_check = 1;
				screen_update = 0;
				menu_data[buzzer].value = (curXY.x - 140) / 2;
				BUZ_hz_set(&menu_data[buzzer].value);
			}
			/* buzzer on */
			else if(buzzer_check == 1 && befo_touched != curXY.touched && curXY.touched == 0){
				buzzer_check = 0;
				buzM = 1; buz_on_time = 500;
			}
		}
		befo_touched = curXY.touched;

		/* auto sensing values */
		if(button_state == on && HAL_GetTick() - auto_tick > 500){
			auto_tick = HAL_GetTick();

			ITEM_Typedef get_item_addr = temp;
			sht41_value = (SHT41_t*)get_sensor(&get_item_addr);
			menu_data[temp].value     = (uint16_t)(sht41_value->temperature * 10);
			menu_data[humidity].value = (uint16_t)(sht41_value->humidity * 10);

			get_item_addr = co2;
			ens160_value = (uint16_t*)get_sensor(&get_item_addr);
			menu_data[co2].value = *ens160_value;

			free_reset((void*)&sht41_value);
			free_reset((void*)&ens160_value);

			screen_update = 0;
		}

		/* LCD screen Update */
		if(screen_update == 0) { screen_update = 1; task_dis(&bar_y); }

		/* When sensing values over the max value start function */
		if(menu_data[temp].value >= 280)     led_state[led1] = 1;
		else led_state[led1] = 0;

		if(menu_data[humidity].value >= 700) led_state[led2] = 1;
		else led_state[led2] = 0;

		if(menu_data[co2].value > 700)       led_state[led3] = 1;
		else led_state[led3] = 0;

		if(led_state[led1] || led_state[led2] || led_state[led3]) buzM = 2;
		else buzM = (buzM == 2) ? 0 : buzM;

		/* LED Light */
		LED(1,led_state[led1]);
		LED(2,led_state[led2]);
		LED(3,led_state[led3]);

		/* save values & sign function */
		if(SW(2) && sign_check == 0){
			sign_f ^= 1;
			sign_check = 1;

			if(sign_f == 1){
				/* touch draw mode on */
				nextion_inst_set("thdra=1");
				nextion_inst_set("thc=RED");
				curXY.touched = 0;

				/* rectangle draw */
				nextion_inst_set("fill 140,50,200,200,WHITE");
				nextion_inst_set("draw 140,50,140+200,50+200,BLACK");
			}
			else{
				/* touch draw mode off */
				nextion_inst_set("thdra=0");
				nextion_inst_set("thdra=0");
				nextion_inst_set("thdra=0");

				/* data save at eeprom */
				for(ITEM_Typedef i = temp ; i < MAX_ITEM ; i++)
					eeprom_save(addr_array[i], &menu_data[i].value);

				/* buzzer on */
				BUZ(1);
				HAL_Delay(100);
				BUZ(0);
				HAL_Delay(100);
				BUZ(1);
				HAL_Delay(100);
				BUZ(0);

				/* LCD clear */
				sign_f = 2;
			}
		}
		else if(SW(2) == 0) sign_check = 0;

		/* coordinate display for task testing */
         sprintf(bf, "xstr 0,0,150,30,0,BLACK,WHITE,0,1,1,\"X: %d Y: %d %d\"", curXY.x, curXY.y, bar_y);
		 nextion_inst_set(bf);
	}
}

/* make interrupt signal every 1ms */
void HAL_SYSTICK_Callback(void){
	if(buzM) buz_cnt++;
	else buz_cnt = 0;

	if(motorM) motor_cnt++;
	else motor_cnt = 0;

	if(buzM == 1){
		if(buz_cnt < buz_on_time) BUZ(1);
		else{
			buz_cnt = buz_on_time = 0;
			buzM = 0;
			BUZ(0);
		}
	}
	else if(buzM == 2){
		if(buz_cnt > 125){
			static uint8_t state = 0;
			buz_cnt = 0;
			state ^= 1;
			BUZ(state);
		}
	}

	if(motorM == 1){
		if(motor_cnt < 2000) motor_set(DRV8830_CW, &motor_voltage[menu_data[motor].value]);
		else{
			motorM = motor_cnt = 0;
			motor_set(DRV8830_STOP, &motor_voltage[menu_data[motor].value]);
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

	LED(1,0);
	LED(2,0);
	LED(3,0);

	/* nextion LCD and MCU uart baudrate change */
	nextion_inst_set("baud=921600");
	nextion_inst_set("baud=921600");
	nextion_inst_set("baud=921600");
	HAL_Delay(50);
	USART1->CR1 &= (~USART_CR1_UE);  // usart stop
	USART1->BRR = 0x23;              // baudrate change
	USART1->CR1 |= USART_CR1_UE;     // usart restart
	HAL_Delay(1000);

	/* data load */
	for(ITEM_Typedef i = temp ; i < MAX_ITEM ; i++)
		menu_data[i].value = eeprom_read(addr_array[i]);

	setMotor(DRV8830_STOP);
	BUZ_hz_set(&menu_data[buzzer].value);

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
