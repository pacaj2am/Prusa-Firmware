#include "temperature.h"
#include "ultralcd.h"
#ifdef ULTRA_LCD
#include "Marlin.h"
#include "language.h"
#include "cardreader.h"
#include "temperature.h"
#include "stepper.h"
#include "ConfigurationStore.h"
#include <string.h>
//#include "Configuration.h"



#define _STRINGIFY(s) #s


int8_t encoderDiff; /* encoderDiff is updated from interrupt context and added to encoderPosition every LCD update */

extern int lcd_change_fil_state;

int babystepMem[3];
float babystepMemMM[3];

union Data
{
  byte b[2];
  int value;
};

int8_t ReInitLCD = 0;

int8_t SDscrool = 0;

int8_t SilentModeMenu = 0;

int lcd_commands_type=0;
int lcd_commands_step=0;
bool isPrintPaused = false;

/* Configuration settings */
int plaPreheatHotendTemp;
int plaPreheatHPBTemp;
int plaPreheatFanSpeed;

int absPreheatHotendTemp;
int absPreheatHPBTemp;
int absPreheatFanSpeed;

int ppPreheatHotendTemp = PP_PREHEAT_HOTEND_TEMP;
int ppPreheatHPBTemp = PP_PREHEAT_HPB_TEMP;
int ppPreheatFanSpeed = PP_PREHEAT_FAN_SPEED;

int petPreheatHotendTemp = PET_PREHEAT_HOTEND_TEMP;
int petPreheatHPBTemp = PET_PREHEAT_HPB_TEMP;
int petPreheatFanSpeed = PET_PREHEAT_FAN_SPEED;

int hipsPreheatHotendTemp = HIPS_PREHEAT_HOTEND_TEMP;
int hipsPreheatHPBTemp = HIPS_PREHEAT_HPB_TEMP;
int hipsPreheatFanSpeed = HIPS_PREHEAT_FAN_SPEED;

int flexPreheatHotendTemp = FLEX_PREHEAT_HOTEND_TEMP;
int flexPreheatHPBTemp = FLEX_PREHEAT_HPB_TEMP;
int flexPreheatFanSpeed = FLEX_PREHEAT_FAN_SPEED;



#ifdef FILAMENT_LCD_DISPLAY
unsigned long message_millis = 0;
#endif

#ifdef ULTIPANEL
static float manual_feedrate[] = MANUAL_FEEDRATE;
#endif // ULTIPANEL

/* !Configuration settings */

//Function pointer to menu functions.
typedef void (*menuFunc_t)();

uint8_t lcd_status_message_level;
char lcd_status_message[LCD_WIDTH + 1] = ""; //////WELCOME!
unsigned char firstrun = 1;

#ifdef DOGLCD
#include "dogm_lcd_implementation.h"
#else
#include "ultralcd_implementation_hitachi_HD44780.h"
#endif

/** forward declarations **/

void copy_and_scalePID_i();
void copy_and_scalePID_d();

/* Different menus */
static void lcd_status_screen();
#ifdef ULTIPANEL
extern bool powersupply;
static void lcd_main_menu();
static void lcd_tune_menu();
static void lcd_prepare_menu();
static void lcd_move_menu();
static void lcd_control_menu();
static void lcd_settings_menu();
static void lcd_language_menu();
static void lcd_control_temperature_menu();
static void lcd_control_temperature_preheat_pla_settings_menu();
static void lcd_control_temperature_preheat_abs_settings_menu();
static void lcd_control_motion_menu();
static void lcd_control_volumetric_menu();

#ifdef DOGLCD
static void lcd_set_contrast();
#endif
static void lcd_control_retract_menu();
static void lcd_sdcard_menu();

#ifdef DELTA_CALIBRATION_MENU
static void lcd_delta_calibrate_menu();
#endif // DELTA_CALIBRATION_MENU

static void lcd_quick_feedback();//Cause an LCD refresh, and give the user visual or audible feedback that something has happened

/* Different types of actions that can be used in menu items. */
static void menu_action_back(menuFunc_t data);
static void menu_action_submenu(menuFunc_t data);
static void menu_action_gcode(const char* pgcode);
static void menu_action_function(menuFunc_t data);
static void menu_action_setlang(unsigned char lang);
static void menu_action_sdfile(const char* filename, char* longFilename);
static void menu_action_sddirectory(const char* filename, char* longFilename);
static void menu_action_setting_edit_bool(const char* pstr, bool* ptr);
static void menu_action_setting_edit_int3(const char* pstr, int* ptr, int minValue, int maxValue);
static void menu_action_setting_edit_float3(const char* pstr, float* ptr, float minValue, float maxValue);
static void menu_action_setting_edit_float32(const char* pstr, float* ptr, float minValue, float maxValue);
static void menu_action_setting_edit_float43(const char* pstr, float* ptr, float minValue, float maxValue);
static void menu_action_setting_edit_float5(const char* pstr, float* ptr, float minValue, float maxValue);
static void menu_action_setting_edit_float51(const char* pstr, float* ptr, float minValue, float maxValue);
static void menu_action_setting_edit_float52(const char* pstr, float* ptr, float minValue, float maxValue);
static void menu_action_setting_edit_long5(const char* pstr, unsigned long* ptr, unsigned long minValue, unsigned long maxValue);
static void menu_action_setting_edit_callback_bool(const char* pstr, bool* ptr, menuFunc_t callbackFunc);
static void menu_action_setting_edit_callback_int3(const char* pstr, int* ptr, int minValue, int maxValue, menuFunc_t callbackFunc);
static void menu_action_setting_edit_callback_float3(const char* pstr, float* ptr, float minValue, float maxValue, menuFunc_t callbackFunc);
static void menu_action_setting_edit_callback_float32(const char* pstr, float* ptr, float minValue, float maxValue, menuFunc_t callbackFunc);
static void menu_action_setting_edit_callback_float43(const char* pstr, float* ptr, float minValue, float maxValue, menuFunc_t callbackFunc);
static void menu_action_setting_edit_callback_float5(const char* pstr, float* ptr, float minValue, float maxValue, menuFunc_t callbackFunc);
static void menu_action_setting_edit_callback_float51(const char* pstr, float* ptr, float minValue, float maxValue, menuFunc_t callbackFunc);
static void menu_action_setting_edit_callback_float52(const char* pstr, float* ptr, float minValue, float maxValue, menuFunc_t callbackFunc);
static void menu_action_setting_edit_callback_long5(const char* pstr, unsigned long* ptr, unsigned long minValue, unsigned long maxValue, menuFunc_t callbackFunc);

#define ENCODER_FEEDRATE_DEADZONE 10

#if !defined(LCD_I2C_VIKI)
#ifndef ENCODER_STEPS_PER_MENU_ITEM
#define ENCODER_STEPS_PER_MENU_ITEM 5
#endif
#ifndef ENCODER_PULSES_PER_STEP
#define ENCODER_PULSES_PER_STEP 1
#endif
#else
#ifndef ENCODER_STEPS_PER_MENU_ITEM
#define ENCODER_STEPS_PER_MENU_ITEM 2 // VIKI LCD rotary encoder uses a different number of steps per rotation
#endif
#ifndef ENCODER_PULSES_PER_STEP
#define ENCODER_PULSES_PER_STEP 1
#endif
#endif


/* Helper macros for menus */
#define START_MENU() do { \
    if (encoderPosition > 0x8000) encoderPosition = 0; \
    if (encoderPosition / ENCODER_STEPS_PER_MENU_ITEM < currentMenuViewOffset) currentMenuViewOffset = encoderPosition / ENCODER_STEPS_PER_MENU_ITEM;\
    uint8_t _lineNr = currentMenuViewOffset, _menuItemNr; \
    bool wasClicked = LCD_CLICKED;\
    for(uint8_t _drawLineNr = 0; _drawLineNr < LCD_HEIGHT; _drawLineNr++, _lineNr++) { \
      _menuItemNr = 0;

#define MENU_ITEM(type, label, args...) do { \
    if (_menuItemNr == _lineNr) { \
      if (lcdDrawUpdate) { \
        const char* _label_pstr = (label); \
        if ((encoderPosition / ENCODER_STEPS_PER_MENU_ITEM) == _menuItemNr) { \
          lcd_implementation_drawmenu_ ## type ## _selected (_drawLineNr, _label_pstr , ## args ); \
        }else{\
          lcd_implementation_drawmenu_ ## type (_drawLineNr, _label_pstr , ## args ); \
        }\
      }\
      if (wasClicked && (encoderPosition / ENCODER_STEPS_PER_MENU_ITEM) == _menuItemNr) {\
        lcd_quick_feedback(); \
        menu_action_ ## type ( args ); \
        return;\
      }\
    }\
    _menuItemNr++;\
  } while(0)

#define MENU_ITEM_DUMMY() do { _menuItemNr++; } while(0)
#define MENU_ITEM_EDIT(type, label, args...) MENU_ITEM(setting_edit_ ## type, label, (label) , ## args )
#define MENU_ITEM_EDIT_CALLBACK(type, label, args...) MENU_ITEM(setting_edit_callback_ ## type, label, (label) , ## args )
#define END_MENU() \
  if (encoderPosition / ENCODER_STEPS_PER_MENU_ITEM >= _menuItemNr) encoderPosition = _menuItemNr * ENCODER_STEPS_PER_MENU_ITEM - 1; \
  if ((uint8_t)(encoderPosition / ENCODER_STEPS_PER_MENU_ITEM) >= currentMenuViewOffset + LCD_HEIGHT) { currentMenuViewOffset = (encoderPosition / ENCODER_STEPS_PER_MENU_ITEM) - LCD_HEIGHT + 1; lcdDrawUpdate = 1; _lineNr = currentMenuViewOffset - 1; _drawLineNr = -1; } \
  } } while(0)

/** Used variables to keep track of the menu */
#ifndef REPRAPWORLD_KEYPAD
volatile uint8_t buttons;//Contains the bits of the currently pressed buttons.
#else
volatile uint8_t buttons_reprapworld_keypad; // to store the reprapworld_keypad shift register values
#endif
#ifdef LCD_HAS_SLOW_BUTTONS
volatile uint8_t slow_buttons;//Contains the bits of the currently pressed buttons.
#endif
uint8_t currentMenuViewOffset;              /* scroll offset in the current menu */
uint32_t blocking_enc;
uint8_t lastEncoderBits;
uint32_t encoderPosition;
#if (SDCARDDETECT > 0)
bool lcd_oldcardstatus;
#endif
#endif //ULTIPANEL

menuFunc_t currentMenu = lcd_status_screen; /* function pointer to the currently active menu */
uint32_t lcd_next_update_millis;
uint8_t lcd_status_update_delay;
bool ignore_click = false;
bool wait_for_unclick;
uint8_t lcdDrawUpdate = 2;                  /* Set to none-zero when the LCD needs to draw, decreased after every draw. Set to 2 in LCD routines so the LCD gets at least 1 full redraw (first redraw is partial) */

//prevMenu and prevEncoderPosition are used to store the previous menu location when editing settings.
menuFunc_t prevMenu = NULL;
uint16_t prevEncoderPosition;
//Variables used when editing values.
const char* editLabel;
void* editValue;
int32_t minEditValue, maxEditValue;
menuFunc_t callbackFunc;

// place-holders for Ki and Kd edits
float raw_Ki, raw_Kd;

static void lcd_goto_menu(menuFunc_t menu, const uint32_t encoder = 0, const bool feedback = true) {
  if (currentMenu != menu) {
    currentMenu = menu;
    encoderPosition = encoder;
    if (feedback) lcd_quick_feedback();

    // For LCD_PROGRESS_BAR re-initialize the custom characters
#if defined(LCD_PROGRESS_BAR) && defined(SDSUPPORT)
    lcd_set_custom_characters(menu == lcd_status_screen);
#endif
  }
}

/* Main status screen. It's up to the implementation specific part to show what is needed. As this is very display dependent */
/*
extern char langbuffer[];
void lcd_printPGM(const char *s1) {
  strncpy_P(langbuffer,s1,LCD_WIDTH);
  lcd.print(langbuffer);
}
*/
unsigned char langsel;

void set_language_from_EEPROM() {
  unsigned char eep = eeprom_read_byte((unsigned char*)EEPROM_LANG);
  if (eep < LANG_NUM)
  {
    lang_selected = eep;
    langsel = 0;
  }
  else
  {
    lang_selected = 1;
    langsel = 1;
  }
}

