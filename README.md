# Hammond Leslie MIDI Relay

(c) Kamil Barański kamilbaranski.com
 
## DESCRIPTION
- Relay for the Leslie motors. Can be installed inside the Leslie.
- Allows Leslie motor speed control with MIDI CC#01, halfmoon or sustain pedal
- Tries to guess the type of the pedal/halfmoon on the fly, but in some cases you'll need to reboot. <small>(Don't press the pedal when booting, turn the halfmoon to slow or stop!)</small>

## HARDWARE
- ESP8266 / NodeMCU v3.
- Connect MIDI input to D7 (GPIO13). (Check [here](https://www.notesandvolts.com/2015/02/midi-and-arduino-build-midi-input.html) for the proper MIDI In schematics.)
- or/and connect jack input to D1 (GPIO5) and D2 (GPIO4) with 10k resistors to ground. Connect the jack sleeve to 3V. Plug the halfmoon or sustain footswitch in.
- Connect relays controls to D5 (GPIO14) & D6 (GPIO12). Connect Leslie motors to relays.
- Probably some Leslie speaker and a MIDI controller

## SOFTWARE
- Arduino IDE with additional libraries:
  - [EspSoftwareSerial](https://www.arduino.cc/reference/en/libraries/espsoftwareserial/)
  - [MIDI Library](https://www.arduino.cc/reference/en/libraries/midi-library/)
