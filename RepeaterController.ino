/* RepeaterController - A sketch to do a 'minimalist' repeater
 * controller using an Arduino and simple interfacing circuitry.
 * The COR input and PTT output pins are specified by defines. 
 * These should be changed to match your implementation.
 * 
 * ID audio is generated by a DIO pin using the 'tone' function
 * and should be filtered with an RC filter (~!2.5Khz corner 
 * Freq), then padded down and AC coupled before mixing with the 
 * repeater audio path. 
 * 
 * Default values for the ID timer (600 Seconds - 10 minutes) and
 * the squelch tail timer (1 second) are specified by defines.
 * The runtime values of these parameters are stored in variables 
 * and could be changed programatically, if desired (e.g. via the
 * serial port). Of course, you'd have to write that code.
 *
 * The ID Time out timer is implemented using the Time library and
 * is based on the elapsed time counter (unsigned long int seconds)
 * so timeout values are restricted to integer values greater than 
 * one second. 
 * The squelch tail timer is implemented the same way, so it has
 * the same restrictions. 
 *
 * (C) 2012 KB4OID Labs - A division of Kodetroll Heavy Industries
 *
 * All rights reserved, but otherwise free to use for personal use
 * No warranty expressed or implied 
 * This code is for educational or personal use only
 *
 */
 
/*
 * Note: The Arduino Time Libnrary is required, it can be found at
 * http://playground.arduino.cc/Code/Time See the Arduino website:
 * http://arduino.cc/en/Guide/Libraries for info on how to import
 * libraries.
 *
 * Please note that this library will only work as-is with pre-1.5.7
 * versions of the Arduino IDE, because it uses PROGMEM. Current 
 * versions of the Arduino IDE (1.5.7 and later) require type 
 * declarations involving PROGMEM to be 'const'
 */
#include <Time.h>
//#include "pitches.h"

// Here we define the starting values of the ID and Squelch Tail 
// Timers
#define DEFAULT_ID_TIMER 600       // In Seconds
#define DEFAULT_SQ_TIMER 1         // In Seconds

// other misc timer values
#define ID_PTT_DELAY  200       // in mS
#define ID_PTT_HANG   500       // in mS
#define CW_MIN_DELAY  30        // in mS
#define COR_DEBOUNCE_DELAY  50  // in mS

// These values define what the DIO input pin state is when this
// input is active or not. These two cases are inverses of each 
// other, comment out the following define to make COR follow 
// NEGATIVE logic

//#define COR_POSITIVE
#ifdef COR_POSITIVE
  #define COR_ON   HIGH    // DIO Pin state when COR is active
  #define COR_OFF  LOW     // DIO Pin state when COR is not active
#else
  #define COR_ON   LOW     // DIO Pin state when COR is active
  #define COR_OFF  HIGH    // DIO Pin state when COR is not active
#endif

// These values define what the DIO output pin state is when this
// output is active or not. These two cases are inverses of each 
// other, comment out the following define to make PTT follow 
// NEGATIVE logic

#define PTT_POSITIVE
#ifdef PTT_POSITIVE
  #define PTT_ON   HIGH    // DIO Pin state when PTT is active
  #define PTT_OFF  LOW     // DIO Pin state when PTT is not active
#else
  #define PTT_ON   LOW     // DIO Pin state when PTT is active
  #define PTT_OFF  HIGH    // DIO Pin state when PTT is not active
#endif

// Master enum of state machine states
enum CtrlStates {
  CS_START,
  CS_IDLE,
  CS_DEBOUNCE_COR_ON,
  CS_PTT_ON,    
  CS_PTT,    
  CS_DEBOUNCE_COR_OFF,  
  CS_SQT_ON,
  CS_SQT_BEEP,
  CS_SQT,
  CS_SQT_OFF,
  CS_PTT_OFF,    
  CS_ID
};  

enum BeepTypes {
  CBEEP_NONE,
  CBEEP_SINGLE,
  CBEEP_DEDOOP,
  CBEEP_DODEEP,
  CBEEP_DEDEEP
};

// This is where we define what DIO PINs map to what functions
int PTT = 9;      // DIO Pin number for the PTT out - 9
int COR = 12;     // DIO Pin number for the COR in - 12
int CORLED = 7;  // DIO Pin number for the COR indicator LED - 11
int ID_PIN = 8;   // DIO Pin for the ID Audio output tone

// This is where the callsign is mapped in dah/dit/spaces
// e.g. N0S would be 3,1,0,3,3,3,3,3,0,3,3,3,0
// Put your call here, then count the number of elements and set 
// NumElements below
int Elements[] = {
  3,1,3,0,3,1,1,1,0,1,1,1,1,3,0,3,3,3,0,1,1,0,3,1,1,0,3,3,1,1,3,3,0,1,3,1
};

