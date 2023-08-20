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
#include "stm32l0xx_hal.h"
#include "sht41.h"
#include "ens160.h"
#include "drv8830.h"
#include "NEXTION.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
enum{ buz_off_state, buz_on_state, };
typedef enum{ menu, temp, hum, co2 }page_name;

typedef enum{
	run_state,
	stop_state,
}FIRSTF_STATE;

typedef enum{
	door_close_state,
	door_open_state,
}DOOR_STATE;

typedef enum{
	buz_off,
	buz_post_on,
	buz_error_on,
	buz_door_on,
}BUZ_STATE;

typedef enum{
	car_wating,
	car_prepare,
	car_arrival,
	car_ride,
}CAR_STATE;

/* for buz_control */
typedef struct _user_buz_control{
	/* user control variable */
	BUZ_STATE buzM;
	uint8_t buz_re;
	uint16_t buzC;

	/* struct reset function */
	void (*buz_reset_fuc)(struct _user_buz_control* user_buz);

	/* user control function */
	void (*buz_check_fuc)(struct _user_buz_control* user_buz);
	void (*buz_run_fuc)(uint8_t x);
}USER_BUZ_CONTROL;

/* for touch */
typedef struct _user_touched_control{
	/* user control variable */
	coordinate curXY;

	/* struct reset function */
	void  (*touch_reset_fuc)(struct _user_touched_control* user_touch);

	/* user control function */
	coordinate (*getXY)(void);
}USER_TOUCHED_CONTROL;

/* for sleep mode */
typedef struct _user_sleep_control{
	/* read only (use with nextion_inst_set) */
	char* sleep_start;
	char* sleep_not_use;
	char* sleep_end;
	char* sleep_wakeup_mode;
	char* sleep_wakeup_mode_com;

	/* struct reset function */
	void (*sleep_reset_fuc)(struct _user_sleep_control* user_sleep);
}USER_SLEEP_CONTROL;

/* for mode change */
typedef struct _user_mode_control{
	/* user control variable */
	page_name now_page;

	/* struct reset function */
	void (*reset_mode_fuc)(struct _user_mode_control* page);

	/* user control function */
	void (*change_fuc)(struct _user_mode_control* page);
	void (*run_fuc)(void);
}USER_MODE_CONTROL;

/* for sht41 control */
typedef struct _user_sht41_control{
	/* user control variable */
	volatile SHT41_t sht41_value;

	/* struct reset function */
	void (*reset_sht41_fuc)(struct _user_sht41_control* user_sht41);

	/* user control function */
	SHT41_t (*get_value_fuc)(void);
}USER_SHT41_CONTROL;

/* for ens160 control */
typedef struct _user_ens160_control{
	/* user control variable */
	volatile uint16_t ens160_value;

	/* struct reset function */
	void (*reset_ens160_fuc)(struct _user_ens160_control* user_ens160);

	/* user control function */
	int (*get_value_fuc)(void);
}USER_ENS160_CONTROL;

/* for menu display */
typedef struct _user_menu_control{
	/* struct reset function */
	void (*struct_reset_fuc)(struct _user_menu_control* user_menu);

	/* touch sensing */
	uint8_t touch_id;

	/* user using function */
	void (*main_fuc)(void);
	void (*init_fuc)(void);
	void (*screen_update_fuc)(void);
	void (*touch_sensing_fuc)(void);
	void (*etc_sensor_fuc)(void);
}USER_MENU_CONTROL;

/* for temp display */
typedef struct _user_temp_control{
	/* struct reset function */
	void (*struct_reset_fuc)(struct _user_temp_control* user_temp);

	/* touch sensing */
	uint8_t touch_id;

	/* user using function */
	void (*main_fuc)(void);
	void (*init_fuc)(void);
	void (*read_sht41_fuc)(void);
	void (*screen_update_fuc)(void);
	void (*exit_fuc)(void);
}USER_TEMP_CONTROL;

