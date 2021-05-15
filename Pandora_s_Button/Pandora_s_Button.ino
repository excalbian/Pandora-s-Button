#include <SD.h>
#include "TFTv2.h"
#include <stdint.h>
#include <SeeedTouchScreen.h>
#include "RTClib.h" // Date and time functions using a DS3231 RTC connected via I2C and Wire lib

// settings used to control program flow
#define DEBOUNCE_DELAY      50      // the debounce time in milliseconds; increase if the output flickers
#define PERIOD_TOUCH_SCAN   100     // milliseconds before we re-scan for touches
#define PERIOD_TIME_PRINT   300000  // milliseconds before we re-print the time ( = 5 minutes)
#define PERIOD_NOW_UPDATE   500     // milliseconds before we update the 'now' value
#define PERIOD_TFT_TIME     30000   // milliseconds to update the time on the TFT 
// Use this to log each update to now: #define VERBOSE_NOW_UPDATES 1
#define TOUCHES_AS_PRESSES  1
#define LOG_TEXT_SIZE       1
#define LOG_TEXT_FG         RED
#define LOG_TEXT_BG         BLACK
#define TEXT_ORIENTATION    LANDSCAPE
#define TFT_BG              BLUE
#define PRESSES_FG          YELLOW
#define PRESSES_BG          RED



#define VERSION       "0.2.7"
#define PIN_BUTTON    3
#define PIN_SD_CS     4
#define PIN_TFT_CS    5
#define PIN_TFT_DC    6
#define PIN_BACKLIGHT 7
#define PIN_MBI_SCK   8
#define PIN_MBI_SDA   9
#define PIN_MBI_LAT   10
//SPI MOSI            11
//SPI MISO            12
//SPI SCK             13
#define YP            A2  // must be an analog pin, use "An" notation!
#define XM            A1  // must be an analog pin, use "An" notation!
#define YM            14  // can be a digital pin, this is A0
#define XP            17  // can be a digital pin, this is A3
#define PIN_RTC_SDA   18  // this is A4 aka I2C SDA
#define PIN_RTC_SCL   19  // this is A5 aka I2C SDL

//Measured ADC values for (0,0) and (210-1,320-1)
//TS_MINX corresponds to ADC value when X = 0
//TS_MINY corresponds to ADC value when Y = 0
//TS_MAXX corresponds to ADC value when X = 240 -1
//TS_MAXY corresponds to ADC value when Y = 320 -1

#define TS_MINX 116*2
#define TS_MAXX 890*2
#define TS_MINY 83*2
#define TS_MAXY 913*2

// The following define the bits (matching output pins on the MBI5026 driver)
// in ledState used for each specified led.
#define LED_BUTTON        1 // In the pushbutton
#define LED_TOP_LEFT      2
#define LED_TOP_RIGHT     3
#define LED_BOTTOM_LEFT   4
#define LED_BOTTOM_RIGHT  5

// For better pressure precision, we need to know the resistance
// between X+ and X- Use any multimeter to read it
// The 2.8" TFT Touch shield has 300 ohms across the X plate
TouchScreen ts = TouchScreen(XP, YP, XM, YM);

RTC_DS3231 rtc;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

// "Global" variables
File logFile;
File touchesFile;
int lastButtonState = LOW; // the previous reading from the input pin
int lastLedState = 0; // our LEDs (through MBI5026 driver)
int ledState = 0; // our desired state for the LEDs
int buttonState = LOW; // the current reading from the input pin
unsigned int myPresses = 0; // how many times button pressed this go round
short touchState = LOW; // to indicate if we are being touched
unsigned int myTouches = 0; // how many touch screen taps we've counted as button presses
boolean sdAvailable = false;

DateTime now; // the time (to approximately the nearest second)
char buffer[50]; // global string used to buffer from flash

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long millisLastTouchScan = 0;
unsigned long millisLastTimePrint = 0;
unsigned long millisLastNowUpdate = 0;
unsigned long millisLastTftTimeUpdate = 0;

