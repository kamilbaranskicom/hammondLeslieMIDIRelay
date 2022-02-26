/**

   Hammond Leslie MIDI relay v. 2.0
   (c) Kamil Baranski 20220226;
   kamilbaranski.com

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
const uint8_t slowRelayPin = 14; // slow
const uint8_t fastRelayPin = 12; // fast

// halfmoon or pedal connected to D1 (GPIO5) and D2 (GPIO4).
const uint8_t slowHalfmoonPin = 5;
const uint8_t fastHalfmoonPin = 4;

// consts
const uint8_t STOP = 0;
const uint8_t SLOW = 1;
const uint8_t FAST = 2;
const uint8_t DEFA = 3;
const String speeds[4] = { "STOP", "SLOW", "FAST", "DEFAULT" };
// what speed to set after boot?
const uint8_t DEFAULTSPEED = SLOW;


// STOP means also not connected, so we won't change relays to STOP if there's nothing connected on startup.
// And that's the behaviour we want.
// TODO: or maybe should we check on startup if we have the pedal which is normally closed?
uint8_t lastHalfmoonState = STOP;
const uint8_t debouncedButtonsAfter = 20; // ms

const uint8_t UNRECOGNIZED = 0;
const uint8_t KURZWEIL = 1; // Korg, Kurzweil; classic sustain pedal (or halfmoon or nothing connected).
const uint8_t ROLAND = 2; // Roland, Nord, Yamaha; classic sustain pedal.
const uint8_t KURZWEIL_OR_HALFMOON = 3;
const uint8_t HALFMOON = 4;
const uint8_t HALFMOON_OR_NOTHING = 5;
const String pedalTypes[6] = {
  "Unrecognized",
  "Korg/Kurzweil sustain pedal",
  "Roland/Nord/Yamaha sustain pedal",
  "Korg/Kurzweil sustain pedal or halfmoon",
  "Halfmoon",
  "Halfmoon or nothing connected"
};
uint8_t pedalType = UNRECOGNIZED;

// 0..5 = stop; 6..64 = slow, 65..127 = fast
const uint8_t STOPTOVALUE = 5;
const uint8_t SLOWTOVALUE = 64;

// what CC#? #1 = modulation
const uint8_t CCNUMBER = 1;

// which channel to listen to? MIDI_CHANNEL_OMNI listens to all.
const uint8_t CHANNEL = MIDI_CHANNEL_OMNI;

// writes info about CCs on serial port (USB). 115200 bps!
const bool serialDebug = false;

void setup() {
  WiFi.mode(WIFI_OFF);

  // software serial port. not sure if this line is correct/needed (MIDI standard is 31.25 kbit/s). But the program works ;)
  myPort.begin(38400, SWSERIAL_8N1, MYPORT_RX, MYPORT_TX, false);

  // we don't use first serial port (USB) if we don't need to.
  if (serialDebug) {
    Serial.begin(115200);   // 9600 feels to slow.
    Serial.println("\n\nHammond Leslie MIDI relay.\n");

    if (!myPort) {
      // If the object did not initialize, then its configuration is invalid
      Serial.println("Invalid SoftwareSerial pin configuration, check config");
    }
  }

  pinMode(slowHalfmoonPin, INPUT);
  pinMode(fastHalfmoonPin, INPUT);

  pedalType = checkPedalType(true);

  pinMode(slowRelayPin, OUTPUT);
  pinMode(fastRelayPin, OUTPUT);

  setRelays(DEFA);

  MIDI.setHandleControlChange(handleControlChange);
  MIDI.begin(CHANNEL);
}

uint8_t checkPedalType(bool boottime) {
  uint8_t newPedalType = pedalType;
  
  if (digitalRead(slowHalfmoonPin) && digitalRead(fastHalfmoonPin)) { // slow & fast pressed; means we have a Roland/Yamaha/Nord pedal here.
    if (boottime) {
      newPedalType = ROLAND;
    } else {
      if ((pedalType == HALFMOON) || (pedalType == HALFMOON_OR_NOTHING)) {
        // this shouldn't happen on halfmoon. This means user has disconnected the halfmoon and plugged something else.
        // We do not know, what is it (it might have been plugged earlier).
        // newPedalType = UNRECOGNIZED;
        // Let's try to assume it is a ROLAND, because it is more popular ;)
        newPedalType = ROLAND;
      } else if (pedalType == KURZWEIL_OR_HALFMOON) {
        // naaah, this is not a halfmoon.
        newPedalType = KURZWEIL;
      }
    }
  } else if (digitalRead(fastHalfmoonPin)) {
    if (boottime) {
      newPedalType = KURZWEIL_OR_HALFMOON; // Kurzweil or halfmoon @ fast
    } else {
      // wrong:
      // if (pedalType == HALFMOON_OR_NOTHING) {
      //   newPedalType = HALFMOON;
      // }
    }
  } else if (digitalRead(slowHalfmoonPin)) {
    if (boottime) {
      newPedalType = HALFMOON; // halfmoon @ slow
    } else {
      if (pedalType == HALFMOON_OR_NOTHING) {
        newPedalType = HALFMOON;
      }
    }
  } else {
    // both pins are open
    if (boottime) {
      newPedalType = HALFMOON_OR_NOTHING;
    } else {
      if (pedalType == KURZWEIL_OR_HALFMOON) {
        newPedalType = HALFMOON;
      } else if (pedalType == ROLAND) {
        newPedalType = HALFMOON_OR_NOTHING;
      }
    }
  }
  
  if (newPedalType != pedalType) {
    Serial.println("Detected: " + pedalTypes[newPedalType] );
  }
  return newPedalType;
}

void handleHalfmoon() {
  uint8_t newSpeed = getHalfmoonSpeed();
  if (newSpeed != lastHalfmoonState) {
    // the position has been changed, but hey, let's wait a moment and check once more.
    delay(debouncedButtonsAfter);
    if (newSpeed == getHalfmoonSpeed()) {
      // still the position is changed :)
      if (serialDebug) {
        Serial.println("Halfmoon/pedal turns " + speeds[newSpeed] + ".");
      }
      setRelays(newSpeed);
      lastHalfmoonState = newSpeed;
    }
    // let's check if something has changed :)
    pedalType = checkPedalType(false);
  }
}

uint8_t getHalfmoonSpeed() {
  bool slowHalfmoonState = digitalRead(slowHalfmoonPin);
  bool fastHalfmoonState = digitalRead(fastHalfmoonPin);
  if (slowHalfmoonState && fastHalfmoonState) {
    // if we have both, we set newSpeed depending on pedalType detected on boot time.
    if ((pedalType == ROLAND) || (pedalType == UNRECOGNIZED)) {
      return SLOW;
    } else if (pedalType == HALFMOON_OR_NOTHING) {
      // this means some pedal has been connected. We don't know what pedal is this.
      // can we change to slow??
      return SLOW;
    } else {
      return FAST;
    }
  } else if (slowHalfmoonState) {
    return SLOW;
  } else if (fastHalfmoonState) {
    if ((pedalType == KURZWEIL) || (pedalType == KURZWEIL_OR_HALFMOON)) {
      return SLOW;
    } else {
      return FAST;
    }
  } else {  // (slowHalfMoonState || fastHalfmoonState) == 0
    if ((pedalType == ROLAND) || (pedalType == KURZWEIL)) {
      // we had a pedal, so this means the pedal has been disconnected; go back to DEFA(ult speed)
      return DEFA;
    } else if ((pedalType == HALFMOON_OR_NOTHING) || (pedalType == UNRECOGNIZED)) {
      // unfortunately we can't set STOP, because on nothing we want DEFA
      // but we may wait for slowState or fastState -> then the pedalType will change and STOP will be possible.
      return DEFA;
    }
    return STOP;
  }
}

void handleControlChange(byte inChannel, byte inNumber, byte inValue) {
  uint8_t newSpeed;
  // we don't check inChannel, as we'll be here only if the channel is right.
  if (inNumber == CCNUMBER) { // cc#01 = modulation
    if (inValue <= STOPTOVALUE) {
      newSpeed = STOP;
    } else if (inValue <= SLOWTOVALUE) {
      newSpeed = SLOW;
    } else {
      newSpeed = FAST;
    }
    setRelays(newSpeed);
    if (serialDebug) {
      Serial.println((String)"ch" + (String)inChannel + (String)" CC#" + (String)inNumber + (String)" " + (String)inValue + "; setting relays to " + speeds[newSpeed] + ".");
    }
  }
}

void setRelays(uint8_t newSpeed) {
  if (newSpeed == DEFA) {
    newSpeed = DEFAULTSPEED;
  }
  if (newSpeed == STOP) {
    digitalWrite(slowRelayPin, !LOW);   // !LOW, because relay turns on when the signal is LOW.
    digitalWrite(fastRelayPin, !LOW);
  } else if (newSpeed == SLOW) {
    digitalWrite(fastRelayPin, !LOW);
    digitalWrite(slowRelayPin, !HIGH);
  } else if (newSpeed == FAST) {
    digitalWrite(slowRelayPin, !LOW);
    digitalWrite(fastRelayPin, !HIGH);
  }
}

void loop() {
  MIDI.read();
  handleHalfmoon();
}
