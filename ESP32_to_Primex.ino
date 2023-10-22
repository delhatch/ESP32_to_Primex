// Emulates a Garmin 18x-LVC GPS unit, for operation with the Primex FM-72 transmitter.
// Just sends GPRMC sentences. Ignores the strings sent from the FM-72. No Garmin start-up handshake.

#include "Arduino.h"
#include <Esp.h>
#include "TimeLib.h"   // Contains the function ( setTime() ) to set ESP32's internal RTC.
#include "WiFi.h"

#define RXp2 16
#define TXp2 17

// The following string is the prototype for simulating the NMEA GPS sentence.
char gpsstring[100] = "$GPRMC,HHMMSS,A,4730.0000,N,12230.0000,W,000.0,306.8,DDMMYY,016.3,E,A*66\0\0\0\0\0\0\0";
int DDMMYYat = 53;
// The following string is
String startup = "$GPRMC,021307,V,4735.5873,N,12233.7814,W,,,060522,016.3,E,N*16";
char str[100];
uint8_t chk;
int i;
char timestrc[10];
int wherestar;

static long int nowTime = 0;     // The current program time, in millis().
static long int lastRanTime = 0; // The last time (in millis() ) that the GPS data was generated. 1 per second.
int state = 0;
int alreadyUpdated = 0;

// WiFi & NTP constants
const char* ssid = "junk";  // Your SSID here.
const char* pass = "";  // Your wifi password here.
int trycount;
const char* ntpServer = "pool.ntp.org";
struct tm ntp_time;   // Holds the time as retreived from the NTP server.
const long  gmtOffset_sec = 0;      // Use this to have NTP return GMT (which is what the SMA inverter uses).
const int   daylightOffset_sec = 0;
tmElements_t TimeLib_time;    // Used to break up a UTC seconds "time_t" item into a structure with hour,min,sec,etc...
time_t thisTime;        // This is set from the NTP server.
time_t useTime;
uint8_t rtc_hour;
uint8_t rtc_minute;
uint8_t rtc_sec;
//uint8_t rtc_dayofweek;
uint8_t rtc_dayofmonth;
uint8_t rtc_month;
uint8_t rtc_year;

// This will detect if the power-up was "cold" (power-cycle) or "warm" (Wifi connection failed, to rebooted).
// If "warm" reboot, just start sending GPS sentences again.
RTC_NOINIT_ATTR  double cold_boot;

void setup() {
  Serial.begin(115200);
  Serial2.begin(4800, SERIAL_8N1, RXp2, TXp2);
  alreadyUpdated = 0;   // Indicate that the ESP32 RTC has not been set, so at 3:30 am local time, set it.

  // Setup Wifi  ------------------------------------------------
  trycount = 0;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    trycount++;
    if ( trycount >= 25) ESP.restart();  // Reboot the ESP32.
  }
  Serial.println(" ");
  // Get time from NTP server  ---------------------------------
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  getLocalTime(&ntp_time);
  if (!getLocalTime(&ntp_time)) {
    Serial.println("Failed to obtain time! Rebooting now!");
    ESP.restart();
  }
  else {
    Serial.print("NTP time (UTC): ");
    Serial.println(&ntp_time, "%A, %B %d %Y %H:%M:%S");
  }
  TimeLib_time.Second = ntp_time.tm_sec;   //       | 0-60
  TimeLib_time.Minute = ntp_time.tm_min;   //       | 0-59
  TimeLib_time.Hour = ntp_time.tm_hour;    //       | 0-23
  TimeLib_time.Wday = ntp_time.tm_wday + 1;//   1-7 | 0-6  (days since Sunday)
  TimeLib_time.Day = ntp_time.tm_mday;     //       | 1-31
  TimeLib_time.Month = ntp_time.tm_mon + 1;//  1-12 | 0-11
  TimeLib_time.Year = ntp_time.tm_year - 70;    //   Offset from 1970 | Years since 1900
  thisTime = makeTime( TimeLib_time );  // Convert tm structure into a time_t integer seconds.
  Serial.print("NTP time in seconds (UTC): ");
  Serial.println( thisTime );
  setTime( thisTime );               // Sets the ESP32 RTC clock from the UTC time

  state = 0;

  lastRanTime = millis() - 1000;  // Ensures the first GPS message will be sent immediately.

} // End of setup()

void loop() {
  nowTime = millis();

  switch ( state ) {
    case 0 : delay(9000);
      state = 1;
      break;

    case 1 :
      // Is it time to send the GPS sentence to the FM-72? (Do it every 1 second.)
      if ( nowTime > (lastRanTime + 1000) ) {
        lastRanTime = nowTime;
        SendGPS();
        break;
      }
      // Now see if it is time to update the ESP32 time from the NTP server (at 3:30 am local time).
      thisTime = now();    // Get UTC time from the ESP32, in "UTC seconds".
      breakTime( thisTime, TimeLib_time );   // Convert local "UTC seconds" time into hour,min,sec, etc...
      rtc_hour       = (uint8_t)TimeLib_time.Hour;
      rtc_minute     = (uint8_t)TimeLib_time.Minute;

      if ( (rtc_hour == 10) && (rtc_minute == 30) ) {   // 10:30 UTC = 3:30 am local.
        if ( alreadyUpdated == 0 ) {
          UpdateClock();
          alreadyUpdated = 1;
        }
      }
      else {
        alreadyUpdated = 0;  // Indicate that the ESP32 RTC has not been set, so at next 3:30 am local time, set it.
      }
      break;

    default : state = 4;
      break;
  } // End of SWITCH statement
} // End of loop()

