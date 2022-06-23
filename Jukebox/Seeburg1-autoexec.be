
#- *************************************** -#
#- *************************************** -#
class SEEBURG_LOADER : Driver

#- *************************************** -#   
    def init()    
        import mqtt
        tasmota.add_rule ("mqtt#connected", /MyObj3 -> self.connected_MQTT(MyObj3)) 
    end

#- *************************************** -#   
    def connected_MQTT(MyObj3)   
        print("Mqtt Connected: ",MyObj3)      
        load ('Seeburg1.be') 
        end  
    end   
  
#- *************************************** -#
SEEBURG_Loader =   SEEBURG_LOADER()
tasmota.add_driver(SEEBURG_Loader)

#- ************ The Very End ************* -#