void lcd_mylang();
static void lcd_status_screen()
{
	
  if (firstrun == 1) 
  {
    firstrun = 0;
    set_language_from_EEPROM();
    strncpy_P(lcd_status_message, WELCOME_MSG, LCD_WIDTH);
	
	if (eeprom_read_byte((uint8_t *)EEPROM_TOTALTIME) == 255 && eeprom_read_byte((uint8_t *)EEPROM_TOTALTIME + 1) == 255 && eeprom_read_byte((uint8_t *)EEPROM_TOTALTIME + 2) == 255 && eeprom_read_byte((uint8_t *)EEPROM_TOTALTIME + 3) == 255)
	{
		eeprom_update_dword((uint32_t *)EEPROM_TOTALTIME, 0);
		eeprom_update_dword((uint32_t *)EEPROM_FILAMENTUSED, 0);
	}
	
	if (langsel) {
      //strncpy_P(lcd_status_message, PSTR(">>>>>>>>>>>> PRESS v"), LCD_WIDTH);
      lcd_mylang();
    }
  }

  
  if (lcd_status_update_delay)
    lcd_status_update_delay--;
  else
    lcdDrawUpdate = 1;
  if (lcdDrawUpdate)
  {
    ReInitLCD++;


    if (ReInitLCD == 30) {
      lcd_implementation_init( // to maybe revive the LCD if static electricity killed it.
#if defined(LCD_PROGRESS_BAR) && defined(SDSUPPORT)
        currentMenu == lcd_status_screen
#endif
      );
      ReInitLCD = 0 ;
    } else {

      if ((ReInitLCD % 10) == 0) {
        //lcd_implementation_nodisplay();
        lcd_implementation_init_noclear( // to maybe revive the LCD if static electricity killed it.
#if defined(LCD_PROGRESS_BAR) && defined(SDSUPPORT)
          currentMenu == lcd_status_screen
#endif
        );

      }

    }


    //lcd_implementation_display();
    lcd_implementation_status_screen();
    //lcd_implementation_clear();







    lcd_status_update_delay = 10;   /* redraw the main screen every second. This is easier then trying keep track of all things that change on the screen */
	if (lcd_commands_type != 0)
	{
		lcd_commands();
	}
	

  }
#ifdef ULTIPANEL

  bool current_click = LCD_CLICKED;

  if (ignore_click) {
    if (wait_for_unclick) {
      if (!current_click) {
        ignore_click = wait_for_unclick = false;
      }
      else {
        current_click = false;
      }
    }
    else if (current_click) {
      lcd_quick_feedback();
      wait_for_unclick = true;
      current_click = false;
    }
  }


  //if (--langsel ==0) {langsel=1;current_click=true;}

  if (current_click)
  {

    lcd_goto_menu(lcd_main_menu);
    lcd_implementation_init( // to maybe revive the LCD if static electricity killed it.
#if defined(LCD_PROGRESS_BAR) && defined(SDSUPPORT)
      currentMenu == lcd_status_screen
#endif
    );
#ifdef FILAMENT_LCD_DISPLAY
    message_millis = millis();  // get status message to show up for a while
#endif
  }

#ifdef ULTIPANEL_FEEDMULTIPLY
  // Dead zone at 100% feedrate
  if ((feedmultiply < 100 && (feedmultiply + int(encoderPosition)) > 100) ||
      (feedmultiply > 100 && (feedmultiply + int(encoderPosition)) < 100))
  {
    encoderPosition = 0;
    feedmultiply = 100;
  }

  if (feedmultiply == 100 && int(encoderPosition) > ENCODER_FEEDRATE_DEADZONE)
  {
    feedmultiply += int(encoderPosition) - ENCODER_FEEDRATE_DEADZONE;
    encoderPosition = 0;
  }
  else if (feedmultiply == 100 && int(encoderPosition) < -ENCODER_FEEDRATE_DEADZONE)
  {
    feedmultiply += int(encoderPosition) + ENCODER_FEEDRATE_DEADZONE;
    encoderPosition = 0;
  }
  else if (feedmultiply != 100)
  {
    feedmultiply += int(encoderPosition);
    encoderPosition = 0;
  }
#endif //ULTIPANEL_FEEDMULTIPLY

  if (feedmultiply < 10)
    feedmultiply = 10;
  else if (feedmultiply > 999)
    feedmultiply = 999;
#endif //ULTIPANEL
}

#ifdef ULTIPANEL

void lcd_commands()
{
	if (lcd_commands_type == 1)   //// load filament sequence
	{
		if (lcd_commands_step == 0) { lcd_commands_step = 5; custom_message = true; }
			if (lcd_commands_step == 1 && !blocks_queued())
			{
				lcd_commands_step = 0;
				lcd_commands_type = 0;
				lcd_setstatuspgm(WELCOME_MSG);
				disable_z();
				custom_message = false;
				custom_message_type = 0;
			}
			if (lcd_commands_step == 2 && !blocks_queued())
			{
				lcd_setstatuspgm(MSG_LOADING_FILAMENT);
				enquecommand_P(PSTR(LOAD_FILAMENT_2));
				lcd_commands_step = 1;
			}
			if (lcd_commands_step == 3 && !blocks_queued())
			{
				enquecommand_P(PSTR(LOAD_FILAMENT_1));
				lcd_commands_step = 2;
			}
			if (lcd_commands_step == 4 && !blocks_queued())
			{
				lcd_setstatuspgm(MSG_INSERT_FILAMENT);
				enquecommand_P(PSTR(LOAD_FILAMENT_0));
				lcd_commands_step = 3;
			}
			if (lcd_commands_step == 5 && !blocks_queued())
			{
				lcd_setstatuspgm(MSG_PLEASE_WAIT);
				enable_z();
				custom_message = true;
				custom_message_type = 2;
				lcd_commands_step = 4;
			}

 
		
		
		
	}

	if (lcd_commands_type == 2)   /// stop print
	{

		if (lcd_commands_step == 0) { lcd_commands_step = 6; custom_message = true;	}

		if (lcd_commands_step == 1 && !blocks_queued())
		{
			lcd_commands_step = 0;
			lcd_commands_type = 0;
			lcd_setstatuspgm(WELCOME_MSG);
			custom_message = false;
		}
		if (lcd_commands_step == 2 && !blocks_queued())
		{
			setTargetBed(0);
			setTargetHotend(0, 0);
			setTargetHotend(0, 1);
			setTargetHotend(0, 2);
			manage_heater();
			lcd_setstatuspgm(WELCOME_MSG);
			cancel_heatup = false;
			lcd_commands_step = 1;
		}
		if (lcd_commands_step == 3 && !blocks_queued())
		{
			enquecommand_P(PSTR("M84"));
			autotempShutdown();
			lcd_commands_step = 2;
		}
		if (lcd_commands_step == 4 && !blocks_queued())
		{
			enquecommand_P(PSTR("G90"));
			#ifdef X_CANCEL_POS 
			enquecommand_P(PSTR("G1 X"  STRINGIFY(X_CANCEL_POS) " Y" STRINGIFY(Y_CANCEL_POS) " E0 F7000"));
			#else
			enquecommand_P(PSTR("G1 X50 Y" STRINGIFY(Y_MAX_POS) " E0 F7000"));
			#endif
			lcd_ignore_click(false);
			lcd_commands_step = 3;
		}
		if (lcd_commands_step == 5 && !blocks_queued())
		{
			lcd_setstatuspgm(MSG_PRINT_ABORTED);
			enquecommand_P(PSTR("G91"));
			enquecommand_P(PSTR("G1 Z15 F1500"));
			lcd_commands_step = 4;
		}
		if (lcd_commands_step == 6 && !blocks_queued())
		{
			lcd_setstatuspgm(MSG_PRINT_ABORTED);
			cancel_heatup = true;
			setTargetBed(0);
			setTargetHotend(0, 0);
			setTargetHotend(0, 1);
			setTargetHotend(0, 2);
			manage_heater();
			lcd_commands_step = 5;
		}

	}

	if (lcd_commands_type == 3)
	{
		lcd_commands_type = 0;
	}

}

static void lcd_return_to_status() {
  lcd_implementation_init( // to maybe revive the LCD if static electricity killed it.
#if defined(LCD_PROGRESS_BAR) && defined(SDSUPPORT)
    currentMenu == lcd_status_screen
#endif
  );

    lcd_goto_menu(lcd_status_screen, 0, false);
}

static void lcd_sdcard_pause() {
  card.pauseSDPrint();
  isPrintPaused = true;
  lcdDrawUpdate = 3;
}

static void lcd_sdcard_resume() {
	card.startFileprint();
	isPrintPaused = false;
	lcdDrawUpdate = 3;
}

float move_menu_scale;
static void lcd_move_menu_axis();



/* Menu implementation */


void lcd_preheat_pla()
{
  setTargetHotend0(plaPreheatHotendTemp);
  setTargetBed(plaPreheatHPBTemp);
  fanSpeed = 0;
  lcd_return_to_status();
  setWatch(); // heater sanity check timer
}

void lcd_preheat_abs()
{
  setTargetHotend0(absPreheatHotendTemp);
  setTargetBed(absPreheatHPBTemp);
  fanSpeed = 0;
  lcd_return_to_status();
  setWatch(); // heater sanity check timer
}

void lcd_preheat_pp()
{
  setTargetHotend0(ppPreheatHotendTemp);
  setTargetBed(ppPreheatHPBTemp);
  fanSpeed = 0;
  lcd_return_to_status();
  setWatch(); // heater sanity check timer
}

void lcd_preheat_pet()
{
  setTargetHotend0(petPreheatHotendTemp);
  setTargetBed(petPreheatHPBTemp);
  fanSpeed = 0;
  lcd_return_to_status();
  setWatch(); // heater sanity check timer
}

void lcd_preheat_hips()
{
  setTargetHotend0(hipsPreheatHotendTemp);
  setTargetBed(hipsPreheatHPBTemp);
  fanSpeed = 0;
  lcd_return_to_status();
  setWatch(); // heater sanity check timer
}

void lcd_preheat_flex()
{
  setTargetHotend0(flexPreheatHotendTemp);
  setTargetBed(flexPreheatHPBTemp);
  fanSpeed = 0;
  lcd_return_to_status();
  setWatch(); // heater sanity check timer
}


void lcd_cooldown()
{
  setTargetHotend0(0);
  setTargetHotend1(0);
  setTargetHotend2(0);
  setTargetBed(0);
  fanSpeed = 0;
  lcd_return_to_status();
}



static void lcd_preheat_menu()
{
  START_MENU();

  MENU_ITEM(back, MSG_MAIN, lcd_main_menu);

  MENU_ITEM(function, PSTR("ABS  -  " STRINGIFY(ABS_PREHEAT_HOTEND_TEMP) "/" STRINGIFY(ABS_PREHEAT_HPB_TEMP)), lcd_preheat_abs);
  MENU_ITEM(function, PSTR("PLA  -  " STRINGIFY(PLA_PREHEAT_HOTEND_TEMP) "/" STRINGIFY(PLA_PREHEAT_HPB_TEMP)), lcd_preheat_pla);
  MENU_ITEM(function, PSTR("PET  -  " STRINGIFY(PET_PREHEAT_HOTEND_TEMP) "/" STRINGIFY(PET_PREHEAT_HPB_TEMP)), lcd_preheat_pet);
  MENU_ITEM(function, PSTR("HIPS -  " STRINGIFY(HIPS_PREHEAT_HOTEND_TEMP) "/" STRINGIFY(HIPS_PREHEAT_HPB_TEMP)), lcd_preheat_hips);
  MENU_ITEM(function, PSTR("PP   -  " STRINGIFY(PP_PREHEAT_HOTEND_TEMP) "/" STRINGIFY(PP_PREHEAT_HPB_TEMP)), lcd_preheat_pp);
  MENU_ITEM(function, PSTR("FLEX -  " STRINGIFY(FLEX_PREHEAT_HOTEND_TEMP) "/" STRINGIFY(FLEX_PREHEAT_HPB_TEMP)), lcd_preheat_flex);

  MENU_ITEM(function, MSG_COOLDOWN, lcd_cooldown);

  END_MENU();
}

static void lcd_support_menu()
{
  START_MENU();

  MENU_ITEM(back, MSG_MAIN, lcd_main_menu);

  MENU_ITEM(back, PSTR(MSG_FW_VERSION " - " FW_version), lcd_main_menu);
  MENU_ITEM(back, MSG_PRUSA3D, lcd_main_menu);
  MENU_ITEM(back, MSG_PRUSA3D_FORUM, lcd_main_menu);
  MENU_ITEM(back, MSG_PRUSA3D_HOWTO, lcd_main_menu);
  MENU_ITEM(back, PSTR("------------"), lcd_main_menu);
  MENU_ITEM(back, PSTR(FILAMENT_SIZE), lcd_main_menu);
  MENU_ITEM(back, PSTR(ELECTRONICS),lcd_main_menu);
  MENU_ITEM(back, PSTR(NOZZLE_TYPE),lcd_main_menu);
  MENU_ITEM(back, PSTR("------------"), lcd_main_menu);
  MENU_ITEM(back, PSTR("Date: "), lcd_main_menu);
  MENU_ITEM(back, PSTR(__DATE__), lcd_main_menu);

  END_MENU();
}

