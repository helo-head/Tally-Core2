# Core2 ATEM Controller Version 1.2
This version of the code allows you to control an ATEM Mini Pro (up to 4 cameras and 12 macros) via a M5Stack Core2 ESP32 device. 

## Current Features (Version 1.2)
- 	4 Camera control/status buttons
- 	12 Marco control buttons via 3 simulated pages
- 	Ability to read network config data from an SD card
- 	Ability to write network config data in encrypted form into EEPROM so on a subsequent boot the SD card can be removed
- 	Ability to validate EEPROM data to ensure its not corrupted or missing
- Ability to use either client static ip or DHCP
- Displays battery charge/discharge
- Displays current connection status
- Displays "On Air" status

## Attribution
This code is a fork of Aaron Parecki's (aaronpk/am5-core2-atem-controller) code and leverages the following GitHub libraries:

- 	kasperskaarhoj / SKAARHOJ-Open-Engineering Arduino ATEM Libraries
- 	bneedhamia / write_eeprom_strings and sdconfigfile libraries
- 	arduino-libraries / Arduino_CRC32 library
- 	josephpal / esp32-Encrypt library
- 	AronHetLam / Updated ATEM library with streaming status
- 	[@BrianTeeman](https://github.com/brianteeman) / Testing and Debugging

#### Based on the following:
- M5Stack Core2 (Amazon or elsewhere)
- Arduino IDE: http://docs.m5stack.com/#/en/arduino/arduino_core2_development (Version 1.8.19)
- IDE Board: M5Stack-Core2 (Version 2.0.6)
- IDE Library: M5Core2 (Version 0.1.5)

## Libraries
- https://github.com/kasperskaarhoj/SKAARHOJ-Open-Engineering
- https://github.com/josephpal/esp32-Encrypt
- https://github.com/bneedhamia/write_eeprom_strings
- https://github.com/bneedhamia/sdconfigfile
- https://github.com/arduino-libraries/Arduino_CRC32

## License:
The MIT License (MIT)

Copyright (c) 2023 Helo-Head

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Requirements:
- An ATEM Mini Pro or greater for "On Air" functionality
- A static IP address must be assigned for the ATEM. This version does not support dynamic network connections. 
- An SD card with a .cfg file containing the network/configuration information. Note that if weeProm is enabled the software will write the required software configuration read from the SD card in encrypted form to the devices EEPROM and on the next boot if there is no SD card the EEPROM data will be used. 

## Configuration File Format:
**Note:** You cannot use a # in your definitions

File format: field=value

- weeProm - true/false, if true saves config data to EEPROM. EEPROM will automatically be used if no SD card inserted.
- cfgVer - integer representing simple version control (**required**)
- waitEnable - true/false, used for troubleshooting to slow the display down
- waitMS - delay in milliseconds
- M5id - Tally Client ID (**required**)
- ssid - WiFi ID (**required**)
- password - Wifi Password (**required**)
- atemIp - IP address of ATEM switch (**required**)
- tallyIp - IP of tally client (required for static config else optional)
- subMask - Subnet mask (required for static config else optional)
- gatewayIp - Gateway IP address (required for static config else optional)
- dnsIp - DNS Server IP address (required for static config else optional)

### 	Example DHCP Configuration File
	weeProm=true
	cfgVer=1
	waitEnable=true
	waitMS=2000
	M5id=ClientNodeName
	ssid=WiFi-SSID
	password=network_password
	atemIp=192.168.10.240

### 	Example Static IP Configuration File
	weeProm=true
	cfgVer=1
	waitEnable=true
	waitMS=2000
	M5id=ClientNodeName
	ssid=WiFi-SSID
	password=wifi_password
	atemIp=192.168.10.240
	tallyIp=192.168.10.199
	subMask=255.255.255.0
	gatewayIp=192.168.10.1
	dnsIp=192.168.10.1	
