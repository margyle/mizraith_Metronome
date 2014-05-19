
// Metronome Project
// author:  Red Byer
// date:   4/18/2014   INITIAL RELEASE
// source link:   github.com/mizraith 
//
//
// Sight and Sound Metronome
//  Neopixels for visual metronome
//  LCD display to display  bpm and italian music speed e.g. "Allegro"
//  Pull solenoid rigidly mounted clicking sound
//  2x buttons (debounced) to toggle sound/lights
//  Rotary encoder for setting temp.
//
// FEATURES:
//   Clicking sound (ON/OFF) set by button
//   LEDs can do a cylon sweep or strobe mode or off (set by button)
//   LCD readout (16x2) with RGB backlighting.  Displays current bpm and name of tempo.
//   EEPROM backed, remembers last state on turn-on
//   LED colors are mapped to BPM for pleasing visual effect.  Faster tempos blue-shift.
//
//
//  PROJECT LICENSE
//    Use of the code in this project is free and unrestricted with no warrantly implied. 
//    However, if you use substantial portions of the code, I ask that you include a
//    citation including my name and source link.  Enjoy!
//


#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include "Metronome.h"
#include "EEPROMAnything.h"



//---------SYSTEM LEVEL CONTROLS -----------------
#define LCD_CONNECTED
#define NEOPIXELS_CONNECTED
//------------------------------------------------

//####################################################################
//   PINOUTS 
//####################################################################
// for Arduino Nano 3.0


//We're using PCINT2 vector, but we could also move one of these
//pins to D3 and use the INT1 vector.  Instead we chose
//to save INT1 for future needs.  
#define encoder0PinA 2
#define encoder0PinB 3
#define BUTTON1    4  //MOVE TO D4 PCINT20   //ADC6 through 1k resistor with 0.1uF to GND for debounce
#define BUTTON2    5 //MOVE TO D5  PCINT21  //ADC7through 1k resistor with 0.1uF to GND for debounce
#define LCD_EN     6      //typ D11 for LCD examples
#define LCD_RS     7            //typ D12
#define NEOPIXEL_PIN 8
#define LCD_RED     9    //PWM port
#define LCD_GREEN   10   //PWM port
#define LCD_BLUE    11   //PWM port

//#define SD_CS      10  // Chip Select pin for the SD Card
//#define SD_MOSI    11  // SPI Port
//#define SD_MISO    12  // SPI Port
//#define SD_SCK     13  // SPI Port
#define LED_PIN    13   // Arduino Nano white LED

#define LCD_D4     A0     //typ D5 for LCD examples
#define LCD_D5     A1     //typ D4 for LiquidCrystal examples
#define LCD_D6     A2     //typ D3
#define LCD_D7     A3     //typ D2
//#define I2C_SDA    A4
//#define I2C_SCL    A5
#define SOLENOID   A5     //Drives a transistor when runs a solenoid for the tick-tick sounds





//#############################################
// CONSTANTS AND GLOBALS
//assuming 32bit per color RGB
//#############################################
#define DEBUG true

#define STRING_WIDTH 17

#define NUMPIXELS 16
#define LEFT_MIDDLE 7
#define RIGHT_MIDDLE 8
#define ENCODER_SENSITIVITY_DIVIDER 4
#define BLINK_LENGTH 50        //in ms

#define LEDs_OFF 0
#define LEDs_SWEEP 1
#define LEDs_PULSE 2
#define MAX_LED_SETVALUE 3

#define EEPROM_SETTINGS_ADDRESS 42
boolean             Settings_Did_Change = false;
unsigned long       Settings_Did_Change_Timeout = 0;
const unsigned long Settings_Write_Out_Delay = 10000;


boolean             Brightness_did_change = false;
boolean             In_Set_Brightness_Mode = false;
unsigned long       Set_Brightness_Timer = 0;
const unsigned long Set_Brightness_Timeout = 5000;

boolean  BPM_did_change = true;

char TopLineString[STRING_WIDTH] = "";
char BottomLineString[STRING_WIDTH] = "";