/* for brightness display */
typedef struct _user_brightness_control{
	/* user control variable */
	uint8_t brightness;

	/* struct reset function */
	void (*struct_reset_fuc)(struct _user_brightness_control* user_brightness);

	/* brightness set function */
	void (*display_fuc)(uint8_t* brightness);

	/* touch sensing */
	uint8_t touch_id;

	/* user using function */
	void (*main_fuc)(void);
	void (*init_fuc)(uint8_t* brightness);
	void (*get_coordinate)(void);
	void (*set_brightness)(uint8_t* brightness);
	void (*screen_update_fuc)(uint8_t* brightness);
	void (*exit_fuc)(void);
}USER_BRIGHTNESS_CONTROL;

/* for co2 display */
typedef struct _user_co2_control{
	/* struct reset function */
	void (*struct_reset_fuc)(struct _user_co2_control* user_co2);

	/* touch sensing */
	uint8_t touch_id;

	/* user using function */
	void (*main_fuc)(void);
	void (*init_fuc)(void);
	void (*read_ens160_fuc)(void);
	void (*screen_update_fuc)(void);
	void (*exit_fuc)(void);
}USER_CO2_CONTROL;

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
char *door_state_text[2] = { "Unlocked Door", "Locked Door" };
char *car_state_text[4] =  { "Waiting for car", "Preparing a car", "Arrival of a car", "Ride in a car" };
char post_state_text[10];

uint8_t Re = 0, buz_state = 0, door_lock = 0, door_open = 0, post = 0;

FIRSTF_STATE firF = run_state;
CAR_STATE car_state = car_wating;

/* for part control start */
USER_BUZ_CONTROL         user_buz;
USER_TOUCHED_CONTROL     user_xy;
USER_SLEEP_CONTROL       user_sleep;
USER_MODE_CONTROL        user_mode;
USER_SHT41_CONTROL       user_sht41;
USER_ENS160_CONTROL      user_ens160;
/* for part control end */

/* for display control start */
USER_MENU_CONTROL        menu_fuc;
USER_TEMP_CONTROL        temp_fuc;
USER_BRIGHTNESS_CONTROL  brightness_fuc;
USER_CO2_CONTROL         co2_fuc;
/* for display control end */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */
void BUZ(uint8_t x);
void car_call(void);
void post_box(void);
void PIR_Sensor(void);
void bright_menu(void);
uint8_t sw(uint8_t sw_num);
void door_lock_control(void);
void door_open_control(void);
void change_page(struct _user_mode_control* page);
void buz_repeat_check(struct _user_buz_control *user_buz);

void reset_buz(struct _user_buz_control* user_buz);
void reset_touch(struct _user_touched_control* user_touch);
void reset_sleep(struct _user_sleep_control* user_sleep);
void reset_mode(struct _user_mode_control* user_mode);
void reset_sht41(struct _user_sht41_control* user_sht41);
void reset_ens160(struct _user_ens160_control* user_ens160);

/* main menu function start */
void menu_struct_reset_fuc(struct _user_menu_control* user_menu);
void menu_init_fuc(void);
void menu_screen_update_fuc(void);
void menu_touch_sensing_fuc(void);
void menu_etc_sensor_fuc(void);
void menu_main_fuc(void);
/* main menu function end */

/* temp menu function start */
void temp_struct_reset_fuc(struct _user_temp_control* user_temp);
void temp_init_fuc(void);
void temp_read_sht41_fuc(void);
void temp_screen_update_fuc(void);
void temp_exit_fuc(void);
void temp_main_fuc(void);
/* temp menu function end */

/* brightness menu function start */
void bright_struct_reset_fuc(struct _user_brightness_control* user_brightness);
void bright_display(uint8_t* brightness);
void bright_init_fuc(uint8_t* brightness);
void bright_get_coordinate_fuc(void);
void bright_set_brightness_fuc(uint8_t* brightness);
void bright_screen_update_fuc(uint8_t* brightness);
void bright_exit_fuc(void);
void bright_main_fuc(void);
/* brightness menu function end */