void lcd_unLoadFilament()
{

  if (degHotend0() > EXTRUDE_MINTEMP) {

    enquecommand_P(PSTR(UNLOAD_FILAMENT_0));
    enquecommand_P(PSTR(UNLOAD_FILAMENT_1));

  } else {

    lcd_implementation_clear();
    lcd.setCursor(0, 0);
    lcd_printPGM(MSG_ERROR);
    lcd.setCursor(0, 2);
    lcd_printPGM(MSG_PREHEAT_NOZZLE);

    delay(2000);
    lcd_implementation_clear();
  }

  lcd_return_to_status();
}

void lcd_change_filament() {

  lcd_implementation_clear();

  lcd.setCursor(0, 1);

  lcd_printPGM(MSG_CHANGING_FILAMENT);


}


void lcd_wait_interact() {

  lcd_implementation_clear();

  lcd.setCursor(0, 1);

  lcd_printPGM(MSG_INSERT_FILAMENT);
  lcd.setCursor(0, 2);
  lcd_printPGM(MSG_PRESS);

}


void lcd_change_success() {

  lcd_implementation_clear();

  lcd.setCursor(0, 2);

  lcd_printPGM(MSG_CHANGE_SUCCESS);


}


void lcd_loading_color() {

  lcd_implementation_clear();

  lcd.setCursor(0, 0);

  lcd_printPGM(MSG_LOADING_COLOR);
  lcd.setCursor(0, 2);
  lcd_printPGM(MSG_PLEASE_WAIT);


  for (int i = 0; i < 20; i++) {

    lcd.setCursor(i, 3);
    lcd.print(".");
    for (int j = 0; j < 10 ; j++) {
      manage_heater();
      manage_inactivity(true);
      delay(85);

    }


  }

}


void lcd_loading_filament() {


  lcd_implementation_clear();

  lcd.setCursor(0, 0);

  lcd_printPGM(MSG_LOADING_FILAMENT);
  lcd.setCursor(0, 2);
  lcd_printPGM(MSG_PLEASE_WAIT);


  for (int i = 0; i < 20; i++) {

    lcd.setCursor(i, 3);
    lcd.print(".");
    for (int j = 0; j < 10 ; j++) {
      manage_heater();
      manage_inactivity(true);
      delay(110);

    }


  }

}

void lcd_alright() {
  int enc_dif = 0;
  int cursor_pos = 1;




  lcd_implementation_clear();

  lcd.setCursor(0, 0);

  lcd_printPGM(MSG_CORRECTLY);

  lcd.setCursor(1, 1);

  lcd_printPGM(MSG_YES);

  lcd.setCursor(1, 2);

  lcd_printPGM(MSG_NOT_LOADED);


  lcd.setCursor(1, 3);
  lcd_printPGM(MSG_NOT_COLOR);


  lcd.setCursor(0, 1);

  lcd.print(">");


  enc_dif = encoderDiff;

  while (lcd_change_fil_state == 0) {

    manage_heater();
    manage_inactivity(true);

    if ( abs((enc_dif - encoderDiff)) > 4 ) {

      if ( (abs(enc_dif - encoderDiff)) > 1 ) {
        if (enc_dif > encoderDiff ) {
          cursor_pos --;
        }

        if (enc_dif < encoderDiff  ) {
          cursor_pos ++;
        }

        if (cursor_pos > 3) {
          cursor_pos = 3;
        }

        if (cursor_pos < 1) {
          cursor_pos = 1;
        }
        lcd.setCursor(0, 1);
        lcd.print(" ");
        lcd.setCursor(0, 2);
        lcd.print(" ");
        lcd.setCursor(0, 3);
        lcd.print(" ");
        lcd.setCursor(0, cursor_pos);
        lcd.print(">");
        enc_dif = encoderDiff;
        delay(100);
      }

    }


    if (lcd_clicked()) {

      lcd_change_fil_state = cursor_pos;
      delay(500);

    }



  };


  lcd_implementation_clear();
  lcd_return_to_status();

}



void lcd_LoadFilament()
{
  if (degHotend0() > EXTRUDE_MINTEMP) 
  {
	  custom_message = true;
	  lcd_commands_type = 1;
	  SERIAL_ECHOLN("Loading filament");
	  // commands() will handle the rest
  } 
  else 
  {

    lcd_implementation_clear();
    lcd.setCursor(0, 0);
    lcd_printPGM(MSG_ERROR);
    lcd.setCursor(0, 2);
	lcd_printPGM(MSG_PREHEAT_NOZZLE);
    delay(2000);
    lcd_implementation_clear();
  }
  lcd_return_to_status();
}


static void lcd_menu_statistics()
{

	if (IS_SD_PRINTING)
	{
		int _met = total_filament_used / 100000;
		int _cm = (total_filament_used - (_met * 100000))/10;
		
		int _t = (millis() - starttime) / 1000;

		int _h = _t / 3600;
		int _m = (_t - (_h * 3600)) / 60;
		int _s = _t - ((_h * 3600) + (_m * 60));
		
		lcd.setCursor(0, 0);
		lcd_printPGM(MSG_STATS_FILAMENTUSED);

		lcd.setCursor(6, 1);
		lcd.print(itostr3(_met));
		lcd.print("m ");
		lcd.print(ftostr32ns(_cm));
		lcd.print("cm");
		
		lcd.setCursor(0, 2);
		lcd_printPGM(MSG_STATS_PRINTTIME);

		lcd.setCursor(8, 3);
		lcd.print(itostr2(_h));
		lcd.print("h ");
		lcd.print(itostr2(_m));
		lcd.print("m ");
		lcd.print(itostr2(_s));
		lcd.print("s");

		if (lcd_clicked())
		{
			lcd_quick_feedback();
			lcd_return_to_status();
		}
	}
	else
	{
		unsigned long _filament = eeprom_read_dword((uint32_t *)EEPROM_FILAMENTUSED);
		unsigned long _time = eeprom_read_dword((uint32_t *)EEPROM_TOTALTIME);
		uint8_t _days, _hours, _minutes;

		float _filament_m = (float)_filament;
		int _filament_km = (_filament >= 100000) ? _filament / 100000 : 0;
		if (_filament_km > 0)  _filament_m = _filament - (_filament_km * 100000);

		_days = _time / 1440;
		_hours = (_time - (_days * 1440)) / 60;
		_minutes = _time - ((_days * 1440) + (_hours * 60));

		lcd_implementation_clear();

		lcd.setCursor(0, 0);
		lcd_printPGM(MSG_STATS_TOTALFILAMENT);
		lcd.setCursor(17 - strlen(ftostr32ns(_filament_m)), 1);
		lcd.print(ftostr32ns(_filament_m));

		if (_filament_km > 0)
		{
			lcd.setCursor(17 - strlen(ftostr32ns(_filament_m)) - 3, 1);
			lcd.print("km");
			lcd.setCursor(17 - strlen(ftostr32ns(_filament_m)) - 8, 1);
			lcd.print(itostr4(_filament_km));
		}


		lcd.setCursor(18, 1);
		lcd.print("m");

		lcd.setCursor(0, 2);
		lcd_printPGM(MSG_STATS_TOTALPRINTTIME);;

		lcd.setCursor(18, 3);
		lcd.print("m");
		lcd.setCursor(14, 3);
		lcd.print(itostr3(_minutes));

		lcd.setCursor(14, 3);
		lcd.print(":");

		lcd.setCursor(12, 3);
		lcd.print("h");
		lcd.setCursor(9, 3);
		lcd.print(itostr3(_hours));

		lcd.setCursor(9, 3);
		lcd.print(":");

		lcd.setCursor(7, 3);
		lcd.print("d");
		lcd.setCursor(4, 3);
		lcd.print(itostr3(_days));



		while (!lcd_clicked())
		{
			manage_heater();
			manage_inactivity(true);
			delay(100);
		}

		lcd_quick_feedback();
		lcd_return_to_status();
	}
}


static void _lcd_move(const char *name, int axis, int min, int max) {
  if (encoderPosition != 0) {
    refresh_cmd_timeout();
    current_position[axis] += float((int)encoderPosition) * move_menu_scale;
    if (min_software_endstops && current_position[axis] < min) current_position[axis] = min;
    if (max_software_endstops && current_position[axis] > max) current_position[axis] = max;
    encoderPosition = 0;
#ifdef DELTA
    calculate_delta(current_position);
    plan_buffer_line(delta[X_AXIS], delta[Y_AXIS], delta[Z_AXIS], current_position[E_AXIS], manual_feedrate[axis] / 60, active_extruder);
#else
    plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], manual_feedrate[axis] / 60, active_extruder);
#endif
    lcdDrawUpdate = 1;
  }
  if (lcdDrawUpdate) lcd_implementation_drawedit(name, ftostr31(current_position[axis]));
  if (LCD_CLICKED) lcd_goto_menu(lcd_move_menu_axis);
}


static void lcd_move_e()
{
  if (encoderPosition != 0)
  {
    current_position[E_AXIS] += float((int)encoderPosition) * move_menu_scale;
    encoderPosition = 0;
#ifdef DELTA
    calculate_delta(current_position);
    plan_buffer_line(delta[X_AXIS], delta[Y_AXIS], delta[Z_AXIS], current_position[E_AXIS], manual_feedrate[E_AXIS] / 60, active_extruder);
#else
    plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], manual_feedrate[E_AXIS] / 60, active_extruder);
#endif
    lcdDrawUpdate = 1;
  }
  if (lcdDrawUpdate)
  {
    lcd_implementation_drawedit(PSTR("Extruder"), ftostr31(current_position[E_AXIS]));
  }
  if (LCD_CLICKED) lcd_goto_menu(lcd_move_menu_axis);
}


void EEPROM_save_B(int pos, int* value)
{
  union Data data;
  data.value = *value;

  eeprom_write_byte((unsigned char*)pos, data.b[0]);
  eeprom_write_byte((unsigned char*)pos + 1, data.b[1]);


}

void EEPROM_read_B(int pos, int* value)
{
  union Data data;
  data.b[0] = eeprom_read_byte((unsigned char*)pos);
  data.b[1] = eeprom_read_byte((unsigned char*)pos + 1);
  *value = data.value;

}



static void lcd_move_x() {
  _lcd_move(PSTR("X"), X_AXIS, X_MIN_POS, X_MAX_POS);
}
static void lcd_move_y() {
  _lcd_move(PSTR("Y"), Y_AXIS, Y_MIN_POS, Y_MAX_POS);
}
static void lcd_move_z() {
  _lcd_move(PSTR("Z"), Z_AXIS, Z_MIN_POS, Z_MAX_POS);
}



static void _lcd_babystep(int axis, const char *msg) {
  if (encoderPosition != 0) 
  {
    babystepsTodo[axis] += (int)encoderPosition;
    babystepMem[axis] += (int)encoderPosition;
    babystepMemMM[axis] = babystepMem[axis]/axis_steps_per_unit[Z_AXIS];
	delay(50);
	encoderPosition = 0;
    lcdDrawUpdate = 1;
  }
  if (lcdDrawUpdate) lcd_implementation_drawedit_2(msg, ftostr13ns(babystepMemMM[axis]));
  if (LCD_CLICKED) lcd_goto_menu(lcd_main_menu);
  EEPROM_save_B(EEPROM_BABYSTEP_X, &babystepMem[0]);
  EEPROM_save_B(EEPROM_BABYSTEP_Y, &babystepMem[1]);
  EEPROM_save_B(EEPROM_BABYSTEP_Z, &babystepMem[2]);
}

static void lcd_babystep_x() {
  _lcd_babystep(X_AXIS, (MSG_BABYSTEPPING_X));
}
static void lcd_babystep_y() {
  _lcd_babystep(Y_AXIS, (MSG_BABYSTEPPING_Y));
}


static void lcd_babystep_z() {
  _lcd_babystep(Z_AXIS, (MSG_BABYSTEPPING_Z));
}


