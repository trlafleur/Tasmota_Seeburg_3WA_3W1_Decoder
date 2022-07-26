
#- *************************************** -#
#- *************************************** -#
tasmota.add_rule('mqtt#connected', 
  def ()
    load('Seeburg1.be')
    tasmota.remove_rule('mqtt#connected')
  end )