// Here's where we define some of the CW ID characteristics
int NumElements = 36;    // This is the number of elements in the ID
int ID_tone = 1200;      // Audio frequency of CW ID 
int BEEP_tone = 1000;    // Audio frequency of Courtesy Beep
int BeepDuration = 2;    // Courtesy Tone length (in CWID increments)
int CW_TIMEBASE = 50;   // CW ID Speed (This is a delay in mS)
// (50 is about 20wpm)

#define CBEEP_TYPE  CBEEP_SINGLE

// Timer definitions
time_t ticks;            // Current elapsed time in seconds
time_t IDTimer;          // next expire time for ID timer
time_t SQTimer;          // next expire time for Squelch Tail timer

// timer reset value definitions
int SQTimerValue;        // Squelch Tail interval time - in Seconds
int IDTimerValue;        // ID Timer interval time - in Seconds

// rptr run states 
int nextState = 0;  // next state
int rptrState = 0;  // current state
int prevState = 0;  // previous state

// various DIO pin states
boolean CORValue;  // current COR value
boolean pCORValue; // previous COR value
boolean PTTValue;  // current PTT state

boolean Need_ID;   // Whether on not we need to ID

/* This function will reset the ID Timer by adding the 
 * timer interval value to the current elapsed time
 */
void reset_id_timer() {
  IDTimer = ticks + IDTimerValue;
}

void beep(int freq, int duration) {
  // Start Playing the beep
  tone(ID_PIN,freq,duration);

  // Wait for the note to end
  delay(duration);

  // stop playing the beep
  noTone(ID_PIN);
}

/* This function will play the courtesy beep. Blocking call */
void do_cbeep(void) {

  // wait 200 mS 
  delay(ID_PTT_DELAY);

  // Calculate the Courtesy Tone duration
  int BeepDelay = BeepDuration * CW_TIMEBASE;

  switch(CBEEP_TYPE)
  {
    case CBEEP_NONE:
      break;
      
    case CBEEP_DEDOOP:
      beep(ID_tone,BeepDelay*2);
      delay(BeepDelay);
      beep(BEEP_tone,BeepDelay);
      break;

    case CBEEP_DODEEP:
      beep(BEEP_tone,BeepDelay*2);
      delay(BeepDelay);
      beep(ID_tone,BeepDelay);
      break;

    case CBEEP_DEDEEP:
      beep(BEEP_tone,BeepDelay);
      delay(BeepDelay);
      beep(BEEP_tone,BeepDelay);
      break;

    case CBEEP_SINGLE:
    default:
      beep(BEEP_tone,BeepDelay);
      break;
  }   
    
  // A little delay never hurts
  delay(CW_MIN_DELAY);

}

/* this function will play the CW ID *BLOCKING CALL* */
void do_ID() {
  // exit if we do not need to ID yet
  if (!Need_ID)
    return;

  // We turn on the PTT output
  PTTValue = PTT_ON;
  digitalWrite(PTT, PTTValue);

  // wait 200 mS 
  delay(ID_PTT_DELAY);

  // calculate the length of time to wait for the ID tone
  // to quit playing.
  int InterElementDelay = CW_TIMEBASE * 1.3;

  // We Play the ID elements
  for (int Element = 0; Element < NumElements; Element++) {
    if (Elements[Element] != 0) {
      tone(ID_PIN,ID_tone,Elements[Element] * CW_TIMEBASE);
      delay(Elements[Element] * InterElementDelay);
      noTone(ID_PIN);
    }
    else
      delay(InterElementDelay);
    
    // add a little extra inter element delay
    delay(CW_MIN_DELAY);
  }

  // wait 200 mS 
  delay(ID_PTT_DELAY);

  // do courtesy beep
  do_cbeep();

  // we give a little PTT hang time
  delay(ID_PTT_HANG);

  // Turn off the PTT  
  PTTValue = PTT_OFF;
  digitalWrite(PTT, PTTValue);

  // reset the ID timer
  reset_id_timer();    
  // turn off need id
  Need_ID = LOW;
}

/* This function will print current repeater operating states
 * to the serial port. For debuggin purposes only.
 */
void show_state_info() {
  Serial.print ("t:");
  Serial.print (now(),DEC);
  Serial.print (":state:");
  Serial.print (prevState,DEC);
  Serial.print (",");
  Serial.print (rptrState,DEC);
  Serial.print (",");
  Serial.print (nextState,DEC);
  Serial.print (":C:");
  Serial.print (CORValue,DEC);
  Serial.print (",");
  Serial.print (pCORValue,DEC);
  Serial.print (":P:");
  Serial.println (PTTValue,DEC);
}  

