//embedded systems includes
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include "bit.h"
#include "io.c"
#include "io.h"

//C function includes
#include <stdio.h>
#include <stdlib.h> //rand() function usage

//Misc includes

//Macro defines
//#define UCHAR_P(x) unsigned char * x
//#define UCHAR unsigned char
//#define USHORT unsigned short
typedef int bool;
#define true 1
#define false 0

//EEPROM Macros
#define read_eeprom_word(address) eeprom_read_word ((const uint16_t*)address)
#define write_eeprom_word(address,value) eeprom_write_word ((uint16_t*)address,(uint16_t)value)
#define update_eeprom_word(address,value) eeprom_update_word ((uint16_t*)address,(uint16_t)value)

//EEPROM Array(s)
unsigned char EEMEM highscore[9]; //highscore lookup table
unsigned long EEMEM random_num;

//structs
typedef struct levelInfo{
	unsigned short level_selected; //currently selected level (input)
	unsigned short pit_count; //generated number of pits (RNG())
	unsigned short cell_count; //predefined, see lookup tables
	unsigned short generated_level[16]; //generated number of pits (RNG())
	unsigned short compared_level[16];
	unsigned char curr_time; //timer
	unsigned short lvl_selected;
	unsigned short lvl_feedback;
	unsigned short lvl_started;
	unsigned short lvl_ended;
	unsigned short hs_recorded;
} Info;

//lookup tables
const unsigned short max_pits[9] = { 5, 6, 7, 8, 9, 10, 11, 12, 13 }; //max pits
const unsigned short max_cells[9] = { 6, 7, 8, 9, 10, 11 , 12, 13, 14 }; //max cells

//custom LED display characters
unsigned char pit[8] = {
	0b11111,
	0b11111,
	0b10001,
	0b10001,
	0b10001,
	0b10001,
	0b10001,
	0b10001
};

unsigned char pin[8] = {
	0b01110,
	0b01110,
	0b01110,
	0b01110,
	0b01110,
	0b01110,
	0b00000,
	0b00000
};

unsigned char fill[8] = {
	0b11111,
	0b11111,
	0b11111,
	0b11111,
	0b11111,
	0b11111,
	0b11111,
	0b11111
};

unsigned char empty[8] = {
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000
};

//Functions
//generating pits and blanks
short RNG(){
	return random() % ((1 - 0 + 1) + 0);
}

//current sum of generated_level
unsigned char generated_sum(Info * lvl_info){
	unsigned char i = 0, sum = 0;
	for(i = 0; i < lvl_info->cell_count; i++){
		sum += lvl_info->generated_level[i];
	}
	return sum;
}

//current sum of compared_level
unsigned char compared_sum(Info * lvl_info){
	unsigned char i = 0, sum = 0;
	for(i = 0; i < lvl_info->cell_count; i++){
		sum += lvl_info->compared_level[i];
	}
	return sum;
}

//display items in the array (testing purposes)
void display_array(Info * lvl_info){
	unsigned char i = 0;
	for(i = 0; i < lvl_info->cell_count; i++){
		LCD_Cursor(i);
		LCD_WriteData(lvl_info->generated_level[i] + '0');
	}
}

//initialize random seed
void init_random(){
	unsigned long state, next_state;

	next_state = state = read_eeprom_word(&random_num);
	srandom(state);

	while (next_state == state)
	next_state = random() + random();
	write_eeprom_word(&random_num, next_state);
}

//function for shift register
void transmit_data(unsigned char data){
	int i;
	for(i= 0; i< 8 ; ++i) {
		// Sets SRCLR to 1 allowing data to be set
		// Also clears SRCLK in preparation of sending data
		PORTA = 0x08;
		// set SER = next bit of data to be sent.
		PORTA|= ((data>> i) & 0x01);
		// set SRCLK = 1. Rising edge shifts next bit of data into the shift register
		PORTA|= 0x02;
	}
	// set RCLK = 1. Rising edge copies data from “Shift” register to “Storage” register
	PORTA|= 0x04;
	// clears all lines in preparation of a new transmission
	PORTA= 0x00;
}

unsigned char cursor = '0';
unsigned char key;

//--------Task scheduler data structure--------------------
// Struct for Tasks represent a running process in our
// simple real-time operating system.
/*Tasks should have members that include: state, period, a
measurement of elapsed time, and a function pointer.*/
typedef struct _task {
	//Task's current state, period, and the time elapsed
	// since the last tick
	signed char state;
	unsigned long int period;
	unsigned long int elapsedTime;
	//Task tick function
	int (*TickFct)(int, Info*); //pass in the struct as `int (*TickFct)(int, *Info)`?
} task;
task tasks[9];
const unsigned char numTasks=9;

