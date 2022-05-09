# ESP32_to_Primex
For a Primex transmitter, uses an ESP32 to simulate the required Garmin GPS receiver. Connects to NTP to get the time.

I have a Primex FM-72 transmitter, but wanted to place it where there is no chance of getting a GPS signal. So I created
this project to use an ESP32 to emulate the required Garmin GPS receiver.

The ESP32 connects to an NTP server over Wifi, and then sends the appropriate messages (NMEA sentences) to the FM-72's "GPS socket".
The FM-72 thinks it is connected to a Garmin GPS receiver, and works perfectly.

WIRING INSTRUCTIONS:

ESP32 TX2 pin: This signal needs logic invertion. You can use a MAX232 chip, or simply invert it using a 74LS14 IC chip. (Run the 74LS14 from +5 volts.) Wire the inverted signal (output of the 74LS14 or output of the MAX232 chip) to pin 3 of the FM-72 "GPS socket" DB-9 connector (RXD).

ESP32 GND pin: Wire this to pin 5 of the FM72 "GPS socket" DB-9 connector (GND).

ESP32 VIN pin: Wire this to pin 9 of the FM72 "GPS socket" DB-9 connector (+5VDC).

GPS SOCKET FIGURE:
As you look at the FM72 connector, the pinout is as follows.
Note that the image below is "upside down" from how you will see it.
   In other words, if you hold the FM72 unit upside down, it will look
   like the figure below.
Note that the DB9 connector has male pins.

<pre><code>
/-----------------\
|  1  2  3  4  5  |
|   6  7  8  9    |
  \------------/
</code></pre>