void lcd_adjust_z() {
  int enc_dif = 0;
  int cursor_pos = 1;
  int fsm = 0;




  lcd_implementation_clear();
  lcd.setCursor(0, 0);
  lcd_printPGM(MSG_ADJUSTZ);
  lcd.setCursor(1, 1);
  lcd_printPGM(MSG_YES);

  lcd.setCursor(1, 2);

  lcd_printPGM(MSG_NO);

  lcd.setCursor(0, 1);

  lcd.print(">");


  enc_dif = encoderDiff;

  while (fsm == 0) {

    manage_heater();
    manage_inactivity(true);

    if ( abs((enc_dif - encoderDiff)) > 4 ) {

      if ( (abs(enc_dif - encoderDiff)) > 1 ) {
        if (enc_dif > encoderDiff ) {
          cursor_pos --;
        }

        if (enc_dif < encoderDiff  ) {
          cursor_pos ++;
        }

        if (cursor_pos > 2) {
          cursor_pos = 2;
        }

        if (cursor_pos < 1) {
          cursor_pos = 1;
        }
        lcd.setCursor(0, 1);
        lcd.print(" ");
        lcd.setCursor(0, 2);
        lcd.print(" ");
        lcd.setCursor(0, cursor_pos);
        lcd.print(">");
        enc_dif = encoderDiff;
        delay(100);
      }

    }


    if (lcd_clicked()) {
      fsm = cursor_pos;
      if (fsm == 1) {
        EEPROM_read_B(EEPROM_BABYSTEP_X, &babystepMem[0]);
        EEPROM_read_B(EEPROM_BABYSTEP_Y, &babystepMem[1]);
        EEPROM_read_B(EEPROM_BABYSTEP_Z, &babystepMem[2]);
        babystepsTodo[Z_AXIS] = babystepMem[2];
      } else {
        babystepMem[0] = 0;
        babystepMem[1] = 0;
        babystepMem[2] = 0;

        EEPROM_save_B(EEPROM_BABYSTEP_X, &babystepMem[0]);
        EEPROM_save_B(EEPROM_BABYSTEP_Y, &babystepMem[1]);
        EEPROM_save_B(EEPROM_BABYSTEP_Z, &babystepMem[2]);
      }
      delay(500);
    }
  };

  lcd_implementation_clear();
  lcd_return_to_status();

}

void lcd_calibration() {
}



void lcd_pick_babystep(){
    int enc_dif = 0;
    int cursor_pos = 1;
    int fsm = 0;
    
    
    
    
    lcd_implementation_clear();
    
    lcd.setCursor(0, 0);
    
    lcd_printPGM(MSG_PICK_Z);
    
    
    lcd.setCursor(3, 2);
    
    lcd.print("1");
    
    lcd.setCursor(3, 3);
    
    lcd.print("2");
    
    lcd.setCursor(12, 2);
    
    lcd.print("3");
    
    lcd.setCursor(12, 3);
    
    lcd.print("4");
    
    lcd.setCursor(1, 2);
    
    lcd.print(">");
    
    
    enc_dif = encoderDiff;
    
    while (fsm == 0) {
        
        manage_heater();
        manage_inactivity(true);
        
        if ( abs((enc_dif - encoderDiff)) > 4 ) {
            
            if ( (abs(enc_dif - encoderDiff)) > 1 ) {
                if (enc_dif > encoderDiff ) {
                    cursor_pos --;
                }
                
                if (enc_dif < encoderDiff  ) {
                    cursor_pos ++;
                }
                
                if (cursor_pos > 4) {
                    cursor_pos = 4;
                }
                
                if (cursor_pos < 1) {
                    cursor_pos = 1;
                }

                
                lcd.setCursor(1, 2);
                lcd.print(" ");
                lcd.setCursor(1, 3);
                lcd.print(" ");
                lcd.setCursor(10, 2);
                lcd.print(" ");
                lcd.setCursor(10, 3);
                lcd.print(" ");
                
                if (cursor_pos < 3) {
                    lcd.setCursor(1, cursor_pos+1);
                    lcd.print(">");
                }else{
                    lcd.setCursor(10, cursor_pos-1);
                    lcd.print(">");
                }
                
   
                enc_dif = encoderDiff;
                delay(100);
            }
            
        }
        
        
        if (lcd_clicked()) {
            fsm = cursor_pos;
            
            EEPROM_read_B(EEPROM_BABYSTEP_Z0+((fsm-1)*2),&babystepMem[2]);
            EEPROM_save_B(EEPROM_BABYSTEP_Z,&babystepMem[2]);
            eeprom_write_byte((unsigned char*)EEPROM_BABYSTEP_Z_SET, 0x01);
            delay(500);
            
        }
        
        
        
    };
    
    
    lcd_implementation_clear();
    lcd_return_to_status();
}

void lcd_move_menu_axis()
{
  START_MENU();
  MENU_ITEM(back, MSG_SETTINGS, lcd_settings_menu);
  MENU_ITEM(submenu, MSG_MOVE_X, lcd_move_x);
  MENU_ITEM(submenu, MSG_MOVE_Y, lcd_move_y);
  if (move_menu_scale < 10.0)
  {
	  if (!isPrintPaused)
	  {
		  MENU_ITEM(submenu, MSG_MOVE_Z, lcd_move_z);
	  }
	  MENU_ITEM(submenu, MSG_MOVE_E, lcd_move_e);
  }
  END_MENU();
}

static void lcd_move_menu_1mm()
{
  move_menu_scale = 1.0;
  lcd_move_menu_axis();
}


void EEPROM_save(int pos, uint8_t* value, uint8_t size)
{
  do
  {
    eeprom_write_byte((unsigned char*)pos, *value);
    pos++;
    value++;
  } while (--size);
}

void EEPROM_read(int pos, uint8_t* value, uint8_t size)
{
  do
  {
    *value = eeprom_read_byte((unsigned char*)pos);
    pos++;
    value++;
  } while (--size);
}



static void lcd_silent_mode_set() {
  SilentModeMenu = !SilentModeMenu;
  EEPROM_save(EEPROM_SILENT, (uint8_t*)&SilentModeMenu, sizeof(SilentModeMenu));
  digipot_init();
  lcd_goto_menu(lcd_settings_menu, 7);
}
static void lcd_set_lang(unsigned char lang) {
  lang_selected = lang;
  firstrun = 1;
  eeprom_write_byte((unsigned char *)EEPROM_LANG, lang);/*langsel=0;*/if (langsel == 1)langsel = 2;
}

void lcd_force_language_selection() {
  eeprom_write_byte((unsigned char *)EEPROM_LANG, 255);
}

static void lcd_language_menu()
{
  START_MENU();
  if (!langsel) {
    MENU_ITEM(back, MSG_SETTINGS, lcd_settings_menu);
  }
  if (langsel == 2) {
    MENU_ITEM(back, MSG_WATCH, lcd_status_screen);
  }
  for (int i=0;i<LANG_NUM;i++){
    MENU_ITEM(setlang, MSG_ALL[i][LANGUAGE_NAME], i);
  }
  //MENU_ITEM(setlang, MSG_ALL[1][LANGUAGE_NAME], 1);
  END_MENU();
}

void lcd_mesh_bedleveling()
{

	enquecommand_P(PSTR("G80"));
	lcd_return_to_status();
}


static void lcd_settings_menu()
{
  EEPROM_read(EEPROM_SILENT, (uint8_t*)&SilentModeMenu, sizeof(SilentModeMenu));
  START_MENU();

  MENU_ITEM(back, MSG_MAIN, lcd_main_menu);

  MENU_ITEM(submenu, MSG_TEMPERATURE, lcd_control_temperature_menu);
  MENU_ITEM(submenu, MSG_MOVE_AXIS, lcd_move_menu_1mm);
  
  if (!isPrintPaused)
  {
#ifndef MESH_BED_LEVELING
	  MENU_ITEM(gcode, MSG_HOMEYZ, PSTR("G28 Z"));
#else
	  MENU_ITEM(submenu, MSG_HOMEYZ, lcd_mesh_bedleveling);
#endif
  }

  if (!isPrintPaused)
  {
	  MENU_ITEM(gcode, MSG_DISABLE_STEPPERS, PSTR("M84"));
	  MENU_ITEM(gcode, MSG_AUTO_HOME, PSTR("G28"));
  }

  if (SilentModeMenu == 0) {
    MENU_ITEM(function, MSG_SILENT_MODE_OFF, lcd_silent_mode_set);
  } else {
    MENU_ITEM(function, MSG_SILENT_MODE_ON, lcd_silent_mode_set);
  }

    EEPROM_read_B(EEPROM_BABYSTEP_X, &babystepMem[0]);
    EEPROM_read_B(EEPROM_BABYSTEP_Y, &babystepMem[1]);
    EEPROM_read_B(EEPROM_BABYSTEP_Z, &babystepMem[2]);
    babystepMemMM[2] = babystepMem[2]/axis_steps_per_unit[Z_AXIS];
  
	if (!isPrintPaused)
	{
		MENU_ITEM(submenu, MSG_BABYSTEP_Z, lcd_babystep_z);//8
	}
	MENU_ITEM(submenu, MSG_LANGUAGE_SELECT, lcd_language_menu);
	if (!isPrintPaused)
	{
		MENU_ITEM(submenu, MSG_SELFTEST, lcd_selftest);
	}
  
	END_MENU();
}
/*
void lcd_mylang_top(int hlaska) {
    lcd.setCursor(0,0);
    lcd.print("                    ");
    lcd.setCursor(0,0);
    lcd_printPGM(MSG_ALL[hlaska-1][LANGUAGE_SELECT]);   
}

void lcd_mylang_drawmenu(int cursor) {
  int first = 0;
  if (cursor>2) first = cursor-2;
  if (cursor==LANG_NUM) first = LANG_NUM-3;
  lcd.setCursor(0, 1);
  lcd.print("                    ");
  lcd.setCursor(1, 1);
  lcd_printPGM(MSG_ALL[first][LANGUAGE_NAME]);

  lcd.setCursor(0, 2);
  lcd.print("                    ");
  lcd.setCursor(1, 2);
  lcd_printPGM(MSG_ALL[first+1][LANGUAGE_NAME]);

  lcd.setCursor(0, 3);
  lcd.print("                    ");
  lcd.setCursor(1, 3);
  lcd_printPGM(MSG_ALL[first+2][LANGUAGE_NAME]);  
  
  if (cursor==1) lcd.setCursor(0, 1);
  if (cursor>1 && cursor<LANG_NUM) lcd.setCursor(0, 2);
  if (cursor==LANG_NUM) lcd.setCursor(0, 3);

  lcd.print(">");
  
  if (cursor<LANG_NUM-1) {
    lcd.setCursor(19,3);
    lcd.print("\x01");
  }
  if (cursor>2) {
    lcd.setCursor(19,1);
    lcd.print("^");
  }  
}
*/

void lcd_mylang_drawmenu(int cursor) {
  int first = 0;
  if (cursor>3) first = cursor-3;
  if (cursor==LANG_NUM && LANG_NUM>4) first = LANG_NUM-4;
  if (cursor==LANG_NUM && LANG_NUM==4) first = LANG_NUM-4;


  lcd.setCursor(0, 0);
  lcd.print("                    ");
  lcd.setCursor(1, 0);
  lcd_printPGM(MSG_ALL[first+0][LANGUAGE_NAME]);

  lcd.setCursor(0, 1);
  lcd.print("                    ");
  lcd.setCursor(1, 1);
  lcd_printPGM(MSG_ALL[first+1][LANGUAGE_NAME]);

  lcd.setCursor(0, 2);
  lcd.print("                    ");

  if (LANG_NUM > 2){
    lcd.setCursor(1, 2);
    lcd_printPGM(MSG_ALL[first+2][LANGUAGE_NAME]);
  }

  lcd.setCursor(0, 3);
  lcd.print("                    ");
  if (LANG_NUM>3) {
    lcd.setCursor(1, 3);
    lcd_printPGM(MSG_ALL[first+3][LANGUAGE_NAME]);
  }
  
  if (cursor==1) lcd.setCursor(0, 0);
  if (cursor==2) lcd.setCursor(0, 1);
  if (cursor>2) lcd.setCursor(0, 2);
  if (cursor==LANG_NUM && LANG_NUM>3) lcd.setCursor(0, 3);

  lcd.print(">");
  
  if (cursor<LANG_NUM-1 && LANG_NUM>4) {
    lcd.setCursor(19,3);
    lcd.print("\x01");
  }
  if (cursor>3 && LANG_NUM>4) {
    lcd.setCursor(19,0);
    lcd.print("^");
  }  
}
 
void lcd_set_custom_characters_arrows();
void lcd_set_custom_characters_degree();