/* co2 menu function start */
void co2_struct_reset_fuc(struct _user_co2_control* user_co2);
void co2_init_fuc(void);
void co2_read_ens160_fuc(void);
void co2_screen_update_fuc(void);
void co2_exit_fuc(void);
void co2_main_fuc(void);
/* co2 menu function end */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* struct reset function start */
void reset_buz(struct _user_buz_control* user_buz){
	user_buz->buzM = buz_off;
	user_buz->buz_re = 0;
	user_buz->buzC = 0;
	user_buz->buz_check_fuc = buz_repeat_check;
	user_buz->buz_run_fuc = BUZ;
}

void reset_touch(struct _user_touched_control* user_touch){
	user_touch->curXY.x = 0;
	user_touch->curXY.y = 0;
	user_touch->getXY = nextion_get_pos;
}

void reset_sleep(struct _user_sleep_control* user_sleep){
	user_sleep->sleep_end             = "sleep=0";
	user_sleep->sleep_start           = "thsp=30";
	user_sleep->sleep_not_use         = "thsp=0";
	user_sleep->sleep_wakeup_mode     = "thup=1";
	user_sleep->sleep_wakeup_mode_com = "usup=1";
}

void reset_mode(struct _user_mode_control* user_mode){
	user_mode->now_page = menu;
	user_mode->change_fuc = change_page;
	user_mode->run_fuc = menu_fuc.main_fuc;
}

void reset_sht41(struct _user_sht41_control* user_sht41){
	user_sht41->sht41_value.temperature = 0.0;
	user_sht41->sht41_value.humidity = 0.0;
	user_sht41->get_value_fuc = getTempSht41;
}

void reset_ens160(struct _user_ens160_control* user_ens160){
	user_ens160->ens160_value = 0;
	user_ens160->get_value_fuc = getCO2;
}
/* struct reset function end */

/* running function change function start */
void change_page(struct _user_mode_control* page){
	if(page->now_page == menu)      page->run_fuc = menu_fuc.main_fuc;
	else if(page->now_page == temp) page->run_fuc = temp_fuc.main_fuc;
	else if(page->now_page == hum)  page->run_fuc = brightness_fuc.main_fuc;
	else if(page->now_page == co2)  page->run_fuc = co2_fuc.main_fuc;
}
/* running function change function end */

/* global using function start */
void BUZ(uint8_t x){
	if(x) HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
	else HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
}

void buz_repeat_check(struct _user_buz_control *user_buz){
	user_buz->buz_re--;
	if(user_buz->buz_re == 0) user_buz->buzM = buz_off;
}

uint8_t sw(uint8_t sw_num){
	static uint8_t old_key = 0;

	if(sw_num == 1){
		if(SW_READ(1)) old_key = 1;
		else if(!SW_READ(1) && old_key == 1) { nextion_inst_set(user_sleep.sleep_start); old_key = 0; return 1; }
	}
	else if(sw_num == 2){
		if(SW_READ(2)) old_key = 2;
		else if(!SW_READ(2) && old_key == 2) { nextion_inst_set(user_sleep.sleep_start); old_key = 0; return 1; }
	}
	else if(sw_num == 3){
		if(SW_READ(3)) old_key = 3;
		else if(!SW_READ(3) && old_key == 3) { nextion_inst_set(user_sleep.sleep_start); old_key = 0; return 1; }
	}
	return 0;
}

void read_sht41_fuc(void){
	user_sht41.sht41_value = user_sht41.get_value_fuc();
}
/* global using function end */

/* menu display function start */
void menu_struct_reset_fuc(struct _user_menu_control* user_menu){
	user_menu->main_fuc          = menu_main_fuc;
	user_menu->init_fuc          = menu_init_fuc;
	user_menu->screen_update_fuc = menu_screen_update_fuc;
	user_menu->touch_sensing_fuc = menu_touch_sensing_fuc;
	user_menu->etc_sensor_fuc    = menu_etc_sensor_fuc;
}

