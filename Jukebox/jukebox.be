

#- 
   To load this file, compile Tasmota32 with the option as needed....
   Then Load the new binary image in your ESP32 and re-boot it. 
   Open the web page for this device, select Console, then Manage File System
   Rename this Berry file to "autoexec.be", then upload it to the ESP32 file system. 
   Reboot Tasmota, this Berry file will run after re-booting.
   
 -#
   
 #- *************************************** -#
 #-
    CHANGE LOG:
 
    DATE         REV  DESCRIPTION
    -----------  ---  ----------------------
    21-Jun-2022  1.0  TRL - 1st release
       
    Notes:  1)  Tested with 11.1.0.3(tasmota)
        
    ToDo:   1)

    tom@lafleur.us


    This is a jukebox player that get tracks to play from the Seeburg 3WA wallbox via driver (xsns_123_SB-3Wx)
    or via MQTT. It requires a change to the MP3 driver (xdrv_14_mp3) to sense an external busy siginal from 
    the MP3 module. this is then made avilable to Berry as a rule to delay next selection untll MP3 module is no busy

    If you select V10 (index = 200), it toggles random play, on, off, if you select anything else, it reset 
    random play and start to play that selection.

    Will respond to MQTT data for Track, Volume etc... -->  subscribe('RSF/JUKEBOX/#')
 
-#

import string
import math
import mqtt
import json

#- *************************************** -#
#- *************************************** -#
class SEEBURG_DRIVER : Driver
    
    #build an global array-->list to store wallbox selection
    static buf = []
    var BusyFlag           # 0 = busy
    var RandomPlay

#- *************************************** -#   
    def init()
        
        tasmota.add_rule ("Wallbox",   /MyObj ->  self.process_wallbox(MyObj) )
        tasmota.add_rule ("MP3Player", /MyObj2 -> self.process_MP3_busy(MyObj2) )

        self.BusyFlag    = 0                               #  0 = busy
        self.RandomPlay  = false

        math.srand( int (tasmota.rtc()['utc'] ))           # seed random number

        mqtt.subscribe('RSF/JUKEBOX/#')  
        
        tasmota.cmd("MP3Volume 80")                        # set volume
        self.buf.clear()                                   # flush the queue

    end

    #- *************************************** -#
    def queue(index)

        var MaxQueueSize = 10

        if (index > 200) index = 200 end                    # do a bounds check
        if (index <= 0)  index = 0   end

        if (index == 200)                                   # if we have a selection of: V10 inx = 200, toggle random play                  
            if (self.RandomPlay == true )  self.RandomPlay = false end
            if (self.RandomPlay == false ) self.RandomPlay = true  end
            self.buf.clear()                                # flush the queue            
            return                                          # exit function
        end

        if (index == 199)                                   # if we have a track of: V9 index = 199. reset
            tasmota.cmd("MP3Reset")
            self.buf.clear()                                # flush the queue 
            tasmota.cmd("MP3Volume 80")                     # set volume
            return
        end

        self.RandomPlay = false                             # reset random play if we select anything else
        if (self.buf.size() >= MaxQueueSize) return  end    # do not add track selection to queue if full
        #print("Selection Index: ", index)

        self.buf.push(index)                                # add selection to list
        print ("Queue: ", self.buf)                         # print queue
        #print(self.buf)

    end

