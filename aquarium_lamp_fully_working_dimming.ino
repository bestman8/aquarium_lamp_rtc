#include <Arduino.h>
#include <AceRoutine.h>  // activates SystemClockCoroutine
#include <AceTimeClock.h>
#include <AceWire.h>  // TwoWireInterface
#include <Wire.h>     // TwoWire, Wire


using ace_time::acetime_t;
using ace_time::TimeZone;
using ace_time::BasicZoneProcessor;
using ace_time::ZonedDateTime;
using ace_time::zonedb::kZoneEurope_Amsterdam;
using ace_time::clock::SystemClockCoroutine;
using ace_routine::CoroutineScheduler;

using ace_time::clock::SystemClockLoop;
using ace_time::clock::NtpClock;
using ace_time::clock::DS3231Clock;

using WireInterface = ace_wire::TwoWireInterface<TwoWire>;
WireInterface wireInterface(Wire);
DS3231Clock<WireInterface> dsClock(wireInterface);

static NtpClock ntpClock;

// ZoneProcessor instance should be created statically at initialization time.
static BasicZoneProcessor pacificProcessor;



void setupWifi() {
  Serial.print(F("Connecting to WiFi"));


  WiFi.begin("Paulusma_IOT", "6xJWH5LMqqXVvoau5V3WACRC8");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting ...");
    delay(500);
  }
  Serial.println("Connected" + WiFi.isConnected());

  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
}

static SystemClockCoroutine systemClock(&ntpClock /*reference*/, &dsClock /*backup*/);
// static SystemClockCoroutine systemClock(&ntpClock /*reference*/, &dsClock /*backup*/ );
void printCurrentTime() {
  acetime_t now = systemClock.getNow();

  // Create Pacific Time and print.
  auto my_time_zone = TimeZone::forZoneInfo(&kZoneEurope_Amsterdam,
                                            &pacificProcessor);
  auto pacificTime = ZonedDateTime::forEpochSeconds(now, my_time_zone);
  pacificTime.printTo(SERIAL_PORT_MONITOR);
  SERIAL_PORT_MONITOR.println();
}
COROUTINE(print) {
  COROUTINE_LOOP() {
    printCurrentTime();
    COROUTINE_DELAY(2000);
  }
}

std::array<int, 4> Current_time() {
  acetime_t now = systemClock.getNow();

  // Create Pacific Time and print.
  auto my_time_zone = TimeZone::forZoneInfo(&kZoneEurope_Amsterdam,
                                            &pacificProcessor);
  auto pacificTime = ZonedDateTime::forEpochSeconds(now, my_time_zone);
  // pacificTime.printTo(SERIAL_PORT_MONITOR);
  // SERIAL_PORT_MONITOR.println();
  return { pacificTime.hour(), pacificTime.minute(), pacificTime.second(), pacificTime.dayOfWeek() };
}
int handle_hour_after_24(int hour) {
  if (hour <= 4) {
    hour += 24;
  }
  return hour;
}

// declare global variables
unsigned long dimming_start_time;
bool is_dimming = false;

// define a function to convert seconds to milliseconds
unsigned long secondsToMillis(int seconds) {
  return seconds * 1000;
}

int mapLong(long value, long from_high, int to_low, int to_high) {

    // Serial.println("start");
    // Serial.println(value-0);
    // Serial.println(from_high);
    // Serial.println(to_low);
    // Serial.println(to_high-to_low);
    // Serial.println(long(to_low) + (((long(value)) / (long(from_high))) * (long(to_high) - long(to_low))));
    // Serial.println(long(to_low) + (((double(value)) / (double(from_high))) * (long(to_high) - long(to_low))));


    return long(to_low) + (((double(value)) / (double(from_high))) * (long(to_high) - long(to_low)));
 
}

// define a function to dim the lamp for a given duration and maximum brightness
int handle_dimming(int dimming_dur, bool before_or_after, int max_val=1000 ){
  // check if the dimming time is not set
  if (!is_dimming) {
    // get the current time in milliseconds and store it as the dimming start time
    dimming_start_time = millis();

    // set the dimming time to true
    is_dimming = true;
  }

  // get the current time in milliseconds
  unsigned long currentTime = millis();

  // calculate the time difference between now and when dimming_start_time was set
  unsigned long time = currentTime - dimming_start_time;

  // convert the dimming_dur parameter from seconds to milliseconds
  unsigned long dimming_duration = secondsToMillis(dimming_dur);

  unsigned long dimming_end_time = dimming_start_time+dimming_duration;

   if (before_or_after) {
        return mapLong(time, dimming_duration, max_val, 0);
  } else {
    return mapLong(time, dimming_duration, 0, max_val);
  }

}

  // return the brightness value



