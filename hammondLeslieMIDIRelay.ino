const String PROGRAMNAME = "Hammond Leslie MIDI relay";
const String PROGRAMVERSION = "3.01";
const String PROGRAMDATE = "20220303";
const String PROGRAMAUTHOR = "(c) Kamil Baranski";
const String PROGRAMWWW = "kamilbaranski.com";


#include <SoftwareSerial.h>
#include <MIDI.h>
#include <ESP8266WiFi.h>  // we use it to disable WiFi ;)
#include "MIDICCList.h"


/**
   working mode. default: MIDIENABLED + HALFMOONENABLED + AUTODETECTPEDAL.

   enable PITCHBENDCCMODE if you have Pitch Bend and Modulation on the same joystick
   (this will change speed of the motors as in eg. Kronos; STOP is disabled from MIDI in this mode)

   enable PEDALSWITCHMODE if you want pedal press to switch speed instead of temporarily turning FAST.

   AUTODETECTPEDAL works OK, it shouldn't be changed.
*/

const uint8_t MIDIENABLED = 1;
const uint8_t HALFMOONENABLED = 2;
const uint8_t AUTODETECTPEDAL = 4;
const uint8_t ROLANDPEDAL = 8; // if !AUTODETECTHALFMOON, program will use ROLAND polarity, else will use KURZWEIL_OR_HALFMOON.
const uint8_t PEDALSWITCHMODE = 16;
const uint8_t PITCHBENDCCMODE = 32;
// const uint8_t WORKINGMODE = MIDIENABLED + HALFMOONENABLED + AUTODETECTPEDAL;
const uint8_t WORKINGMODE = MIDIENABLED + HALFMOONENABLED + AUTODETECTPEDAL + PITCHBENDCCMODE + PEDALSWITCHMODE;
// const uint8_t WORKINGMODE = MIDIENABLED + HALFMOONENABLED + ROLANDPEDAL + PITCHBENDCCMODE + PEDALSWITCHMODE;


/**
   debug
*/

// writes debug info on serial port (USB). 115200 bps!
const bool serialDebug = true;

// you might temporarily turn on/off debugMore if you send a Note On/Off.
bool debugMore = false;
const unsigned long slowerTheHalfmoonMs = 0; // use ~50 for halfmoon debugMore ;)
unsigned long previousMillis = 0;


/**
   relays
*/

// relays connected to GPIO14 (D5 on NodeMCU v3) and GPIO12 (D6 on NodeMCU v3).
const uint8_t slowRelayPin = 14; // slow
const uint8_t fastRelayPin = 12; // fast

// consts
const uint8_t STOP = 0;
const uint8_t SLOW = 1;
const uint8_t FAST = 2;
const uint8_t DEFA = 3;
const uint8_t ERRORSPEED = 4;
const String speeds[5] = { "STOP", "SLOW", "FAST", "DEFAULT", "ERROR" };
// what speed to set after boot?
const uint8_t DEFAULTSPEED = SLOW;

struct speedWithReason {
  uint8_t speed;
  uint8_t reason;
};

// we use it for PITCHBENDCCMODE & PEDALSWITCHMODE
uint8_t currentRelaysState = STOP;
uint8_t lastRelaysStateExcludingStop = SLOW; // will be corrected by setRelays();


/**
   halfmoon/pedal
*/

// halfmoon or pedal connected to GPIO5 (D1 on NodeMCU v3) and GPIO4 (D2 on NodeMCU v3).
const uint8_t slowHalfmoonPin = 5;
const uint8_t fastHalfmoonPin = 4;

// STOP usually means also not connected, so we won't change relays to STOP if there's nothing connected on startup.
// And that's the behaviour we want.
uint8_t lastHalfmoonState = STOP;

// we use it for the PEDALSWITCHMODE
bool pedalPressedDuringSwitchMode = false;

const uint8_t debouncedButtonsAfter = 20; // ms

const uint8_t UNRECOGNIZED = 0;
const uint8_t KURZWEIL = 1; // Korg, Kurzweil; classic sustain pedal (or halfmoon or nothing connected).
const uint8_t ROLAND = 2; // Roland, Nord, Yamaha; classic sustain pedal.
const uint8_t KURZWEIL_OR_HALFMOON = 3;
const uint8_t HALFMOON = 4;
const uint8_t HALFMOON_OR_NOTHING = 5;
const String pedalTypes[6] = {
  "Unrecognized",
  "Korg/Kurzweil/Casio sustain pedal",
  "Roland/Nord/Yamaha sustain pedal",
  "Korg/Kurzweil/Casio sustain pedal or halfmoon",
  "Halfmoon",
  "Halfmoon or nothing connected"
};
uint8_t pedalType = UNRECOGNIZED;
// todo: check compatibility and if we can handle Nord Triple Pedal, Studiologic Triple Pedal, Roland DP8, Yamaha FC3a, etc.