enum keypad_states{SM_1};
int tick_getkeys(int state){
	key = GetKeypadKey();
	return state;
}

//-----------------------------------------------------------------------------------------------
enum key_states{ KEY_INIT, KEY_WAIT, KEY_DOWN, KEY_UP, KEY_ACTION } key_state;

int keypad_tick(int state, Info * lvl_info){
	switch(state) { // Transitions
		case -1:
		state = KEY_INIT;
		break;
		
		case KEY_INIT:
		state = KEY_WAIT;
		break;

		case KEY_WAIT:
		if (key == '8' && cursor > '0') {
			state = KEY_UP;
			cursor--;
		}
		else if (key == '5' && cursor < '3') {
			state = KEY_DOWN;
			cursor++;
		}
		else if (key == '2' || key == '0') {
			state = KEY_ACTION;
		}
		else{
			state = KEY_WAIT;
		}
		break;
		
		case KEY_DOWN:
		if (key == '5') {
			state = KEY_DOWN;
		}
		else{
			state = KEY_WAIT;
		}
		break;
		
		case KEY_UP:
		if (key == '8') {
			state = KEY_UP;
		}
		else{
			state = KEY_WAIT;
		}
		break;
		
		case KEY_ACTION:
		if (key == '2' || key == '0') {
			state = KEY_ACTION;
		}
		else{
			state = KEY_WAIT;
		}
		break;
		
		default:
		state = KEY_INIT;
	} // Transitions

	switch(state) { // State actions
		case KEY_INIT:
		cursor = '0';
		break;
		
		case KEY_WAIT:
		break;
		
		case KEY_DOWN:
		break;
		
		case KEY_UP:
		break;
		
		case KEY_ACTION:
		break;
		
		default:
		break;

	} // State actions
	
	return state;
}

//-----------------------------------------------------------------------------------------------

