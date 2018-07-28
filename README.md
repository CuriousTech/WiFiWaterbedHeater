# WiFi Smart Waterbed Heater
Why do I keep making thermostats?  This is an off-shoot of the ESP07_Multi ESP8266 project for use as a thermostat control with OLED display, 2 buttons and sound transducer.  

There are 3 servo style I/Os on the bottom back. 1: Power in + SSR control. 2: ADC + 3V3 for thermistor, water sensor etc. 3: Digital for DS18B20 probe.  

![3D](http://curioustech.net/images/wbcase.jpg)

Display and web interface below.  Waterbed.ino history before 1/16/2016 had a day/night setting only.  The new version has 1-8 schedules, dynamically adustable through the web interface.  It's only daily, but has provisions for future weekday control.  There's also a vacation toggle, OLED toggle (full display with scroller, temps, blinking "Heat", or only strobing a pixel when heating for a dark room) and option to average the target and threshold between schedules instead of jump.  Just keep the schedules in chronological order or it may get confused.  

![Control](http://curioustech.net/images/wb_ui.png)


![Slave](http://curioustech.net/images/wb_slave.png)

Version 2 slave: $14.00 PCB from OSHPark, $1.77 3.3V 700mA supply (eBay), $8.59 SSR, $0.30 PPTC (20~100mA), $1.48 plug
![Slave2](http://curioustech.net/images/wb_slave2.jpg)

It's live and logging as of Jan 5th.  The temp can shoot up if you slosh it around a bit, but overall it works quite well, and safer and has more features than what's on the market.  I'll work on efficiency when more data comes in.  

Averaged schedule 
![Log](http://curioustech.net/images/wbchart.png)