//----------------------------------------------------------
void SendGPS( void ) {
  
  useTime = now();    // Get UTC time from the ESP32.
  breakTime( useTime, TimeLib_time );   // Convert UTC seconds into integer hour, min, sec, etc...
  rtc_hour       = (uint8_t)TimeLib_time.Hour;
  rtc_minute     = (uint8_t)TimeLib_time.Minute;
  rtc_sec        = (uint8_t)TimeLib_time.Second;
  //rtc_dayofweek  = (uint8_t)TimeLib_time.Wday;
  rtc_dayofmonth = (uint8_t)TimeLib_time.Day;
  rtc_month      = (uint8_t)TimeLib_time.Month;
  rtc_year       = (uint8_t)TimeLib_time.Year - 30;   // breakTime returns "Years since 1970"

  // Replace the time portion of the GPS sentence with the current time.
  sprintf(str, "%02d", rtc_hour);
  gpsstring[7] = str[0];
  gpsstring[8] = str[1];
  sprintf(str, "%02d", rtc_minute);
  gpsstring[9] = str[0];
  gpsstring[10] = str[1];
  sprintf(str, "%02d", rtc_sec);
  gpsstring[11] = str[0];
  gpsstring[12] = str[1];

  // Replace the date portion of the GPS sentence with the current date.
  sprintf(str, "%02d", rtc_dayofmonth);
  gpsstring[DDMMYYat] = str[0];
  gpsstring[DDMMYYat+1] = str[1];
  sprintf(str, "%02d", rtc_month);
  gpsstring[DDMMYYat+2] = str[0];
  gpsstring[DDMMYYat+3] = str[1];
  sprintf(str, "%02d", rtc_year);
  gpsstring[DDMMYYat+4] = str[0];
  gpsstring[DDMMYYat+5] = str[1];

  // Now calculate the checksum.
  chk = 0;
  for ( i = 1; i <= 99; i++ ) {
    chk ^= gpsstring[i];
    if ( gpsstring[i + 1] == '*' ) break;
  }
  //sprintf(str, "checksum (in hex) = %X\r\n",chk);
  wherestar = i + 2;
  sprintf(str, "%02X", chk);
  gpsstring[wherestar] = str[0];
  gpsstring[wherestar + 1] = str[1];
  Serial.println( &gpsstring[0] );     // Debugging. To see the GPS sentence being sent to the FM-72. Can comment out.
  Serial2.println( &gpsstring[0] );
  //Serial.println(&str[0]);

  return;
}

//-------------------------------------------------------------------------
void UpdateClock( void ) {
  // Check to see if the WiFi is connected. If not, re-connect.

  while ( WiFi.status() != WL_CONNECTED ) {
    Serial.println("Required reconnecting to WIFI network! First attempt.");
    WiFi.disconnect();
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    trycount = 0;
    while ( WiFi.status() != WL_CONNECTED ) {
      delay(500);
      Serial.print(".");
      trycount++;
      if ( trycount >= 25) break;  // Reboot the ESP32.
    }
    if ( WiFi.status() != WL_CONNECTED ) {
      Serial.println("Required reconnecting to WIFI network! Second attempt.");
      WiFi.disconnect();
      delay(100);
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, pass);
      trycount = 0;
      while ( WiFi.status() != WL_CONNECTED ) {
        delay(500);
        Serial.print(".");
        trycount++;
        if ( trycount >= 25) ESP.restart();  // Reboot the ESP32.
      }
    }
  } // End of "While not connected."

  // Connected, so get NTP time.
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  getLocalTime(&ntp_time);
  if (!getLocalTime(&ntp_time)) {
    Serial.println("Failed to obtain time! Rebooting now!");
    ESP.restart();
  }
  else {
    Serial.print("NTP time (UTC): ");
    Serial.println(&ntp_time, "%A, %B %d %Y %H:%M:%S");
  }
  TimeLib_time.Second = ntp_time.tm_sec;   //       | 0-60
  TimeLib_time.Minute = ntp_time.tm_min;   //       | 0-59
  TimeLib_time.Hour = ntp_time.tm_hour;    //       | 0-23
  TimeLib_time.Wday = ntp_time.tm_wday + 1;//   1-7 | 0-6  (days since Sunday)
  TimeLib_time.Day = ntp_time.tm_mday;     //       | 1-31
  TimeLib_time.Month = ntp_time.tm_mon + 1;//  1-12 | 0-11
  TimeLib_time.Year = ntp_time.tm_year - 70;    //   Offset from 1970 | Years since 1900
  thisTime = makeTime( TimeLib_time );  // Convert tm structure into a time_t integer seconds.
  Serial.print("NTP time in seconds (UTC): ");
  Serial.println( thisTime );
  setTime( thisTime );               // Sets the ESP32 RTC clock from the adjusted time

  return;
}