void menu_init_fuc(void){
	if(firF == run_state){
		nextion_inst_set("dp=0");
		nextion_inst_set("hum_t.txt=\"Brightness\"");
		nextion_inst_set(user_sleep.sleep_start);
		firF = stop_state;
	}
}

void menu_screen_update_fuc(void){
	if(!Re){
		sprintf(bf, "door_button.txt=\"%s\"", door_state_text[door_lock]);
		nextion_inst_set(bf);

		sprintf(bf, "car_t.txt=\"%s\"", car_state_text[car_state]);
		nextion_inst_set(bf);

		sprintf(bf, "post_t.txt=\"%s\"", post > 0 ? post_state_text : "Empty");
		nextion_inst_set(bf);

		Re = 1;
	}
}

void menu_touch_sensing_fuc(void){
	if(menu_fuc.touch_id == 3)      { user_mode.now_page = temp; firF = 0; Re = 0; nextion_inst_set(user_sleep.sleep_not_use); }
	else if(menu_fuc.touch_id == 4) { user_mode.now_page = hum;  firF = 0; Re = 0; nextion_inst_set(user_sleep.sleep_not_use); }
	else if(menu_fuc.touch_id == 5) { user_mode.now_page = co2;  firF = 0; Re = 0; nextion_inst_set(user_sleep.sleep_not_use); }
}

void menu_etc_sensor_fuc(void){
	if(menu_fuc.touch_id == 8 && !door_open) door_lock_control();
	door_open_control();

	PIR_Sensor();
	car_call();
	post_box();
}

void menu_main_fuc(void){
	/* touch sensing */
	menu_fuc.touch_id = NEXTION_Get_Touch(0);

	/* function play */
	menu_fuc.init_fuc();
	menu_fuc.screen_update_fuc();
	menu_fuc.touch_sensing_fuc();
	menu_fuc.etc_sensor_fuc();
}
/* menu display function end */

/* menu display etc function start */
void door_lock_control(void){
	door_lock = !door_lock;
	Re = 0;
}

void door_open_control(void){
	static uint32_t t;
	static DOOR_STATE door_state = door_close_state;
	static uint8_t pushed_sw = 0;

	LED(1, door_state);

	if(sw(1)){
		if(door_lock == 0 && door_state == door_close_state){
			user_buz.buz_re = 2;
			user_buz.buzM = buz_door_on;
			pushed_sw = 1;
			t = HAL_GetTick();
		}
		else if(door_state == door_open_state){
			pushed_sw = 2;
			t = HAL_GetTick();
		}
		else if(door_lock == 1){
			user_buz.buz_re = 4;
			user_buz.buzM = buz_door_on;
		}
	}

	if(pushed_sw == 1){
		if(user_buz.buzM == 0){
			if(HAL_GetTick() - t > 3000){                            // wait 3 second
				if(HAL_GetTick() - t < 4000) setMotor(DRV8830_CW);   // run motor 1 second
				else{
					t = HAL_GetTick();
					door_state = door_open_state;
					door_open = 1;
					pushed_sw = 0;
					setMotor(DRV8830_STOP);
				}
			}
		}
		else { setMotor(DRV8830_STOP); t = HAL_GetTick(); }
	}
	else if(pushed_sw == 2){
		if(HAL_GetTick() - t < 1000) setMotor(DRV8830_CCW);
		else{
			door_state = door_close_state;
			door_open = 0;
			pushed_sw = 0;
			setMotor(DRV8830_STOP);
		}
	}
}

void PIR_Sensor(void){
	static uint32_t t;
	static uint8_t pir_sen = 0;

	LED(2, pir_sen);

	if((PIR_LEFT || PIR_RIGHT) && pir_sen == 0) { pir_sen = 1; t = HAL_GetTick(); nextion_inst_set(user_sleep.sleep_start); }

	if(pir_sen == 1){
		if(HAL_GetTick() - t > 5000) pir_sen = 0;
	}
}

