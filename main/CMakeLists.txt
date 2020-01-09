set(COMPONENT_SRCS "main.c")
                   "mod_log.c"
                   "mod_ota.c"
                   "mod_sntp.c"
                   "mod_watt_hour_meter.c"
                   "mod_web_server.c"
                   "mod_wifi.c")
set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()