boolean       OnBeat = false;
unsigned long MilliCounter = 1000;   //default value
unsigned long currentmillis = 0;
unsigned long LastBeat = 0;

int8_t        LeadPosition = 8;
boolean       DirectionRight = true;
unsigned long Last_Sweep_Update = 0;
uint16_t      Sweep_Step_Delay = 25;


boolean             Button1_Flag = false;
unsigned long       Button1_Time = 0;
boolean             Button2_Flag = false;
unsigned long       Button2_Time = 0;
boolean             Ignore_Next_Button_Interrupt = false;  //Needed for double button depress, since that generates an INT
const unsigned long Buttons_Same_Time_Delta = 250;


//##############################################
// USER SETTINGS
//##############################################
struct user_settings  {
        boolean SOUND_ON;        
        uint8_t LED_SETTING;    
        uint8_t LED_BRIGHTNESS;       //0-255    1 is OFF    255 (or 0) is MAX
        uint8_t bpm;
     } SETTINGS ;
    
//from metronome.h, just a 3-byte struct
uint32_t BPM_Color = 0x000000;

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);





//#############################################
// COLORS
//assuming 32bit per color RGB
//#############################################
#define RED  0xFF0000
#define ORANGE 0xFF5500
#define YELLOW 0xFFFF00
#define GREEN 0x00FF00
#define AQUA 0x00FFFF
#define BLUE 0x0000FF
//#define INDIGO 0x3300FF
#define VIOLET 0xFF00FF
#define WHITE  0xFFFFFF
#define BLACK  0x000000


//        prog_char * stringaddress;
//        stringaddress =  (prog_char *)pgm_read_word_near(  &(days_of_the_week[DayOfTheWeek])  );
//        strcat_P(stringbuffer, stringaddress);




//----------- LCD -----------------------------------
// include the library code:
//
// BAD NEWS -- IT LOOKS LIKE THE LiquidCrystal library and the delay libraries use
//   Timer/Counter 0

#ifdef LCD_CONNECTED

#include <LiquidCrystal.h>

// SEE THE UNIFIED PINOUT SECTION

//no pot needed on contrast if you hardwire
// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);   // (was lcd(12, 11, 5, 4, 3, 2)

//necessary for interrupt robustness
//volatile int cursor_x = 0;
//volatile int cursor_y = 0;

#endif 
//----------------------------------------------------




//####################################################################################
// INTERRUPT SERVICE ROUTINES
//####################################################################################

volatile int16_t encoder0Pos = 0;
/* If pinA and pinB are both high or both low, it is spinning
   * forward. If they're different, it's going backward.
   */
ISR( INT0_vect) {
   //ASSUMES WE ARE ONLY INTERRUPTING ON OUR D6 PIN AND NOTHING ELSE
   //This is 'half" resolution...but that's fine for our use here.
   //one of the pins D0 to D7 has changed) 
   
   //digitalWrite(LED_PIN, !digitalRead(LED_PIN) );   
   if (digitalRead(encoder0PinA) == digitalRead(encoder0PinB)) {
      encoder0Pos++;
   } else {
      encoder0Pos--;
   }
}


//We're using PCINT2 vector, but we could also move one of these
//pins to D3 and use the INT1 vector.  Instead we chose
//to save INT1 for future needs.  
/* If pinA and pinB are both high or both low, it is spinning
   * forward. If they're different, it's going backward.
   */
ISR( PCINT2_vect) {
    //immediately capture pin strate on D4 and D5
    if(  !digitalRead(BUTTON1)  ) {
        //low = pressed
        if (!Ignore_Next_Button_Interrupt) {  //we ignore on depress after a double-button press
            Button1_Flag = true;
            Button1_Time = millis();
        } else {
            Ignore_Next_Button_Interrupt = false;
        }
    }
    
    if(  !digitalRead(BUTTON2) ) {
        if(!Ignore_Next_Button_Interrupt) {   //we ignore on depress after a double-button press
            Button2_Flag = true;
            Button2_Time = millis();
        } else {
            Ignore_Next_Button_Interrupt = false;
        }
    }
}




//####################################################################################
// SETUP
//####################################################################################