#- *************************************** -# 
    def mqtt_data(topic, idx, strdata, bindata)

    #print("Topic: ", topic) 
    var MyCmd

        if ( string.find(topic, "Track") > 0) 
            var MQTT_Index = json.load(strdata)
            if (MQTT_Index > 200) MQTT_Index = 200 end
            if (MQTT_Index <  0)  MQTT_Index = 0   end
            #print ("MQTT Index: ", MQTT_Index)
            self.queue(MQTT_Index)                          # send to queue
            return true
        end
      
        if ( string.find(topic, "Volume") > 0) 
            var MyVolume = json.load(strdata)
            if (MyVolume > 100) MyVolume = 100 end
            if (MyVolume <  0)  MyVolume = 0   end
            #print ("Volume: ", MyVolume)
            MyCmd = string.format("MP3Volume %u", int (MyVolume))
            tasmota.cmd(MyCmd)                              # set volume
            return true
        end

        if ( string.find(topic, "EQ") > 0) 
            var MyEQ = json.load(strdata)
            if (MyEQ > 5)  MyEQ  = 5   end
            if (MyEQ < 0)  MyEQ  = 0   end
            #print ("EQ: ", MyEQ)
            MyCmd = string.format("MP3EQ %u", int (MyEQ))
            tasmota.cmd(MyCmd)                              # set command
            return true
        end

        # Folder format use folder to store track, starting at folder 01 -> 99,
        # each folder can have 1->254 songs, folder 00 is not alowed, nor is track 0
        # so, mininmal value is 256 + 1 = 257, max is  99 * 256 = 25,344 + 254 = 25,598
        # used caution, experimental at this time!
        if ( string.find(topic, "Folder") > 0) 
            var MyFolder = json.load(strdata)
            if (MyFolder > 25598)  MyFolder  = 257 end      # we use 257 at folder 1, track 1
            if (MyFolder < 257)    MyFolder  = 257 end
            #print ("Folder: ", MyFolder)
            MyCmd = string.format("MP3Folder %u", int (MyFolder))
            tasmota.cmd(MyCmd)                              # set command
            return true
        end

        if ( string.find(topic, "Reset") > 0) 
            tasmota.cmd("MP3Reset")
            self.buf.clear()                                # flush the queue 
            tasmota.cmd("MP3Volume 80")                     # set volume
            return true 
        end

        if ( string.find(topic, "Pause") > 0) tasmota.cmd("MP3Pause") return true end
        if ( string.find(topic, "Play")  > 0) tasmota.cmd("MP3Play")  return true end
        if ( string.find(topic, "Stop")  > 0) tasmota.cmd("MP3Stop")  return true end

        return false
    end


#- *************************************** -# 
    def process_MP3_busy(MyObj2) 

        #print (" In Process MP3")
        #print("Obj2: ", MyObj2)
        if  MyObj2 == nil print("Bad Obj2")     return end
        if !(MyObj2.contains('MP3Busy'))        return end

        self.BusyFlag = real(MyObj2['MP3Busy'] )
        #print("BusyFlag: ",self.BusyFlag)
    end

#- *************************************** -# 
    def process_wallbox(MyObj)

        #print (" In Process Wallbox")
        #print("Obj: ", MyObj)
        
        if  MyObj == nil print("Bad Obj")       return end
       
        #if !(MyObj.contains('Number_Index'))    return end
        #if !(MyObj.contains('Letter_Index'))    return end
        if !(MyObj.contains('Selection_Index')) return end
            
        #var num = real(MyObj['Number_Index'] )
        #var let = real(MyObj['Letter_Index'] )
        var index = real(MyObj['Selection_Index'] )
        
        self.queue(index)                           # send to queue  
    end


#- *************************************** -#
    def play(Index)

        if (self.BusyFlag == 1)                     # if not busy...
            print ("Playing Track: ", Index)
            var MyCmd = string.format("MP3Track %u", int (Index))
            #print ("MyCmd: ", MyCmd)
            tasmota.cmd(MyCmd)
            print ("Queue: ", self.buf)             # print queue
        end
    end

#- *************************************** -#
    def every_second()

        if (self.RandomPlay == true )               # select a random track, range 1 -> 198      
            if (self.BusyFlag == 1)                 # if not busy...
                var random = math.rand() % (198 + 1 - 1) + 1    # rand() % (Max + 1 - Min) + Min
                print ("Random Track: ", random)
                self.play(random)
                return
            end
        end
    
        if (self.buf.size() == 0) return  end       # nothing to do, selection queue is empty
        if (self.BusyFlag == 1)                     # if not busy...
            var NextTrack = self.buf.pop(0)         # get next track from stack
            self.play(NextTrack)
        end
    end
  
   
#- *************************************** -#
    def web_sensor()
        #tasmota.web_send_decimal(msg)
    end
  

#- *************************************** -#
    def json_append()
        #tasmota.response_append(msg)     
    end
  

end     # end of class SEEBURG_Driver : Driver


#- *************************************** -#
SEEBURG_Driver =   SEEBURG_DRIVER()
tasmota.add_driver(SEEBURG_Driver)

#- ************ The Very End ************* -#

