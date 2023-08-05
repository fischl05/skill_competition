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

	/* move mode variable */
	uint8_t move_mode;
	uint8_t left_menu_y[3];
	uint8_t right_menu_y[3];

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

	/* background pic */
	uint8_t pic;

	/* user using function */
	void (*main_fuc)(void);
	void (*init_fuc)(void);
	void (*read_sht41_fuc)(void);
	void (*screen_update_fuc)(void);
	void (*exit_fuc)(void);
}USER_TEMP_CONTROL;

/* for humidity display */
typedef struct _user_humidity_control{
	/* struct reset function */
	void (*struct_reset_fuc)(struct _user_humidity_control* user_humidity);

	/* touch sensing */
	uint8_t touch_id;

	/* background pic */
	uint8_t pic;

	/* user using function */
	void (*main_fuc)(void);
	void (*init_fuc)(void);
	void (*read_sht41_fuc)(void);
	void (*screen_update_fuc)(void);
	void (*exit_fuc)(void);
}USER_HUMIDITY_CONTROL;

/* for co2 display */
typedef struct _user_co2_control{
	/* struct reset function */
	void (*struct_reset_fuc)(struct _user_co2_control* user_co2);

	/* touch sensing */
	uint8_t touch_id;

	/* background pic */
	uint8_t pic;

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
char post_state_text[10];

uint8_t Re = 0, buz_state = 0, door_lock = 0, door_open = 0, post = 0;

FIRSTF_STATE firF = run_state;
CAR_STATE car_state = car_wating;

/* for part control start */
USER_BUZ_CONTROL         user_buz;
USER_MODE_CONTROL        user_mode;
USER_SHT41_CONTROL       user_sht41;
USER_ENS160_CONTROL      user_ens160;
/* for part control end */

/* for display control start */
USER_MENU_CONTROL        menu_fuc;
USER_TEMP_CONTROL        temp_fuc;
USER_HUMIDITY_CONTROL    humidity_fuc;
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
void post_box(void);
void PIR_Sensor(void);
uint8_t sw(uint8_t sw_num);
void door_lock_control(void);
void door_open_control(void);
void change_page(struct _user_mode_control* page);
void buz_repeat_check(struct _user_buz_control *user_buz);

void reset_buz(struct _user_buz_control* user_buz);
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

void menu_left_coordinate_set(void);
void menu_right_coordinate_set(void);

void menu_move_mode_check(void);
void menu_left_move_mode(void);
void menu_right_move_mode(void);
uint8_t touch_id_adjust(uint8_t *buf);
/* main menu function end */

/* temp menu function start */
void temp_struct_reset_fuc(struct _user_temp_control* user_temp);
void temp_init_fuc(void);
void temp_read_sht41_fuc(void);
void temp_screen_update_fuc(void);
void temp_exit_fuc(void);
void temp_main_fuc(void);
/* temp menu function end */

/* humidity menu function start */
void humidity_struct_reset_fuc(struct _user_humidity_control* user_humidity);
void humidity_init_fuc(void);
void humidity_read_sht41_fuc(void);
void humidity_screen_update_fuc(void);
void humidity_exit_fuc(void);
void humidity_main_fuc(void);
/* humidity menu function end */

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
	else if(page->now_page == hum)  page->run_fuc = humidity_fuc.main_fuc;
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
		else if(!SW_READ(1) && old_key == 1) { old_key = 0; return 1; }
	}
	else if(sw_num == 2){
		if(SW_READ(2)) old_key = 2;
		else if(!SW_READ(2) && old_key == 2) { old_key = 0; return 1; }
	}
	else if(sw_num == 3){
		if(SW_READ(3)) old_key = 3;
		else if(!SW_READ(3) && old_key == 3) { old_key = 0; return 1; }
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
		firF = stop_state;
	}
}

void menu_screen_update_fuc(void){
	if(!Re){
		/* left menu coordinate set */
		menu_left_coordinate_set();

		/* right menu coordinate set */
		menu_right_coordinate_set();

		sprintf(bf, "door_button.txt=\"%s\"", door_state_text[door_lock]);
		nextion_inst_set(bf);

		sprintf(bf, "car_t.txt=\"%s\"", menu_fuc.move_mode == 0 ? "Normal Mode" : "Custom Mode");
		nextion_inst_set(bf);

		sprintf(bf, "post_t.txt=\"%s\"", post > 0 ? post_state_text : "Empty");
		nextion_inst_set(bf);

		Re = 1;
	}
}