enum nav_states { INIT, WAIT_MENU, HS_MENU, HS_LEVEL_SELECT, HS_SEARCH, HS1, HS2, HS3, HS4, HS5, HS6, HS7, HS8, HS9, LEVEL_SELECT, LEVEL_SELECT_WAIT, RESET, CONFIRM, CONFIRM_WAIT } nav_state;
int nav_tick(int state1, Info * lvl_info) {

	const char * main_disp[8] = {
		"1. Level Select 2. Highscores", //29, [0]
		"1. Level Select 2. Clear Scores", //31, [1]
		"B. Back", //7, [2]
		"Confirm Reset?  1. YES   2. NO", //30, [3] //NO first due to pressing `1`
		"CLEARED...Press 'B' to Return", //7, [4]
		"Select highscore level: ", //24, [5]
		"Select level to play: ", //22, [6]
		"Highscore for   level   is " //26, [7]
	};
	
	unsigned short i = 0; //iterator to traverse highscore EEPROM
	unsigned char temp = 0;
	unsigned char _highscore = 0;
	
	switch(state1) { // Transitions
		case -1:
		state1 = INIT;
		break;
		
		case INIT:
		if(1){
			state1 = WAIT_MENU;
			LCD_DisplayString(1, main_disp[0]);
		}
		break;

		case WAIT_MENU:
		if(key == '1'){
			state1 = LEVEL_SELECT;
			LCD_DisplayString(1, main_disp[6]);
		}
		else if(key == '2'){
			state1 = HS_MENU;
			LCD_DisplayString(1, main_disp[1]);
		}
		else{
			state1 = WAIT_MENU;
		}
		break;
		
		case HS_MENU:
		if(key == '1'){
			state1 = HS_LEVEL_SELECT;
			LCD_DisplayString(1, main_disp[5]);
		}
		else if(key == '2'){
			state1 = RESET;
			LCD_DisplayString(1, main_disp[3]);
		}
		else if(key == 'B'){
			state1 = WAIT_MENU;
			LCD_DisplayString(1, main_disp[0]);
		}
		else{
			state1 = HS_MENU;
		}
		break;
		
		case HS_LEVEL_SELECT:
		if(key == '1'){
			temp = 1;
			LCD_DisplayString(1, main_disp[7]);
			LCD_Cursor(23);
			LCD_WriteData(temp + '0');
			LCD_Cursor(28);
			_highscore = read_eeprom_word(&highscore[temp]);
			LCD_WriteData(_highscore / 10 + '0');
			LCD_WriteData(_highscore % 10 + '0');

			state1 = HS_SEARCH;
		}
		if(key == '2'){
			temp = 2;
			LCD_DisplayString(1, main_disp[7]);
			LCD_Cursor(23);
			LCD_WriteData(temp + '0');
			LCD_Cursor(28);
			_highscore = read_eeprom_word(&highscore[temp]);
			LCD_WriteData(_highscore / 10 + '0');
			LCD_WriteData(_highscore % 10 + '0');
			
			state1 = HS_SEARCH;
		}
		if(key == '3'){
			temp = 3;
			LCD_DisplayString(1, main_disp[7]);
			LCD_Cursor(23);
			LCD_WriteData(temp + '0');
			LCD_Cursor(28);
			_highscore = read_eeprom_word(&highscore[temp]);
			LCD_WriteData(_highscore / 10 + '0');
			LCD_WriteData(_highscore % 10 + '0');
			
			state1 = HS_SEARCH;
		}
		if(key == '4'){
			temp = 4;
			LCD_DisplayString(1, main_disp[7]);
			LCD_Cursor(23);
			LCD_WriteData(temp + '0');
			LCD_Cursor(28);
			_highscore = read_eeprom_word(&highscore[temp]);
			LCD_WriteData(_highscore / 10 + '0');
			LCD_WriteData(_highscore % 10 + '0');
			
			state1 = HS_SEARCH;
		}
		if(key == '5'){
			temp = 5;
			LCD_DisplayString(1, main_disp[7]);
			LCD_Cursor(23);
			LCD_WriteData(temp + '0');
			LCD_Cursor(28);
			_highscore = read_eeprom_word(&highscore[temp]);
			LCD_WriteData(_highscore / 10 + '0');
			LCD_WriteData(_highscore % 10 + '0');
			
			state1 = HS_SEARCH;
		}
		if(key == '6'){
			temp = 6;
			LCD_DisplayString(1, main_disp[7]);
			LCD_Cursor(23);
			LCD_WriteData(temp + '0');
			LCD_Cursor(28);
			_highscore = read_eeprom_word(&highscore[temp]);
			LCD_WriteData(_highscore / 10 + '0');
			LCD_WriteData(_highscore % 10 + '0');
			
			state1 = HS_SEARCH;
		}
		if(key == '7'){
			temp = 7;
			LCD_DisplayString(1, main_disp[7]);
			LCD_Cursor(23);
			LCD_WriteData(temp + '0');
			LCD_Cursor(28);
			_highscore = read_eeprom_word(&highscore[temp]);
			LCD_WriteData(_highscore / 10 + '0');
			LCD_WriteData(_highscore % 10 + '0');
			
			state1 = HS_SEARCH;
		}
		if(key == '8'){
			temp = 8;
			LCD_DisplayString(1, main_disp[7]);
			LCD_Cursor(23);
			LCD_WriteData(temp + '0');
			LCD_Cursor(28);
			_highscore = read_eeprom_word(&highscore[temp]);
			LCD_WriteData(_highscore / 10 + '0');
			LCD_WriteData(_highscore % 10 + '0');
			
			state1 = HS_SEARCH;
		}
		if(key == '9'){
			temp = 9;
			LCD_DisplayString(1, main_disp[7]);
			LCD_Cursor(23);
			LCD_WriteData(temp + '0');
			LCD_Cursor(28);
			_highscore = read_eeprom_word(&highscore[temp]);
			LCD_WriteData(_highscore / 10 + '0');
			LCD_WriteData(_highscore % 10 + '0');
			
			state1 = HS_SEARCH;
		}
		break;

		case HS_SEARCH:
		if(key == 'B'){
			state1 = HS_MENU;
			LCD_DisplayString(1, main_disp[1]);
		}
		else{
			//"Highscore for   level   is "
			state1 = HS_SEARCH;
		}
		break;
		
		case LEVEL_SELECT:
		if(key == '1'){
			lvl_info->level_selected = 1;
			lvl_info->pit_count = max_pits[lvl_info->level_selected - 1];
			lvl_info->cell_count = max_cells[lvl_info->level_selected - 1];
			lvl_info->lvl_selected = 1;
			state1 = LEVEL_SELECT_WAIT;
		}
		else if(key == '2'){
			lvl_info->level_selected = 2;
			lvl_info->pit_count = max_pits[lvl_info->level_selected - 1];
			lvl_info->cell_count = max_cells[lvl_info->level_selected - 1];
			lvl_info->lvl_selected = 1;
			state1 = LEVEL_SELECT_WAIT;
		}
		else if(key == '3'){
			lvl_info->level_selected = 3;
			lvl_info->pit_count = max_pits[lvl_info->level_selected - 1];
			lvl_info->cell_count = max_cells[lvl_info->level_selected - 1];
			lvl_info->lvl_selected = 1;
			state1 = LEVEL_SELECT_WAIT;
		}
		else if(key == '4'){
			lvl_info->level_selected = 4;
			lvl_info->pit_count = max_pits[lvl_info->level_selected - 1];
			lvl_info->cell_count = max_cells[lvl_info->level_selected - 1];
			lvl_info->lvl_selected = 1;
			state1 = LEVEL_SELECT_WAIT;
		}
		else if(key == '5'){
			lvl_info->level_selected = 5;
			lvl_info->pit_count = max_pits[lvl_info->level_selected - 1];
			lvl_info->cell_count = max_cells[lvl_info->level_selected - 1];
			lvl_info->lvl_selected = 1;
			state1 = LEVEL_SELECT_WAIT;
		}
		else if(key == '6'){
			lvl_info->level_selected = 6;
			lvl_info->pit_count = max_pits[lvl_info->level_selected - 1];
			lvl_info->cell_count = max_cells[lvl_info->level_selected - 1];
			lvl_info->lvl_selected = 1;
			state1 = LEVEL_SELECT_WAIT;
		}
		else if(key == '7'){
			lvl_info->level_selected = 7;
			lvl_info->pit_count = max_pits[lvl_info->level_selected - 1];
			lvl_info->cell_count = max_cells[lvl_info->level_selected - 1];
			lvl_info->lvl_selected = 1;
			state1 = LEVEL_SELECT_WAIT;
		}
		else if(key == '8'){
			lvl_info->level_selected = 8;
			lvl_info->pit_count = max_pits[lvl_info->level_selected - 1];
			lvl_info->cell_count = max_cells[lvl_info->level_selected - 1];
			lvl_info->lvl_selected = 1;
			state1 = LEVEL_SELECT_WAIT;
		}
		else if(key == '9'){
			lvl_info->level_selected = 9;
			lvl_info->pit_count = max_pits[lvl_info->level_selected - 1];
			lvl_info->cell_count = max_cells[lvl_info->level_selected - 1];
			lvl_info->lvl_selected = 1;
			state1 = LEVEL_SELECT_WAIT;
		}
		else{
			state1 = LEVEL_SELECT;
		}
		break;

		case LEVEL_SELECT_WAIT:
		if(lvl_info->lvl_selected >= 1){
			state1 = LEVEL_SELECT_WAIT;
		}
		else{
			lvl_info->level_selected = 0;
			lvl_info->pit_count = 0;
			lvl_info->cell_count = 0;
			for(i = 0; i < 16; i++){
				lvl_info->generated_level[i] = 0;
				lvl_info->compared_level[i] = 0;
			}
			lvl_info->lvl_selected = 0;
			lvl_info->lvl_feedback = 0;
			lvl_info->lvl_started = 0;
			lvl_info->lvl_ended = 0;
			lvl_info->hs_recorded = 0;
			lvl_info->curr_time = 0;
			LCD_DisplayString(1, main_disp[0]);
			state1 = WAIT_MENU;
		}
		break;
		
		case RESET:
		if (key == '1') {
			state1 = CONFIRM;
		}
		else if (key == '2') {
			state1 = HS_MENU;
			LCD_DisplayString(1, main_disp[1]);
			cursor = '0';
		}
		else{
			state1 = RESET;
		}
		break;
		
		case CONFIRM:
		if (1) {
			state1 = CONFIRM_WAIT;
			LCD_DisplayString(1, main_disp[4]);
		}
		break;
		
		case CONFIRM_WAIT:
		if(key == 'B'){
			state1 = WAIT_MENU;
			LCD_DisplayString(1, main_disp[0]);
		}
		else{
			state1 = CONFIRM_WAIT;
		}
		break;
		
		default:
		state1 = INIT;
	} // Transitions

	switch(state1) { // State actions
		case INIT:
		break;
		
		case WAIT_MENU:
		break;
		
		case HS_MENU:
		break;

		case HS_LEVEL_SELECT:
		break;
		
		case HS_SEARCH:
		break;
		
		case LEVEL_SELECT:
		break;

		case LEVEL_SELECT_WAIT:
		break;
		
		case RESET:
		break;
		
		case CONFIRM:
		for(i = 0; i < 8; i++){
			write_eeprom_word(&highscore[i], 0);
		}
		break;
		
		case CONFIRM_WAIT:
		for(i = 0; i < 8; i++){
			write_eeprom_word(&highscore[i], 0);
		}
		break;
		
		default: // ADD default behaviour below
		break;

	} // State actions
	return state1;
}