void lcd_mylang_drawcursor(int cursor) {
  
  if (cursor==1) lcd.setCursor(0, 1);
  if (cursor>1 && cursor<LANG_NUM) lcd.setCursor(0, 2);
  if (cursor==LANG_NUM) lcd.setCursor(0, 3);

  lcd.print(">");
  
}  

void lcd_mylang() {
  int enc_dif = 0;
  int cursor_pos = 1;
  lang_selected=255;
  int hlaska=1;
  int counter=0;
  lcd_set_custom_characters_arrows();

  lcd_implementation_clear();

  //lcd_mylang_top(hlaska);

  lcd_mylang_drawmenu(cursor_pos);


  enc_dif = encoderDiff;

  while ( (lang_selected == 255) && (MYSERIAL.available() < 2) ) {

    manage_heater();
    manage_inactivity(true);

    if ( abs((enc_dif - encoderDiff)) > 4 ) {

      //if ( (abs(enc_dif - encoderDiff)) > 1 ) {
        if (enc_dif > encoderDiff ) {
          cursor_pos --;
        }

        if (enc_dif < encoderDiff  ) {
          cursor_pos ++;
        }

        if (cursor_pos > LANG_NUM) {
          cursor_pos = LANG_NUM;
        }

        if (cursor_pos < 1) {
          cursor_pos = 1;
        }

        lcd_mylang_drawmenu(cursor_pos);
        enc_dif = encoderDiff;
        delay(100);
      //}

    } else delay(20);


    if (lcd_clicked()) {

      lcd_set_lang(cursor_pos-1);
      delay(500);

    }
    /*
    if (++counter == 80) {
      hlaska++;
      if(hlaska>LANG_NUM) hlaska=1;
      lcd_mylang_top(hlaska);
      lcd_mylang_drawcursor(cursor_pos);
      counter=0;
    }
    */
  };

  if(MYSERIAL.available() > 1){
    lang_selected = 0;
    firstrun = 0;
  }

  lcd_set_custom_characters_degree();
  lcd_implementation_clear();
  lcd_return_to_status();

}




static void lcd_main_menu()
{

  SDscrool = 0;
  /*
  if (langsel == 1)
  {
    lcd_goto_menu(lcd_language_menu);
  }
  */
  START_MENU();

  // Majkl superawesome menu

  
 MENU_ITEM(back, MSG_WATCH, lcd_status_screen);

  if ( ( IS_SD_PRINTING || is_usb_printing ) && (current_position[Z_AXIS] < 0.5) ) 
  {
    EEPROM_read_B(EEPROM_BABYSTEP_X, &babystepMem[0]);
	EEPROM_read_B(EEPROM_BABYSTEP_Y, &babystepMem[1]);
	EEPROM_read_B(EEPROM_BABYSTEP_Z, &babystepMem[2]);
	
	MENU_ITEM(submenu, MSG_BABYSTEP_Z, lcd_babystep_z);//8
  }


  if ( movesplanned() || IS_SD_PRINTING || is_usb_printing )
  {
    MENU_ITEM(submenu, MSG_TUNE, lcd_tune_menu);
  } else 
  {
    MENU_ITEM(submenu, MSG_PREHEAT, lcd_preheat_menu);
  }

#ifdef SDSUPPORT
  if (card.cardOK)
  {
    if (card.isFileOpen())
    {
		if (card.sdprinting)
		{
			MENU_ITEM(function, MSG_PAUSE_PRINT, lcd_sdcard_pause);
		}
		else
		{
			MENU_ITEM(function, MSG_RESUME_PRINT, lcd_sdcard_resume);
		}
		MENU_ITEM(submenu, MSG_STOP_PRINT, lcd_sdcard_stop);
	}
	else
	{
		if (!is_usb_printing)
		{
			MENU_ITEM(submenu, MSG_CARD_MENU, lcd_sdcard_menu);
		}
#if SDCARDDETECT < 1
      MENU_ITEM(gcode, MSG_CNG_SDCARD, PSTR("M21"));  // SD-card changed by user
#endif
    }
  } else 
  {
    MENU_ITEM(submenu, MSG_NO_CARD, lcd_sdcard_menu);
#if SDCARDDETECT < 1
    MENU_ITEM(gcode, MSG_INIT_SDCARD, PSTR("M21")); // Manually initialize the SD-card via user interface
#endif
  }
#endif


  if (IS_SD_PRINTING || is_usb_printing)
  {
  } 
  else 
  {
    MENU_ITEM(function, MSG_LOAD_FILAMENT, lcd_LoadFilament);
    MENU_ITEM(function, MSG_UNLOAD_FILAMENT, lcd_unLoadFilament);
    MENU_ITEM(submenu, MSG_SETTINGS, lcd_settings_menu);
  }

  if (!is_usb_printing)
  {
	  MENU_ITEM(submenu, MSG_STATISTICS, lcd_menu_statistics);
  }
  MENU_ITEM(submenu, MSG_SUPPORT, lcd_support_menu);

  END_MENU();
}



#ifdef SDSUPPORT
static void lcd_autostart_sd()
{
  card.lastnr = 0;
  card.setroot();
  card.checkautostart(true);
}
#endif



static void lcd_silent_mode_set_tune() {
  SilentModeMenu = !SilentModeMenu;
  EEPROM_save(EEPROM_SILENT, (uint8_t*)&SilentModeMenu, sizeof(SilentModeMenu));
  digipot_init();
  lcd_goto_menu(lcd_tune_menu, 9);
}

static void lcd_tune_menu()
{
  EEPROM_read(EEPROM_SILENT, (uint8_t*)&SilentModeMenu, sizeof(SilentModeMenu));

  

  START_MENU();
  MENU_ITEM(back, MSG_MAIN, lcd_main_menu); //1
  MENU_ITEM_EDIT(int3, MSG_SPEED, &feedmultiply, 10, 999);//2

  MENU_ITEM_EDIT(int3, MSG_NOZZLE, &target_temperature[0], 0, HEATER_0_MAXTEMP - 10);//3
  MENU_ITEM_EDIT(int3, MSG_BED, &target_temperature_bed, 0, BED_MAXTEMP - 10);//4

  MENU_ITEM_EDIT(int3, MSG_FAN_SPEED, &fanSpeed, 0, 255);//5
  MENU_ITEM_EDIT(int3, MSG_FLOW, &extrudemultiply, 10, 999);//6
#ifdef FILAMENTCHANGEENABLE
  MENU_ITEM(gcode, MSG_FILAMENTCHANGE, PSTR("M600"));//7
#endif
  
  if (SilentModeMenu == 0) {
    MENU_ITEM(function, MSG_SILENT_MODE_OFF, lcd_silent_mode_set_tune);
  } else {
    MENU_ITEM(function, MSG_SILENT_MODE_ON, lcd_silent_mode_set_tune);
  }
  END_MENU();
}




static void lcd_move_menu_01mm()
{
  move_menu_scale = 0.1;
  lcd_move_menu_axis();
}

static void lcd_control_temperature_menu()
{
#ifdef PIDTEMP
  // set up temp variables - undo the default scaling
  raw_Ki = unscalePID_i(Ki);
  raw_Kd = unscalePID_d(Kd);
#endif

  START_MENU();
  MENU_ITEM(back, MSG_SETTINGS, lcd_settings_menu);
  //MENU_ITEM(back, MSG_CONTROL, lcd_control_menu);
#if TEMP_SENSOR_0 != 0
  MENU_ITEM_EDIT(int3, MSG_NOZZLE, &target_temperature[0], 0, HEATER_0_MAXTEMP - 10);
#endif
#if TEMP_SENSOR_1 != 0
  MENU_ITEM_EDIT(int3, MSG_NOZZLE1, &target_temperature[1], 0, HEATER_1_MAXTEMP - 10);
#endif
#if TEMP_SENSOR_2 != 0
  MENU_ITEM_EDIT(int3, MSG_NOZZLE2, &target_temperature[2], 0, HEATER_2_MAXTEMP - 10);
#endif
#if TEMP_SENSOR_BED != 0
  MENU_ITEM_EDIT(int3, MSG_BED, &target_temperature_bed, 0, BED_MAXTEMP - 3);
#endif
  MENU_ITEM_EDIT(int3, MSG_FAN_SPEED, &fanSpeed, 0, 255);
#if defined AUTOTEMP && (TEMP_SENSOR_0 != 0)
  MENU_ITEM_EDIT(bool, MSG_AUTOTEMP, &autotemp_enabled);
  MENU_ITEM_EDIT(float3, MSG_MIN, &autotemp_min, 0, HEATER_0_MAXTEMP - 10);
  MENU_ITEM_EDIT(float3, MSG_MAX, &autotemp_max, 0, HEATER_0_MAXTEMP - 10);
  MENU_ITEM_EDIT(float32, MSG_FACTOR, &autotemp_factor, 0.0, 1.0);
#endif

  END_MENU();
}


#if SDCARDDETECT == -1
static void lcd_sd_refresh()
{
  card.initsd();
  currentMenuViewOffset = 0;
}
#endif
static void lcd_sd_updir()
{
  SDscrool = 0;
  card.updir();
  currentMenuViewOffset = 0;
}


void lcd_sdcard_stop()
{
	
	lcd.setCursor(0, 0);
	lcd_printPGM(MSG_STOP_PRINT);
	lcd.setCursor(2, 2);
	lcd_printPGM(MSG_NO);
	lcd.setCursor(2, 3);
	lcd_printPGM(MSG_YES);
	lcd.setCursor(0, 2); lcd.print(" ");
	lcd.setCursor(0, 3); lcd.print(" ");

	if ((int32_t)encoderPosition > 2) { encoderPosition = 2; }
	if ((int32_t)encoderPosition < 1) { encoderPosition = 1; }
	
	lcd.setCursor(0, 1 + encoderPosition);
	lcd.print(">");

	if (lcd_clicked())
	{
		if ((int32_t)encoderPosition == 1)
		{
			lcd_return_to_status();
		}
		if ((int32_t)encoderPosition == 2)
		{
				cancel_heatup = true;
				quickStop();
				lcd_setstatuspgm(MSG_PRINT_ABORTED);
				card.sdprinting = false;
				card.closefile();

				stoptime = millis();
				unsigned long t = (stoptime - starttime) / 1000;
				save_statistics(total_filament_used, t);

				lcd_return_to_status();
				lcd_ignore_click(true);
				lcd_commands_type = 2;
		}
	}

}

void lcd_sdcard_menu()
{

	int tempScrool = 0;
  if (lcdDrawUpdate == 0 && LCD_CLICKED == 0)
    //delay(100);
    return; // nothing to do (so don't thrash the SD card)
  uint16_t fileCnt = card.getnrfilenames();

  START_MENU();
  MENU_ITEM(back, MSG_MAIN, lcd_main_menu);
  card.getWorkDirName();
  if (card.filename[0] == '/')
  {
#if SDCARDDETECT == -1
    MENU_ITEM(function, LCD_STR_REFRESH MSG_REFRESH, lcd_sd_refresh);
#endif
  } else {
    MENU_ITEM(function, LCD_STR_FOLDER "..", lcd_sd_updir);
  }

  for (uint16_t i = 0; i < fileCnt; i++)
  {
    if (_menuItemNr == _lineNr)
    {
#ifndef SDCARD_RATHERRECENTFIRST
      card.getfilename(i);
#else
      card.getfilename(fileCnt - 1 - i);
#endif
      if (card.filenameIsDir)
      {
        MENU_ITEM(sddirectory, MSG_CARD_MENU, card.filename, card.longFilename);
      } else {

        MENU_ITEM(sdfile, MSG_CARD_MENU, card.filename, card.longFilename);




      }
    } else {
      MENU_ITEM_DUMMY();
    }
  }
  END_MENU();
}

