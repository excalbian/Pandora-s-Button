# Change log

## v0.2.7
 - Better handling when SD card not present
 - Fixed SPI speed when SD initialization fails

```
Sketch uses 28750 bytes (89%) of program storage space. Maximum is 32256 bytes.
Global variables use 1427 bytes (69%) of dynamic memory, leaving 621 bytes for local variables. Maximum is 2048 bytes.
```

## v0.2.6
 - Log button presses to CSV

## v0.2.5
 - Show time on the TFT (updated every 30 seconds)
 - Log free heap to serial and the file (and TFT log area)
 - Moved static log message to flash memory (saves 20 bytes of SRAM)
 - Increased global buffer to 50 characters
 - Showing nubmer of button presses since restart on TFT
 - Counting touchscreen taps as button presses (with log message and indicator)

```
Sketch uses 27756 bytes (86%) of program storage space. Maximum is 32256 bytes.
Global variables use 1442 bytes (70%) of dynamic memory, leaving 606 bytes for local variables. Maximum is 2048 bytes.
```

## v0.2.4
 - Moved to using a global buffer for Progmem (flash) strings
 - Fixed PSTR messages that weren't being logged correctly
 - Removed aborts when RTC or SD don't initialize
 - Only set the RTC if it initialized
 - Added header of program to TFT
 - Added log messages on TFT
 - Removed TFT samples

## v0.2.3
 - Added TFT initialization
 - Added text samples to test

```
Sketch uses 27482 bytes (85%) of program storage space. Maximum is 32256 bytes.
Global variables use 1528 bytes (74%) of dynamic memory, leaving 520 bytes for local variables. Maximum is 2048 bytes.
```

## v0.2.2
 - Added log message when button is pressed
 - Added memory profiling (first pass)

```
Sketch uses 21284 bytes (65%) of program storage space. Maximum is 32256 bytes.
Global variables use 1339 bytes (65%) of dynamic memory, leaving 709 bytes for local variables. Maximum is 2048 bytes.
```

 ## v0.2.1
 - Added version log message to setup
 - Added timestamp (millis) to log messages

```
Sketch uses 20850 bytes (64%) of program storage space. Maximum is 32256 bytes.
Global variables use 1391 bytes (67%) of dynamic memory, leaving 657 bytes for local variables. Maximum is 2048 bytes.
```

## v0.2.0
 - Moving to sprintf (c-style) strings

```
Sketch uses 20614 bytes (63%) of program storage space. Maximum is 32256 bytes.
Global variables use 1387 bytes (67%) of dynamic memory, leaving 661 bytes for local variables. Maximum is 2048 bytes.
```

## v0.1.0
 - Initial scaffolding

```
"C:\\Program Files (x86)\\Arduino\\hardware\\tools\\avr/bin/avr-size" -A "c:\\Users\\adam\\OneDrive\\Documents\\Pandora-s-Button\\build/Pandora_s_Button.ino.elf"
Sketch uses 21412 bytes (66%) of program storage space. Maximum is 32256 bytes.
Global variables use 1819 bytes (88%) of dynamic memory, leaving 229 bytes for local variables. Maximum is 2048 bytes.
```