void car_call(void){
	static uint32_t t;

	if(car_state == car_wating)       {  if(sw(2))                     { car_state = car_prepare; Re = 0; t = HAL_GetTick(); }  }
	else if(car_state == car_prepare) {  if(HAL_GetTick() - t > 5000)  { car_state = car_arrival; Re = 0;                    }  }
	else if(car_state == car_arrival) {  if(menu_fuc.touch_id == 6)    { car_state = car_ride;    Re = 0; t = HAL_GetTick(); }  }
	else if(car_state == car_ride)    {  if(HAL_GetTick() - t > 10000) { car_state = car_wating;  Re = 0;                    }  }
}

void post_box(void){
	if(sw(3)){
		post++, Re = 0;
		sprintf(post_state_text, "%d Box", post);
	}

	if(menu_fuc.touch_id == 7 && post > 0){
		if(!door_lock){
			Re = 0;
			user_buz.buzM = buz_post_on;
			user_buz.buz_re = post;
			post = 0;
		}
		else{
			user_buz.buzM = buz_error_on;
			user_buz.buz_re = 1;
		}
	}

	if(post > 0) LED(3,1);
	else LED(3,0);
}
/* menu display etc function end */

/* temp display function start */
void temp_struct_reset_fuc(struct _user_temp_control* user_temp){
	user_temp->main_fuc          = temp_main_fuc;
	user_temp->init_fuc          = temp_init_fuc;
	user_temp->read_sht41_fuc    = read_sht41_fuc;
	user_temp->screen_update_fuc = temp_screen_update_fuc;
	user_temp->exit_fuc          = temp_exit_fuc;
}

void temp_init_fuc(void){
	if(firF == run_state){
		nextion_inst_set("dp=1");
		firF = stop_state;
	}
}

void temp_screen_update_fuc(void){
	static uint32_t t;

	if(HAL_GetTick() - t > 1000){
		t = HAL_GetTick();

		sprintf(bf, "temp_n.val=%d", (uint16_t)(user_sht41.sht41_value.temperature * 10));
		nextion_inst_set(bf);

		sprintf(bf, "temp_gauge.val=%d", (uint8_t)((user_sht41.sht41_value.temperature - 20) * 6.6));
		nextion_inst_set(bf);
	}
}

void temp_exit_fuc(void){
	if(temp_fuc.touch_id == 8) { user_mode.now_page = menu; firF = 0; }
}

void temp_main_fuc(void){
	/* touch sensing */
	temp_fuc.touch_id = NEXTION_Get_Touch(1);

	/* function play */
	temp_fuc.init_fuc();
	temp_fuc.read_sht41_fuc();
	temp_fuc.screen_update_fuc();
	temp_fuc.exit_fuc();
}
/* temp display function end */

/* brightness display function start */
void bright_struct_reset_fuc(struct _user_brightness_control* user_brightness){
	/* brightness set function */
	user_brightness->display_fuc       = bright_display;

	/* user using function */
	user_brightness->main_fuc          = bright_main_fuc;
	user_brightness->init_fuc          = bright_init_fuc;
	user_brightness->get_coordinate    = bright_get_coordinate_fuc;
	user_brightness->set_brightness    = bright_set_brightness_fuc;
	user_brightness->screen_update_fuc = bright_screen_update_fuc;
	user_brightness->exit_fuc          = bright_exit_fuc;
}

void bright_display(uint8_t* brightness){
	sprintf(bf, "%d", *brightness);
	uint8_t len = strlen(bf);
	sprintf(bf, "hum_n.vvs0=%d", len);
	nextion_inst_set(bf);

	sprintf(bf, "hum_n.val=%d", *brightness);
	nextion_inst_set(bf);

	sprintf(bf, "dims=%d", *brightness);
	nextion_inst_set(bf);

	sprintf(bf, "hum_color.val=%d", *brightness);
	nextion_inst_set(bf);
}

