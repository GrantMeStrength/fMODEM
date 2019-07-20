# fMODEM
This repo contains the source code for an Arduino-based "fake modem" that can connect older RS232-only computers to the internet over WiFi. It runs on an Arduino UNO WiFi Rev 2 connected to a cheap MAX3232-based TTL-to-RS232 Serial port card on its TX/RX pins.

![The inside of a fMODEM, showing the Arduino UNO Wifi Rev 2, and a TTL RS232 card](modem1.jpeg)

Using the fMODEM connected to a RS232 port - and running an app such as KERMIT under CP/M - allows vintage computers (or their modern recreations) to experience the horror of the modern internet like the rest of us. In practice this means using Telnet servers, so you're spared the worst parts.

![The outside of a fMODEM, connected to a Altair-Duino](modem2.jpeg)

# Operation

Once the Arduino is turned on, it communicates over the serial port with the computer and displays a welcome message on whatever terminal program is running. For old time's sake, the project uses "AT" codes - the way in which old Hayes compatible modems were controlled. For example, typing **ATI** will return a status message. 

Typically you would assign the WiFi hotspot details using **AT SSID=** and **AT password=** and save those: **AT SAVE**. You would then either type **AT WIFI** or restart. Once connected to the local WiFi, you would use **AT TELNET=A.SERVER.COM** to start the fun.

Here's the list of commands:

## Configure WiFi:
  * AT ssid=Your WiFI Name e.g. AT ssid=MYHOTSPOT
  * AT password=Your Wifi password e.g. AT passowrd=SecretPW
  * AT SAVE
  
## Connect to WiFi:
  * AT WIFI (Once you have done an AT SAVE it will store details in EEPROM and use them next power up)

## Connect to server:
  * AT TELNET=telnet.server.address e.g. AT TELNET=towel.blinkenlights.nl
  
## Other commands
  * AT LOGOUT to cancel connection
  * AT DROP to drop WiFi connection
  * AT LIST to list available access points
  * ATI for a status message
  * AT HELP to see this text

# Telnet servers to try

* towel.blinkenlights.nl
* FreeChess.org
* More at http://mewbies.com/acute_terminal_fun_telnet_public_servers_watch_star_wars_play_games_etc.htm

# Building the device

The hardware consists of:

* An Arduino UNO Wifi Rev2
* A MAX3232 based RS232 to TTL adaptor, e.g. [Anmbest 2PCS MAX3232 3.3V to 5V DB9 Male RS232 Serial](https://www.amazon.com/gp/product/B07LBDZ9WG)
* Four LEDs
* Four 330Ohm resisters

Connect the Arduino TX/RX to the RX/TX pins on the RS232/TTL adaptor. Connect the power and GND pins on the adaptor to GND and 5V on the Arduino.

The LEDs connect to pins 4,5,6 and 7 on the Arduino, with a resistor to ground for each of them.



# Issues

* I've found that the WiFi connection is not working when a DC power supply is connected directly to the Arduino, rather than the USB connection. This could be nothing more than using the wrong spec power supply on my part.

* No hardware flow control is even attempted never mind supported. There are, however, four LEDs.

* After starting work on this project, I found other solutions such as [Bo Zimmerman's Zimodem](https://github.com/bozimmerman/Zimodem), which is a decidedly more comprehensive solution. As Bo's modem runs on the ESP8266/ESP, it can be built on a single circuit board - see [8-Bit Bruno's WiFiModem](https://github.com/8bit-bruno/WiFiModem) - and so my project really makes sense only if you have an Arduno UNO WiFi Rev2 lying around, or just want to do things in a different way.