/**
   MIDI
*/

// MIDI In goes to GPIO13 (D7 on NodeMCU v3). We do not need to send anything, so TX=0.
#define MYPORT_TX 0
#define MYPORT_RX 13
SoftwareSerial myPort(MYPORT_RX, MYPORT_TX);

MIDI_CREATE_INSTANCE(SoftwareSerial, myPort, MIDI);

// 0..5 = stop; 6..63 = slow, 64..127 = fast;
const uint8_t STOPBELOWVALUE = 6;
const uint8_t SLOWTOVALUE = 63;

// we use it for PITCHBENDCCMODE
uint8_t previousCCValue = 0;

// what CC#? #1 = modulation
const uint8_t CCNUMBER = 1;

// which channel to listen to? MIDI_CHANNEL_OMNI listens to all.
const uint8_t CHANNEL = MIDI_CHANNEL_OMNI;


/***********************************************************************************
   let's begin!
*/

void setup() {
  disableWiFi();
  initSerial();
  delay(1000);  // to ensure COM is ready.
  debugMessage("\n\n" + PROGRAMNAME + " v. " + PROGRAMVERSION + " [" + PROGRAMDATE + "].\n(c) " + PROGRAMAUTHOR + "\n" + PROGRAMWWW + "\n");
  loadSettings();
  initMIDI();
  setPinModes();
  setRelays(DEFA); // right after setPinModes(), cause else we have both relays opened.
  pedalType = checkPedalType(true);
}

void loadSettings() {
  // TODO; at the moment we don't load and save, so let's just write on debugMessage what are the current settings.
  debugMessage("Current settings:");
  debugMessage((String)"MIDI " +
               (String)((WORKINGMODE && MIDIENABLED) ?
                        ("enabled on channel " +
                         ((String)CHANNEL == 0 ?
                          "OMNI" :
                          (String)CHANNEL) +
                         " with CC#" +
                         (String)CCNUMBER) + " [" + CCLIST[CCNUMBER] + "]" :
                        "disabled")
               + ".");
  debugMessage((String)"Halfmoon/pedal " + (String)((WORKINGMODE && HALFMOONENABLED) ? "enabled" : "disabled") + ".");
  debugMessage((String)"Pedal autodetection " + (String)((WORKINGMODE && AUTODETECTPEDAL) ? "enabled" : "disabled") + ".");
  if (!(WORKINGMODE && AUTODETECTPEDAL)) {
    debugMessage((String)"Pedal type is " + (String)((WORKINGMODE && ROLANDPEDAL) ? "set" : "not set") + " to " + pedalTypes[ROLAND] + ".");
  }
  debugMessage((String)"Pedal switch mode " + (String)((WORKINGMODE && PEDALSWITCHMODE) ? "enabled" : "disabled") + ".");
  debugMessage((String)"Pitchbend CC mode " + (String)((WORKINGMODE && PITCHBENDCCMODE) ? "enabled" : "disabled") + ".\n");
}

void disableWiFi() {
  WiFi.mode(WIFI_OFF);
}

void initSerial() {
  // we don't use first serial port (USB) if we don't need to.
  if (serialDebug) {
    Serial.begin(115200);   // 9600 feels to slow.
  }
}

void initMIDI() {
  // software serial port. not sure if this line is correct/needed (MIDI standard is 31.25 kbit/s). But the program works ;)
  myPort.begin(38400, SWSERIAL_8N1, MYPORT_RX, MYPORT_TX, false);
  if (!myPort) {
    // If the object did not initialize, then its configuration is invalid
    debugMessage("Invalid SoftwareSerial pin configuration, check config");
  }
  MIDI.setHandleControlChange(handleControlChange);

  MIDI.setHandleNoteOn([](byte inChannel, byte inPitch, byte inVel) { debugMore = true; });
  MIDI.setHandleNoteOff([](byte inChannel, byte inPitch, byte inVel) { debugMore = false; });
  MIDI.begin(CHANNEL);
}