/************************ Functions and methods ******************************/

#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif  // __arm__

int freeMemory() {
  char top;
#ifdef __arm__
  return &top - reinterpret_cast<char*>(sbrk(0));
#elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
  return &top - __brkval;
#else  // __arm__
  return __brkval ? &top - __brkval : &top - __malloc_heap_start;
#endif  // __arm__
}

#define LOG_BUFFER(MSG) snprintf_P(buffer, sizeof(buffer), PSTR(MSG)); log(buffer);
#define FREE_MEM Serial.print(F("Free RAM = ")); Serial.println(freeMemory(), DEC);  // print how much RAM is available.
#define FREE_MEM_LOG snprintf_P(buffer, sizeof(buffer), PSTR("Free RAM=%u"), freeMemory()); log(buffer);


File openFile(const char* path) {
  File myFile;
  if (!sdAvailable) {
    snprintf_P(buffer, sizeof(buffer), PSTR("ERROR: SD not available. Not opening %s."), 
      path
    );
    log(buffer);
    return;
  }
  snprintf_P(buffer, sizeof(buffer), PSTR("Looking for %s ..."), 
    path
  );
  log(buffer);
  
  if (SD.exists(path)) {
    LOG_BUFFER("... found it. Opening.");
  } else {
    LOG_BUFFER("... doesn't exist, creating it.");
  }
  myFile = SD.open(path, FILE_WRITE);
  myFile.flush();
  return myFile;
}

void log(const char* message) {
  #define TEXT_ORIENTATION  LANDSCAPE
  #define LOG_TEXT_X        0
  #define LOG_TEXT_Y        0
  #define LOG_TEXT_WIDTH    12
  #define LOG_TEXT_HEIGHT   320

  Serial.print('[');
  Serial.print(millis(), DEC);
  Serial.print("] ");
  Serial.println(message);
  if (sdAvailable) {
    logFile.print('[');
    logFile.print(millis(), DEC);
    logFile.print("] ");
    logFile.println(message);
    logFile.flush(); // make sure the data is commited to the SD card
  } else { // if the file isn't open, pop up an error:
    char myBuff[50]; // use a new buffer so we don't overwrite the message
    snprintf_P(myBuff, sizeof(myBuff), PSTR("ERROR: unable to write to log file on SD card."));
    Serial.println(myBuff);
    Tft.drawString(myBuff, LOG_TEXT_X, LOG_TEXT_Y + (LOG_TEXT_WIDTH * 2), LOG_TEXT_SIZE, RED, TEXT_ORIENTATION);
    FREE_MEM
  }

  Tft.fillRectangle(LOG_TEXT_Y, LOG_TEXT_X, LOG_TEXT_WIDTH, LOG_TEXT_HEIGHT, LOG_TEXT_BG);
  Tft.drawString(message, LOG_TEXT_X, LOG_TEXT_Y, LOG_TEXT_SIZE, RED, TEXT_ORIENTATION);
}

void setup() {  
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_MBI_LAT, OUTPUT);
  pinMode(PIN_MBI_SCK, OUTPUT);
  pinMode(PIN_SD_CS, OUTPUT);
  Serial.begin(9600);
  while (!Serial);  // wait for Serial Monitor to connect. Needed for native USB port boards only..
  FREE_MEM

  // Initalize the TFT screen
  // TFT_BL_ON;      // turn on the background light
  Tft.TFTinit();  // init TFT library
  Tft.fillScreen(0, 240, 0, 320, TFT_BG);

  Serial.print("Initializing SD card...");

  if (!SD.begin(PIN_SD_CS)) {
    Serial.println(F("initialization failed."));
    SD.end();
    SPI.setClockDivider(SPI_CLOCK_DIV4);
  } else {
    Serial.println(F("SD card online."));
    sdAvailable = true;
    logFile = openFile("pandora.log");
  }

  LOG_BUFFER("Pandora's Button v" VERSION)
  Tft.drawString(buffer, 0, 220, 2, YELLOW, TEXT_ORIENTATION);
  LOG_BUFFER(__DATE__ " " __TIME__);
  updateTftTime();
  updateTftPresses();

  touchesFile = openFile("touches.csv");

  if (! rtc.begin()) {
    log("Couldn't find RTC");
  } else {
    log("RTC initialized.");
    if (rtc.lostPower()) {
      LOG_BUFFER("RTC lost power, settng the time to compile time: %s %s.");
      LOG_BUFFER(__DATE__ " " __TIME__);
      // When time needs to be set on a new device, or after a power loss, the
      // following line sets the RTC to the date & time this sketch was compiled
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      // This line sets the RTC with an explicit date & time, for example to set
      // January 21, 2014 at 3am you would call:
      // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    }
    // When time needs to be re-set on a previously configured device, the
    // following line sets the RTC to the date & time this sketch was compiled
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }


  FREE_MEM_LOG
}

