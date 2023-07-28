/*
  xsns_123_Seeburg_3Wx.ino - Seeburg 3WA-3W1 Wallbox sensor support for Tasmota)

  tom@lafleur.us
  Copyright (C) 2022  Tom Lafleur and Theo Arends

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* **************************************************************************************
 * CHANGE LOG:
 *
 *  DATE        Who REV  DESCRIPTION
 *  ----------- --- ---  ----------------------------------------------------------
 *  12-May-2022 TRL 1.0   TRL - first build
 *  18-May-2022 TRL 1.0a  Update comments and re-sync LED if needed
 *  24-May-2022 TRL 1.0b  Fixed timeout timer
 *  01-Jun-2022 TRL 1.0c  Remove addlog from interrupt logic
 *  23-Jul-2023 TRL 1.0d  Move to Tasmota 13.x.x.x
 *                        change from uint8_t --> bool Xsns123(uint32_t function)
 *  18-Jul-2023 TRL 1.0e  Remove sending info in telperod
 *
 *  Notes:  1)  Tested with TASMOTA  13.0.0.3dev 23-Jul-2023
 *          2)  ESP32, ESP32S3
 *          3)  All addlog statements have been commented out of interrupt functions
 *                as they can cause reboots. (they were use for debug only)
 *
 *
 *    TODO:
 *          1)  Add coin relay outputs, with MQTT commands.
 *          2)  Add command to play a song to MP3 player
 *          3)
 *
 *    tom@lafleur.us
 *
 */
/* ************************************************************************************** */

/* **************************************************************************************
 *  Notes: 
 *
 * This program expects a pulse on a pin from a Seeburg 3WA or 3W1 Wallbox...
 * it decodes the pulses and sends the decoded information on as MQTT JSON object
 * 
 * It support the 3WA-200 or the 3W1-100 wallboxies. One or the other is selected
 *  in the configure module or template. 'Seeburg 3WA' or 'Seeburg 3W1'. There is also 
 *  an status LED that toggles on every pulse edge. 'Seeburg LED'
 *
 * The wallboxes send out a 24V AC pulse, that must be rectified and converted to 3.3v
 *
 * This code was adapted from Derek Konigs excellent post at:
 *    http://hecgeek.blogspot.com/2017/10/wall-o-matic-interface-1.html
 *
 * 
 * 
 * MQTT JSON frame:
 * ---- Real Time...
 * {
 * "Wallbox":{
 *     "Number_Index":10,
 *     "Letter_Index":1,
 *     "Selection_Index":10,
 *     "Selection":"A10"
 *  }
 * }
 * 
 * ---- At Telemetry period
 * {
 *  "Time":"2022-05-17T10:54:28",
 *   "Wallbox":{
 *     "Model":"3W1",
 *     "Last_Selection":"A10"
 *  }
 * }
 * 
 * 
 * 
 *   There is a very small chance of a race condition, from when the code has finish decoding 
 *    the pulse stream in the ISR until we send the data out on the one second loop. One could  
 *    quickly push another set of button during that time missing a selection, but unlikely.
 * 
 *   
 *
************************************************************************************** */

 
/* ************************************************************************************* */
/*
Changes made to Tasmota base code... See integration notes...

include/tasmota/tasmota_template.h  13.0.0.3

line 214
GPIO_SB_3WA, GPIO_SB_3W1, GPIO_SB_LED,                          // <---------------  TRL

line 475
D_SENSOR_SB_3WA "|" D_SENSOR_SB_3W1 "|" D_SENSOR_SB_LED "|"     // <---------------  TRL 

line 559
#ifdef USE_SB3Wx
  AGPIO(GPIO_SB_3WA),                  // xsns_123              // <---------------  TRL
  AGPIO(GPIO_SB_3W1),
  AGPIO(GPIO_SB_LED),
#endif

tasmota/language/en_GB.h
at line 937
// XSNS_123 Seeburg 3WA Jukebox
#define D_SENSOR_SB_3WA        "Seeburg 3WA"                    // <---------------  TRL
#define D_SENSOR_SB_3W1        "Seeburg 3W1"
#define D_SENSOR_SB_LED        "Seeburg Led"
*/