//-----------------------------------------------------------------------------------------------
enum level_states { GEN_INIT, LEVEL_GEN, LEVEL_GEN_WAIT, HS_INIT, REC_HS } level_state;
int level_tick(int state, Info * lvl_info) {
	unsigned short i = 0, temp = 0;

	switch(state) { // Transitions
		case -1:
		state = GEN_INIT;
		break;

		case GEN_INIT:
		if (lvl_info->lvl_selected == 1){
			lvl_info->lvl_selected = 2;
			state = LEVEL_GEN;
		}
		else{
			state = GEN_INIT;
		}
		break;

		case LEVEL_GEN:
		if(lvl_info->lvl_selected == 2){
			//generate the level, display on LCD
			while(i < lvl_info->cell_count && lvl_info->pit_count > 0){
				temp = RNG();
				if(temp == 1){
					lvl_info->pit_count--;
					LCD_Build(0,pit);
					LCD_Cursor(i);
					LCD_WriteData(0x00);
				}
				lvl_info->generated_level[i] = temp;
				lvl_info->compared_level[i] = temp;
				i++;
			}
			lvl_info->lvl_started = 1;
		}
		if(lvl_info->pit_count >= (lvl_info->cell_count / 2)){
			state = LEVEL_GEN; //restart due to too few pits
		}
		else{
			lvl_info->lvl_started = 1;
			state = LEVEL_GEN_WAIT; //testing
		}
		break;
		
		case LEVEL_GEN_WAIT:
		if(lvl_info->lvl_started == 1 || lvl_info->lvl_ended != 1){
			lvl_info->lvl_started = 3;
			state = LEVEL_GEN_WAIT;
		}
		else if(lvl_info->lvl_ended == 1){
			state = HS_INIT;
		}
		else{
			state = LEVEL_GEN_WAIT;
		}
		break;
		
		case HS_INIT:
		if(read_eeprom_word(&highscore[lvl_info->level_selected] > lvl_info->curr_time)){
			state = REC_HS;
			LCD_DisplayString(1, "New Highscore!");
		}
		else{
			lvl_info->lvl_selected = 0;
			lvl_info->lvl_started = 0;
			lvl_info->lvl_ended = 0;
			state = GEN_INIT;
		}
		break;

		case REC_HS:
		write_eeprom_word(&highscore[lvl_info->level_selected], lvl_info->curr_time);
		lvl_info->lvl_selected = 0;
		lvl_info->lvl_started = 0;
		lvl_info->lvl_ended = 0;
		state = GEN_INIT;
		break;

		default:
		state = GEN_INIT;
	} // Transitions

	switch(state) { // State actions
		case GEN_INIT:
		break;

		case LEVEL_GEN:
		LCD_ClearScreen();
		break;
		
		case LEVEL_GEN_WAIT:
		break;

		case HS_INIT:
		break;

		case REC_HS:
		break;

		default: // ADD default behaviour below
		break;

	} // State actions
	return state;
}
//-----------------------------------------------------------------------------------------------

