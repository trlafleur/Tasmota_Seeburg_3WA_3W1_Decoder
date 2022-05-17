
//  Seeburg 3W1- 3WA JuteBox Simulator

/*
 *  Tested on a ESP32
 *  11 May 2022
 * 
 * 
 *  ***************************************************************************
 * The Seebug 3W1-100 wallbox is for 100 selections.
 *  A-k (no I) and 1-10
 *  
 *  It send a series of pulses of about 45ms on and 30ms off. Total of ~75ms
 *  One cycle time is ~2100ms...
 *  
 *  The first set of pulses is for the number and is from 2 to 10 pulses, 
 *  
 *  Then a serial of 10 more pulses if the letter code is B-D-F-H-K or
 *  if the leter code is A-C-E-G-J, then it send one long pulse of 10 * 60ms --> ~600ms
 *  
 *  Then a gap of about 170ms
 *  
 *  Then it send 1 to 5 pulses for the letter code. 1 = A-B, 2 = C-D, ect...
 *  
 *  ***************************************************************************
 *  The Seebug 3WA wallbox is for 200 selections.
 *  A-V 1-20 (no I or O) for Letters and 1-10 for number
 *  
 *  It sends a series of pulses of about 45ms on and 30ms off. Total of ~60ms
 *  One cycle time is ~2100ms...
 *  
 *  The first set of pulses is for the letter code and is from 2 to 21 pulses, 
 *  (a start pulse is added to the series.)
 *  
 *  Then a gap of about 200ms
 *  
 *  Then a series of pulses for the number code, 1 to 10.
 */

//#define Seeburg3AW                // if defined, Seeburg 3WA, if not 3W1

#include <Arduino.h>

// Attach a output pin to a GPIO 
#define Output_PIN 13               // using LED pin, as it give visual status of pulse's

#define CycleTime       10000       // how often to send a pulse stream

// set song code to send here....
#define LetterCode      'a'
#define NumberCode       10


#ifdef Seeburg3WA     // Seeburg 3WA 200
uint32_t Number_Count;              // 1 = A  .... to 10 = K, (No "I")             
uint32_t Letter_Count;
uint32_t MS = 45;
uint32_t GAP = 30;
uint32_t number_gap = 170;

#else   // Seeburg 3A1 100 
uint32_t Number_Count;              // 1 = A  .... to 10 = K, (No "I")             
uint32_t Letter_Count;
uint32_t MS = 45;
uint32_t GAP = 30;
uint32_t letter_gap = 170;
#endif


#ifdef Seeburg3WA

/* *************************************** */
/* **************** 3WA-200 ************** */ 
/* *************************************** */

void Pulse_3AW (uint32_t number_count, uint32_t letter_count, uint32_t MS, uint32_t GAP)
{   
    // Lets send the letter pulse's 1-->20   
    for (uint32_t i = 1; i <= letter_count; ++i)
    {
      printf("Letter Pulse I1: %u\r\n", i);
      digitalWrite(Output_PIN, 1);
      delay (MS);
      digitalWrite(Output_PIN, 0);
      delay (GAP);
    }

    // Send Start pulse's
    printf("Start Pulse\r\n"); 
    digitalWrite(Output_PIN, 1);                         // Sent Start pulse
    delay (MS);
    digitalWrite(Output_PIN, 0);
    delay (GAP);

    // send short gap...
    printf("Number Gap\r\n");
    delay (number_gap);

    // send number pulse's 1-->10
    for (uint32_t i = 1; i <= number_count; ++i)
    {
      printf("Number Pulse I2: %u\r\n", i);
      digitalWrite(Output_PIN, 1);
      delay (MS);
      digitalWrite(Output_PIN, 0);
      delay (GAP);
    }
}

#else

/* *************************************** */
/* **************** 3W1-100 ************** */ 
/* *************************************** */

