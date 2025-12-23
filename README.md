# MAE170_UsbTester
Handheld USB Tester consisting of an Adafruit Feather RP2040 with USB-A host port and OLED display feather board.

Current test sequences include the RAMBo CNC setup and the Heat Transfer setup.

## Running Tests
### RAMBo
Once USB is connected, press the button on the top level menu to run RAMBo test. When succesfully connected to RAMBo the current RAMBo firmware version will be displayed. You can then set the current position to zero the G54 work offset, or run a 2.5 cm x 2.5 cm test pattern to confirm X/Y motors are working. Tester can be hotswapped between RAMBos.
### Heat Transfer
Tester runs a canned sequence where it commands the heat transfer apparatus to a setpoint of 26 Celsius, with the tester printing the current reported plate and ambient temperatures from the heat transfer setup.

## Known Issues
- RAMBo
-- Sometimes it may require 2 attempts to connect to the RAMBo. This is due to an errant serial character transmitted to the RAMBo that causes RAMBo to respond with  `echo:Unknown command: "�M115"`. I have tried to mitigate this with clearing SerialHost rx/tx buffers but have not fully fixed it.
- Heat Transfer
-- The currently installed version of code on the Heat Transfer setup does not fully reset the state of the heat transfer between runs. If the arduino is not given a hard reset, every run after the first run (post reset) will be limited to lowering the lamp, turning on the lamp, immediately turning off the lamp and reporting the current temperature. This provides all information needed to assess the functionality of the Heat Transfer setup, so I haven't planned to flash them all with an update for this edge case.