enum easylvl_states { E_INIT, E_PLAY, E_WAIT } easylvl_state;
int easylvl_tick(int state, Info * lvl_info) {
	
	static unsigned char j;
	//static unsigned char sum1 = 0, sum2 = 0;

	switch(state) { // Transitions
		case -1:
		state = E_INIT;
		break;

		
		case E_INIT:
		if ((lvl_info->lvl_started == 3) && (lvl_info->level_selected < 3) && (lvl_info->lvl_ended == 0)){
			j = 0;
			state = E_PLAY;
		}
		else{
			state = E_INIT;
		}
		break;

		
		case E_PLAY:
		if(j >= max_cells[lvl_info->level_selected - 1]){
			j = 0;
			LCD_Build(3,empty);
			LCD_Cursor(max_cells[lvl_info->level_selected - 1] + 16);
			LCD_WriteData(0x03);
		}
		
		LCD_Build(3,empty);
		LCD_Cursor(j+16);
		LCD_WriteData(0x03);
		
		LCD_Build(2,pin);
		LCD_Cursor(j+17);
		LCD_WriteData(0x02);
		
		if(key == '1' || key == '2' || key == '3' || key == '4' || key == '5' || key == '6' || key == '7' || key == '8' || key == '9' || key == '0' || key == '*' || key == '#'){
			if(lvl_info->generated_level[j] == 1){
				lvl_info->generated_level[j] = 2;
				LCD_Build(1,fill);
				LCD_Cursor(j);
				LCD_WriteData(0x01);
			}
			else if(lvl_info->generated_level[j] == 2){
				lvl_info->generated_level[j] = 1;
				LCD_Build(0,pit);
				LCD_Cursor(j);
				LCD_WriteData(0x00);
			}
		}
		else if(key == 'B' || ((generated_sum(lvl_info)-1) == ((compared_sum(lvl_info)-1)*2)) || (lvl_info->curr_time > 90)){
			lvl_info->lvl_ended = 1;
			
			//sum1 = generated_sum(lvl_info);
			//sum2 = compared_sum(lvl_info);
			state = E_WAIT;
		}
		else{
			j++;
			state = E_PLAY;
		}
		break;

		
		case E_WAIT:
		if(lvl_info->lvl_ended == 1){
			state = E_WAIT;
		}

		
		default:
		state = E_INIT;
		break;

	} // Transitions

	switch(state) { // State actions
		case E_INIT:
		break;
		
		case E_PLAY:
		break;
		
		case E_WAIT:
		break;
		
		default: // ADD default behaviour below
		break;

		
	} // State actions
	return state;
}
//-----------------------------------------------------------------------------------------------