void setPinModes() {
  pinMode(slowHalfmoonPin, INPUT);
  pinMode(fastHalfmoonPin, INPUT);

  pinMode(slowRelayPin, OUTPUT);
  pinMode(fastRelayPin, OUTPUT);
}

uint8_t checkPedalType(bool boottime) {
  uint8_t newPedalType = pedalType;
  if (WORKINGMODE && AUTODETECTPEDAL) {
    newPedalType = autodetectPedalType(boottime);
    debugMessage("autodetectPedalType() = " + (String) newPedalType, true);
    return newPedalType;
  };

  // autodetection turned off.
  if (WORKINGMODE && ROLAND) {
    newPedalType = ROLAND;
  } else {
    newPedalType = KURZWEIL_OR_HALFMOON;
  }
  if (boottime) {
    debugMessage("Pedal type set to: " + pedalTypes[newPedalType] + ". Autodetection is off.");
  }
  return newPedalType;
}

uint8_t autodetectPedalType(bool boottime) {
  uint8_t newPedalType = pedalType;
  bool slowHalfmoonState = digitalRead(slowHalfmoonPin);
  bool fastHalfmoonState = digitalRead(fastHalfmoonPin);

  // both slow & fast shorted; means we have an unpressed Roland/Yamaha/Nord pedal here (or pressed Kurzweil). This is definitely not a halfmoon.
  if (slowHalfmoonState && fastHalfmoonState) {
    if (boottime) {
      newPedalType = ROLAND;
    } else {
      if ((pedalType == HALFMOON) || (pedalType == HALFMOON_OR_NOTHING)) {
        // We had a halfmoon previously, but this means that user had to disconnect the halfmoon and had to plug something else.
        // We do NOT know what is it because it might have been plugged earlier.
        // We may guess it is ROLAND, but we may be wrong. In this case user will need to reset the program.
        // newPedalType = UNRECOGNIZED;
        // Let's try to assume it is a ROLAND, because it is(?) more popular ;)
        newPedalType = ROLAND;
      } else if (pedalType == KURZWEIL_OR_HALFMOON) {
        // naaah, this is not a halfmoon.
        newPedalType = KURZWEIL;
      }
    }
  }

  // only the fast is shorted; unpressed Kurzweil (or Halfmoon)
  if (fastHalfmoonState && !slowHalfmoonState) {
    if (boottime) {
      newPedalType = KURZWEIL_OR_HALFMOON; // Kurzweil or halfmoon @ fast
    }
  }

  // only the slow is shorted; probably Halfmoon
  if (slowHalfmoonState && !fastHalfmoonState) {
    if (boottime) {
      newPedalType = HALFMOON; // halfmoon @ slow
    } else {
      if (pedalType == HALFMOON_OR_NOTHING) {
        newPedalType = HALFMOON;
      }
    }
  }

  // both pins are open; probably Halfmoon
  if (!slowHalfmoonState && !fastHalfmoonState) {
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
    debugMessage("Detected: " + pedalTypes[newPedalType] );
  }
  return newPedalType;
}

void handleHalfmoon() {
  if (!(WORKINGMODE && HALFMOONENABLED)) {
    return;
  }

  speedWithReason newSpeed = getDebouncedWantedHalfmoonState();
  debugMessage("getDebouncedWantedHalfmoonState() = " + (String)newSpeed.speed + "; reason = " + (String)newSpeed.reason, true);
  if (newSpeed.speed != lastHalfmoonState) {
    if (WORKINGMODE && PEDALSWITCHMODE) {
      if (newSpeed.speed == STOP) {
        pedalPressedDuringSwitchMode = true;
      } else {
        pedalPressedDuringSwitchMode = false;
      }
    }
    debugMessage(halfmoonOrPedal(pedalType) + " turns " + speeds[newSpeed.speed] + ".");
    setRelays(newSpeed.speed);
    lastHalfmoonState = newSpeed.speed;
    // let's check if something has changed :)
    pedalType = checkPedalType(false);
  }
}

speedWithReason getDebouncedWantedHalfmoonState() {
  speedWithReason newSpeed = getWantedHalfmoonState();
  debugMessage("getWantedHalfmoonState(newSpeed) = " + (String)newSpeed.speed + "; reason = " + (String)newSpeed.reason, true);
  if (newSpeed.speed != lastHalfmoonState) {
    delay(debouncedButtonsAfter);
    speedWithReason newSpeed2 = getWantedHalfmoonState();
    debugMessage("getWantedHalfmoonState(newSpeed2) = " + (String)newSpeed2.speed + "; reason = " + (String)newSpeed2.reason, true);
    if (newSpeed.speed == newSpeed2.speed) {
      return newSpeed;
    }
  }
  return {lastHalfmoonState, 254}; // nothing changed.
}