void setup () {
    Serial.begin(57600);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    pinMode(NEOPIXEL_PIN, OUTPUT);
    pinMode(BUTTON1, INPUT);
    pinMode(BUTTON2, INPUT);
    digitalWrite(BUTTON1, HIGH);  //set pullups
    digitalWrite(BUTTON2, HIGH);
    pinMode(LCD_RED, OUTPUT);
    pinMode(LCD_GREEN, OUTPUT);
    pinMode(LCD_BLUE, OUTPUT);
    pinMode(SOLENOID, OUTPUT);
    digitalWrite(SOLENOID, LOW);

    //--------Settings
    //SETTINGS = new struct user_settings; 
    loadSettingsFromEEPROM();
    
    
    //--------LCD Init
    // set up the lcd's number of columns and rows: 
    lcd.begin(16,2);

    //--------Neopixel Strip Init
    strip.begin();
    strip.show(); // Initialize all pixels to 'off'
    strip.setBrightness(SETTINGS.LED_BRIGHTNESS);   //set the persistent brightness


    //--------STARTUP SCREEN
    displayStartupInformation();
    lcd.clear();
    
    //--------Interrupt Init
    setupArduinoPCINT2();
    setupArduinoINT0();


    LastBeat = millis();
}



void setupArduinoINT0() {
  //-------ENABLE ARDUINO INT0 interrupt routines------------------
  //-------do this before we start expecting to see interrupts-----
  //--------INT 0---------
  EICRA = 0;               //clear it
  EICRA |= (1 << ISC01);
  EICRA |= (1 << ISC00);   //ISC0[1:0] = 0b11  rising edge INT0 creates interrupt
  EIMSK |= (1 << INT0);    //enable INT0 interrupt
  // Note:  instead of above, could use Arduino version of same
  // attachInterrupt(0,  functionname , RISING);
}

void setupArduinoPCINT2() {
  PCICR = 0;
  PCICR |= (1 << PCIE2);    //turn on Pin Change Interrupt Enable 2
  PCMSK2 = 0;
  PCMSK2 |= (1 << PCINT20);  //enable interrupt on Port D4 Pin D6 (PCINT20)]
  PCMSK2 |= (1 << PCINT21);  //enable interrupt on Port D5
}



//####################################################################################
//####################################################################################
// MAIN
//####################################################################################
//####################################################################################

void loop () {
    currentmillis = millis();
    if(Button1_Flag || Button2_Flag) {
        handleButtonCondition();
    }
    
    //------SET BRIGHTNESS MODE ------//
    if (In_Set_Brightness_Mode) {
        handleSetBrightnessMode();
    //---------NORMAL MODE ----------//    
    } else {    
        handleNormalMode(); 
    }
    
    if ( Settings_Did_Change && 
                   (currentmillis - Settings_Did_Change_Timeout > Settings_Write_Out_Delay) ){
        saveSettingsToEEPROM();
        Settings_Did_Change = false; 
    }
}


void handleNormalMode() {
    updatebpm();  //add our encoder0Pos to our bpm, intelligently
    if(BPM_did_change) {
        updateMilliCounter(); 
        updateLCD();
        BPM_did_change = false;
    }

    currentmillis = millis();
    if(SETTINGS.LED_SETTING == LEDs_SWEEP) {
        sweepMode();
    }
    
    if( (OnBeat) && (currentmillis - LastBeat >= BLINK_LENGTH) ) {
        //time to turn off beat sound or strobe
        OnBeat = false;
        digitalWrite(LED_PIN, LOW);
        if(SETTINGS.SOUND_ON) {
            digitalWrite(SOLENOID, LOW);
        }
        if(SETTINGS.LED_SETTING == LEDs_PULSE) {
            solidMode(BLACK);
        }
    }    
    
    if (currentmillis - LastBeat > MilliCounter) {
        //time for a beat
        OnBeat = true;
        Serial.println("*BLINK*");
        digitalWrite(LED_PIN, HIGH);
        
        if(SETTINGS.SOUND_ON) {
            digitalWrite(SOLENOID, HIGH);
        }
        if(SETTINGS.LED_SETTING == LEDs_PULSE) {
            solidMode(BPM_Color);
        }
        if(SETTINGS.LED_SETTING == LEDs_SWEEP) {
           syncSweepMode();
        }
        
        LastBeat = currentmillis;
    }
}