int test12345(int hour_on, int min_on, int hour_off, int min_off, int hour_on1, int min_on1, int hour_off1, int min_off1, std::array<int, 3> dimming_dur, std::array<int, 4> hour_min_sec, int brightness) {
  // if (hour>=hour_on and ntp.hours<=hour_off ){
  int hour = hour_min_sec[0];
  int minute = hour_min_sec[1];
  int second = hour_min_sec[2];
  hour = handle_hour_after_24(hour);

  int dimming = (dimming_dur[0] * 60 + dimming_dur[1]) * 60 + dimming_dur[2];
  // int dimming= 2*60;

  int time = (hour * 60 + minute) * 60 + second;
  int time_on_0 = (hour_on * 60 + min_on) * 60;
  int time_on_1 = (hour_on1 * 60 + min_on1) * 60;

  int time_off_0 = (hour_off * 60 + min_off) * 60;
  int time_off_1 = (hour_off1 * 60 + min_off1) * 60;

  std::array<int, 4> dimming_times = { (time_on_0 - dimming), (time_on_1 - dimming), (time_off_0 + dimming), (time_off_1 + dimming) };

  if ((time >= time_on_0 && time <= time_off_0) || (time >= time_on_1 && time <= time_off_1)) {
    is_dimming = false;
    return brightness;

  } else if (time >= dimming_times[0] && time <= time_on_0) {
    return handle_dimming(dimming, false, brightness);
  } else if (time >= dimming_times[1] && time <= time_on_1) {
    return handle_dimming(dimming, false, brightness);
  } else if (time >= time_off_0 && time <= dimming_times[2]) {
    return handle_dimming(dimming, true, brightness);
  } else if (time >= time_off_1 && time <= dimming_times[3]) {
    return handle_dimming(dimming, true, brightness);
  } else
    is_dimming = false;
    return 0;
}
void light_brightness(uint brightness) {
  analogWrite(0, int(brightness * 1.023));
  // Serial.println(brightness);
}

void setup() {
#if !defined(EPOXY_DUINO)
  delay(1000);
#endif
  SERIAL_PORT_MONITOR.begin(9600);
  while (!SERIAL_PORT_MONITOR)
    ;  // Wait until ready - Leonardo/Micro
  setupWifi();
  Wire.begin();
  analogWriteRange(1023);
  analogWriteFreq(500);
  // analogWrite(0, 1023);
  analogWrite(2, 1023);
  light_brightness(0);
  // systemClock.setup();
  dsClock.setup();
  ntpClock.setup();

  // Creating timezones is cheap, so we can create them on the fly as needed.
  auto my_time_zone = TimeZone::forZoneInfo(&kZoneEurope_Amsterdam, &pacificProcessor);

  // Set the SystemClock using these components.
  auto pacificTime = ZonedDateTime::forComponents(2023, 6, 0, 0, 0, 0, my_time_zone);
  systemClock.setNow(pacificTime.toEpochSeconds());

  CoroutineScheduler::setup();
}
int light_brightness_new = 0;
int light_brightness_old = -1;
void loop() {
  CoroutineScheduler::loop();
  std::array<int, 4> current_time_val = Current_time();

  switch (current_time_val[3]) {  //hours, min on, hours, min off,hours, min on, hours, min off, current time
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
      light_brightness_new = test12345(8, 0, 11, 15,  17, 0, 20, 30, { 0, 10, 0 }, current_time_val, 650); /*monday*/
      break;
    // case 2:
    // light_brightness_new = test12345(7, 30, 11, 30, 17, 0, 21, 0, {0,20,0}, current_time_val, 700 ); /*tuesday*/   break;
    // case 3:
    // light_brightness_new = test12345(8, 30, 12, 30, 17, 0, 21, 0, {0,20,0}, current_time_val, 700 ); /*wednesday*/ break;
    // case 4:
    // light_brightness_new = test12345(7, 30, 11, 30, 17, 0, 21, 0, {0,20,0}, current_time_val, 700 ); /*thursday*/  break;
    // case 5:
    // light_brightness_new = test12345(9, 30, 13, 30, 17, 0, 21, 0, {0,20,0}, current_time_val, 700 ); /*friday*/    break;
    // case 6:
    // light_brightness_new = test12345(10, 0, 13, 0,  17, 0, 22, 0, {0,20,0}, current_time_val, 700 ); /*saturday*/   break;
    case 6:
    case 7:
      light_brightness_new = test12345(10, 0, 13, 15, 17, 0, 20, 30, { 0, 10, 0 }, current_time_val, 650); /*sunday*/
      break;
  }


  // light_brightness_new = test12345(10, 0, 14, 0, 20, 5, 20, 7, {0,2,0}, current_time_val, 700 ); /*sunday*/     /*break;*/
  // light_brightness_new = test12345(10, 50, 14, 0, 17, 50, 21, 0, {0,0,0}, current_time_val, 800 ); /*sunday*/     /*break;*/
  // Serial.println(light_brightness_new);

  if (light_brightness_new != light_brightness_old) {
    light_brightness_old = light_brightness_new;
    light_brightness(light_brightness_new);
  }
  // light_brightness(test12345(9, 30, 13, 30, 17,0,21,0,current_time_val ));
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("wifi");
  }
}