#ifdef USE_SB3Wx
#define XSNS_123 123

/*********************************************************************************************\
 * Seeburg 3WA and 3w1 wallbox decoder to MQTT
\*********************************************************************************************/

/* ********************************************** */
// Forward references
void wb_pulse_list_clear();
bool wb_pulse_list_tally_3w1_100(char *letter, uint32_t *number);
bool wb_pulse_list_tally_3wa_200(char *letter, uint32_t *number);
void wp_pulse_gpio_intr_handler(void *arg);
void wb_pulse_timer_func(void* arg);  // void* arg


/* ******************************************************** */
// Our Local global variables...
typedef struct wb_selection_pulse 
{
    uint32_t elapsed;                           // time since the end of the last pulse
    uint32_t duration;                          // duration of the current pulse
} wb_selection_pulse;

typedef enum wallbox_type 
{
    UNKNOWN_WALLBOX = 0,
    SEEBURG_3W1_100,
    SEEBURG_3WA_200,
    MAX_WALLBOX_TYPES
} wallbox_type;

#define MAX_WB_SELECTION_PULSES 64              // Maximum accumulated length of a pulse stream 
#define DEBOUNCE_GAP 10000                      // Minimum allowable pulse gap, in microseconds
#define D_Wallbox "Wallbox"
#define MaxTimeout 3000
//#define Debug_SB_ISR                            // debug in ISR loop

const char  WB_LETTERS[] = "ABCDEFGHJKLMNPQRSTUV";    // missing 'I' and 'O' as they are not used

volatile    bool SBLedState =     false;
volatile    bool MQTT_Send_Flag = false;

volatile    wallbox_type  wb_active_type;
volatile    uint32_t      wb_pulse_last_value;
volatile    uint32_t      wb_pulse_last_time;
volatile    uint32_t      wb_pulse_index;
volatile    uint32_t      wb_pulse_timer;
volatile    uint32_t      currentPulseValue;

wb_selection_pulse wb_pulse_list[MAX_WB_SELECTION_PULSES];

uint32_t    timeout = MaxTimeout;
char        letter;
uint32_t    letter_count;
uint32_t    number;


#include "esp_timer.h"
esp_timer_handle_t  _timer;

portMUX_TYPE SBmux = portMUX_INITIALIZER_UNLOCKED;


/* ******************************************************** */
void IRAM_ATTR Ticker_detach() 
{
  if (_timer) 
  {
    esp_timer_stop(_timer);
    esp_timer_delete(_timer);
    _timer = nullptr;
  }
}


/* ******************************************************** */
void IRAM_ATTR Ticker_attach_ms(uint32_t milliseconds, bool repeat, esp_timer_cb_t callback, uint32_t arg) 
{
  esp_timer_create_args_t _timerConfig;
  _timerConfig.arg = reinterpret_cast<void*>(arg);
  _timerConfig.callback = callback;
  _timerConfig.dispatch_method = ESP_TIMER_TASK;
  _timerConfig.name = "SB_Ticker";

  if (_timer) 
  {
    esp_timer_stop(_timer);
    esp_timer_delete(_timer);
  }

  esp_timer_create(&_timerConfig, &_timer);

  if (repeat) 
    {esp_timer_start_periodic(_timer, milliseconds * 1000ULL);} 
  else 
    {esp_timer_start_once(_timer, milliseconds * 1000ULL);}
}


/* ******************************************************** */
void IRAM_ATTR wb_pulse_list_clear()
{
    wb_pulse_index = 0;
    memset (wb_pulse_list, 0x00, sizeof(wb_pulse_list));
}