void handleSetBrightnessMode() {
     //exit by setting timer or by pressing button or hitting the timeout
    if(currentmillis - Set_Brightness_Timer > Set_Brightness_Timeout) {
        solidMode(BLACK);
        In_Set_Brightness_Mode = false; 
        updateLCD();    //put it back to normal view
    } else {
        solidMode(BPM_Color);
        displayBrightnessOnLCD();
        updateSetBrightness();
        if(Brightness_did_change) {
            strip.setBrightness(SETTINGS.LED_BRIGHTNESS); //we only save brightness on button press
            Set_Brightness_Timer = currentmillis;
        }
    } 
}


//#################################################################
// SETTINGS HELPERS
//#################################################################

void loadSettingsFromEEPROM( void ) {
        EEPROM_readAnything(EEPROM_SETTINGS_ADDRESS, SETTINGS);
        printOutUserSettings();
        verifyAndCorrectSettings();
}    
    
void saveSettingsToEEPROM() {
    EEPROM_writeAnything(EEPROM_SETTINGS_ADDRESS, SETTINGS);
    Serial.println(F("New User Settings Saved:"));
    printOutUserSettings();
}
    
void verifyAndCorrectSettings( void ) {
        bool needToWriteOut = false;
        //printOutUserSettings();
        //A setting should either equal 1 or 0.  Otherwise, set to 1;
        if ( isNotValid(SETTINGS.SOUND_ON) )    { 
            SETTINGS.SOUND_ON = 0x01; 
            needToWriteOut = true;
        }
        if ( (SETTINGS.LED_SETTING) >= MAX_LED_SETVALUE )    { 
            SETTINGS.LED_SETTING = 0x01; 
            needToWriteOut = true;
        }
        if(needToWriteOut) {
            saveSettingsToEEPROM();
            Serial.println(F("Corrected User Settings Follow:"));
            printOutUserSettings();
        }
    } 

//check that our EEPROM isn't just uninitialized 0xFF's  
bool isNotValid(uint8_t setting) {
        bool isnotvalid = true;
        if( (setting == (uint8_t)0x00) || (setting == (uint8_t)0x01) ) {
            //valid
            isnotvalid = false;
        } else if (  ((int) setting == (int) -1) || ((uint8_t)setting == (uint8_t)0xFF)  ) {
            isnotvalid = true;      //unprogrammed EEPROMs are 0xFF  
        } else {
            isnotvalid = true;
        }
        return isnotvalid;
    }  
    
    
void printOutUserSettings(void) {
    Serial.println(F("---User Settings---"));
    Serial.print(F("BPM           : "));Serial.println(SETTINGS.bpm, DEC);
    Serial.print(F("LED_SETTING   : "));Serial.println(SETTINGS.LED_SETTING, DEC);
    Serial.print(F("LED_BRIGHTNESS: "));Serial.println(SETTINGS.LED_BRIGHTNESS, DEC);
    Serial.print(F("SOUND_ON      ? "));Serial.println(SETTINGS.SOUND_ON, BIN);
    Serial.println();
}

    
    
    
    
//################################################################
// UPDATERS
//#################################################################
    
    
void updatebpm() {
    int delta = encoder0Pos / ENCODER_SENSITIVITY_DIVIDER;
    if (delta == 0) {
        return;
    } else {
        uint8_t temp = SETTINGS.bpm + delta;
        encoder0Pos = 0;
        if((SETTINGS.bpm <= 10) && (delta < 0)) {
            SETTINGS.bpm = 10;
            BPM_did_change = true;
            return;
        } else if ((SETTINGS.bpm >=250) && (delta > 0) ) {
            SETTINGS.bpm = 250;
            BPM_did_change = true; 
        } else if ((temp < 10) || (temp > 250)) {
            return;
        } else {
            BPM_did_change = true;
            SETTINGS.bpm = temp;
            return;
        }
    }
}