void nowToBuffer() {
    snprintf_P(buffer, sizeof(buffer), PSTR("%i/%i/%i (%s) %i:%i:%i"), 
      now.year(),
      now.month(),
      now.day(),
      daysOfTheWeek[now.dayOfTheWeek()],
      now.hour(),
      now.minute(),
      now.second()
    );
}

void updateTftTime() {
  #define TIME_X        0
  #define TIME_Y        180
  // Rotated for landscape
  #define TIME_WIDTH    15     
  #define TIME_HEIGHT   320
  #define TIME_FG       WHITE
  #define TIME_BG       BLUE

  LOG_BUFFER("Updateing time on the TFT")
  nowToBuffer();
  Tft.fillRectangle(TIME_Y, TIME_X, TIME_WIDTH, TIME_HEIGHT, TIME_BG);
  Tft.drawString(buffer, TIME_X, TIME_Y, 1, TIME_FG, TEXT_ORIENTATION);

  FREE_MEM_LOG
}

/**
 * Logs a message with the current value of `now` (global variable)
 */
void printTime() {
    snprintf_P(buffer, sizeof(buffer), PSTR("Temperature: %i C"), 
      rtc.getTemperature()
    );
    log(buffer);
    nowToBuffer();
    log(buffer);
    snprintf_P(buffer, sizeof(buffer), PSTR("Since midnight 1/1/1970 that's %l"), 
      now.unixtime()
    );
    log(buffer);
    
    FREE_MEM_LOG
}

/**
 * Update the global `now` variable from the real time clock (RTC) module.
 * Designed to be called more than once a second. See PERIOD_NOW_UDPATE.
 */
void updateNow() {
  now = rtc.now();
  #ifdef VERBOSE_NOW_UPDATES
  snprintf_P(buffer, sizeof(buffer), PSTR("Updated now to %u"), 
    now.unixtime()
  );
  log(buffer);
  #endif
}

void updateTftPresses() {
  #define PRESSES_X       15
  #define PRESSES_Y       120
  // Rotated for landscape
  #define PRESSES_WIDTH   34
  #define PRESSES_HEIGHT  290

  // LOG_BUFFER("Updateing time on the TFT")
  Tft.fillRectangle(PRESSES_Y + 2, PRESSES_X, PRESSES_WIDTH, PRESSES_HEIGHT, PRESSES_BG);
  snprintf_P(buffer, sizeof(buffer), PSTR("Presses: %03u"), myPresses);
  log(buffer);
  Tft.drawString(buffer, PRESSES_X, PRESSES_Y, 4, PRESSES_FG, TEXT_ORIENTATION);

  FREE_MEM_LOG
}