void bright_init_fuc(uint8_t* brightness){
	if(firF == run_state){
		nextion_inst_set("dp=2");
		nextion_inst_set("title.txt=\"Brightness\"");
		nextion_inst_set("hum_n.vvs1=0");
		nextion_inst_set("hum_n.vvs0=3");
		nextion_inst_set("hum_state.txt=\"\"");

		nextion_inst_set("prints dims,1");
		HAL_UART_Receive(&huart1, brightness, 1, HAL_MAX_DELAY);
		brightness_fuc.display_fuc(&brightness_fuc.brightness);

		nextion_inst_set("sendxy=1");

		firF = stop_state;
	}
}

void bright_get_coordinate_fuc(void){
	user_xy.curXY = user_xy.getXY();
}

void bright_set_brightness_fuc(uint8_t* brightness){
	if((user_xy.curXY.x >= 255 && user_xy.curXY.x <= 260 + 155) && (user_xy.curXY.y >= 98 && user_xy.curXY.y <= 98 + 75))
		*brightness = (uint8_t)((float)(user_xy.curXY.x - 260 < 0 ? 0 : user_xy.curXY.x - 260) / 1.5f);
	*brightness = *brightness > 100 ? 100 : *brightness;
}

void bright_screen_update_fuc(uint8_t* brightness){
	static uint32_t t;

	if(HAL_GetTick() - t > 500){
		t = HAL_GetTick();

		brightness_fuc.display_fuc(&brightness_fuc.brightness);
	}
}

void bright_exit_fuc(void){
	if(brightness_fuc.touch_id == 6) { user_mode.now_page = menu; firF = 0; nextion_inst_set("sendxy=0"); }
}

void bright_main_fuc(void){
	/* touch sensing */
	brightness_fuc.touch_id = NEXTION_Get_Touch(2);

	/* function play */
	brightness_fuc.init_fuc(&brightness_fuc.brightness);
	brightness_fuc.get_coordinate();
	brightness_fuc.set_brightness(&brightness_fuc.brightness);
	brightness_fuc.screen_update_fuc(&brightness_fuc.brightness);
	brightness_fuc.exit_fuc();
}
/* brightness display function end */

/* co2 display function start */
void co2_struct_reset_fuc(struct _user_co2_control* user_co2){
	user_co2->main_fuc          = co2_main_fuc;
	user_co2->init_fuc          = co2_init_fuc;
	user_co2->read_ens160_fuc   = co2_read_ens160_fuc;
	user_co2->screen_update_fuc = co2_screen_update_fuc;
	user_co2->exit_fuc          = co2_exit_fuc;
}

void co2_init_fuc(void){
	if(firF == run_state){
		nextion_inst_set("dp=3");
		firF = stop_state;
	}
}

void co2_read_ens160_fuc(void){
	user_ens160.ens160_value = user_ens160.get_value_fuc();
}

void co2_screen_update_fuc(void){
	static uint32_t t;

	if(HAL_GetTick() - t > 1000){
		t = HAL_GetTick();

		/* co2 value setting */
		sprintf(bf, "co2_n.val=%d", user_ens160.ens160_value);
		nextion_inst_set(bf);

		/* co2_image setting */
		uint8_t image_state = 0;
		image_state = (user_ens160.ens160_value - 1) / 500;
		sprintf(bf, "co2_state.pic=%d", image_state);
		nextion_inst_set(bf);
	}
}

void co2_exit_fuc(void){
	if(co2_fuc.touch_id == 5) { user_mode.now_page = menu; firF=0; }
}

void co2_main_fuc(void){
	/* touch sensing */
	co2_fuc.touch_id = NEXTION_Get_Touch(3);

	/* function play */
	co2_fuc.init_fuc();
	co2_fuc.read_ens160_fuc();
	co2_fuc.screen_update_fuc();
	co2_fuc.exit_fuc();
}
/* co2 display function end */