void menu_touch_sensing_fuc(void){
	if(menu_fuc.move_mode == 0){
		if(menu_fuc.touch_id == 3)      { user_mode.now_page = temp; firF = 0; Re = 0; temp_fuc.pic     = menu_fuc.left_menu_y[0] + 4; }
		else if(menu_fuc.touch_id == 4) { user_mode.now_page = hum;  firF = 0; Re = 0; humidity_fuc.pic = menu_fuc.left_menu_y[1] + 4; }
		else if(menu_fuc.touch_id == 5) { user_mode.now_page = co2;  firF = 0; Re = 0; co2_fuc.pic      = menu_fuc.left_menu_y[2] + 4; }
		else if(menu_fuc.touch_id == 6) { menu_fuc.move_mode = 1; menu_fuc.touch_id = 0; Re = 0; }
	}
	else if(menu_fuc.move_mode == 1){
		/* left menu move mode */
		menu_left_move_mode();

		/* right menu move mode */
		menu_right_move_mode();

		/* move mode end condition */
		menu_move_mode_check();
	}
}

void menu_etc_sensor_fuc(void){
	if(menu_fuc.touch_id == 8 && !door_open && menu_fuc.move_mode == 0) door_lock_control();
	door_open_control();

	PIR_Sensor();
	post_box();
}

void menu_main_fuc(void){
	/* touch sensing */
	menu_fuc.touch_id = NEXTION_Get_Touch(menu);

	/* function play */
	menu_fuc.init_fuc();
	menu_fuc.screen_update_fuc();
	menu_fuc.touch_sensing_fuc();
	menu_fuc.etc_sensor_fuc();
}
/* menu display function end */

/* menu display part coordinate set function start */
void menu_left_coordinate_set(void){
	sprintf(bf, "temp_t.y=sys%d", menu_fuc.left_menu_y[0]);
	nextion_inst_set(bf);

	sprintf(bf, "hum_t.y=sys%d", menu_fuc.left_menu_y[1]);
	nextion_inst_set(bf);

	sprintf(bf, "co2_t.y=sys%d", menu_fuc.left_menu_y[2]);
	nextion_inst_set(bf);
}

void menu_right_coordinate_set(void){
	sprintf(bf, "door_button.y=sys%d", menu_fuc.right_menu_y[0]);
	nextion_inst_set(bf);

	sprintf(bf, "car_t.y=sys%d", menu_fuc.right_menu_y[1]);
	nextion_inst_set(bf);

	sprintf(bf, "post_t.y=sys%d", menu_fuc.right_menu_y[2]);
	nextion_inst_set(bf);
}
/* menu display part coordinate set function end */

/* menu display move mode function start */
void menu_move_mode_check(void){
	static uint8_t touch_state = 0;
	static uint32_t tick = 0;

	if(menu_fuc.touch_id == 6){
		if(touch_state == 0)      { tick = HAL_GetTick(); touch_state = 1; }
		else if(touch_state == 1) { touch_state = 2; }
	}

	if(touch_state == 1)          { if(HAL_GetTick() - tick > 500) touch_state = 0; }
	else if(touch_state == 2)     { menu_fuc.move_mode = 0; touch_state = 0; firF = run_state; Re = 0; }
}

void menu_left_move_mode(void){
	static uint8_t touch_state = 0, selected_button = 0;

	/* if not move mode touch state and selected button reset */
	if(menu_fuc.move_mode == 0) { touch_state = 0; selected_button = 0; }

	/* first button select */
	if(touch_state == 0){
		if(menu_fuc.touch_id == 3)      { nextion_inst_set("temp_t.pco=BLACK"); selected_button = 3; touch_state = 1; }
		else if(menu_fuc.touch_id == 4) { nextion_inst_set("hum_t.pco=BLACK");  selected_button = 4; touch_state = 1; }
		else if(menu_fuc.touch_id == 5) { nextion_inst_set("co2_t.pco=BLACK");  selected_button = 5; touch_state = 1; }
	}
	else if(touch_state == 1){
		/* if previously selected button equal now selected button selected button reset */
		if(menu_fuc.touch_id == selected_button){
			sprintf(bf, "%s.pco=WHITE", menu_fuc.touch_id == 3 ? "temp_t" : menu_fuc.touch_id == 4 ? "hum_t" : "co2_t");
			nextion_inst_set(bf);
			touch_state = 0; selected_button = 0;
		}
		/* if previously selected button not equal now selected button each other  swap */
		else if(menu_fuc.touch_id >= 3 && menu_fuc.touch_id <= 5){
			/* Swap selected button and now selected button */
			uint8_t temp = menu_fuc.left_menu_y[selected_button - 3];
			menu_fuc.left_menu_y[selected_button - 3] = menu_fuc.left_menu_y[menu_fuc.touch_id - 3];
			menu_fuc.left_menu_y[menu_fuc.touch_id - 3] = temp;

			/* text color reset */
			nextion_inst_set("temp_t.pco=WHITE");
			nextion_inst_set("hum_t.pco=WHITE");
			nextion_inst_set("co2_t.pco=WHITE");

			touch_state = 0; selected_button = 0;
			Re = 0;
		}
	}
}

