This is the PCB design file for rev-2 of this project.

Some minor changes were made:

  Move the ESP32, MP3 player and audio jack over 0.050in
  
  Change pulsein pin assigment
  
  Change voltage regulator package and type

Note: the voltage regulator used in the project must be of an older type like the 78M05 that does not use a soft-start function 
in the design, as they will not respond quick enough... 
Also when using a 25VAC supply, the DC voltage will be about 36V DC, 
so makesure you are using properly rated parts.