#define menu_edit_type(_type, _name, _strFunc, scale) \
  void menu_edit_ ## _name () \
  { \
    if ((int32_t)encoderPosition < 0) encoderPosition = 0; \
    if ((int32_t)encoderPosition > maxEditValue) encoderPosition = maxEditValue; \
    if (lcdDrawUpdate) \
      lcd_implementation_drawedit(editLabel, _strFunc(((_type)((int32_t)encoderPosition + minEditValue)) / scale)); \
    if (LCD_CLICKED) \
    { \
      *((_type*)editValue) = ((_type)((int32_t)encoderPosition + minEditValue)) / scale; \
      lcd_goto_menu(prevMenu, prevEncoderPosition); \
    } \
  } \
  void menu_edit_callback_ ## _name () { \
    menu_edit_ ## _name (); \
    if (LCD_CLICKED) (*callbackFunc)(); \
  } \
  static void menu_action_setting_edit_ ## _name (const char* pstr, _type* ptr, _type minValue, _type maxValue) \
  { \
    prevMenu = currentMenu; \
    prevEncoderPosition = encoderPosition; \
    \
    lcdDrawUpdate = 2; \
    currentMenu = menu_edit_ ## _name; \
    \
    editLabel = pstr; \
    editValue = ptr; \
    minEditValue = minValue * scale; \
    maxEditValue = maxValue * scale - minEditValue; \
    encoderPosition = (*ptr) * scale - minEditValue; \
  }\
  static void menu_action_setting_edit_callback_ ## _name (const char* pstr, _type* ptr, _type minValue, _type maxValue, menuFunc_t callback) \
  { \
    prevMenu = currentMenu; \
    prevEncoderPosition = encoderPosition; \
    \
    lcdDrawUpdate = 2; \
    currentMenu = menu_edit_callback_ ## _name; \
    \
    editLabel = pstr; \
    editValue = ptr; \
    minEditValue = minValue * scale; \
    maxEditValue = maxValue * scale - minEditValue; \
    encoderPosition = (*ptr) * scale - minEditValue; \
    callbackFunc = callback;\
  }
menu_edit_type(int, int3, itostr3, 1)
menu_edit_type(float, float3, ftostr3, 1)
menu_edit_type(float, float32, ftostr32, 100)
menu_edit_type(float, float43, ftostr43, 1000)
menu_edit_type(float, float5, ftostr5, 0.01)
menu_edit_type(float, float51, ftostr51, 10)
menu_edit_type(float, float52, ftostr52, 100)
menu_edit_type(unsigned long, long5, ftostr5, 0.01)


static void lcd_selftest()
{
	int _progress = 0;
	bool _result = false;

	_progress = lcd_selftest_screen(-1, _progress, 4, true, 2000);

	_progress = lcd_selftest_screen(0, _progress, 3, true, 2000);
	_result = lcd_selfcheck_endstops();

	if (_result)
	{
		_progress = lcd_selftest_screen(1, _progress, 3, true, 1000);
		_result = lcd_selfcheck_check_heater(false);
	}

	if (_result)
	{
		_progress = lcd_selftest_screen(2, _progress, 3, true, 2000);
		_result = lcd_selfcheck_axis(0, X_MAX_POS);
	}

	if (_result)
	{
		_progress = lcd_selftest_screen(3, _progress, 3, true, 1500);
		_result = lcd_selfcheck_axis(1, Y_MAX_POS);
	}

	if (_result)
	{
		current_position[X_AXIS] = current_position[X_AXIS] - 3;
		current_position[Y_AXIS] = current_position[Y_AXIS] - 14;
		_progress = lcd_selftest_screen(4, _progress, 3, true, 1500);
		_result = lcd_selfcheck_axis(2, Z_MAX_POS);
	}

	if (_result)
	{
		_progress = lcd_selftest_screen(5, _progress, 3, true, 2000);
		_result = lcd_selfcheck_check_heater(true);
	}
	if (_result)
	{
		_progress = lcd_selftest_screen(6, _progress, 3, true, 5000);
	}
	else
	{
		_progress = lcd_selftest_screen(7, _progress, 3, true, 5000);
	}

	lcd_implementation_clear();
	lcd_next_update_millis = millis() + LCD_UPDATE_INTERVAL;

	if (_result)
	{
		LCD_ALERTMESSAGERPGM(MSG_SELFTEST_OK);
	}
	else
	{
		LCD_ALERTMESSAGERPGM(MSG_SELFTEST_FAILED);
	}
}
static bool lcd_selfcheck_endstops()
{
	bool _result = true;

	if (READ(X_MIN_PIN) ^ X_MIN_ENDSTOP_INVERTING == 1 || READ(Y_MIN_PIN) ^ Y_MIN_ENDSTOP_INVERTING == 1 || READ(Z_MIN_PIN) ^ Z_MIN_ENDSTOP_INVERTING == 1)
	{
		current_position[0] = (READ(X_MIN_PIN) ^ X_MIN_ENDSTOP_INVERTING == 1) ? current_position[0] = current_position[0] + 10 : current_position[0];
		current_position[1] = (READ(Y_MIN_PIN) ^ Y_MIN_ENDSTOP_INVERTING == 1) ? current_position[1] = current_position[1] + 10 : current_position[1];
		current_position[2] = (READ(Z_MIN_PIN) ^ Z_MIN_ENDSTOP_INVERTING == 1) ? current_position[2] = current_position[2] + 10 : current_position[2];
	}
	plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], manual_feedrate[0] / 60, active_extruder);
	delay(500);

	if (READ(X_MIN_PIN) ^ X_MIN_ENDSTOP_INVERTING == 1 || READ(Y_MIN_PIN) ^ Y_MIN_ENDSTOP_INVERTING == 1 || READ(Z_MIN_PIN) ^ Z_MIN_ENDSTOP_INVERTING == 1)
	{
		_result = false;
		String  _error = String((READ(X_MIN_PIN) ^ X_MIN_ENDSTOP_INVERTING == 1) ? "X" : "") +
			String((READ(Y_MIN_PIN) ^ Y_MIN_ENDSTOP_INVERTING == 1) ? "Y" : "") +
			String((READ(Z_MIN_PIN) ^ Z_MIN_ENDSTOP_INVERTING == 1) ? "Z" : "");
		lcd_selftest_error(3, _error.c_str(), "");
	}
	manage_heater();
	manage_inactivity();
	return _result;
}
static bool lcd_selfcheck_axis(int _axis, int _travel)
{
	bool _stepdone = false;
	bool _stepresult = false;
	int _progress = 0;
	int _travel_done = 0;
	int _err_endstop = 0;
	int _lcd_refresh = 0;
	_travel = _travel + (_travel / 10);

	do {

		if (_axis == 2)
		{
			current_position[_axis] = current_position[_axis] - 1;
		}
		else
		{
			current_position[_axis] = current_position[_axis] - 3;
		}

		plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[3], manual_feedrate[0] / 60, active_extruder);
		st_synchronize();

		if (READ(X_MIN_PIN) ^ X_MIN_ENDSTOP_INVERTING == 1 || READ(Y_MIN_PIN) ^ Y_MIN_ENDSTOP_INVERTING == 1 || READ(Z_MIN_PIN) ^ Z_MIN_ENDSTOP_INVERTING == 1)
		{
			if (_axis == 0)
			{
				_stepresult = (READ(X_MIN_PIN) ^ X_MIN_ENDSTOP_INVERTING == 1) ? true : false;
				_err_endstop = (READ(Y_MIN_PIN) ^ Y_MIN_ENDSTOP_INVERTING == 1) ? 1 : 2;
				disable_x();
			}
			if (_axis == 1)
			{
				_stepresult = (READ(Y_MIN_PIN) ^ Y_MIN_ENDSTOP_INVERTING == 1) ? true : false;
				_err_endstop = (READ(X_MIN_PIN) ^ X_MIN_ENDSTOP_INVERTING == 1) ? 0 : 2;
				disable_y();
			}
			if (_axis == 2)
			{
				_stepresult = (READ(Z_MIN_PIN) ^ Z_MIN_ENDSTOP_INVERTING == 1) ? true : false;
				_err_endstop = (READ(X_MIN_PIN) ^ X_MIN_ENDSTOP_INVERTING == 1) ? 0 : 1;
				disable_z();
			}
			_stepdone = true;
		}

		if (_lcd_refresh < 6)
		{
			_lcd_refresh++;
		}
		else
		{
			_progress = lcd_selftest_screen(2 + _axis, _progress, 3, false, 0);
			_lcd_refresh = 0;
		}

		manage_heater();
		manage_inactivity();

		delay(100);
		(_travel_done <= _travel) ? _travel_done++ : _stepdone = true;

	} while (!_stepdone);


	current_position[_axis] = current_position[_axis] + 15;
	plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[3], manual_feedrate[0] / 60, active_extruder);

	if (!_stepresult)
	{
		const char *_error_1;
		const char *_error_2;

		if (_axis == X_AXIS) _error_1 = "X";
		if (_axis == Y_AXIS) _error_1 = "Y";
		if (_axis == Z_AXIS) _error_1 = "Z";

		if (_err_endstop == 0) _error_2 = "X";
		if (_err_endstop == 1) _error_2 = "Y";
		if (_err_endstop == 2) _error_2 = "Z";

		if (_travel_done >= _travel)
		{
			lcd_selftest_error(5, _error_1, _error_2);
		}
		else
		{
			lcd_selftest_error(4, _error_1, _error_2);
		}
	}

	return _stepresult;
}
static bool lcd_selfcheck_check_heater(bool _isbed)
{
	int _counter = 0;
	int _progress = 0;
	bool _stepresult = false;
	bool _docycle = true;

	int _checked_snapshot = (_isbed) ? degBed() : degHotend(0);
	int _opposite_snapshot = (_isbed) ? degHotend(0) : degBed();
	int _cycles = (_isbed) ? 120 : 30;

	target_temperature[0] = (_isbed) ? 0 : 100;
	target_temperature_bed = (_isbed) ? 100 : 0;
	manage_heater();
	manage_inactivity();

	do {
		_counter++;
		(_counter < _cycles) ? _docycle = true : _docycle = false;

		manage_heater();
		manage_inactivity();
		_progress = (_isbed) ? lcd_selftest_screen(5, _progress, 2, false, 400) : lcd_selftest_screen(1, _progress, 2, false, 400);

	} while (_docycle);

	target_temperature[0] = 0;
	target_temperature_bed = 0;
	manage_heater();

	int _checked_result = (_isbed) ? degBed() - _checked_snapshot : degHotend(0) - _checked_snapshot;
	int _opposite_result = (_isbed) ? degHotend(0) - _opposite_snapshot : degBed() - _opposite_snapshot;

	if (_opposite_result < (_isbed) ? 10 : 3)
	{
		if (_checked_result >= (_isbed) ? 3 : 10)
		{
			_stepresult = true;
		}
		else
		{
			lcd_selftest_error(1, "", "");
		}
	}
	else
	{
		lcd_selftest_error(2, "", "");
	}

	manage_heater();
	manage_inactivity();
	return _stepresult;

}
static void lcd_selftest_error(int _error_no, const char *_error_1, const char *_error_2)
{
	lcd_implementation_quick_feedback();

	target_temperature[0] = 0;
	target_temperature_bed = 0;
	manage_heater();
	manage_inactivity();

	lcd_implementation_clear();

	lcd.setCursor(0, 0);
	lcd_printPGM(MSG_SELFTEST_ERROR);
	lcd.setCursor(0, 1);
	lcd_printPGM(MSG_SELFTEST_PLEASECHECK);

	switch (_error_no)
	{
	case 1:
		lcd.setCursor(0, 2);
		lcd_printPGM(MSG_SELFTEST_HEATERTHERMISTOR);
		lcd.setCursor(0, 3);
		lcd_printPGM(MSG_SELFTEST_NOTCONNECTED);
		break;
	case 2:
		lcd.setCursor(0, 2);
		lcd_printPGM(MSG_SELFTEST_BEDHEATER);
		lcd.setCursor(0, 3);
		lcd_printPGM(MSG_SELFTEST_WIRINGERROR);
		break;
	case 3:
		lcd.setCursor(0, 2);
		lcd_printPGM(MSG_SELFTEST_ENDSTOPS);
		lcd.setCursor(0, 3);
		lcd_printPGM(MSG_SELFTEST_WIRINGERROR);
		lcd.setCursor(17, 3);
		lcd.print(_error_1);
		break;
	case 4:
		lcd.setCursor(0, 2);
		lcd_printPGM(MSG_SELFTEST_MOTOR);
		lcd.setCursor(18, 2);
		lcd.print(_error_1);
		lcd.setCursor(0, 3);
		lcd_printPGM(MSG_SELFTEST_ENDSTOP);
		lcd.setCursor(18, 3);
		lcd.print(_error_2);
		break;
	case 5:
		lcd.setCursor(0, 2);
		lcd_printPGM(MSG_SELFTEST_ENDSTOP_NOTHIT);
		lcd.setCursor(0, 3);
		lcd_printPGM(MSG_SELFTEST_MOTOR);
		lcd.setCursor(18, 3);
		lcd.print(_error_1);
		break;

	}

	delay(1000);
	lcd_implementation_quick_feedback();

	do {
		delay(100);
		manage_heater();
		manage_inactivity();
	} while (!lcd_clicked());

	LCD_ALERTMESSAGERPGM(MSG_SELFTEST_FAILED);
	lcd_return_to_status();

}
static int lcd_selftest_screen(int _step, int _progress, int _progress_scale, bool _clear, int _delay)
{
	lcd_next_update_millis = millis() + (LCD_UPDATE_INTERVAL * 10000);

	int _step_block = 0;
	const char *_indicator = (_progress > _progress_scale) ? "-" : "|";

	if (_clear) lcd_implementation_clear();


	lcd.setCursor(0, 0);

	if (_step == -1) lcd_printPGM(MSG_SELFTEST_START);
	if (_step == 0) lcd_printPGM(MSG_SELFTEST_CHECK_ENDSTOPS);
	if (_step == 1) lcd_printPGM(MSG_SELFTEST_CHECK_HOTEND);
	if (_step == 2) lcd_printPGM(MSG_SELFTEST_CHECK_X);
	if (_step == 3) lcd_printPGM(MSG_SELFTEST_CHECK_Y);
	if (_step == 4) lcd_printPGM(MSG_SELFTEST_CHECK_Z);
	if (_step == 5) lcd_printPGM(MSG_SELFTEST_CHECK_BED);
	if (_step == 6) lcd_printPGM(MSG_SELFTEST_CHECK_ALLCORRECT);
	if (_step == 7) lcd_printPGM(MSG_SELFTEST_FAILED);

	lcd.setCursor(0, 1);
	lcd.print("--------------------");

	_step_block = 1;
	lcd_selftest_screen_step(3, 9, ((_step == _step_block) ? 1 : (_step < _step_block) ? 0 : 2), "Hotend", _indicator);

	_step_block = 2;
	lcd_selftest_screen_step(2, 2, ((_step == _step_block) ? 1 : (_step < _step_block) ? 0 : 2), "X", _indicator);

	_step_block = 3;
	lcd_selftest_screen_step(2, 8, ((_step == _step_block) ? 1 : (_step < _step_block) ? 0 : 2), "Y", _indicator);

	_step_block = 4;
	lcd_selftest_screen_step(2, 14, ((_step == _step_block) ? 1 : (_step < _step_block) ? 0 : 2), "Z", _indicator);

	_step_block = 5;
	lcd_selftest_screen_step(3, 0, ((_step == _step_block) ? 1 : (_step < _step_block) ? 0 : 2), "Bed", _indicator);


	if (_delay > 0) delay(_delay);
	_progress++;

	return (_progress > _progress_scale * 2) ? 0 : _progress;
}
static void lcd_selftest_screen_step(int _row, int _col, int _state, const char *_name, const char *_indicator)
{
	lcd.setCursor(_col, _row);

	switch (_state)
	{
	case 1:
		lcd.print(_name);
		lcd.setCursor(_col + strlen(_name), _row);
		lcd.print(":");
		lcd.setCursor(_col + strlen(_name) + 1, _row);
		lcd.print(_indicator);
		break;
	case 2:
		lcd.print(_name);
		lcd.setCursor(_col + strlen(_name), _row);
		lcd.print(":");
		lcd.setCursor(_col + strlen(_name) + 1, _row);
		lcd.print("OK");
		break;
	default:
		lcd.print(_name);
	}
}