/* user_buz running function start */
void HAL_SYSTICK_Callback(void){
	if(user_buz.buzM) user_buz.buzC++;
	else user_buz.buzC = 0;

	if(user_buz.buz_re > 0){
		if(user_buz.buzM == buz_post_on){
			if(user_buz.buzC < 100) user_buz.buz_run_fuc(buz_on_state);
			else if(user_buz.buzC < 150) user_buz.buz_run_fuc(buz_off_state);
			else{
				user_buz.buz_run_fuc(buz_off_state);
				user_buz.buz_check_fuc(&user_buz);
				user_buz.buzC = 0;
			}
		}
		else if(user_buz.buzM == buz_error_on){
			if(user_buz.buzC < 50) user_buz.buz_run_fuc(buz_on_state);
			else if(user_buz.buzC < 100) user_buz.buz_run_fuc(buz_off_state);
			else if(user_buz.buzC < 200) user_buz.buz_run_fuc(buz_on_state);
			else{
				user_buz.buz_run_fuc(buz_off_state);
				user_buz.buz_check_fuc(&user_buz);
				user_buz.buzC = 0;
			}
		}
		else if(user_buz.buzM == buz_door_on){
			if(user_buz.buzC < 500) user_buz.buz_run_fuc(buz_on_state);
			else if(user_buz.buzC < 1000) user_buz.buz_run_fuc(buz_off_state);
			else{
				user_buz.buz_run_fuc(buz_off_state);
				user_buz.buz_check_fuc(&user_buz);
				user_buz.buzC = 0;
			}
		}
	}
}
/* user_buz running function end */

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
	MX_I2C1_Init();
	MX_TIM2_Init();
	/* USER CODE BEGIN 2 */
	initEns160();
	initDrv8830();

	/* struct reset_function reset start */
	user_buz.buz_reset_fuc = reset_buz;
	user_xy.touch_reset_fuc = reset_touch;
	user_sleep.sleep_reset_fuc = reset_sleep;
	user_mode.reset_mode_fuc = reset_mode;
	user_sht41.reset_sht41_fuc = reset_sht41;
	user_ens160.reset_ens160_fuc = reset_ens160;

	menu_fuc.struct_reset_fuc = menu_struct_reset_fuc;
	temp_fuc.struct_reset_fuc = temp_struct_reset_fuc;
	brightness_fuc.struct_reset_fuc = bright_struct_reset_fuc;
	co2_fuc.struct_reset_fuc = co2_struct_reset_fuc;
	/* struct reset_function reset end */

	/* struct reset_function play start */
	user_buz.buz_reset_fuc(&user_buz);
	user_xy.touch_reset_fuc(&user_xy);
	user_sleep.sleep_reset_fuc(&user_sleep);
	user_mode.reset_mode_fuc(&user_mode);
	user_sht41.reset_sht41_fuc(&user_sht41);
	user_ens160.reset_ens160_fuc(&user_ens160);

	menu_fuc.struct_reset_fuc(&menu_fuc);
	temp_fuc.struct_reset_fuc(&temp_fuc);
	brightness_fuc.struct_reset_fuc(&brightness_fuc);
	co2_fuc.struct_reset_fuc(&co2_fuc);
	/* struct reset_function play end */

	LED(1,0);
	LED(2,0);
	LED(3,0);

	setMotor(DRV8830_STOP);
	nextion_inst_set("sendxy=0");

	nextion_inst_set(user_sleep.sleep_end);
	nextion_inst_set(user_sleep.sleep_start);
	nextion_inst_set(user_sleep.sleep_wakeup_mode);
	nextion_inst_set(user_sleep.sleep_wakeup_mode_com);
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1)
	{
		user_mode.change_fuc(&user_mode);
		user_mode.run_fuc();
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
