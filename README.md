# WiFi Smart Waterbed Heater
Why do I keep making thermostats?  This is an off-shoot of the ESP07_Multi ESP8266 project for use as a thermostat control with OLED display, 4 buttons, sound transducer, and photoresistor.  Sound now has the option of using PWM or i2s from the RX pin.

There are 2 servo style I/Os on the bottom back. 1: Power in + relay control. 2: Digital for DS18B20 probe.  

Original model:  
![3D](http://curioustech.net/images/wbcase.jpg)

Display and web interface below.  Waterbed.ino history before 1/16/2016 had a day/night setting only.  The new version has 1-8 schedules, dynamically adustable through the web interface.  It's only daily, but has provisions for future weekday control.  There's also a vacation toggle, OLED toggle (full display with scroller, temps, blinking "Heat", or only strobing a pixel when heating for a dark room) and option to average the target and threshold between schedules instead of jump.  Just keep the schedules in chronological order or it may get confused.  

![Control](http://curioustech.net/images/wb_ui.png)  

![NewControl](http://curioustech.net/images/waterbed.jpg)  

![NewControl2](http://curioustech.net/images/waterbedr2.jpg)  


SSR model:  
![Slave2](http://curioustech.net/images/wb_slave2.jpg)  

Averaged schedule  
![Log](http://curioustech.net/images/wbchart.png)  