/* ******************************************************** */
// this is called from timer timeout in ISR...
void IRAM_ATTR wb_pulse_timer_func(void* arg)
{
    bool result;

    //noInterrupts();                                 // Disable interrupts while we process the results
    portENTER_CRITICAL_ISR(&SBmux);

    // lets stop the timer...
    Ticker_detach();

    if (wb_active_type == SEEBURG_3W1_100) 
    { result = wb_pulse_list_tally_3w1_100(&letter, &number);}

    else if(wb_active_type == SEEBURG_3WA_200) 
    { result = wb_pulse_list_tally_3wa_200(&letter, &number);} 

    else 
    {
      result = false;                         // wallbox selection error if here
      ////AddLog( LOG_LEVEL_INFO, PSTR("%s"), "---> Error --> No Wallbox Selected!\n");
    }

#if 0
    uint32_t i;
    ////AddLog( LOG_LEVEL_INFO, PSTR("%s"), "----BEGIN PULSES---\n");
    for (i = 0; i < wb_pulse_index; i++) 
    {
       ////AddLog( LOG_LEVEL_INFO, PSTR("%d, %d\n", wb_pulse_list[i].elapsed / 1000, wb_pulse_list[i].duration / 1000));  
    }
    ////AddLog( LOG_LEVEL_INFO, PSTR("%s"), "----END PULSES---\n");
#endif

    wb_pulse_list_clear();
    SBLedState  =  false;

    if (result) 
    {
      ////AddLog( LOG_LEVEL_INFO, PSTR("---> Selection: %c%d"), letter, number); 

      // Initialize state variables
      wb_pulse_last_value = 0;
      wb_pulse_last_time  = micros();
      memset ( (void*) &wb_pulse_timer, 0x00, sizeof(wb_pulse_timer));

      MQTT_Send_Flag = true;        // we have an event to send, set flag
        
    } else 
    { 
      ////AddLog( LOG_LEVEL_INFO, PSTR("%s"), "--> Timeout decode error\r\n"); // <-----------------------  TRL
    }   // error if here
  
  //interrupts();                    // Re-enable interrupts
   portEXIT_CRITICAL_ISR(&SBmux);
}


/* ******************************************************** */
 /* Count the signal pulses.
 *  Wallbox: Seeburg Wall-O-Matic 3W-200
 */
// This is called from wb_pulse_timer_func(void* arg) within an ISR
bool IRAM_ATTR wb_pulse_list_tally_3wa_200(char *letter, uint32_t *number)
{
     uint32_t i;
     uint32_t p1 = 0;
     uint32_t p2 = 0;
     bool delimiter = false;
     char letter_val;
     uint32_t number_val;

    for (i = 0; i < wb_pulse_index; i++) 
    {
        // Gap delimiter is ~230ms, so use 125ms to be safe
        if (p1 > 0 && !delimiter && wb_pulse_list[i].elapsed > 125000) 
        {
            delimiter = true;
            ////AddLog( LOG_LEVEL_INFO, PSTR("%s"), "---- Number Gap ----\r\n");
        }
        if (!delimiter) {p1++;}
        else            {p2++;}
    }

    if (p1 < 2 || p1 > 21 || p2 < 1 || p2 > 10) 
    {
        // Reject invalid or incomplete values
        return false;
    }

    // convert pulses to numeric values
    letter_val = WB_LETTERS[p1 - 2];
    letter_count = p1-1;
    number_val = p2;

    if (letter && number)
    {
        *letter = letter_val;
        *number = number_val;
    }
    return true;
}


/* ******************************************************** */
 /* Count the signal pulses.
 *  Wallbox: Seeburg Wall-O-Matic 3W1-100
 */
