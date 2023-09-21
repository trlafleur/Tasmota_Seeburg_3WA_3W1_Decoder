
# Seeburg3.be 
#- 
   To run this file, compile Tasmota32 with the xsns_123 driver option
   Then Load the new binary image in your ESP32. Then add this Seeburg1.be to file system
   and re-boot it. 
   See notes below on using autoexec.be
 -#
   
 #- *************************************** -#
 #-
    CHANGE LOG:
 
    DATE         REV  DESCRIPTION
    -----------  ---  ----------------------
    21-Jun-2022  1.0  TRL - 1st release
    23-Jun-2022  1.0a TRL - fixed issues with MQTT startup, V10, V9 logic added
    25-Jun-2022  1.0b TRL - added DAC commands, change EQ default
    28-Jun-2022  1.0c TRL - added Seeburg 'B6' style track display 
    09-Jul-2022  1.0d TRL - added Seeburg 'B6' style track selection via MQTT
    26-Jul-2022  1.0e TRL - re-seed math.rand at each play
    22-Jul-2023  1.0f TRL - moved to tasmota 13.0.0.3
    23-Jul-2023  1.0g TRL - fixed mqtt_data return = true, number to mynumber
    26-Jul-2023  1.0h TRL - removed disconnected_MQTT() and connected_MQTT()
    28-Jul-2023  2.0h TRL - change method of mqtt.subscribe to full topic based
    22-Sep-2023  3.0i TRL - This is a special version of the MP3 player code to support
                            Mini-Decoder, decode /*/*/SENSOR messages for track information

    Notes:  1)  Tested with 13.1.0.3(tasmota)
        
    ToDo:   1)

    tom@lafleur.us

    
   To use this file, compile Tasmota32 with the option as needed...
   Then Load the new binary image in your ESP32 and re-boot it. 
   
   Open the web page for this device, select Console, then Manage File System,
   then upload Seeburg1.be it to the ESP32 file system. Also upload the Seeburg1-autoexec.be,
   and rename it to autoexec.be
   
   Reboot Tasmota, the autoexec.be will run after re-booting, it will wait for MQTT to be connected,
   then it will load the Seeburg1.be file.
   
   Note: one must do a startup scrip that waits for MQTT to be started, prior to running this code...
      see: Seeburg1-autoexec.be

      This requires the Seeburg X123 decoder running under Tasmota if you have a Seeburg 3WA wallbox, 
      and the Tasmota MP3 driver greater than ver 12.0.3 of Tasmota
 
-#

    import string
    import math
    import json
    import mqtt


#- *************************************** -#
#- *************************************** -#
class SEEBURG_DRIVER : Driver
    
    # build an global array-->list to store wallbox selection
    static buf = []
    static alpha = ["A", "B", "C", "D", "E", "F", "G", "H", "J", "K", "L", "M", "N", "P", "Q", "R", "S", "T", "U", "V"]
    var BusyFlag                                          # 0 = busy
    var AutoPlay
    var DelayCnt 

#- *************************************** -#   
def init()
    
    print("In init")

    # tasmota.add_rule ("Wallbox",    /MyObj  ->  self.process_wallbox(MyObj) )
    tasmota.add_rule ("MP3Player",  /MyObj2 ->  self.process_MP3_busy(MyObj2) )
    
    self.BusyFlag    = 0                               #  0 = busy
    self.AutoPlay    = false  
    self.DelayCnt    = 5

    mqtt.subscribe("RSF/JUKEBOX/Track",    /x1,x2,x3,x4 -> self.xtrack  (x1,x2,x3,x4))      #  These command are direct from MQTT
    mqtt.subscribe("RSF/JUKEBOX/Volume",   /x1,x2,x3,x4 -> self.xvolume (x1,x2,x3,x4))
    mqtt.subscribe("RSF/JUKEBOX/EQ",       /x1,x2,x3,x4 -> self.xeq     (x1,x2,x3,x4))
    mqtt.subscribe("RSF/JUKEBOX/DAC",      /x1,x2,x3,x4 -> self.xdac    (x1,x2,x3,x4))
    mqtt.subscribe("RSF/JUKEBOX/Folder",   /x1,x2,x3,x4 -> self.xfolder (x1,x2,x3,x4))
    mqtt.subscribe("RSF/JUKEBOX/Reset",    /x1,x2,x3,x4 -> self.xreset  (x1,x2,x3,x4))
    mqtt.subscribe("RSF/JUKEBOX/Pause",    /x1,x2,x3,x4 -> self.xpause  (x1,x2,x3,x4))
    mqtt.subscribe("RSF/JUKEBOX/Play",     /x1,x2,x3,x4 -> self.xplay   (x1,x2,x3,x4))
    mqtt.subscribe("RSF/JUKEBOX/Stop",     /x1,x2,x3,x4 -> self.xstop   (x1,x2,x3,x4)) 

    mqtt.subscribe("RSF/JUKEBOX/SENSOR",   /x1,x2,x3,x4 -> self.xMiniPlay   (x1,x2,x3,x4))  # this command come fron the Tasmota X123 hardware driver

    tasmota.cmd("MP3Volume 100")                       # set volume
    tasmota.cmd("MP3EQ 3")                             # set EQ

    self.buf.clear()                                   # flush the queue
