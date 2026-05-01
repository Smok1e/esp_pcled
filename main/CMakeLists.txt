idf_component_register(
	SRCS
		"src/main.cpp"
		"src/buffer.cpp"
		"src/utils.cpp"
		
	INCLUDE_DIRS
		"include/"
	
	EMBED_FILES
		"resources/default.lua"
	
	REQUIRES
		nvs_flash
		esp_driver_gpio
		esp_driver_usb_serial_jtag
		led_strip
		esp_timer
		lua
)