// This is called from wb_pulse_timer_func(void* arg) within an ISR
bool IRAM_ATTR wb_pulse_list_tally_3w1_100(char *letter, uint32_t *number)
{
    uint32_t i;
    uint32_t p1 = 0;
    uint32_t p2 = 0;
    bool delimiter = false;
    char letter_val;
    uint32_t number_val;

//AddLog( LOG_LEVEL_DEBUG, PSTR("Pulse_Index: %u\n"), wb_pulse_index);

    for (i = 0; i < wb_pulse_index; i++)
    {
        // Number Pulse delimiter is ~800ms, so use 500 to be safe
        if (p1 > 0 && !delimiter && wb_pulse_list[i].duration > 500000) 
        {
            delimiter = true;
            ////AddLog( LOG_LEVEL_DEBUG, PSTR("%s"), "---- Pulse Gap ----\n");
        }
        else {
            // Letter Gap delimiter is ~170ms, so use 100 to be safe
            if (p1 > 0 && !delimiter && wb_pulse_list[i].elapsed > 100000) 
            {
                delimiter = true;
                ////AddLog( LOG_LEVEL_DEBUG, PSTR("%s"), "---- Letter Gap ----\n");  
            }
            if (!delimiter) {p1++;}
            else            {p2++;}
        }
    }

    if (p2 < 1 || p2 > 5) 
    {
        // Reject invalid or incomplete values
        return false;
    }

    if (p1 >= 1 && p1 <= 10) 
    {
        number_val = p1;
        letter_count = ((p2 - 1) * 2) + 1;
        letter_val = 'A' + (p2 - 1) * 2;
    }

    else if (p1 >= 12 && p1 <= 21) 
    {
        number_val = p1 - 11;
        letter_count = ((p2 - 1) * 2) + 2;
        letter_val = 'A' + ((p2 - 1) * 2) + 1;
    }

    else 
    {
        // Reject invalid pulse P1 value
        return false;
    }

    // This wallbox skips the letter 'I'
    if (letter_val > 'H') { letter_val++; }

    if (letter && number) 
    {
        *letter = letter_val;
        *number = number_val;
    }
    return true;
}


/* ******************************************************** */
/* ********************** ISR ***************************** */
/* ******************************************************** */
void IRAM_ATTR SB_Isr(void)
{    
  // Optional external LED to show each pulse edge
  if (PinUsed(GPIO_SB_LED)) 
  {    
    SBLedState = (SBLedState == 0) ? 1 : 0;
    digitalWrite(Pin(GPIO_SB_LED), SBLedState);
  } 

  timeout = MaxTimeout;                                 // cycle time is ~2100ms, so we set timeout to 3000ms

  if (PinUsed(GPIO_SB_3W1)) currentPulseValue = digitalRead(Pin(GPIO_SB_3W1));
  if (PinUsed(GPIO_SB_3WA)) currentPulseValue = digitalRead(Pin(GPIO_SB_3WA));

  uint32_t currentPulseTime = micros();                 // system_get_time();

  // Do something
  if(currentPulseValue != wb_pulse_last_value) 
  {      
    // lets stop the timer...
    Ticker_detach(); 

    uint32_t elapsed = currentPulseTime - wb_pulse_last_time;
    if (currentPulseValue == 1) 
    {
      ////AddLog( LOG_LEVEL_INFO, PSTR("Gap:   %dms"), elapsed / 1000);
      if (wb_pulse_list[wb_pulse_index].duration == 0) 
      {
        wb_pulse_list[wb_pulse_index].elapsed = elapsed;
      } 
      
      else 
      {
       // Error
      ////AddLog( LOG_LEVEL_INFO, PSTR("%s"), "*** In ISR, Pulse P1 error\n"); 
      }
    }

    else if (currentPulseValue == 0) 
    {
      ////AddLog( LOG_LEVEL_INFO, PSTR("Pulse: %dms"), elapsed / 1000); 
      if (wb_pulse_list[wb_pulse_index].elapsed > 0) 
      {
        if (wb_pulse_index > 0 && wb_pulse_list[wb_pulse_index].elapsed < DEBOUNCE_GAP) 
        {
          // If this pulse had a negligible gap from the previous
          // pulse, then merge them.
          wb_pulse_list[wb_pulse_index - 1].duration +=
          wb_pulse_list[wb_pulse_index].elapsed  + elapsed;

          wb_pulse_list[wb_pulse_index].elapsed = 0;
          wb_pulse_list[wb_pulse_index].duration = 0;
          ////AddLog( LOG_LEVEL_INFO, PSTR("%s"), "*** In ISR, Debounce\n");
        } 
        
        else 
        {
          wb_pulse_list[wb_pulse_index].duration = elapsed;
          wb_pulse_index++;
        }
      } 
        
      else 
      {
       // Error
       ////AddLog( LOG_LEVEL_INFO, PSTR("%s"), "*** In ISR, Pulse P2 error\n"); 
      }
    }

    if (wb_pulse_index >= MAX_WB_SELECTION_PULSES) 
    {
      ////AddLog( LOG_LEVEL_INFO, PSTR("%s"), "*** In ISR, Max Pulse error\n");
      wb_pulse_list_clear();
    } 
    
    else 
    {
      // Count existing pulses
    if (wb_active_type == SEEBURG_3W1_100) 
      {
        if (wb_pulse_list_tally_3w1_100 (0, 0)) 
            {timeout = 250;}
      } 
      
    else if(wb_active_type == SEEBURG_3WA_200) 
      {
        if (wb_pulse_list_tally_3wa_200 (0, 0)) 
            {timeout = 250;}
      } 
      
    else 
      {
        // Unknown wallbox type
        ////AddLog( LOG_LEVEL_INFO, PSTR("%s"), "*** In ISR, Wallbox Undefined\n");
        wb_pulse_list_clear();
      }
    }

    wb_pulse_last_value = currentPulseValue;
    wb_pulse_last_time  = currentPulseTime; 
  
    // this will start or restart our timer
    Ticker_attach_ms(timeout, false, wb_pulse_timer_func, 0); 
  }
}


