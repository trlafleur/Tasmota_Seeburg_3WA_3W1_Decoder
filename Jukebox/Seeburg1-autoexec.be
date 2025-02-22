
#- *************************************** -#
#- *************************************** -#
tasmota.add_rule('mqtt#connected', 
  def ()
    load('Seeburg4.be')
    tasmota.remove_rule('mqtt#connected')
  end )
