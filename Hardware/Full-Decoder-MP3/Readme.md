This is the PCB design file for rev-2b of this project.

Some minor changes were made:

  Move the ESP32, MP3 player, and audio jack over 0.050in
  
  Change pulsein pin assigment
  
  Change the voltage regulator package and type

  Added an extra 4-pin jack to support an external BlueTooth transmitter.

Note: the voltage regulator used in the project must be of an older type like
the 78M05 or 720M05 that do not use a soft-start function,
as they will not respond quickly enough... 
Also when using a 25VAC supply, the DC voltage will be about 36V DC, 
so make sure you are using properly rated parts.