/* ******************************************************** */
void SB_Init(void)
{
 // This is an optional LED indicator of Seeburg pulse's from the wallbox
  if (PinUsed(GPIO_SB_LED)) 
  {
    SBLedState  =  false;
    pinMode(Pin(GPIO_SB_LED), OUTPUT);   // set pin to output
    digitalWrite(Pin(GPIO_SB_LED), 0);   // turn off led for now
  }

  if (PinUsed(GPIO_SB_3WA) || PinUsed(GPIO_SB_3W1)) 
  {
    // Initialize state variables
    wb_pulse_last_value = 0;
    wb_pulse_last_time  = micros();
    memset ( (void*) &wb_pulse_timer, 0x00, sizeof(wb_pulse_timer));
    wb_pulse_list_clear();

    if (PinUsed(GPIO_SB_3WA) && PinUsed(GPIO_SB_3W1))
    {
      AddLog( LOG_LEVEL_INFO, PSTR("%s"), "*** Only one Wallbox Model can be assigned!");
    }

    else
    {
    if (PinUsed(GPIO_SB_3WA))
      {
        wb_active_type      = SEEBURG_3WA_200;
        AddLog( LOG_LEVEL_DEBUG, PSTR("%s"), "*** 3WA Interrupt added!");
        attachInterrupt(Pin(GPIO_SB_3WA), SB_Isr, CHANGE);
      }

    else if (PinUsed(GPIO_SB_3W1))
      {
        wb_active_type      = SEEBURG_3W1_100;
        AddLog( LOG_LEVEL_DEBUG, PSTR("%s"), "*** 3W1 Interrupt added!");
        attachInterrupt(Pin(GPIO_SB_3W1), SB_Isr, CHANGE);  
      }
    }
  }
}   // end of: void SB_Init(void)


/* ******************************************************** */
/* ******************************************************** */
void SB_EVERY_SECOND(void)
{
   if (PinUsed(GPIO_SB_3WA) || PinUsed(GPIO_SB_3W1))    // check to make sure we have a pin assigned
   {
     if (MQTT_Send_Flag == true)                        // send MQTT event...
     {
      MQTT_Send_Flag = false;
      ResponseClear();
      Response_P(PSTR("{\"Wallbox\":{"));
      ResponseAppend_P(PSTR("\"Number_Index\":%u,"), number );
      ResponseAppend_P(PSTR("\"Letter_Index\":%u,"), letter_count );
      ResponseAppend_P(PSTR("\"Selection_Index\":%u,"), ((letter_count - 1) * 10) + number );
      ResponseAppend_P(PSTR("\"Selection\":\"%c%u\""), letter, number );
      ResponseJsonEndEnd();
      MqttPublishPayloadPrefixTopicRulesProcess_P(STAT, D_RSLT_SENSOR, (char*) TasmotaGlobal.mqtt_data.c_str());
      ResponseClear();
     }
   }             
}   // end of: SBCtrEverySecond(void)
 

