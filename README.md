For many years now, I have had a working Seeburg 3WA-200 in my kid's den, connected to an iPod. 
This allows them and their friends to play whatever they wanted... but as time goes on, they grew 
up and moved away... (hard to believe...)

So it has been sitting unused for years... As I keep seeing it unused, I keep thinking about what to do with it... 
Well, Tasmota came to the rescue....

I found a number of references to projects with the Seeburg wallbox, many are listed below. But Derek Konigs post at:
~~~
   http://hecgeek.blogspot.com/2017/10/wall-o-matic-interface-1.html
~~~
has a very detailed post on how to decode and use the device to control his Sonos system. His code was developed for the ESP-8266.

I had a number of ESP32 here running Tasmota, so I adapted his code for ESP32 under Tasmota  (Yes.. it's a learning thing...)
This was developed as a sensor under Tasmota, as pulses for the wallboxes are in the 30-50ms range and with a cycle time of ~2.1 seconds.

I now use A1-->A10 to control things around the house, and B1-->V1 for music selection.

It was developed under Tasmota 11.0.0.3 under Visual Studio.

To keep my sanity, I first developed a 3WA-3W1 simulator written in C under Arduino, 
I program this into another ESP32 to send the selection pulses to my Tasmota decoder'
as the 3WA-200 is very noisy...

This program expects a pulse on a pin from a Seeburg 3WA or 3W1 Wallbox...
it decodes the pulses and sends the decoded information on as an MQTT JSON object
  
It supports the 3WA-200 or the 3W1-100 wallboxies. One or the other is selected
in the configure module or template. 'Seeburg 3WA' or 'Seeburg 3W1'. There is also 
a status LED that toggles on every pulse edge. 'Seeburg LED'
 
The wallboxes send out a 24V AC pulse, that must be rectified and converted to 3.3v.

 
It makes use of a FreeRTOS software timer, to timeout improper pulse streams.
  
~~~
MQTT JSON frame:
  ---- Real Time...
  {
  "Wallbox":{
      "Number_Index":10,
      "Letter_Index":1,
      "Model":"3W1",
      "Selection_Index":10,
      "Selection":"A10"
   }
  }
  
  ---- At Telemetry period
  {
   "Time":"2022-05-17T10:54:28",
   "Wallbox":{
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
htps://jukeboxaddicts.proboards.com/thread/1412/seeburg-sch1-4-apu-missing?page=2
https://github.com/dkonigsberg/wallbox-board
~~~

There is a very small chance of a race condition, from when the code has finish decoding the pulse stream 
in the ISR, unitil we send the data out on the one second loop. 
One could quickley push another set of button during that time missing a selection, but unlikely.
  
  
  
