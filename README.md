# WiFi Smart Waterbed Heater
Why do I keep making thermostats?  This is an off-shoot of the ESP07_Multi ESP8266 project for use as a thermostat control with OLED display, 2 buttons and sound transducer.  To use as a cooling thermostat, just modify the tempCheck function to switch on at high temp crossover.  

There are 3 servo style I/Os on the bottom back. 1: Power in + SSR control. 2: ADC + 3V3 for thermistor, water sensor etc. 3: Digital for DS18B20 probe or other acc.  Rev 1 + PIC will actually be used.  Since the ESP easily resets or freezes it isn't safe enough for this project and PWM is a pain, the 10F320 will be used to control the heater by listening to a heartbeat as well as handling PWM for the speaker.  

Adruino code will be posted soon.   

![Prototype](http://www.curioustech.net/images/waterbed.png)

Display and web interface below.  It currently has a day / night setting for automatic efficiency.  Logging to chart the power usage and temperature changes.  As well as web remote control and global access for vacation adjustment.

![Control](http://curioustech.net/images/waterbed_ui.png)
