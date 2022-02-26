# Leslie MIDI Relay

(c) Kamil Barański kamilbaranski.com
 
## HARDWARE
- ESP8266 / NodeMCU v3.
- Connect MIDI input to D7 (GPIO13). (Check [here](https://www.notesandvolts.com/2015/02/midi-and-arduino-build-midi-input.html) for the proper MIDI In schematics.)
- Connect relays controls to D5 (GPIO14) & D6 (GPIO12). Connect Leslie motors to relays.
- Probably some Leslie speaker and a MIDI controller

## SOFTWARE
- Arduino IDE with additional libraries:
  - [EspSoftwareSerial](https://www.arduino.cc/reference/en/libraries/espsoftwareserial/)
  - [MIDI Library](https://www.arduino.cc/reference/en/libraries/midi-library/)

## USAGE
- Send CCs #01 (Modulation), enjoy :)