/************************************************************\
 * MQTT and Webserver display
\************************************************************/
void SB_Show(bool json)
{
     if (PinUsed(GPIO_SB_3WA) || PinUsed(GPIO_SB_3W1))  
     {
       if (json)                // send MQTT
       {
       //  ResponseAppend_P(PSTR(",\"Wallbox\":{"));
       // if (wb_active_type == SEEBURG_3W1_100)  ResponseAppend_P(PSTR("\"Model\":\"%s\","), "3W1" );
       // if (wb_active_type == SEEBURG_3WA_200)  ResponseAppend_P(PSTR("\"Model\":\"%s\","), "3WA" );
       // ResponseAppend_P(PSTR("\"Last_Selection\":\"%c%u\""), letter, number);
       // ResponseJsonEnd();

 #ifdef USE_WEBSERVER
       }   // end of:  if (json)

       else                           // display Web status
       { 
         WSContentSend_PD(PSTR("{s}" "Last Selection:" "{m}%c%u{e}"), letter, number);
        
 #endif      // end of: USE_WEBSERVER
       }   // end of: if (json) else
     }   // end of: if (PinUsed(GPIO_SB_3WA) || PinUsed(GPIO_SB_3W1)) 
}   // end of: 


/*********************************************************************************************\
 * Commands
\*********************************************************************************************/
bool SB_Commands(void)
{
  char argument[XdrvMailbox.data_len];
  AddLog( LOG_LEVEL_DEBUG, PSTR("%s: %u"), "ArgC", ArgC());
  AddLog( LOG_LEVEL_DEBUG, PSTR("%s: %u"), "Payload", XdrvMailbox.payload );
  AddLog( LOG_LEVEL_DEBUG, PSTR("%s: %u"), "Payload Length", XdrvMailbox.data_len );
  AddLog( LOG_LEVEL_DEBUG, PSTR("%s: %u"), "Arg 1", strtol(ArgV(argument, 1), nullptr, 10) );
  AddLog( LOG_LEVEL_DEBUG, PSTR("%s: %u"), "Arg 2", strtol(ArgV(argument, 2), nullptr, 10) );
  //AddLog( LOG_LEVEL_DEBUG, PSTR("%s: %u"), "Arg 3", strtol(ArgV(argument, 3), nullptr, 10) );
  //AddLog( LOG_LEVEL_DEBUG, PSTR("%s: %u"), "Index", XdrvMailbox.index );

  if (ArgC() > 1)                                       // we always expect a 2nd parameter
  {
    
  }

  return true;

}   // end of bool Xsns123Cmnd(void)


/*********************************************************************************************\
 * Interface
\*********************************************************************************************/
bool Xsns123(uint32_t function)
{
  bool result = false;
  
  if (FUNC_INIT == function) 
  {
    SB_Init();
  }
  else 
  {
    switch (function) 
    {
      case FUNC_EVERY_250_MSECOND:
        break;

      case FUNC_EVERY_SECOND:

        TasmotaGlobal.seriallog_timer = 600;                // DEBUG Tool   <---------- TRL

        SB_EVERY_SECOND();
        break;

      case FUNC_COMMAND_SENSOR:
        if (XSNS_123 == XdrvMailbox.index) 
        {
          result = SB_Commands();
        }
        break;

      case FUNC_JSON_APPEND:
        SB_Show(true);
        break;

#ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:
        SB_Show(false);
        break;

#endif // USE_WEBSERVER

      case FUNC_PIN_STATE:
      break;
    }
  }
  return result;
}   // end of: bool Xsns123(uint8_t function)

#endif    // end of USE_SB3Wx

/* ************************* The Very End ************************ */