/** End of menus **/

static void lcd_quick_feedback()
{
  lcdDrawUpdate = 2;
  blocking_enc = millis() + 500;
  lcd_implementation_quick_feedback();
}

/** Menu action functions **/
static void menu_action_back(menuFunc_t data) {
  lcd_goto_menu(data);
}
static void menu_action_submenu(menuFunc_t data) {
  lcd_goto_menu(data);
}
static void menu_action_gcode(const char* pgcode) {
  enquecommand_P(pgcode);
}
static void menu_action_setlang(unsigned char lang) {
  lcd_set_lang(lang);
}
static void menu_action_function(menuFunc_t data) {
  (*data)();
}
static void menu_action_sdfile(const char* filename, char* longFilename)
{
  char cmd[30];
  char* c;
  sprintf_P(cmd, PSTR("M23 %s"), filename);
  for (c = &cmd[4]; *c; c++)
    *c = tolower(*c);
  enquecommand(cmd);
  enquecommand_P(PSTR("M24"));
  lcd_return_to_status();
}
static void menu_action_sddirectory(const char* filename, char* longFilename)
{
  card.chdir(filename);
  encoderPosition = 0;
}
static void menu_action_setting_edit_bool(const char* pstr, bool* ptr)
{
  *ptr = !(*ptr);
}
static void menu_action_setting_edit_callback_bool(const char* pstr, bool* ptr, menuFunc_t callback)
{
  menu_action_setting_edit_bool(pstr, ptr);
  (*callback)();
}
#endif//ULTIPANEL

/** LCD API **/
void lcd_init()
{
  lcd_implementation_init();

#ifdef NEWPANEL
  SET_INPUT(BTN_EN1);
  SET_INPUT(BTN_EN2);
  WRITE(BTN_EN1, HIGH);
  WRITE(BTN_EN2, HIGH);
#if BTN_ENC > 0
  SET_INPUT(BTN_ENC);
  WRITE(BTN_ENC, HIGH);
#endif
#ifdef REPRAPWORLD_KEYPAD
  pinMode(SHIFT_CLK, OUTPUT);
  pinMode(SHIFT_LD, OUTPUT);
  pinMode(SHIFT_OUT, INPUT);
  WRITE(SHIFT_OUT, HIGH);
  WRITE(SHIFT_LD, HIGH);
#endif
#else  // Not NEWPANEL
#ifdef SR_LCD_2W_NL // Non latching 2 wire shift register
  pinMode (SR_DATA_PIN, OUTPUT);
  pinMode (SR_CLK_PIN, OUTPUT);
#elif defined(SHIFT_CLK)
  pinMode(SHIFT_CLK, OUTPUT);
  pinMode(SHIFT_LD, OUTPUT);
  pinMode(SHIFT_EN, OUTPUT);
  pinMode(SHIFT_OUT, INPUT);
  WRITE(SHIFT_OUT, HIGH);
  WRITE(SHIFT_LD, HIGH);
  WRITE(SHIFT_EN, LOW);
#else
#ifdef ULTIPANEL
#error ULTIPANEL requires an encoder
#endif
#endif // SR_LCD_2W_NL
#endif//!NEWPANEL

#if defined (SDSUPPORT) && defined(SDCARDDETECT) && (SDCARDDETECT > 0)
  pinMode(SDCARDDETECT, INPUT);
  WRITE(SDCARDDETECT, HIGH);
  lcd_oldcardstatus = IS_SD_INSERTED;
#endif//(SDCARDDETECT > 0)
#ifdef LCD_HAS_SLOW_BUTTONS
  slow_buttons = 0;
#endif
  lcd_buttons_update();
#ifdef ULTIPANEL
  encoderDiff = 0;
#endif
}




//#include <avr/pgmspace.h>


void lcd_update()
{
	static unsigned long timeoutToStatus = 0;


#ifdef LCD_HAS_SLOW_BUTTONS
  slow_buttons = lcd_implementation_read_slow_buttons(); // buttons which take too long to read in interrupt context
#endif

  lcd_buttons_update();

#if (SDCARDDETECT > 0)
  if ((IS_SD_INSERTED != lcd_oldcardstatus && lcd_detected()))
  {
	  lcdDrawUpdate = 2;
	  lcd_oldcardstatus = IS_SD_INSERTED;
	  lcd_implementation_init( // to maybe revive the LCD if static electricity killed it.
#if defined(LCD_PROGRESS_BAR) && defined(SDSUPPORT)
		  currentMenu == lcd_status_screen
#endif
	  );

	  if (lcd_oldcardstatus)
	  {
		  card.initsd();
		  LCD_MESSAGERPGM(MSG_SD_INSERTED);
	  }
	  else
	  {
		  card.release();
		  LCD_MESSAGERPGM(MSG_SD_REMOVED);
	  }
  }
#endif//CARDINSERTED

  if (lcd_next_update_millis < millis())
  {
#ifdef ULTIPANEL
#ifdef REPRAPWORLD_KEYPAD
	  if (REPRAPWORLD_KEYPAD_MOVE_Z_UP) {
		  reprapworld_keypad_move_z_up();
	  }
	  if (REPRAPWORLD_KEYPAD_MOVE_Z_DOWN) {
		  reprapworld_keypad_move_z_down();
	  }
	  if (REPRAPWORLD_KEYPAD_MOVE_X_LEFT) {
		  reprapworld_keypad_move_x_left();
	  }
	  if (REPRAPWORLD_KEYPAD_MOVE_X_RIGHT) {
		  reprapworld_keypad_move_x_right();
	  }
	  if (REPRAPWORLD_KEYPAD_MOVE_Y_DOWN) {
		  reprapworld_keypad_move_y_down();
	  }
	  if (REPRAPWORLD_KEYPAD_MOVE_Y_UP) {
		  reprapworld_keypad_move_y_up();
	  }
	  if (REPRAPWORLD_KEYPAD_MOVE_HOME) {
		  reprapworld_keypad_move_home();
	  }
#endif
	  if (abs(encoderDiff) >= ENCODER_PULSES_PER_STEP)
	  {
		  lcdDrawUpdate = 1;
		  encoderPosition += encoderDiff / ENCODER_PULSES_PER_STEP;
		  encoderDiff = 0;
		  timeoutToStatus = millis() + LCD_TIMEOUT_TO_STATUS;
	  }
	  if (LCD_CLICKED)
		  timeoutToStatus = millis() + LCD_TIMEOUT_TO_STATUS;
#endif//ULTIPANEL

#ifdef DOGLCD        // Changes due to different driver architecture of the DOGM display
	  blink++;     // Variable for fan animation and alive dot
	  u8g.firstPage();
	  do
	  {
		  u8g.setFont(u8g_font_6x10_marlin);
		  u8g.setPrintPos(125, 0);
		  if (blink % 2) u8g.setColorIndex(1); else u8g.setColorIndex(0); // Set color for the alive dot
		  u8g.drawPixel(127, 63); // draw alive dot
		  u8g.setColorIndex(1); // black on white
		  (*currentMenu)();
		  if (!lcdDrawUpdate)  break; // Terminate display update, when nothing new to draw. This must be done before the last dogm.next()
	  } while (u8g.nextPage());
#else
	  (*currentMenu)();
#endif

#ifdef LCD_HAS_STATUS_INDICATORS
	  lcd_implementation_update_indicators();
#endif

#ifdef ULTIPANEL
	  if (timeoutToStatus < millis() && currentMenu != lcd_status_screen)
	  {
		  lcd_return_to_status();
		  lcdDrawUpdate = 2;
	  }
#endif//ULTIPANEL
	  if (lcdDrawUpdate == 2) lcd_implementation_clear();
	  if (lcdDrawUpdate) lcdDrawUpdate--;
	  lcd_next_update_millis = millis() + LCD_UPDATE_INTERVAL;
	  }
   
}

void lcd_ignore_click(bool b)
{
  ignore_click = b;
  wait_for_unclick = false;
}

void lcd_finishstatus() {
  int len = strlen(lcd_status_message);
  if (len > 0) {
    while (len < LCD_WIDTH) {
      lcd_status_message[len++] = ' ';
    }
  }
  lcd_status_message[LCD_WIDTH] = '\0';
#if defined(LCD_PROGRESS_BAR) && defined(SDSUPPORT)
#if PROGRESS_MSG_EXPIRE > 0
  messageTick =
#endif
    progressBarTick = millis();
#endif
  lcdDrawUpdate = 2;

#ifdef FILAMENT_LCD_DISPLAY
  message_millis = millis();  //get status message to show up for a while
#endif
}
void lcd_setstatus(const char* message)
{
  if (lcd_status_message_level > 0)
    return;
  strncpy(lcd_status_message, message, LCD_WIDTH);
  lcd_finishstatus();
}
void lcd_setstatuspgm(const char* message)
{
  if (lcd_status_message_level > 0)
    return;
  strncpy_P(lcd_status_message, message, LCD_WIDTH);
  lcd_finishstatus();
}
void lcd_setalertstatuspgm(const char* message)
{
  lcd_setstatuspgm(message);
  lcd_status_message_level = 1;
#ifdef ULTIPANEL
  lcd_return_to_status();
#endif//ULTIPANEL
}
void lcd_reset_alert_level()
{
  lcd_status_message_level = 0;
}

#ifdef DOGLCD
void lcd_setcontrast(uint8_t value)
{
  lcd_contrast = value & 63;
  u8g.setContrast(lcd_contrast);
}
#endif