enum medlvl_states { M_INIT, M_PLAY, M_WAIT } medlvl_state;
int medlvl_tick(int state, Info * lvl_info) {
	
	static unsigned char j;
	//static unsigned char sum1 = 0, sum2 = 0;

	switch(state) { // Transitions
		case -1:
		state = M_INIT;
		break;

		
		case M_INIT:
		if ((lvl_info->lvl_started == 3) && (lvl_info->level_selected >= 3) && (lvl_info->level_selected <= 4) && (lvl_info->lvl_ended == 0)){
			j = 0;
			state = M_PLAY;
		}
		else{
			state = M_INIT;
		}
		break;

		
		case M_PLAY:
		if(j >= max_cells[lvl_info->level_selected - 1]){
			j = 0;
			LCD_Build(3,empty);
			LCD_Cursor(max_cells[lvl_info->level_selected - 1] + 16);
			LCD_WriteData(0x03);
		}
		
		LCD_Build(3,empty);
		LCD_Cursor(j+16);
		LCD_WriteData(0x03);
		
		LCD_Build(2,pin);
		LCD_Cursor(j+17);
		LCD_WriteData(0x02);
		
		if(key == '1' || key == '2' || key == '3' || key == '4' || key == '5' || key == '6' || key == '7' || key == '8' || key == '9' || key == '0' || key == '*' || key == '#'){
			if(lvl_info->generated_level[j] == 1){
				lvl_info->generated_level[j] = 2;
				LCD_Build(1,fill);
				LCD_Cursor(j);
				LCD_WriteData(0x01);
			}
			else if(lvl_info->generated_level[j] == 2){
				lvl_info->generated_level[j] = 1;
				LCD_Build(0,pit);
				LCD_Cursor(j);
				LCD_WriteData(0x00);
			}
		}
		else if(key == 'B' || ((generated_sum(lvl_info)-1) == ((compared_sum(lvl_info)-1)*2)) || (lvl_info->curr_time > 90)){
			lvl_info->lvl_ended = 1;
			
			//sum1 = generated_sum(lvl_info);
			//sum2 = compared_sum(lvl_info);
			state = M_WAIT;
		}
		else{
			j++;
			state = M_PLAY;
		}
		break;

		
		case M_WAIT:
		if(lvl_info->lvl_ended == 1){
			state = M_WAIT;
		}

		
		default:
		state = M_INIT;
		break;

	} // Transitions

	switch(state) { // State actions
		case M_INIT:
		break;
		
		case M_PLAY:
		break;
		
		case M_WAIT:
		break;
		
		default: // ADD default behaviour below
		break;

		
	} // State actions
	return state;
}
//-----------------------------------------------------------------------------------------------