void buttonPressed() {
  char longBuffer[100];
  snprintf_P(buffer, sizeof(buffer), PSTR("PRESSED at %u"), now.unixtime());
  log(buffer);
  myPresses++;
  if (sdAvailable && touchesFile) {
    // version, milliseconds, unixtime, year, month, day, hour, minute, second, 1 (for pressed count), myPresses, myTouches
    snprintf_P(longBuffer, sizeof(longBuffer), PSTR(VERSION ",%u,%u,%u,%u,%u,%u,%u,%u,1,%u,%u"),
      millis(),
      now.unixtime(),
      now.year(),
      now.month(),
      now.day(),
      now.hour(),
      now.minute(),
      now.second(),
      myPresses,
      myTouches
    );
    touchesFile.println(longBuffer);
    touchesFile.flush(); // make sure the data is commited to the SD card
    FREE_MEM_LOG
  } else { // if the file isn't open, pop up an error:
    Serial.println(F("ERROR: unable to write to touches file on SD card."));
  }
  updateTftPresses();
}

void loop() {
  if((unsigned long)(millis() - millisLastTouchScan) > PERIOD_TOUCH_SCAN) {
    millisLastTouchScan = millis();
    Point p = ts.getPoint();

    // we have some minimum pressure we consider 'valid'
    // pressure of 0 means no pressing!
    if (p.z > __PRESSURE) {
      snprintf_P(buffer, sizeof(buffer), PSTR("Raw X = %04i  Raw Y = %04i  Pressure = %03i."), 
        p.x,
        p.y,
        p.z
      );
      log(buffer);
      // scale the touch (analog) to size of the display
      p.x = map(p.x, TS_MINX, TS_MAXX, 0, 240);
      p.y = map(p.y, TS_MINY, TS_MAXY, 0, 320);
      snprintf_P(buffer, sizeof(buffer), PSTR("X = %03i  Y = %03i  Pressure = %03i."), 
        p.x,
        p.y,
        p.z
      );
      log(buffer);
      #ifdef TOUCHES_AS_PRESSES
      if (touchState == LOW) { // we weren't touching, but now we are
        touchState = HIGH;
        myTouches ++;
        buttonPressed();
        snprintf_P(buffer, sizeof(buffer), PSTR("Counted touchscreen touch %u as button press."), myTouches);
        log(buffer);
      }
      #endif

    } else {
      touchState = LOW; // we aren't touching any more ;)
    }
  } // last touch scan

  if((unsigned long)(millis() - millisLastTimePrint) > PERIOD_TIME_PRINT) {
    millisLastTimePrint = millis();
    printTime();
    FREE_MEM_LOG
  }

  if((unsigned long)(millis() - millisLastNowUpdate) > PERIOD_NOW_UPDATE) {
    millisLastNowUpdate = millis();
    updateNow();
  }
  
  if((unsigned long)(millis() - millisLastTftTimeUpdate) > PERIOD_TFT_TIME) {
    millisLastTftTimeUpdate = millis();
    updateTftTime();
  }

  // Check on our pushbutton -- it is only why we are here ;)
  int reading = digitalRead(PIN_BUTTON);
  // check to see if you just pressed the button
  // (i.e. the input went from LOW to HIGH), and you've waited long enough
  // since the last press to ignore any noise:

  // If the switch changed, due to noise or pressing:
  if (reading != lastButtonState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }

  if ((unsigned long)(millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed:
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == HIGH) {
        bitSet(ledState, LED_BUTTON); // turn the button back on
      } else { // buttonState == LOW
        buttonPressed();
        bitClear(ledState, LED_BUTTON); // turn off the button led
      }
    }
  }

  // Manage the LED(s) driven through the MBI5026 driver
  if (lastLedState != ledState) {
    LOG_BUFFER("Updating LED driver");
    // shift out highbyte
    shiftOut(PIN_MBI_SDA, PIN_MBI_SCK, MSBFIRST, (ledState >> 8));
    // shift out lowbyte
    shiftOut(PIN_MBI_SDA, PIN_MBI_SCK, MSBFIRST, ledState);
    // latch the values from the shift register to the output
    digitalWrite(PIN_MBI_LAT, HIGH);
    delayMicroseconds(1);
    digitalWrite(PIN_MBI_LAT, LOW);
    // lastly, save the state we have published to the driver
    lastLedState = ledState;
  }
}