#ifdef ULTIPANEL
/* Warning: This function is called from interrupt context */
void lcd_buttons_update()
{
#ifdef NEWPANEL
  uint8_t newbutton = 0;
  if (READ(BTN_EN1) == 0)  newbutton |= EN_A;
  if (READ(BTN_EN2) == 0)  newbutton |= EN_B;
#if BTN_ENC > 0
  if ((blocking_enc < millis()) && (READ(BTN_ENC) == 0))
    newbutton |= EN_C;
#endif
  buttons = newbutton;
#ifdef LCD_HAS_SLOW_BUTTONS
  buttons |= slow_buttons;
#endif
#ifdef REPRAPWORLD_KEYPAD
  // for the reprapworld_keypad
  uint8_t newbutton_reprapworld_keypad = 0;
  WRITE(SHIFT_LD, LOW);
  WRITE(SHIFT_LD, HIGH);
  for (int8_t i = 0; i < 8; i++) {
    newbutton_reprapworld_keypad = newbutton_reprapworld_keypad >> 1;
    if (READ(SHIFT_OUT))
      newbutton_reprapworld_keypad |= (1 << 7);
    WRITE(SHIFT_CLK, HIGH);
    WRITE(SHIFT_CLK, LOW);
  }
  buttons_reprapworld_keypad = ~newbutton_reprapworld_keypad; //invert it, because a pressed switch produces a logical 0
#endif
#else   //read it from the shift register
  uint8_t newbutton = 0;
  WRITE(SHIFT_LD, LOW);
  WRITE(SHIFT_LD, HIGH);
  unsigned char tmp_buttons = 0;
  for (int8_t i = 0; i < 8; i++)
  {
    newbutton = newbutton >> 1;
    if (READ(SHIFT_OUT))
      newbutton |= (1 << 7);
    WRITE(SHIFT_CLK, HIGH);
    WRITE(SHIFT_CLK, LOW);
  }
  buttons = ~newbutton; //invert it, because a pressed switch produces a logical 0
#endif//!NEWPANEL

  //manage encoder rotation
  uint8_t enc = 0;
  if (buttons & EN_A) enc |= B01;
  if (buttons & EN_B) enc |= B10;
  if (enc != lastEncoderBits)
  {
    switch (enc)
    {
      case encrot0:
        if (lastEncoderBits == encrot3)
          encoderDiff++;
        else if (lastEncoderBits == encrot1)
          encoderDiff--;
        break;
      case encrot1:
        if (lastEncoderBits == encrot0)
          encoderDiff++;
        else if (lastEncoderBits == encrot2)
          encoderDiff--;
        break;
      case encrot2:
        if (lastEncoderBits == encrot1)
          encoderDiff++;
        else if (lastEncoderBits == encrot3)
          encoderDiff--;
        break;
      case encrot3:
        if (lastEncoderBits == encrot2)
          encoderDiff++;
        else if (lastEncoderBits == encrot0)
          encoderDiff--;
        break;
    }
  }
  lastEncoderBits = enc;
}

bool lcd_detected(void)
{
#if (defined(LCD_I2C_TYPE_MCP23017) || defined(LCD_I2C_TYPE_MCP23008)) && defined(DETECT_DEVICE)
  return lcd.LcdDetected() == 1;
#else
  return true;
#endif
}

void lcd_buzz(long duration, uint16_t freq)
{
#ifdef LCD_USE_I2C_BUZZER
  lcd.buzz(duration, freq);
#endif
}

bool lcd_clicked()
{
  return LCD_CLICKED;
}
#endif//ULTIPANEL

/********************************/
/** Float conversion utilities **/
/********************************/
//  convert float to string with +123.4 format
char conv[8];
char *ftostr3(const float &x)
{
  return itostr3((int)x);
}

char *itostr2(const uint8_t &x)
{
  //sprintf(conv,"%5.1f",x);
  int xx = x;
  conv[0] = (xx / 10) % 10 + '0';
  conv[1] = (xx) % 10 + '0';
  conv[2] = 0;
  return conv;
}

// Convert float to string with 123.4 format, dropping sign
char *ftostr31(const float &x)
{
  int xx = x * 10;
  conv[0] = (xx >= 0) ? '+' : '-';
  xx = abs(xx);
  conv[1] = (xx / 1000) % 10 + '0';
  conv[2] = (xx / 100) % 10 + '0';
  conv[3] = (xx / 10) % 10 + '0';
  conv[4] = '.';
  conv[5] = (xx) % 10 + '0';
  conv[6] = 0;
  return conv;
}

// Convert float to string with 123.4 format
char *ftostr31ns(const float &x)
{
  int xx = x * 10;
  //conv[0]=(xx>=0)?'+':'-';
  xx = abs(xx);
  conv[0] = (xx / 1000) % 10 + '0';
  conv[1] = (xx / 100) % 10 + '0';
  conv[2] = (xx / 10) % 10 + '0';
  conv[3] = '.';
  conv[4] = (xx) % 10 + '0';
  conv[5] = 0;
  return conv;
}

char *ftostr32(const float &x)
{
  long xx = x * 100;
  if (xx >= 0)
    conv[0] = (xx / 10000) % 10 + '0';
  else
    conv[0] = '-';
  xx = abs(xx);
  conv[1] = (xx / 1000) % 10 + '0';
  conv[2] = (xx / 100) % 10 + '0';
  conv[3] = '.';
  conv[4] = (xx / 10) % 10 + '0';
  conv[5] = (xx) % 10 + '0';
  conv[6] = 0;
  return conv;
}

//// Convert float to rj string with 123.45 format
char *ftostr32ns(const float &x) {
	long xx = abs(x);
	conv[0] = xx >= 10000 ? (xx / 10000) % 10 + '0' : ' ';
	conv[1] = xx >= 1000 ? (xx / 1000) % 10 + '0' : ' ';
	conv[2] = xx >= 100 ? (xx / 100) % 10 + '0' : '0';
	conv[3] = '.';
	conv[4] = (xx / 10) % 10 + '0';
	conv[5] = xx % 10 + '0';
	return conv;
}


// Convert float to string with 1.234 format
char *ftostr43(const float &x)
{
  long xx = x * 1000;
  if (xx >= 0)
    conv[0] = (xx / 1000) % 10 + '0';
  else
    conv[0] = '-';
  xx = abs(xx);
  conv[1] = '.';
  conv[2] = (xx / 100) % 10 + '0';
  conv[3] = (xx / 10) % 10 + '0';
  conv[4] = (xx) % 10 + '0';
  conv[5] = 0;
  return conv;
}

//Float to string with 1.23 format
char *ftostr12ns(const float &x)
{
  long xx = x * 100;

  xx = abs(xx);
  conv[0] = (xx / 100) % 10 + '0';
  conv[1] = '.';
  conv[2] = (xx / 10) % 10 + '0';
  conv[3] = (xx) % 10 + '0';
  conv[4] = 0;
  return conv;
}

//Float to string with 1.234 format
char *ftostr13ns(const float &x)
{
    long xx = x * 1000;
    if (xx >= 0)
        conv[0] = ' ';
    else
        conv[0] = '-';
    xx = abs(xx);
    conv[1] = (xx / 1000) % 10 + '0';
    conv[2] = '.';
    conv[3] = (xx / 100) % 10 + '0';
    conv[4] = (xx / 10) % 10 + '0';
    conv[5] = (xx) % 10 + '0';
    conv[6] = 0;
    return conv;
}

//  convert float to space-padded string with -_23.4_ format
char *ftostr32sp(const float &x) {
  long xx = abs(x * 100);
  uint8_t dig;

  if (x < 0) { // negative val = -_0
    conv[0] = '-';
    dig = (xx / 1000) % 10;
    conv[1] = dig ? '0' + dig : ' ';
  }
  else { // positive val = __0
    dig = (xx / 10000) % 10;
    if (dig) {
      conv[0] = '0' + dig;
      conv[1] = '0' + (xx / 1000) % 10;
    }
    else {
      conv[0] = ' ';
      dig = (xx / 1000) % 10;
      conv[1] = dig ? '0' + dig : ' ';
    }
  }

  conv[2] = '0' + (xx / 100) % 10; // lsd always

  dig = xx % 10;
  if (dig) { // 2 decimal places
    conv[5] = '0' + dig;
    conv[4] = '0' + (xx / 10) % 10;
    conv[3] = '.';
  }
  else { // 1 or 0 decimal place
    dig = (xx / 10) % 10;
    if (dig) {
      conv[4] = '0' + dig;
      conv[3] = '.';
    }
    else {
      conv[3] = conv[4] = ' ';
    }
    conv[5] = ' ';
  }
  conv[6] = '\0';
  return conv;
}

char *itostr31(const int &xx)
{
  conv[0] = (xx >= 0) ? '+' : '-';
  conv[1] = (xx / 1000) % 10 + '0';
  conv[2] = (xx / 100) % 10 + '0';
  conv[3] = (xx / 10) % 10 + '0';
  conv[4] = '.';
  conv[5] = (xx) % 10 + '0';
  conv[6] = 0;
  return conv;
}

// Convert int to rj string with 123 or -12 format
char *itostr3(const int &x)
{
  int xx = x;
  if (xx < 0) {
    conv[0] = '-';
    xx = -xx;
  } else if (xx >= 100)
    conv[0] = (xx / 100) % 10 + '0';
  else
    conv[0] = ' ';
  if (xx >= 10)
    conv[1] = (xx / 10) % 10 + '0';
  else
    conv[1] = ' ';
  conv[2] = (xx) % 10 + '0';
  conv[3] = 0;
  return conv;
}

// Convert int to lj string with 123 format
char *itostr3left(const int &xx)
{
  if (xx >= 100)
  {
    conv[0] = (xx / 100) % 10 + '0';
    conv[1] = (xx / 10) % 10 + '0';
    conv[2] = (xx) % 10 + '0';
    conv[3] = 0;
  }
  else if (xx >= 10)
  {
    conv[0] = (xx / 10) % 10 + '0';
    conv[1] = (xx) % 10 + '0';
    conv[2] = 0;
  }
  else
  {
    conv[0] = (xx) % 10 + '0';
    conv[1] = 0;
  }
  return conv;
}

// Convert int to rj string with 1234 format
char *itostr4(const int &xx) {
  conv[0] = xx >= 1000 ? (xx / 1000) % 10 + '0' : ' ';
  conv[1] = xx >= 100 ? (xx / 100) % 10 + '0' : ' ';
  conv[2] = xx >= 10 ? (xx / 10) % 10 + '0' : ' ';
  conv[3] = xx % 10 + '0';
  conv[4] = 0;
  return conv;
}

// Convert float to rj string with 12345 format
char *ftostr5(const float &x) {
  long xx = abs(x);
  conv[0] = xx >= 10000 ? (xx / 10000) % 10 + '0' : ' ';
  conv[1] = xx >= 1000 ? (xx / 1000) % 10 + '0' : ' ';
  conv[2] = xx >= 100 ? (xx / 100) % 10 + '0' : ' ';
  conv[3] = xx >= 10 ? (xx / 10) % 10 + '0' : ' ';
  conv[4] = xx % 10 + '0';
  conv[5] = 0;
  return conv;
}

// Convert float to string with +1234.5 format
char *ftostr51(const float &x)
{
  long xx = x * 10;
  conv[0] = (xx >= 0) ? '+' : '-';
  xx = abs(xx);
  conv[1] = (xx / 10000) % 10 + '0';
  conv[2] = (xx / 1000) % 10 + '0';
  conv[3] = (xx / 100) % 10 + '0';
  conv[4] = (xx / 10) % 10 + '0';
  conv[5] = '.';
  conv[6] = (xx) % 10 + '0';
  conv[7] = 0;
  return conv;
}

// Convert float to string with +123.45 format
char *ftostr52(const float &x)
{
  long xx = x * 100;
  conv[0] = (xx >= 0) ? '+' : '-';
  xx = abs(xx);
  conv[1] = (xx / 10000) % 10 + '0';
  conv[2] = (xx / 1000) % 10 + '0';
  conv[3] = (xx / 100) % 10 + '0';
  conv[4] = '.';
  conv[5] = (xx / 10) % 10 + '0';
  conv[6] = (xx) % 10 + '0';
  conv[7] = 0;
  return conv;
}

// Callback for after editing PID i value
// grab the PID i value out of the temp variable; scale it; then update the PID driver
void copy_and_scalePID_i()
{
#ifdef PIDTEMP
  Ki = scalePID_i(raw_Ki);
  updatePID();
#endif
}

// Callback for after editing PID d value
// grab the PID d value out of the temp variable; scale it; then update the PID driver
void copy_and_scalePID_d()
{
#ifdef PIDTEMP
  Kd = scalePID_d(raw_Kd);
  updatePID();
#endif
}

#endif //ULTRA_LCD
