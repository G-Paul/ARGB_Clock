**[WORK IN PROGRESS !!]**

# ARGB Clock -> Code

Arduino Code for the ARGB 7-segment Clock.
Few things to keep in mind: 

### External Libraries: 
- [FastLED](https://fastled.io/) v3.7.0
- iarduino_APDS9930 : For the ambient light sensor

### Configurations: 
- Set the WiFi SSID and Password for your own network. The clock uses NTP to sync time every time it powers on.
- NTP - GMTC offset and Daylight offset - as per your location
- SERIAL_DEBUG: set to false to get rid of all the serial print statements.
- STATUS_LED: set to true if you have a status led (single neopixel, for the Waveshare ESP32-S3-Mini, there is a LED on pin 21)
- ENABLE_OTA_UPDATE: set to false if you don't want to perform any updates over WiFi. Setting this to false also disables WiFi after first fetching time from NTP.
- FORMAT_12HR: Clock mode 12hr or 24 hr. true means 12 hr, false means 24hr
- FIXED_BRIGHTNESS: Uncomment the line and set the value in order to fix the brightness and disable ambient light detection.
- SHOW_FIRST_ZERO: when false, disables the first zero in time. Thus, if time is 01:00, it shows 1:00 instead. Only for zeros



## To Do: 
- Configure the use of the Button for enabling and changing modes.
- More LED modes
- Changing LED modes on the fly
- Option to use an RTC and making it completely offline
- Weather notifications. 