void menu_right_move_mode(void){
	static uint8_t touch_state = 0, selected_button = 0;

	/* if not move mode touch state and selected button reset */
	if(menu_fuc.move_mode == 0) { touch_state = 0; selected_button = 0; }

	/* first button select */
	if(touch_state == 0){
		if(menu_fuc.touch_id == 6) { nextion_inst_set("car_t.pco=RED"); selected_button = 6; touch_state = 1; }
		else if(menu_fuc.touch_id == 7) { nextion_inst_set("post_t.pco=RED"); selected_button = 7; touch_state = 1; }
		else if(menu_fuc.touch_id == 8) { nextion_inst_set("door_button.pco=RED"); selected_button = 8; touch_state = 1; }
	}
	else if(touch_state == 1){
		/* if previously selected button equal now selected button selected button reset */
		if(menu_fuc.touch_id == selected_button){
			sprintf(bf, "%s.pco=WHITE", menu_fuc.touch_id == 6 ? "car_t" : menu_fuc.touch_id == 7 ? "post_t" : "door_button");
			nextion_inst_set(bf);
			touch_state = 0; selected_button = 0;
		}
		/* if previously selected button not equal now selected button each other  swap */
		else if(menu_fuc.touch_id >= 6 && menu_fuc.touch_id <= 8){
			uint8_t id_buf = menu_fuc.touch_id;

			/* touch id adjust */
			id_buf          = touch_id_adjust(&id_buf);
			selected_button = touch_id_adjust(&selected_button);

			/* Swap selected button and now selected button */
			uint8_t temp = menu_fuc.right_menu_y[selected_button - 6];
			menu_fuc.right_menu_y[selected_button - 6] = menu_fuc.right_menu_y[id_buf - 6];
			menu_fuc.right_menu_y[id_buf - 6] = temp;

			/* text color reset */
			nextion_inst_set("door_button.pco=WHITE");
			nextion_inst_set("car_t.pco=WHITE");
			nextion_inst_set("post_t.pco=WHITE");

			touch_state = 0; selected_button = 0;
			Re = 0;
		}
	}
}

uint8_t touch_id_adjust(uint8_t *buf){ // input value`s range is 6 ~ 8
	uint8_t res = *buf + 1;
	if(res >= 9) res = 6;
	return res;
}
/* menu display move mode function end */

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

	if((PIR_LEFT || PIR_RIGHT) && pir_sen == 0) { pir_sen = 1; t = HAL_GetTick(); }

	if(pir_sen == 1){
		if(HAL_GetTick() - t > 5000) pir_sen = 0;
	}
}