end


#- *************************************** -# 
    def queue(index)

        # print ("In Queue")

        var MaxQueueSize = 32

        if (index > 200) index = 200 end                #  Set range limits
        if (index <  1)  index = 1   end

        if (index == 200)   
            self.buf.clear()                            # clear the current buffer         
            if (self.AutoPlay == true )  
                self.AutoPlay = false 
                tasmota.cmd("MP3Reset")                 # Stop current play
                print("AutoPlay Off")
            else 
                self.AutoPlay = true
                print("AutoPlay On") 
            end
            return            
        end

        if (index == 199)          
            print ("Reset")
            tasmota.cmd("MP3Reset")
            self.buf.clear()           
            self.AutoPlay = false
            tasmota.cmd("MP3Volume 100") 
            tasmota.cmd("MP3EQ 3")                       # set EQ 
            return    
        end

        # All other request...
        self.AutoPlay = false                 
        if (self.buf.size() >= MaxQueueSize)
            print("Max Queue") 
        else
            # Add request to the queue
            self.buf.push(index)                            
            print ("Queue: ", self.buf) 
        end       
    end


#- *************************************** -# 
    def xtrack(topic, idx, strdata, bindata)

        print ("In Track")

        var MyCmd 
        var MQTT_Index

        if  (strdata == nil)  return true end

        if ((bindata[0] >= 0x30) && (bindata[0] <= 0x39))                    # we have just a number         
            MQTT_Index = int (strdata )

        else                                                                 # we have an alpha + number
            if ( ( (bindata[0] >= 0x41) && (bindata[0] <= 0x56) ) || ( (bindata[0] >= 0x61) && (bindata[0] <= 0x76) ) )  # we allow both cases  
                if ( (bindata[0] == 0x49) || (bindata[0] == 0x4f) ) return true end     # Check for I and O  <-- not used
                if ( (bindata[0] == 0x69) || (bindata[0] == 0x6f) ) return true end 
                
                MQTT_Index = int (bindata[0] & 0x1f )                   
                if (MQTT_Index > 8)  MQTT_Index = MQTT_Index -1 end          # adjust for missing I                         
                if (MQTT_Index > 13) MQTT_Index = MQTT_Index -1 end          # adjust for missing O       
                var mynumber = int ( ( bindata [1] & 0x0f) )                 # strip ASCII MSB to get a number     
                if (mynumber == 0) mynumber = 10 end       
                MQTT_Index = ( (MQTT_Index -1) * 10 ) + mynumber                  
            end
        end  # end of if-else
        
        print("MQTT_Index", MQTT_Index)
        self.queue(MQTT_Index) 
        return true
    end
      

#- *************************************** -# 
def xMiniPlay(topic, idx, strdata, bindata)

    print("In xMiniPlay")
   
    import json
    import string
    var sensor=json.load(strdata)

    if (sensor.contains('Wallbox') )
        var sensor2 = sensor.item('Wallbox')
        if (sensor2.contains('Selection_Index'))
            var Index = sensor2.item('Selection_Index')
            print("989", Index)
            self.queue(Index)
        else
            print("Bad Opject", sensor) 
        end
    return true
    end
end


#- *************************************** -# 
     def xvolume(topic, idx, strdata, bindata)

        print("In Volume")

        if  (strdata == nil)  return true end
        var MyVolume = int (strdata)
        if (MyVolume > 100) MyVolume = 100 end
        if (MyVolume <  0)  MyVolume = 0   end
        var MyCmd = string.format("MP3Volume %u", int (MyVolume))
        print(MyCmd)
        tasmota.cmd(MyCmd)                              # set volume
        return true
    end

   
#- *************************************** -# 
    def xeq(topic, idx, strdata, bindata)
        
        print("In EQ")
        
        if  (strdata == nil)  return true end
        var MyEQ = int (strdata)
        if (MyEQ > 5)  MyEQ  = 5   end
        if (MyEQ < 0)  MyEQ  = 0   end
        var MyCmd = string.format("MP3EQ %u", int (MyEQ))
        tasmota.cmd(MyCmd)                              # set command
        return true
    end


