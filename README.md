For many years now, I have had a working Seeburg 3WA-200 in my kid's den, connected to an iPod. 
This allows them and their friends to play whatever they wanted... but as time goes on, they grew 
up and moved away... (hard to believe...)

So it has been sitting unused for years... As I keep seeing it unused, I keep thinking about what to do with it... 
Well, Tasmota came to the rescue...

I found a number of references to projects with the Seeburg wall box, many are listed below. But "Derek Konigs" post at:
~~~
   http://hecgeek.blogspot.com/2017/10/wall-o-matic-interface-1.html
~~~
He has a very detailed post on how to decode and use the device to control his Sonos system. 
His code was developed for the ESP-8266.

I had a number of ESP32 here running Tasmota, so I adapted his code for ESP32 under Tasmota  (Yes.. it's a learning thing...)
This was developed as a sensor under Tasmota, as pulses for the wall boxes are in the 30-50ms range and with a cycle time of ~2.1 seconds.

I now use A1-->A10 to control things around the house, and B1-->V8 for music selection.
V10 selects random mode, and V9 resets the MP3 module.

It was developed/tested under Tasmota 12.0.2 and updated to 13.0.0.3 under Visual Studio Code with PaltformIO.

To keep my sanity, I first developed a 3WA-3W1 emulator written in C under Arduino, 
I program this into another ESP32 to send the selection pulses to my Tasmota decoder,
as the 3WA-200 is very noisy... and my finger was tired of pressing the button...

There is now a Standalone version for Arduino without Tasmota.

This program expects a pulse on a pin from a Seeburg 3WA or 3W1 Wall box...
it decodes the pulses and sends the decoded information as an MQTT JSON object,
that I use in Node-Red to make decisions.
  
It supports the 3WA-200 (also 3WA-160) or the 3W1-100 wall boxes. One or the other is selected
in the configure module or template (NOT both!). 'Seeburg 3WA' or 'Seeburg 3W1'. 

There is also an optional status LED that toggles on every pulse edge. 'Seeburg LED'
 
The wall boxes send out a 24V AC pulse, that must be rectified and converted to 3.3v.
See the URL above for detail on the converter from 24VAC...
 
Added a small PCB with an Adafruit Feather ESP32, decoder, power supply, and a DFRobot DFR0299 MP3 player.
(Flyron Technology Co), Schematic and PCB files are located on GitHub. Parts in the design are 
what I had on hand.  The PC board was made by Seeed Fusion.

There are now two PCB decoder boards, the Original version as above with decoder and MP3 player and a new mini-version that is about 1.1 X 2.1in that just contains the decoder and ESP32 processor that can be embedded inside the wall box.

There is now a Berry script that connects the 3WA code with the Tasmota MP3 player, this can take 
command from the 3WA wall box or via MQTT, making it a virtual jukebox. It will work without the 
3WA wall box as a general-purpose MQTT control jukebox. It would be easy to expand its track selection
from the current 200 track. Selecting V10, set-reset random mode, and V9 will reset the MP3 player.

This project requires some knowledge of Tasmota and the ability to modify files and compile a special version that includes the 'xsns123' driver in the build. It also requires some working knowledge of the Berry scripting language used in Tasmota.

There is also a hardware component that needs to be built to connect the Seeburg wall box to the ESP32 processor used with Tasmota.


~~~
MQTT JSON frame:
  ----> Real Time...
  {
  "Wallbox":{
      "Number_Index":10,
      "Letter_Index":1,
      "Selection_Index":10,
      "Selection":"A10"
   }
  }
  
  ----> At the Telemetry period
  {
   "Time":"2022-05-17T10:54:28",
   "Wallbox":{
       "Model":"3W1",
       "Last_Selection":"A10"
   }
  }
  ~~~

~~~
Seeburg References:

https://www.thejukeboxman.com/shop/Seeburg
https://www.jukeboxparts.nl/en/product/seeburg-springs-for-wallbox-pages-3w1/
https://www.jukebox-world.de/en/Jukebox-Parts-USA-oxid/Seeburg/Seeburg-Accessories/
http://www.wallbox2mp3.com/en/step-by-step-integrate-wallbox2mp3-in-seeburg-3w1/
https://web.archive.org/web/20210228154529/http://wallbox.weebly.com/index.html
https://www.retrofutureelectrics.com/seeburg-wall-o-matic/
http://www.cdadapter.com/download/wipod.pdf
https://web.archive.org/web/20210308161120/http://wallbox.weebly.com/1-interfacing-the-rpi.html
https://github.com/CottonThePirate/wallboxController
https://www.mikesarcade.com/arcade/titlestrips.html
https://www.dropbox.com/sh/0fbnbqzyi3citzt/AAD7SpEoWXZuEbpDUHgknUwDa?dl=0
https://jukeboxaddicts.proboards.com/thread/1412/seeburg-sch1-4-apu-missing?page=2
https://github.com/dkonigsberg/wallbox-board
https://wiki.dfrobot.com/DFPlayer_Mini_SKU_DFR0299
https://github.com/PowerBroker2/DFPlayerMini_Fast
https://blog.michaelamerz.com/wordpress/seeburg-wall-o-matic-counting-pulses-on-esp-8266/
https://www.kerryveenstra.com/2017/12/15/dusty-diners-juke-o-matic/
https://github.com/Abac71/Seeburg-IO/
https://walljuke.com/the-walljuke-manual/
https://arduinoplusplus.wordpress.com/2018/07/23/yx5300-serial-mp3-player-catalex-module/
~~~

There is a very small chance of a race condition, from when the code has finished decoding the pulse stream 
in the ISR, until we send the data out on the one-second loop. 
One could quickly push another set of buttons during that time missing a selection, but unlikely.
  
  
  