//This update should take over only when In_Set_Brightness_Mode
//Uses encoder to adjust SETTINGS.SET_BRIGHTNESS and allows
//it to wrap
void updateSetBrightness() {
    int delta = encoder0Pos / ENCODER_SENSITIVITY_DIVIDER;
    if (delta == 0) {
        return;
    } else {
        uint8_t temp = SETTINGS.LED_BRIGHTNESS + delta;
        encoder0Pos = 0;
        Brightness_did_change = true;
        SETTINGS.LED_BRIGHTNESS  = temp;
        return;    
    }
}



void updateMilliCounter() {
    unsigned long numerator = (unsigned long) 60 * 1000;
    unsigned long denominator = ((unsigned long) SETTINGS.bpm);
    MilliCounter = (numerator / denominator);
    
    
    Sweep_Step_Delay = MilliCounter / 16;
    
//    Serial.print("   MilliCounter: ");
//    Serial.print(numerator);
//    Serial.print(" / ");
//    Serial.print(denominator);
//    Serial.print(" = ");
//    Serial.println(MilliCounter);
}

void updateLCD() {
      updateBacklightColor();  
      lcd.clear();
      lcd.setCursor(0,0);
      setTopLineStringFromBPM();
      lcd.print(TopLineString);
      
      sprintf(BottomLineString, "            %4d", SETTINGS.bpm);     //12spaces, 4 numbers, not zero padded
      lcd.setCursor(0,1);
      lcd.print(BottomLineString);
}

void updateBacklightColor() {
    color_24bits * coloraddress;
    coloraddress = (color_24bits *)pgm_read_word_near(  &(BPM_COLOR_LIST[SETTINGS.bpm])  );
//    Serial.print("  BPM_COLOR_LIST[0]_ADDRESS: ");
//    Serial.print((uint16_t)&BPM_COLOR_LIST[0], DEC);
//    Serial.print("   &bpmcolor60: ");
//    Serial.print((uint16_t)&bpmcolor60, DEC);
//    Serial.print("   &bpmcolor61: ");
//    Serial.print((uint16_t)&bpmcolor61, DEC);
//    Serial.print("   &(BPM_COLOR_LIST[60]): ");
//    Serial.print((uint16_t)&(BPM_COLOR_LIST[60]), DEC);
//    Serial.print("  COLOR ADDRESS: ");
//    Serial.println((uint16_t)&coloraddress, DEC);
    uint8_t r = (uint8_t)pgm_read_byte_near(  &(coloraddress->red_value) );
    analogWrite(LCD_RED, r);
    uint8_t g = (uint8_t)pgm_read_byte_near(  &(coloraddress->green_value) );
    analogWrite(LCD_GREEN, g);
    uint8_t b = (uint8_t)pgm_read_byte_near(  &(coloraddress->blue_value) );
    analogWrite(LCD_BLUE, b);

    BPM_Color = 0;
    BPM_Color = BPM_Color + ((uint32_t)r << 16);
    BPM_Color = BPM_Color + ((uint32_t)g << 8);
    BPM_Color = BPM_Color + b;
    
    if (DEBUG) {
        Serial.print(F("BPMCOLOR: "));
        Serial.println(BPM_Color, HEX);
        Serial.print(F("LCD_Red: "));
        Serial.print(r, DEC);
        Serial.print(F("    LCD_Green: "));
        Serial.print(g, DEC);
        Serial.print(F("    LCD_Blue: "));
        Serial.println(b, DEC);
    }
}


void setTopLineStringFromBPM() {
   if(DEBUG) { 
       Serial.print("BPM: ");
       Serial.println(SETTINGS.bpm);
   }
   for (uint8_t i=0;  i< TEMPO_LIST_LENGTH; i++) {
       if (SETTINGS.bpm <= LIST_OF_TEMPOS[i]) {
           //we have a winner
           prog_char * stringaddress;
           stringaddress = (prog_char *)pgm_read_word_near(  &(LIST_OF_TEMPO_NAMES[i])  );
           strcpy_P(TopLineString, stringaddress);
           return;
       }  
   }
   //just in case
   strcpy_P(TopLineString, CalicoString);
   return;
}



