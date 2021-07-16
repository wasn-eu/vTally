# vTally - Wireless Tally and VISCA converter for vMix
# (c) 2021 by CaliHC.de

This is a tally system based on an esp8266 (Wemos D1 mini) and the vMix TCP API.
It can convert VISCA UDP Control Signals to VISCA over serial.
It is tested on a cisco PHD1080p12x camera.

## Installation

### Hardware

To use it as tally only you need a Wemos D1 mini board and a Wemos D1 mini RGB shield.  
For using the converter function you need the shield, see folder shield.

### Software

#### 1. Install Arduino IDE

Download the Arduino IDE from the [Arduino website](https://www.arduino.cc/en/main/software) and install it.  
After the installation is complete go to File > Preferences and add http://arduino.esp8266.com/stable/package_esp8266com_index.json to the additional Board Manager URL. Go to Tools > Board > Board Manager, search for *esp8266* and install the latest version. After the installation go to Tools > Board and select *Wemos D1 R2* as your default board.  

#### 2. libraries

Adafruit_NeoPixel library is needed.

#### 3. Uploading static files

Connect the Arduino to the computer with a USB cable.  
The static files in the vTally/data folder must be uploaded using the Tools > ESP8266 Sketch Data Upload in the Arduino IDE.  

#### 4. Uploading firmware

Upload the vTally/vTally.ino from this repository to the Arduino by pressing the Upload button. After the upload the tally will restart in Connecting mode (see the Muliple states section).  

## Getting Started

### Multiple states

There are three states in which a tally can find itself.  

#### 1. Connecting

| Color  | Meaning      | Led intensity | 
|--------|--------------|---------------|
| blue   | Connecting   | Dim           |

In this state the tally is trying to connect to WiFi and vMix based on the settings.  

#### 2. Access point

| Color  | Meaning      | Led intensity | 
|--------|--------------|---------------|
| purple | Settings     | Dim           |

In this state the tally was unable to connect to WiFi and it turned itself to access point mode. It can be accessed by connecting to the WiFi network with the SSID *vMix_Tally_#* (# is the tally number) and password *vMix_Tally_#_pwd* (# is the tally number). Once connected the settings can be changed by going to the webpage on IP address 192.168.4.1.  

#### 3. Tally

| Color  | Meaning      | Led intensity | 
|--------|--------------|---------------|
| green  | Preview      | Full          |
| orange | Live/program | Full          |
| blue   | Off          | Dim           |

In this state the tally is connected to WiFi and vMix. It detects new tally states and shows them using the rgb led.  
You can adjust the colors and brightness on the web page.

### Setup over Webpage

Network and tally settings can be edited on the built-in webpage. To access the webpage connect to the same WiFi network and navigate to the IP address in a browser.  
On this webpage the WiFi SSID, WiFi password, vMix hostname, tally number, LED colors, LED brightness and VISCA settings can be changed. It also shows some basic information of the device. 