#- *************************************** -# 
    def xdac(topic, idx, strdata, bindata) 

        print("In DAC")
        
        if  (strdata == nil)  return true end
        var MyDAC = int (strdata) 
        if (MyDAC > 1)  MyDAC  = 1   end
        if (MyDAC < 0)  MyDAC  = 0   end
        var MyCmd = string.format("MP3DAC %u", int (MyDAC))
        tasmota.cmd(MyCmd)                              # set command
        return true
    end


 #- *************************************** -# 
    # Folder format uses folder to store track, starting at folder 01 -> 99,
    # each folder can have 1->254 songs, folder 00 is not alowed, nor is track 0
    # so, mininmal value is 256 + 1 = 257, max is  99 * 256 = 25,344 + 254 = 25,598
    # used caution, experimental at this time!
    def xfolder(topic, idx, strdata, bindata) 

        print("In Folder")
        
        if (strdata == nil)  return true end
        var MyFolder = int (strdata) 
        if (MyFolder > 25598)  MyFolder  = 257 end      # we use 257 at folder 1, track 1
        if (MyFolder < 257)    MyFolder  = 257 end
        var MyCmd = string.format("MP3Folder %u", int (MyFolder))
        tasmota.cmd(MyCmd)                              # set command
        return true
    end

    
#- *************************************** -# 
    def xreset(topic, idx, strdata, bindata)

        print("In Reset")
        
        tasmota.cmd("MP3Reset")
        self.buf.clear()                                # flush the queue 
        tasmota.cmd("MP3Volume 100")                    # set volume
        tasmota.cmd("MP3EQ 3")                          # set EQ
        return true 
    end


#- *************************************** -# 
    def xpause(topic, idx, strdata, bindata) 

        print("In Pause")
        
        tasmota.cmd("MP3Pause") 
        return true 
    end

            
#- *************************************** -# 
    def xplay(topic, idx, strdata, bindata) 

        print("In Play")
        
        tasmota.cmd("MP3Play") 
        return true 
    end


#- *************************************** -# 
    def xstop(topic, idx, strdata, bindata) 
        
        print("In Stop")
        
        tasmota.cmd("MP3Stop")
        return true
    end


#- *************************************** -# 
    def process_MP3_busy(MyObj2) 

        if  MyObj2 == nil print("Bad Obj2")     return false end
        if !(MyObj2.contains('MP3Busy'))        return false end
        self.BusyFlag = int (MyObj2['MP3Busy'] )                # get busy flag from MP3 driver
        return true
    end


#- *************************************** -# 
#-    def process_wallbox(MyObj)

        print("In Process Wallbox Command")
        print ("MyObj: ", MyObj)
        if  MyObj == nil print("Bad Command")   return end
       
        #if !(MyObj.contains('Number_Index'))   return end
        #if !(MyObj.contains('Letter_Index'))   return end
        if !(MyObj.contains('Selection_Index')) return true end      
            
        #var num = real(MyObj['Number_Index'] )
        #var let = real(MyObj['Letter_Index'] )
        var index = int(MyObj['Selection_Index'] )              # get track selection from Wallbox
        print("Index: ", index)
        self.queue(index)                                       # send to queue 
        return true
    end
-#

#- *************************************** -#
    def play(Index)

        print("In Play: ", Index)

        if (self.BusyFlag == 1)                                  # not busy = 1...
            Index = int (Index)
            var a = int (Index/10)
            var n = int (Index % 10)
            if (n == 0) a = a - 1 end                            # adjust for Seeburg odd number of 1-->0 (10)
            # print("a: ", a, " n: ", n)
            var alpha1 = string.format("%s%d", self.alpha[a], int (n) )
            print ("Playing Track: ", Index, ":", alpha1)

            var MyCmd = string.format("MP3Track %u", int (Index))
            tasmota.cmd(MyCmd)
            print ("Queue: ", self.buf)                          # print queue
        end
    end


#- *************************************** -#
    def every_second()

       # print ("In Every Second")

       self.DelayCnt = self.DelayCnt + 1                            # delay between plays
       if ( self.DelayCnt >= 5)                                     # set at 5 second
            self.DelayCnt = 0
            if (self.AutoPlay == true )                             # select a random track, range 1 -> 198      
                if (self.BusyFlag == 1)                             # if not busy...
                    math.srand( int (tasmota.rtc()['utc'] ))        # seed random number
                    var random = math.rand() % (198 + 1 - 1) + 1    # rand() % (Max + 1 - Min) + Min
                    print ("Playing Random Track:  ", random)
                    self.play(random)
                end
            else
            if (self.buf.size() == 0) return true end               # nothing to do, selection queue is empty
                if (self.BusyFlag == 1)                             # if not busy...
                    var NextTrack = self.buf.pop(0)                 # get next track from stack
                    self.play(NextTrack)                            # play it!
                end
            end
        end
    return true
    end   # end of def every_second()

end     # end of class SEEBURG_Driver : Driver

#- *************************************** -#
SEEBURG_Driver =   SEEBURG_DRIVER()
tasmota.add_driver(SEEBURG_Driver)

#- ************ The Very End ************* -#