void Pulse_3A1 (uint32_t number_count, uint32_t letter_count, uint32_t MS, uint32_t GAP)
{   
  bool even_letter = false;
  // A-C-E-G-J = odd, then it send one long pulse, B-D-F-H-K = true, then we send a series of 11 pulse'e
  if (letter_count % 2 == 0)     even_letter = true;    // even (B-D-F-H-K)
    
  for (uint32_t i = 1; i <= number_count; ++i)
  {
    printf("Number Pulse I1: %u\r\n", i);
    digitalWrite(Output_PIN, 1);
    delay (MS);
    digitalWrite(Output_PIN, 0);
    delay (GAP);
  }
    // (even_letter) ? printf("Even \r\n") : printf("Odd \r\n");
    for (uint32_t i = 1; i <= 11; ++i)
    {
      (even_letter) ? printf("Pulse I2: %u, %u\r\n", i, (uint32_t) even_letter ) : printf("Gap I2: %u, %u\r\n", i, (uint32_t) even_letter );
      digitalWrite(Output_PIN, 1);
      delay (MS);
      (even_letter) ? digitalWrite(Output_PIN, 0) : digitalWrite(Output_PIN, 1);
      delay (GAP);
    }
    digitalWrite(Output_PIN, 0);
    
    printf("Letter Gap\r\n");
    delay (letter_gap);
    
  // This select the correct number of pulse for letter code...
  // a-b = 1, c-d = 2...    
    switch ( Letter_Count )
    {
      case 1:
      case 2:
        letter_count = 1;   // a-b
        break;
      case 3:
      case 4:
        letter_count = 2;   // c-d
        break;
      case 5:
      case 6:
        letter_count = 3;   // e-f
        break;
      case 7:
      case 8:
        letter_count = 4;   // g-h
        break;
      case 9:
      case 10:
        letter_count = 5;   // j-k
        break;
    }
    for (uint32_t i = 1; i <= (letter_count) ; ++i)
    { 
      printf("Letter Pulse I3: %u\r\n", i);
      digitalWrite(Output_PIN, 1);
      delay (MS);
      digitalWrite(Output_PIN, 0);
      delay (GAP);
    } 

}
#endif

/* *************************************** */
/* *************** Setup ***************** */
/* *************************************** */
void setup() 
{
  Serial.begin (115200);
  delay(2000);
  printf("\r\nSeebug 3Wx Wallbox Simulator\r\n");
  
  pinMode(Output_PIN, OUTPUT);                         // setup output pin...
  digitalWrite(Output_PIN, LOW);

#ifdef Seeburg3WA
  // Convert letter to a number, a = 61, A = 41, mask off msb, we get a = 1, b = 2...
  Letter_Count = (LetterCode & 0x1f);               // mask off MSB's
  if (Letter_Count > 8)  Letter_Count--;            // No I, so subtract one...
  if (Letter_Count > 13) Letter_Count--;            // No O, so subtract one...
  // do a bounds check
  if ((Letter_Count < 1) || (Letter_Count > 20))   Letter_Count = 1;
  
  Number_Count = NumberCode;
  if ((NumberCode < 1) || (NumberCode > 10))       Number_Count = 1;

#else
  // Convert letter to a number, a = 61, A = 41, mask off msb, we get a = 1, b = 2...
  Letter_Count = (LetterCode & 0x0f);               // mask off MSB's
  if (Letter_Count > 8) Letter_Count--;             // No I, so subtract one...
  // do a bounds check
  if ((Letter_Count < 1) || (Letter_Count > 10))   Letter_Count = 1;
  
  Number_Count = NumberCode;
  if ((NumberCode < 1) || (NumberCode > 10))       Number_Count = 1;
#endif
}

uint32_t TotalTime;
/* *************************************** */
/* *************** Loop ****************** */
/* *************************************** */
void loop() 
{

TotalTime = millis();
#ifdef Seeburg3WA
  uint32_t Letter = Letter_Count;
  if (Letter > 8)         Letter = Letter +1;
  if (Letter_Count > 13)  Letter = Letter +1;
  printf("\r\n*** Sending %c%u\r\n", (Letter | 0x40), Number_Count);
  Pulse_3AW (Number_Count, Letter_Count, MS, GAP);     // Send pulse's

#else
  uint32_t Letter = Letter_Count;
  if (Letter > 8)   Letter = Letter +1;
  printf("\r\n*** Sending: %c%u\r\n", (Letter | 0x40), Number_Count);
  Pulse_3A1 (Number_Count, Letter_Count, MS, GAP);     // send pulse's...
 
#endif

printf("Time: %ums\r\n", (millis() - TotalTime));

  delay(CycleTime);
}
