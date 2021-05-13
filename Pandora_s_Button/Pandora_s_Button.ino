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


#define VERSION       "0.2.3"
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
int lastButtonState = LOW;   // the previous reading from the input pin
int lastLedState = 0; // our LEDs (through MBI5026 driver)
int ledState = 0; // our desired state for the LEDs
int buttonState = LOW;             // the current reading from the input pin
DateTime now; // the time (to approximately the nearest second)

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long millisLastTouchScan = 0;
unsigned long millisLastTimePrint = 0;
unsigned long millisLastNowUpdate = 0;

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

#define FREE_MEM Serial.println(F("Free RAM = ")); Serial.println(freeMemory(), DEC);  // print how much RAM is available.


File openFile(const char* path) {
  File myFile;
  char message[35];
  snprintf_P(message, sizeof(message), PSTR("Looking for %s ..."), 
    path
  );
  log(message);
  
  if (SD.exists(path)) {
    log(PSTR("... found it. Opening."));
  } else {
    log(PSTR("... doesn't exist, creating it."));
  }
  myFile = SD.open(path, FILE_WRITE);
  myFile.flush();
  return myFile;
}

void log(const char* message) {
  Serial.print('[');
  Serial.print(millis(), DEC);
  Serial.print("] ");
  Serial.println(message);
  if (logFile) {
    logFile.print('[');
    logFile.print(millis(), DEC);
    logFile.print("] ");
    logFile.println(message);
    logFile.flush(); // make sure the data is commited to the SD card
  } else { // if the file isn't open, pop up an error:
    Serial.println(PSTR("ERROR: unable to write to log file on SD card."));
  }
}

void tftTextSamples() {
  Tft.drawString("original", 0, 14, 2, GREEN);

  TextOrientation orientation;
  Tft.drawString("portrait", 140, 300, 2, YELLOW, orientation);
  orientation = PORTRAIT_BACKWARDS;
  Tft.drawString("backwards", 128, 280, 2, YELLOW, orientation);
  orientation = PORTRAIT_UPSIDE_DOWN_BACKWARDS;
  Tft.drawString("downback", 128, 240, 2, YELLOW, orientation);
  orientation = PORTRAIT_UPSIDE_DOWN;
  Tft.drawString("upside down", 100, 200, 2, YELLOW, orientation);
  orientation = PORTRAIT_VERTICAL;
  Tft.drawString("vertical", 8, 220, 2, YELLOW, orientation);

  orientation = LANDSCAPE;
  Tft.drawString("landscape normal", 100, 18, 2, WHITE, orientation);
  orientation = LANDSCAPE_UPSIDE_DOWN;
  Tft.drawString("landscape updown", 100, 0, 2, WHITE, orientation);
  orientation = LANDSCAPE_BACKWARDS;
  Tft.drawString("landscape back", 120, 64, 2, WHITE, orientation);
  orientation = LANDSCAPE_UPSIDE_DOWN_BACKWARDS;
  Tft.drawString("landscape downback", 100, 70, 2, WHITE, orientation);
  orientation = LANDSCAPE_VERTICAL;
  Tft.drawString("landscape vertical", 0, 0, 2, WHITE, orientation);
}

void setup() {  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_MBI_LAT, OUTPUT);
  pinMode(PIN_MBI_SCK, OUTPUT);
  pinMode(PIN_SD_CS, OUTPUT);
  Serial.begin(9600);
  while (!Serial);  // wait for Serial Monitor to connect. Needed for native USB port boards only..
  FREE_MEM

  // Initalize the TFT screen
  // TFT_BL_ON;      // turn on the background light
  Tft.TFTinit();  // init TFT library
  Tft.fillScreen(0, 240, 0, 320, BLUE);

  tftTextSamples();

  Serial.print("Initializing SD card...");

  if (!SD.begin(PIN_SD_CS)) {
    Serial.println(PSTR("initialization failed."));
    Serial.println(PSTR("Note: press reset or reopen this serial monitor after fixing your issue!"));
    abort();
  } else {
    Serial.println("SD card online.");
  }
  logFile = openFile("pandora.log");
  log(PSTR("Pandora's Button v" VERSION));
  touchesFile = openFile("touches.csv");

  if (! rtc.begin()) {
    log("Couldn't find RTC");
    Serial.flush();
    abort();
  } else {
    log("RTC initialized.");
  }

  if (rtc.lostPower()) {
    // char message[100];
    // snprintf_P(message, sizeof(message), PSTR("RTC lost power, settng the time based on sketch compile time: %s %s."), PSTR(__DATE__), PSTR(__TIME__));
    // log(message);
    log(PSTR("RTC lost power, settng the time based on sketch compile time: " __DATE__ " " __TIME__ "."));
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
  FREE_MEM
}

/**
 * Logs a message with the current value of `now` (global variable)
 */
void printTime() {
    char message[37];
    snprintf_P(message, sizeof(message), PSTR("%i/%i/%i (%s) %i:%i:%i"), 
      now.year(),
      now.month(),
      now.day(),
      daysOfTheWeek[now.dayOfTheWeek()],
      now.hour(),
      now.minute(),
      now.second()
    );
    log(message);
    snprintf_P(message, sizeof(message), PSTR("Since midnight 1/1/1970 that's %l"), 
      now.unixtime()
    );
    log(message);
    
    snprintf_P(message, sizeof(message), PSTR("Temperature: %i C"), 
      rtc.getTemperature()
    );
    log(message);
    FREE_MEM
}

/**
 * Update the global `now` variable from the real time clock (RTC) module.
 * Designed to be called more than once a second. See PERIOD_NOW_UDPATE.
 */
void updateNow() {
  now = rtc.now();
  char message[30];
  snprintf_P(message, sizeof(message), PSTR("Updated now to %u"), 
    now.unixtime()
  );
  log(message);
}

void loop() {
  if((unsigned long)(millis() - millisLastTouchScan) > PERIOD_TOUCH_SCAN) {
    millisLastTouchScan = millis();
    Point p = ts.getPoint();

    if (p.z > __PRESSURE) {
      char message[45];
      snprintf_P(message, sizeof(message), PSTR("Raw X = %i\tRaw Y = %i\tPressure = %i."), 
        p.x,
        p.y,
        p.z
      );
      log(message);
    }

    // we have some minimum pressure we consider 'valid'
    // pressure of 0 means no pressing!
    if (p.z > __PRESSURE) {
      p.x = map(p.x, TS_MINX, TS_MAXX, 0, 240);
      p.y = map(p.y, TS_MINY, TS_MAXY, 0, 320);

      char message[45];
      snprintf_P(message, sizeof(message), PSTR("X = %i\tY = %i\tPressure = %i."), 
        p.x,
        p.y,
        p.z
      );
      log(message);
    }
  } // last touch scan

  if((unsigned long)(millis() - millisLastTimePrint) > PERIOD_TIME_PRINT) {
    millisLastTimePrint = millis();
    printTime();
    FREE_MEM
  }

  if((unsigned long)(millis() - millisLastNowUpdate) > PERIOD_NOW_UPDATE) {
    millisLastNowUpdate = millis();
    updateNow();
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
        char message[25];
        snprintf_P(message, sizeof(message), PSTR("PRESSED at %u"), now.unixtime());
        log(message);
        bitClear(ledState, LED_BUTTON); // turn off the button led
      }
    }
  }

  // Manage the LED(s) driven through the MBI5026 driver
  if (lastLedState != ledState) {
    log("Updating LED driver");
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
