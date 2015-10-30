# WiFi Smart Waterbed Heater
This is an off-shoot of the ESP07_Multi ESP8266 project for use as a thermostat control with OLED display, 2 buttons and sound transducer.  To use as a cooling thermostat, just modify the tempCheck function to switch on at low temp crossover.  

Version 0 has 3 I/Os on the bottom back for 3V3 power in and control of a SSR, an ADC input for thermistor or water sensor, and power/digital in for DS18B20 probe.  Rev 1 has an extra pad for 3V3 on the ADC input, so all 3 connectors are 3-pin.  Adruino code will be posted soon.   

![Prototype](http://www.curioustech.net/images/wb_proto.png)

Display and web interface below.  It currently has a day / night setting for automatic efficiency.  Logging to chart the power usage and temperature changes.  As well as web remote control and global access for vacation adjustment.

![Control](http://curioustech.net/images/waterbed_ui.png)
