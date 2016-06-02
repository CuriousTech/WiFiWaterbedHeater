# WiFi Smart Waterbed Heater
Why do I keep making thermostats?  This is an off-shoot of the ESP07_Multi ESP8266 project for use as a thermostat control with OLED display, 2 buttons and sound transducer.

There are 3 servo style I/Os on the bottom back. 1: Power in + SSR control. 2: ADC + 3V3 for thermistor, water sensor etc. 3: Digital for DS18B20 probe.  Since the ESP easily resets or freezes it isn't safe enough for this project and PWM is a pain, the 10F320 is used to  listen to a heartbeat, reset the ESP on timeout, and handle PWM for the speaker.  The beep can be bypassed by solder jumping across for direct control of the speaker.  

![rev1](http://www.curioustech.net/images/waterbedr1.png)

Display and web interface below.  Waterbed.ino history before 1/16/2016 had a day/night setting only.  The new version has 1-8 schedules, dynamically adustable through the web interface.  It's only daily, but has provisions for future weekday control.  There's also a vacation toggle, OLED toggle (full display with scroller, temps, blinking "Heat", or only blinking dot when heating), schedule display name changing (i.e. ?N3="Afternoon"&N4="Dusk") and option to average the target and threshold between schedules instead of jump.  Just keep the schedules in chronological order or it may get confused.  

![Control](http://curioustech.net/images/wb_ui2.png)


![Slave](http://curioustech.net/images/wb_slave.png)


It's live and logging as of Jan 5th.  The temp can shoot up if you slosh it around a bit, but overall it works quite well, and safer and has more features than what's on the market.  I'll work on efficiency when more data comes in.  

Simple schedule  
![Log](http://curioustech.net/images/wb_hist1.png)

Averaged schedule 

![Log2](http://curioustech.net/images/wb_hist.png)