void post_box(void){
	if(sw(3)){
		post++, Re = 0;
		sprintf(post_state_text, "%d Box", post);
	}

	if(menu_fuc.touch_id == 7 && post > 0 && menu_fuc.move_mode == 0){
		post--; Re = 0;
		sprintf(post_state_text, "%d Box", post);
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

		sprintf(bf, "background.pic=%d", temp_fuc.pic);
		nextion_inst_set(bf);

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
	temp_fuc.touch_id = NEXTION_Get_Touch(temp);

	/* function play */
	temp_fuc.init_fuc();
	temp_fuc.read_sht41_fuc();
	temp_fuc.screen_update_fuc();
	temp_fuc.exit_fuc();
}
/* temp display function end */

/* humidity display function start */
void humidity_struct_reset_fuc(struct _user_humidity_control* user_humidity){
	user_humidity->main_fuc = humidity_main_fuc;
	user_humidity->init_fuc = humidity_init_fuc;
	user_humidity->read_sht41_fuc = humidity_read_sht41_fuc;
	user_humidity->screen_update_fuc = humidity_screen_update_fuc;
	user_humidity->exit_fuc = humidity_exit_fuc;
}

void humidity_init_fuc(void){
	if(!firF){
		nextion_inst_set("dp=2");

		sprintf(bf, "background.pic=%d", humidity_fuc.pic);
		nextion_inst_set(bf);

		firF = 1;
	}
}

void humidity_read_sht41_fuc(void){
	user_sht41.sht41_value = user_sht41.get_value_fuc();
}

void humidity_screen_update_fuc(void){
	static uint32_t t;
	static uint8_t hum_value = 0;

	if(HAL_GetTick() - t > 1000){
		t = HAL_GetTick();
		sprintf(bf, "hum_n.val=%d", (uint16_t)(user_sht41.sht41_value.humidity * 10));
		nextion_inst_set(bf);

		hum_value = (uint8_t)user_sht41.sht41_value.humidity;
		if(hum_value < 40){
			nextion_inst_set("hum_state.txt=\"Dry\"");
			nextion_inst_set("hum_color.pco=61195");
		}
		else if(hum_value < 60){
			nextion_inst_set("hum_state.txt=\"Appropriate\"");
			nextion_inst_set("hum_color.pco=1024");
		}
		else{
			nextion_inst_set("hum_state.txt=\"Humid\"");
			nextion_inst_set("hum_color.pco=63488");
		}
	}
}

void humidity_exit_fuc(void){
	if(humidity_fuc.touch_id == 6) { user_mode.now_page = menu; firF = 0; }
}

void humidity_main_fuc(void){
	/* touch sensing */
	humidity_fuc.touch_id = NEXTION_Get_Touch(hum);

	/* function play */
	humidity_fuc.init_fuc();
	humidity_fuc.read_sht41_fuc();
	humidity_fuc.screen_update_fuc();
	humidity_fuc.exit_fuc();
}
/* humidity display function end */

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

		sprintf(bf, "background.pic=%d", co2_fuc.pic);
		nextion_inst_set(bf);

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
	co2_fuc.touch_id = NEXTION_Get_Touch(co2);

	/* function play */
	co2_fuc.init_fuc();
	co2_fuc.read_ens160_fuc();
	co2_fuc.screen_update_fuc();
	co2_fuc.exit_fuc();
}
/* co2 display function end */

/* buzzer running function start */
void HAL_SYSTICK_Callback(void){
	if(user_buz.buzM) user_buz.buzC++;
	else user_buz.buzC = 0;

	if(user_buz.buz_re > 0){
		if(user_buz.buzM == buz_door_on){
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
/* buzzer running function end */

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
	user_mode.reset_mode_fuc = reset_mode;
	user_sht41.reset_sht41_fuc = reset_sht41;
	user_ens160.reset_ens160_fuc = reset_ens160;

	menu_fuc.struct_reset_fuc = menu_struct_reset_fuc;
	temp_fuc.struct_reset_fuc = temp_struct_reset_fuc;
	humidity_fuc.struct_reset_fuc = humidity_struct_reset_fuc;
	co2_fuc.struct_reset_fuc = co2_struct_reset_fuc;
	/* struct reset_function reset end */

	/* struct reset_function play start */
	user_buz.buz_reset_fuc(&user_buz);
	user_mode.reset_mode_fuc(&user_mode);
	user_sht41.reset_sht41_fuc(&user_sht41);
	user_ens160.reset_ens160_fuc(&user_ens160);

	menu_fuc.struct_reset_fuc(&menu_fuc);
	temp_fuc.struct_reset_fuc(&temp_fuc);
	humidity_fuc.struct_reset_fuc(&humidity_fuc);
	co2_fuc.struct_reset_fuc(&co2_fuc);
	/* struct reset_function play end */

	LED(1,0);
	LED(2,0);
	LED(3,0);

	setMotor(DRV8830_STOP);

	nextion_inst_set("rest");
	HAL_Delay(1000);

	nextion_inst_set("sys0=temp_t.y");
	nextion_inst_set("sys1=hum_t.y");
	nextion_inst_set("sys2=co2_t.y");

	temp_fuc.pic     = 4;
	humidity_fuc.pic = 5;
	co2_fuc.pic      = 6;

	for(uint8_t i = 0 ; i < 3 ; i++) {
		menu_fuc.left_menu_y[i] = i;
		menu_fuc.right_menu_y[i] = i;
	}

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
