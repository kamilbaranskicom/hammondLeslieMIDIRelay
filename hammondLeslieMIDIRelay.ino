/**
 * 
 * Hammond Leslie MIDI relay v. 1.0
 * (c) Kamil Baranski 20220222;
 * kamilbaranski.com
 * 
 */

#include <SoftwareSerial.h>
#include <MIDI.h>
#include <ESP8266WiFi.h>  // we use it to disable WiFi ;)

// MIDI In goes to GPIO13. We do not need to send anything.
#define MYPORT_TX 0
#define MYPORT_RX 13
SoftwareSerial myPort(MYPORT_RX, MYPORT_TX);

MIDI_CREATE_INSTANCE(SoftwareSerial, myPort, MIDI);

// relays connected to GPIO14 and GPIO12.
const uint8_t relay1Pin = 14; // slow
const uint8_t relay2Pin = 12; // fast

// consts
const uint8_t STOP = 0; 
const uint8_t SLOW = 1; 
const uint8_t FAST = 2; 

// what speed to set after boot?
const uint8_t DEFAULTSPEED = SLOW;

// 0..5 = stop; 6..64 = slow, 65..127 = fast
const uint8_t STOPTOVALUE = 5;
const uint8_t SLOWTOVALUE = 64;

// what CC#? #1 = modulation
const uint8_t CCNUMBER = 1;

// which channel to listen to? MIDI_CHANNEL_OMNI listens to all.
const uint8_t CHANNEL = MIDI_CHANNEL_OMNI;

// writes info about CCs on serial port (USB). 115200 bps!
const boolean serialDebug = false;

void setup() {
  WiFi.mode(WIFI_OFF);
  
  // software serial port. not sure if this line is correct/needed (MIDI standard is 31.25 kbit/s). But the program works ;)
  myPort.begin(38400, SWSERIAL_8N1, MYPORT_RX, MYPORT_TX, false);

  // we don't use first serial port (USB) if we don't need to.
  if (serialDebug) {
    Serial.begin(115200);   // 9600 feels to slow.
    Serial.println("Hammond Leslie MIDI relay.");

    if (!myPort) {
      // If the object did not initialize, then its configuration is invalid
      Serial.println("Invalid SoftwareSerial pin configuration, check config");
    };
  }

  pinMode(relay1Pin, OUTPUT);
  pinMode(relay2Pin, OUTPUT);

  setRelays(DEFAULTSPEED);

  MIDI.setHandleControlChange(handleControlChange);

  MIDI.begin(CHANNEL);
}

void loop() {
  MIDI.read();
}

void handleControlChange(byte inChannel, byte inNumber, byte inValue) {
  String message = "";
  if (inNumber == CCNUMBER) { // #cc01 = modulation
    if (inValue <= STOPTOVALUE) {
      setRelays(STOP);
      message = "STOP";
    } else if (inValue <= SLOWTOVALUE) {
      setRelays(SLOW);
      message = "SLOW";
    } else {
      setRelays(FAST);
      message = "FAST";
    }
  }
  if (serialDebug) {
    Serial.println((String)"ch" + (String)inChannel + (String)" CC#" + (String)inNumber + (String)" " + (String)inValue + (message != "" ? "; setting relays to " + message + "." : ""));
  }
}

void setRelays(uint8_t mode) {
  if (mode == STOP) {
    digitalWrite(relay1Pin, !LOW);   // !LOW, because relay turns on when the signal is LOW (?)
    digitalWrite(relay2Pin, !LOW);
  } else if (mode == SLOW) {
    digitalWrite(relay2Pin, !LOW);
    digitalWrite(relay1Pin, !HIGH);
  } else if (mode == FAST) {
    digitalWrite(relay1Pin, !LOW);
    digitalWrite(relay2Pin, !HIGH);
  }
}