String halfmoonOrPedal(uint8_t pedalType) {
  if ((pedalType == HALFMOON) || (pedalType == HALFMOON_OR_NOTHING)) {
    return "Halfmoon" + halfmoonOrPedalDebugMore(pedalType);
  } else if ((pedalType == ROLAND) || (pedalType == KURZWEIL)) {
    return "Pedal" + halfmoonOrPedalDebugMore(pedalType);
  } else {
    return "Halfmoon/pedal" + halfmoonOrPedalDebugMore(pedalType);
  }
}

String halfmoonOrPedalDebugMore(uint8_t pedalType) {
  if (debugMore) {
    return " [" + (String)pedalType + "]";
  }
  return "";
}

speedWithReason getWantedHalfmoonState() {
  bool slowHalfmoonState = digitalRead(slowHalfmoonPin);
  bool fastHalfmoonState = digitalRead(fastHalfmoonPin);
  if (slowHalfmoonState && fastHalfmoonState) {
    // not pressed Roland
    if ((pedalType == ROLAND) || (pedalType == UNRECOGNIZED)) {
      if (WORKINGMODE && PEDALSWITCHMODE) {
        if (pedalPressedDuringSwitchMode) {
          // we had a pressed pedal, so release should it change the speed to the oposite.
          return {getTheOtherSpeed(lastRelaysStateExcludingStop), 1};
        } else {
          // the pedal hasn't been pressed and isn't pressed now.
          return {lastRelaysStateExcludingStop, 2};
        }
      } else {
        return {SLOW, 3};
      }
    }

    if (pedalType == HALFMOON_OR_NOTHING) {
      // this means some pedal has been connected. We don't know what pedal is this.
      // can we change to slow??
      // HALFMOON OR NOTHING means we cannot use PEDALSWITCHMODE.
      return {SLOW, 4};
    }

    // pressed Kurzweil pedal
    if (WORKINGMODE && PEDALSWITCHMODE) {
      return {STOP, 5};
    } else {
      return {FAST, 6};
    }
  }

  if (slowHalfmoonState) { // slowHalfmoonState == true; fastHalfmoonState == false;
    if ((pedalType == HALFMOON) || (pedalType == HALFMOON_OR_NOTHING)) {
      // we need to ensure this before, else on PEDALSWITCHMODE when user connects halfmoon we might get a disco.
      return {SLOW, 7};
    }
    if (WORKINGMODE && PEDALSWITCHMODE) {
      if (pedalPressedDuringSwitchMode) {
        return {getTheOtherSpeed(lastRelaysStateExcludingStop), 8};
      } else {
        return {STOP, 9};
      }
    }
    return {SLOW, 10};
  }

  // unpressed Kurzweil pedal, pressed Roland or halfmoon on fast
  if (fastHalfmoonState) { // slowHalfmoonState == false; fastHalfmoonState == true;
    if (WORKINGMODE && PEDALSWITCHMODE) {
      if ((pedalType == KURZWEIL) || (pedalType == KURZWEIL_OR_HALFMOON)) {
        if (pedalPressedDuringSwitchMode) {
          return {getTheOtherSpeed(lastRelaysStateExcludingStop), 11};
        } else {
          return {lastRelaysStateExcludingStop, 12};
        }
      } else {
        if (pedalType == HALFMOON) {
          // you should not use PEDALSWITCHMODE with halfmoon, but if you do we try to handle it.
          return {FAST, 13};
        } else {
          // pressed Roland pedal
          return {STOP, 14};
        }
      }
    }

    // pedalswitchmode = off.
    if ((pedalType == KURZWEIL) || (pedalType == KURZWEIL_OR_HALFMOON)) {
      return {SLOW, 15};
    } else {
      return {FAST, 16};
    }
  }

  // halfmoon on stop.
  // slowHalfMoonState == false; fastHalfmoonState == false
  if (WORKINGMODE && PEDALSWITCHMODE) {
    if (pedalType == HALFMOON) {
      return {STOP, 17};
    } else {
      // this happen when the user had removed the pedal!
      return {DEFA, 18};
    }
  } else {
    if ((pedalType == ROLAND) || (pedalType == KURZWEIL)) {
      // we had a pedal, so this means the pedal has been disconnected; go back to DEFA(ult speed)
      return {DEFA, 19};
    } else if ((pedalType == HALFMOON_OR_NOTHING) || (pedalType == UNRECOGNIZED)) {
      // unfortunately we can't set STOP, because on nothing we want DEFA
      // but we may wait for slowState or fastState -> then the pedalType will change and STOP will be possible.
      return {DEFA, 20};
    }
    return {STOP, 21};
  }

  return {ERRORSPEED, 255};    // can't happen; just in case we've missed something in order to find the problem quickly.
}