enum hardlvl_states { H_INIT, H_PLAY, H_WAIT } hardlvl_state;
int hardlvl_tick(int state, Info * lvl_info) {
	
	static unsigned char j;
	//static unsigned char sum1 = 0, sum2 = 0;

	switch(state) { // Transitions
		case -1:
		state = H_INIT;
		break;

		
		case H_INIT:
		if ((lvl_info->lvl_started == 3) && (lvl_info->level_selected >= 5) && (lvl_info->level_selected <= 6) && (lvl_info->lvl_ended == 0)){
			j = 0;
			state = H_PLAY;
		}
		else{
			state = H_INIT;
		}
		break;

		
		case H_PLAY:
		if(j >= max_cells[lvl_info->level_selected - 1]){
			j = 0;
			LCD_Build(3,empty);
			LCD_Cursor(max_cells[lvl_info->level_selected - 1] + 16);
			LCD_WriteData(0x03);
		}
		
		LCD_Build(3,empty);
		LCD_Cursor(j+16);
		LCD_WriteData(0x03);
		
		LCD_Build(2,pin);
		LCD_Cursor(j+17);
		LCD_WriteData(0x02);
		
		if(key == '1' || key == '2' || key == '3' || key == '4' || key == '5' || key == '6' || key == '7' || key == '8' || key == '9' || key == '0' || key == '*' || key == '#'){
			if(lvl_info->generated_level[j] == 1){
				lvl_info->generated_level[j] = 2;
				LCD_Build(1,fill);
				LCD_Cursor(j);
				LCD_WriteData(0x01);
			}
			else if(lvl_info->generated_level[j] == 2){
				lvl_info->generated_level[j] = 1;
				LCD_Build(0,pit);
				LCD_Cursor(j);
				LCD_WriteData(0x00);
			}
		}
		else if(key == 'B' || ((generated_sum(lvl_info)-1) == ((compared_sum(lvl_info)-1)*2)) || (lvl_info->curr_time > 90)){
			lvl_info->lvl_ended = 1;
			
			//sum1 = generated_sum(lvl_info);
			//sum2 = compared_sum(lvl_info);
			state = H_WAIT;
		}
		else{
			j++;
			state = H_PLAY;
		}
		break;

		
		case H_WAIT:
		if(lvl_info->lvl_ended == 1){
			state = H_WAIT;
		}
		
		default:
		state = H_INIT;
		break;

	} // Transitions

	switch(state) { // State actions
		case H_INIT:
		break;
		
		case H_PLAY:
		break;
		
		case H_WAIT:
		break;
		
		default: // ADD default behaviour below
		break;
		
	} // State actions
	return state;
}
//-----------------------------------------------------------------------------------------------

enum exlvl_states { X_INIT, X_PLAY, X_WAIT } exlvl_state;
int exlvl_tick(int state, Info * lvl_info) {
	
	static unsigned char j;
	//static unsigned char sum1 = 0, sum2 = 0;

	switch(state) { // Transitions
		case -1:
		state = X_INIT;
		break;
		
		case X_INIT:
		if ((lvl_info->lvl_started == 3) && (lvl_info->level_selected >= 7) && (lvl_info->level_selected <= 9) && (lvl_info->lvl_ended == 0)){
			j = 0;
			state = X_PLAY;
		}
		else{
			state = X_INIT;
		}
		break;
		
		case X_PLAY:
		if(j >= max_cells[lvl_info->level_selected - 1]){
			j = 0;
			LCD_Build(3,empty);
			LCD_Cursor(max_cells[lvl_info->level_selected - 1] + 16);
			LCD_WriteData(0x03);
		}
		
		LCD_Build(3,empty);
		LCD_Cursor(j+16);
		LCD_WriteData(0x03);
		
		LCD_Build(2,pin);
		LCD_Cursor(j+17);
		LCD_WriteData(0x02);
		
		if(key == '1' || key == '2' || key == '3' || key == '4' || key == '5' || key == '6' || key == '7' || key == '8' || key == '9' || key == '0' || key == '*' || key == '#'){
			if(lvl_info->generated_level[j] == 1){
				lvl_info->generated_level[j] = 2;
				LCD_Build(1,fill);
				LCD_Cursor(j);
				LCD_WriteData(0x01);
			}
			else if(lvl_info->generated_level[j] == 2){
				lvl_info->generated_level[j] = 1;
				LCD_Build(0,pit);
				LCD_Cursor(j);
				LCD_WriteData(0x00);
			}
		}
		else if(key == 'B' || ((generated_sum(lvl_info)-1) == ((compared_sum(lvl_info)-1)*2)) || (lvl_info->curr_time > 90)){
			lvl_info->lvl_ended = 1;
			state = X_WAIT;
		}
		else{
			j++;
			state = X_PLAY;
		}
		break;
		
		case X_WAIT:
		if(lvl_info->lvl_ended == 1){
			state = X_WAIT;
		}
		
		default:
		state = X_INIT;
		break;

	} // Transitions

	switch(state) { // State actions
		case X_INIT:
		break;
		
		case X_PLAY:
		break;
		
		case X_WAIT:
		break;
		
		default: // ADD default behaviour below
		break;

		
	} // State actions
	return state;
}
//-----------------------------------------------------------------------------------------------