//#################################################
// PCINT2    BUTTON HANDLER
//#################################################
void handleButtonCondition() {
    currentmillis = millis();
    //check to see if both flag is raised, if so, do the math to see if it is close enough to trigger
    if(Button1_Flag && Button2_Flag) {
        if (  (Button1_Time - Button2_Time <  Buttons_Same_Time_Delta) ||
              (Button2_Time = Button1_Time <  Buttons_Same_Time_Delta) ) {
            
            if (DEBUG) { Serial.println(F("BOTH BUTTONS PRESSED")); }
                        
            Set_Brightness_Timer = currentmillis;
            In_Set_Brightness_Mode = true;
            Button1_Flag = false;
            Button2_Flag = false;
            Ignore_Next_Button_Interrupt = true;
        }
    } else if (Button1_Flag) {         //HANDLE LIGHT BUTTON
        if ( currentmillis - Button1_Time > Buttons_Same_Time_Delta) {
            uint8_t t = SETTINGS.LED_SETTING;
            t++;
            t = t % MAX_LED_SETVALUE;
            SETTINGS.LED_SETTING = t;
            solidMode(BLACK);        //MAKE FOR A CLEAN TRANSITION without straggler LEDs
            if (DEBUG) {
                Serial.print(F("LIGHT BUTTON - NEW LED SETTING: "));
                Serial.println(SETTINGS.LED_SETTING, DEC);
            }
            
            updateLCD();
            Settings_Did_Change = true;
            Settings_Did_Change_Timeout = currentmillis;
            In_Set_Brightness_Mode = false;  //break out of it
            Button1_Flag = false; 
            Button2_Flag = false; 
        }
    } else if (Button2_Flag) {        //HANDLE SOUND BUTTON
        if ( currentmillis - Button1_Time > Buttons_Same_Time_Delta) {
            SETTINGS.SOUND_ON = !(SETTINGS.SOUND_ON);
            
            if(DEBUG) { Serial.println(F("SOUND BUTTON TOGGLE")); }
            
            updateLCD();
            Settings_Did_Change = true;
            Settings_Did_Change_Timeout = currentmillis;
            In_Set_Brightness_Mode = false;  //break out of it
            Button1_Flag = false; 
            Button2_Flag = false; 
        }
    }
}





//#################################################
// BRIGHTNESS HANDLER
//#################################################

void displayBrightnessOnLCD() {
    char buffer[STRING_WIDTH] = "";
    lcd.setCursor(0,0);
    lcd.print(F("LED BRIGHTNESS"));
    
    sprintf(buffer, "            %4d", SETTINGS.LED_BRIGHTNESS);     //12spaces, 4 numbers, not zero padded
    lcd.setCursor(0,1);
    lcd.print(buffer);
}









//#################################################
// NEOPIXEL CODE
//#################################################
void solidMode(uint32_t color) {
  for(uint16_t i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, color);   
  }
  strip.show();
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, c);
      strip.show();
      delay(wait);
  }
}


