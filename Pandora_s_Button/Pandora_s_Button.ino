
#include <SD.h>
#include "TFTv2.h"
#include <stdint.h>
#include <SeeedTouchScreen.h>
#include "RTClib.h" // Date and time functions using a DS3231 RTC connected via I2C and Wire lib

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

void setup() {
  Serial.begin(9600);
  Serial.println("Pandora's Button v0.1");

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
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

  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_MBI_LAT, OUTPUT);
  pinMode(PIN_MBI_SCK, OUTPUT);
  pinMode(PIN_SD_CS, OUTPUT);
  Serial.println("Pin modes setup");

}

void printTime() {
    DateTime now = rtc.now();

    Serial.print(now.year(), DEC);
    Serial.print("/");
    Serial.print(now.month(), DEC);
    Serial.print("/");
    Serial.print(now.day(), DEC);
    Serial.print(" (");
    Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
    Serial.print(") ");
    Serial.print(now.hour(), DEC);
    Serial.print(":");
    Serial.print(now.minute(), DEC);
    Serial.print(":");
    Serial.print(now.second(), DEC);
    Serial.println();

    Serial.print(" since midnight 1/1/1970 = ");
    Serial.print(now.unixtime());

    Serial.print("Temperature: ");
    Serial.print(rtc.getTemperature());
    Serial.println(" C");

    Serial.println();
}

int lastButtonState = LOW;   // the previous reading from the input pin
int lastLedState = 0; // our LEDs (through MBI5026 driver)
int ledState = 0; // our desired state for the LEDs
int buttonState = LOW;             // the current reading from the input pin



// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long millisLastTouchScan = 0;
unsigned long millisLastTimePrint = 0;


// settings used to control program flow
#define DEBOUNCE_DELAY      50    // the debounce time; increase if the output flickers
#define PERIOD_TOUCH_SCAN   100 // how many milliseconds before we re-scan for touches
#define PERIOD_TIME_PRINT   300000 // how many milliseconds before we re-print the time ( = 5 minutes)

void loop() {
  if((unsigned long)(millis() - millisLastTouchScan) > PERIOD_TOUCH_SCAN) {
    millisLastTouchScan = millis();
    Point p = ts.getPoint();

    if (p.z > __PRESSURE) {
      Serial.print("Raw X = "); Serial.print(p.x);
      Serial.print("\tRaw Y = "); Serial.print(p.y);
      Serial.print("\tPressure = "); Serial.println(p.z);
    }

    // we have some minimum pressure we consider 'valid'
    // pressure of 0 means no pressing!
    if (p.z > __PRESSURE) {
      p.x = map(p.x, TS_MINX, TS_MAXX, 0, 240);
      p.y = map(p.y, TS_MINY, TS_MAXY, 0, 320);

      Serial.print("X = "); Serial.print(p.x);
      Serial.print("\tY = "); Serial.print(p.y);
      Serial.print("\tPressure = "); Serial.println(p.z);
    }
  } // last touch scan

  if((unsigned long)(millis() - millisLastTimePrint) > PERIOD_TIME_PRINT) {
    millisLastTimePrint = millis();
    printTime();
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

      // only log if the new button state is HIGH
      if (buttonState == HIGH) {
        bitSet(ledState, LED_BUTTON); // turn the button back on
        Serial.println("A log needs to be made of this time");
        // TODO: Log this button press to the file
      } else { // buttonState == LOW
        bitClear(ledState, LED_BUTTON); // turn off the button
      }
    }
  }

  // Manage the LED(s) driven through the MBI5026 driver
  if (lastLedState != ledState) {
    Serial.println("Updating LED driver");
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
