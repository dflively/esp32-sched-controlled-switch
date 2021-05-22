idf_component_register(SRCS "esp32-sched-controlled-switch.c"
                       INCLUDE_DIRS "."
					   REQUIRES nvs_flash wifi_prov_connect loki)