enum time_states { TIME_INIT, START_TIME, STOP_TIME } time_state;
int time_tick(int state, Info * lvl_info) {
	unsigned char temp;
	
	switch(state) { // Transitions
		case -1:
		state = TIME_INIT;
		break;
		
		case TIME_INIT:
		if (lvl_info->lvl_started == 3 && (key == '1' || key == '2' || key == '3' || key == '4' || key == '5' || key == '6' || key == '7' || key == '8' || key == '9' || key == '0' || key == '*' || key == '#')){
			state = START_TIME;
		}
		else{
			state = TIME_INIT;
		}
		break;
		
		case START_TIME:
		if(lvl_info->lvl_ended == 0 && lvl_info->lvl_started >= 2 && (lvl_info->curr_time < 90)){
			lvl_info->curr_time++;
			temp = lvl_info->curr_time;
			LCD_Cursor(15);
			LCD_WriteData(temp / 10 + '0');
			LCD_WriteData(temp % 10 + '0');
			state = START_TIME;
		}
		else{
			state = STOP_TIME;
		}
		break;
		
		case STOP_TIME:
		if(lvl_info->lvl_ended == 1 || (lvl_info->curr_time > 90)){
			state = STOP_TIME;
			lvl_info->curr_time = 0;
		}
		else if(lvl_info->lvl_ended == 0 || (lvl_info->curr_time == 0) || (lvl_info->lvl_selected == 0)){
			state = TIME_INIT;
		}
		else{ state = STOP_TIME; }
		break;
		
		default:
		state = TIME_INIT;
		break;

	} // Transitions

	switch(state) { // State actions
		case TIME_INIT:
		break;
		
		case START_TIME:
		break;
		
		case STOP_TIME:
		break;
		
		default: // ADD default behaviour below
		break;

		
	} // State actions
	return state;
}
//-----------------------------------------------------------------------------------------------

// --------END User defined FSMs---------------------------// Implement scheduler code from PES.
int main(void){
	//Initialize ports
	DDRA = 0xFF; PORTA = 0x00; // LCD
	DDRB = 0xFF; PORTB = 0x00; // 7SEG
	DDRD = 0xFF; PORTD = 0x00; // LCD
	DDRC = 0xF0; PORTC = 0x0F; // KEYPAD
	
	//Initialize struct
	Info lvl_info;
	
	//Initialize LCD Display
	LCD_init();
	LCD_ClearScreen();
	
	//Initialize RNGeezus
	init_random();
	
	unsigned char i=0;
	
	tasks[i].state = SM_1;
	tasks[i].period = 2;
	tasks[i].elapsedTime = 1;
	tasks[i].TickFct = &tick_getkeys;
	++i;
	
	tasks[i].state = KEY_INIT;
	tasks[i].period = 2;
	tasks[i].elapsedTime = 1;
	tasks[i].TickFct = &keypad_tick;
	++i;
	
	tasks[i].state = INIT;
	tasks[i].period = 4;
	tasks[i].elapsedTime = 1;
	tasks[i].TickFct = &nav_tick;
	++i;

	tasks[i].state = GEN_INIT;
	tasks[i].period = 4;
	tasks[i].elapsedTime = 1;
	tasks[i].TickFct = &level_tick;
	++i;

	tasks[i].state = E_INIT;
	tasks[i].period = 20;
	tasks[i].elapsedTime = 1;
	tasks[i].TickFct = &easylvl_tick;
	++i;
	
	tasks[i].state = M_INIT;
	tasks[i].period = 15;
	tasks[i].elapsedTime = 1;
	tasks[i].TickFct = &medlvl_tick;
	++i;
	
	tasks[i].state = H_INIT;
	tasks[i].period = 10;
	tasks[i].elapsedTime = 1;
	tasks[i].TickFct = &hardlvl_tick;
	++i;
	
	tasks[i].state = X_INIT;
	tasks[i].period = 5;
	tasks[i].elapsedTime = 1;
	tasks[i].TickFct = &exlvl_tick;
	++i;
	
	tasks[i].state = TIME_INIT;
	tasks[i].period = 40;
	tasks[i].elapsedTime = 1;
	tasks[i].TickFct = &time_tick;

	LCD_init();
	
	PWM_on();
	TimerOn();
	
	TimerSet(25);
	TimerOn();

	while(1)
	{
		for (i = 0; i < numTasks; i++ ) {
			// Task is ready to tick
			if ( tasks[i].elapsedTime ==
			tasks[i].period ) {
				// Setting next state for task
				tasks[i].state =
				tasks[i].TickFct(tasks[i].state, &lvl_info);
				// Reset elapsed time for next tick.
				tasks[i].elapsedTime = 0;
			}
			tasks[i].elapsedTime += 1;
		}
		while(!TimerFlag);
		TimerFlag = 0;
	}
}