uint8_t getTheOtherSpeed(uint8_t thisSpeed) {
  if (thisSpeed == SLOW) {
    return FAST;
  } else {
    return SLOW;
  }
}

void handleControlChange(byte inChannel, byte inNumber, byte inValue) {
  if (!(WORKINGMODE && MIDIENABLED)) {
    return;
  }

  // we don't check inChannel, as we'll be here only if the channel is right.
  if (inNumber != CCNUMBER) { // cc#01 = modulation
    return;
  }

  speedWithReason newSpeed = getNewSpeedOnCorrectCC(inValue);
  if (newSpeed.speed == currentRelaysState) {
    // nothing happened (not important CC change)
    debugMessage((String)"ch" + (String)inChannel + " CC#" + (String)inNumber + " " + (String)inValue + "; setting relays to " + speeds[newSpeed.speed] + "; reason: " + (String)newSpeed.reason + ".", true);
    return;
  }

  // change happened!
  if (debugMore) {
    debugMessage((String)"ch" + (String)inChannel + " CC#" + (String)inNumber + " " + (String)inValue + "; setting relays to " + speeds[newSpeed.speed] + "; reason: " + (String)newSpeed.reason + ".", true);
  } else {
    debugMessage((String)"ch" + (String)inChannel + " CC#" + (String)inNumber + " " + (String)inValue + "; setting relays to " + speeds[newSpeed.speed] + ".");
  }
  setRelays(newSpeed.speed);
}

speedWithReason getNewSpeedOnCorrectCC(byte inValue) {
  if (WORKINGMODE && PITCHBENDCCMODE) { // joystick (eg. Kronos) mode
    if ((inValue > SLOWTOVALUE) && (previousCCValue <= SLOWTOVALUE)) {
      // we've just passed the SLOWTOVALUE; let's change speed!
      previousCCValue = inValue;
      if (currentRelaysState == FAST) {
        return {SLOW, 100};
      } else {
        return {FAST, 101};
      }
    }
    // naah, don't change.
    previousCCValue = inValue;
    return {currentRelaysState, 102};
  }

  // !workingMode && PITCHBENDCCMODE
  if (inValue < STOPBELOWVALUE) {  // "<" not "<=", because some users may want 0 as slow (in this case STOPTOVALUE=SLOWTOVALUE=0).
    return {STOP, 103};
  } else if (inValue <= SLOWTOVALUE) {
    return {SLOW, 104};
  } else {
    return {FAST, 105};
  }
}


void setRelays(uint8_t newSpeed) {
  debugMessage("setRelays(" + (String)newSpeed + "=" + speeds[newSpeed] + ");", true);
  if ((newSpeed == DEFA) || (newSpeed == ERRORSPEED)) {
    newSpeed = DEFAULTSPEED;
  }
  if (newSpeed == STOP) {
    digitalWrite(slowRelayPin, !LOW);   // !LOW, because relay turns on when the signal is LOW.
    digitalWrite(fastRelayPin, !LOW);
  } else if (newSpeed == FAST) {
    digitalWrite(slowRelayPin, !LOW);
    digitalWrite(fastRelayPin, !HIGH);
    lastRelaysStateExcludingStop = FAST;
  } else { // if (newSpeed == SLOW) // commented out, cause in case of any problems we don't want both relays open (and the ESP boots like that).
    digitalWrite(fastRelayPin, !LOW);
    digitalWrite(slowRelayPin, !HIGH);
    lastRelaysStateExcludingStop = SLOW;
  }
  currentRelaysState = newSpeed;
}

void loop() {
  MIDI.read();

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= slowerTheHalfmoonMs) {
    previousMillis = currentMillis;
    handleHalfmoon();
  }
}

void debugMessage(String message, bool lowerPriority) {
  if (lowerPriority && debugMore) {
    debugMessage(message);
  }
}

void debugMessage(String message) {
  if (serialDebug) {
    Serial.println(message);
  }
}