/* One time startup init loop */
void setup() {

  // set up the Serial Connection 
  Serial.begin (9600);

  // Determine the size of the Elements array  
  NumElements = sizeof(Elements)/2;

  // Get a current tick timer value
  ticks = now();

  // initialize the timers
  SQTimerValue = DEFAULT_SQ_TIMER;
  IDTimerValue = DEFAULT_ID_TIMER;

  // incase any setup code needs to know what state we are in
  rptrState = CS_START;  
  
  // setup the DIO pins for the right modes 
  pinMode(PTT, OUTPUT);
  pinMode(COR, INPUT);  
  
  // make sure we start with PTT off
  digitalWrite(PTT, PTT_OFF);

  // Get current values for COR
  CORValue = digitalRead(COR);
  pCORValue = CORValue;

  // Here is the first state we jump to
  rptrState = CS_IDLE;
  
  Serial.print ("Start Time:");
  Serial.println (now(),DEC);

  Serial.print ("ID_Tone:");
  Serial.println (ID_tone,DEC);

  Serial.print ("Beep_Tone:");
  Serial.println (BEEP_tone,DEC);

  Serial.print ("NumElements:");
  Serial.println (NumElements,DEC);

  // make sure we ID at startup.
  Need_ID = HIGH;  
}

void get_cor() {
  // Read the COR input and store it in a global
  CORValue = digitalRead(COR);
  // lite the external COR indicator LED
  if (CORValue == COR_ON)
    digitalWrite(CORLED,HIGH);
  else
    digitalWrite(CORLED,LOW);
}

void loop() {

  // grab the current elapsed time
  ticks = now();

  // grab the current COR value
  get_cor();  

  // execute the state machibe   
  switch(rptrState)
  {
    case CS_START:
      // do nothing
      break;
      
    case CS_IDLE:
      // wait for COR to activate, then jump to debounce
      if (CORValue == COR_ON) {
        pCORValue = CORValue;
        rptrState = CS_DEBOUNCE_COR_ON;
      }
      
      // look for ID timer expiry
      if (ticks > IDTimer && Need_ID)
        rptrState = CS_ID;
        
      break;
      
    case CS_DEBOUNCE_COR_ON:
      // ideally we will delay here a little while and test
      // the current value (after the delay) with the pCORValue 
      // to prove its not a flake
      delay(COR_DEBOUNCE_DELAY);
      if ( pCORValue != digitalRead(COR)) {
        rptrState = CS_IDLE;  // FLAKE - bail back to IDLE
      } else {
        nextState = CS_PTT;    // where we will go after PTT_ON
        rptrState = CS_PTT_ON;  // good COR - PTT ON
      }
      break;

    case CS_PTT_ON:
      // turn on PTT
      PTTValue = PTT_ON;
      digitalWrite(PTT, PTTValue);
      // jump to the desired next state (set by the previous state)
      rptrState = nextState;
      break;

    case CS_PTT:
      // we stay in this state and wait for COR to DROP (de-activate), 
      // then jump to debounce
      if (CORValue != COR_ON)
        rptrState = CS_DEBOUNCE_COR_OFF;
      break;
      
    case CS_DEBOUNCE_COR_OFF:
      // ideally we will delay here a little while and test
      // the result with the pCORValue to prove its not a flake
      delay(COR_DEBOUNCE_DELAY);
      if ( CORValue != digitalRead(COR))
        rptrState = CS_PTT;  // FLAKE - ignore
      else
        rptrState = CS_SQT_ON;  // COR dropped, go to sqt
      break;

    case CS_SQT_ON:
      // set SQTimer active
      SQTimer = ticks + SQTimerValue;
      // jump to next state
      rptrState = CS_SQT_BEEP;
      break;
      
    case CS_SQT_BEEP:
      // Do the courtesy beep
      do_cbeep();
      // jump to CS_SQT to wait for SQT timer
      rptrState = CS_SQT;
      break;
      
    case CS_SQT:
      // We stay in this state until SQTimer expires, then
      // we jump to to CS_SQT_OFF
      if (ticks > SQTimer)
        rptrState = CS_SQT_OFF;
      if (CORValue == COR_ON) {
        pCORValue = CORValue;
        rptrState = CS_DEBOUNCE_COR_ON;
      }
      break;

    case CS_SQT_OFF:
      // set SQTail not active
      nextState = CS_IDLE;
      rptrState = CS_PTT_OFF;
      // We just got done transmitting, so we need
     // to ID next time the ID timer expires
      Need_ID = HIGH;
      break;

    case CS_PTT_OFF:
      // Turn the PTT off
      PTTValue = PTT_OFF;
      digitalWrite(PTT, PTTValue);
      // jump to the desired next state (set by the previous state)
      rptrState = nextState;
      break;

    case CS_ID:
      // Go do the ID (this is a *blocking* call)
      do_ID();
      // back to the IDLE state when
      rptrState = CS_IDLE;
      // we have satisfied our need to ID, so NO
      Need_ID = LOW;
      break;
      
    default:
      // do nothing
      break;
  }

  // Comment this out to stop reporting this info
  show_state_info();

  // capture the current machine state and COR value and
  // save as 'previous' for the next loop.
  prevState = rptrState;
  pCORValue = CORValue;
  
}