void sweepMode() {
    currentmillis = millis();
    if (currentmillis - Last_Sweep_Update > Sweep_Step_Delay) {
        Last_Sweep_Update = currentmillis;
      
        uint32_t c = 0;     //temp color
        
        incrementLeadPosition();
        //Serial.print("LEAD:\t");
        //Serial.println(LeadPosition, DEC);
        
        int8_t leadm2, leadm1, leadp1, leadp2;
        leadm2 = LeadPosition - 2;
        leadm1 = LeadPosition - 1;
        leadp1 = LeadPosition + 1;
        leadp2 = LeadPosition + 2;
    
        strip.setPixelColor( LeadPosition - 4 ,  0);
        strip.setPixelColor( LeadPosition - 3 ,  0);  //added to avoid stragglers
        strip.setPixelColor( leadm2,   0);    
        
        if ( (leadm1 == LEFT_MIDDLE) || (leadm1 == RIGHT_MIDDLE) ) {
            strip.setPixelColor( leadm1,   dimColor(WHITE, 1));      
        } else {
            strip.setPixelColor( leadm1,   dimColor(BPM_Color, 3));    
        }
        
        if ( (LeadPosition == LEFT_MIDDLE) || (LeadPosition == RIGHT_MIDDLE) ) {
            strip.setPixelColor(LeadPosition, WHITE);
        } else {
            strip.setPixelColor(LeadPosition, dimColor(BPM_Color, 1));
        }
        
        if ( (leadp1 == LEFT_MIDDLE) || (leadp1 == RIGHT_MIDDLE) ) {
            strip.setPixelColor( leadp1,   dimColor(WHITE, 1));
        } else {
            strip.setPixelColor( leadp1,   dimColor(BPM_Color, 3)); 
        }
        
        strip.setPixelColor( leadp2,   0);
        strip.setPixelColor( LeadPosition + 3 , 0);  //added these to avoid stragglers.
        strip.setPixelColor( LeadPosition + 4 , 0);
           
        strip.show();
       //delay(Sweep_Step_Delay);   
    }
}

//Used to keep the timing from wandering too much.
void syncSweepMode() {
   if (DirectionRight) {
       LeadPosition = LEFT_MIDDLE;
   } else {
       LeadPosition = RIGHT_MIDDLE;
   }
}


void incrementLeadPosition() {
      if(DirectionRight) {  
        LeadPosition++;  
        if(LeadPosition >= NUMPIXELS ) {
            LeadPosition = NUMPIXELS-1;
            DirectionRight = false;
        }
    } else {  
        LeadPosition--;  
        if(LeadPosition < 0) {
            LeadPosition = 0;
            DirectionRight = true; 
        }
    } 
}

//dim the color in factors of 2....basically right shift
// the r, g, b bytes.   Can only shift 0-7 
uint32_t dimColor(uint32_t c, uint8_t dim) {
    uint8_t r,g,b;
    r = (uint8_t)(c >> 16);
    g = (uint8_t)(c >> 8);
    b = (uint8_t) c;
    if (dim > 7) {
      return c;    //do nothing
    }
    r = r >> dim;
    g = g >> dim;
    b = b >> dim;
    c = 0;
    c = ((uint32_t)r ) << 16;
    c = c + (uint32_t)(g << 8);
    c = c + b;
    
    return c;
    
}



//#################################################
// STARTUP SERIAL INFORMATION
//#################################################
//assumes Serial port and LCD have been initialized
void displayStartupInformation() {
    //prog_char * stringaddress;
    //stringaddress =  (prog_char *)pgm_read_word_near(  &(days_of_the_week[DayOfTheWeek])  );
    //strcat_P(stringbuffer, stringaddress);
    char stringbuffer[STRING_WIDTH];
    strcpy_P(stringbuffer, CalicoString);
  
    lcd.setCursor(0,0);
    lcd.print(stringbuffer);  
    strcpy_P(stringbuffer, MetronomeString);
    lcd.setCursor(0,1);
    lcd.print(stringbuffer); 
    
    Serial.println();
    Serial.println();
    Serial.println(F("-------------------------------"));
    Serial.println(F("  Arduino Metronome     v1.0"));
    Serial.println(F("  for Calico Music      "));
    Serial.println(F("  Written by Red Byer   "));
    Serial.println(F("  5/11/14               "));
    Serial.println(F("  Happy Mother's Day to Steph!"));
    Serial.println(F("  Happy Mother's        "));
    Serial.println(F("-------------------------------"));
    Serial.println();
    
    digitalWrite(LCD_RED, HIGH);
    colorWipe(RED, 25);
    delay(750);
    solidMode(BLACK);
    digitalWrite(LCD_RED,LOW);
    digitalWrite(LCD_GREEN, HIGH);
    colorWipe(GREEN, 25);
    delay(750);
    solidMode(BLACK);
    digitalWrite(LCD_GREEN, LOW);
    digitalWrite(LCD_BLUE, HIGH);
    colorWipe(BLUE, 25);
    delay(750);
    solidMode(BLACK);
    digitalWrite(LCD_BLUE, LOW);
}



