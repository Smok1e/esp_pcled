#include <main.hpp>
#include <utils.hpp>
#include <buffer.hpp>

#include <esp_timer.h>

//========================================

static const char* TAG = "main";

extern const uint8_t DEFAULT_LUA_BEGIN[] asm("_binary_default_lua_start");
extern const uint8_t DEFAULT_LUA_END  [] asm("_binary_default_lua_end"  );

//======================================== NVS

void Main::initNVS()
{
	auto result = nvs_flash_init();
	if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		result = nvs_flash_init();
	}
	ESP_ERROR_CHECK(result);
	
	ESP_LOGI(TAG, "nvs initialized");
}

//======================================== Lua

void Main::initLua()
{
	m_lua = luaL_newstate();
	luaL_openlibs(m_lua);
	
	// Exposing ledstrip API
	lua_newtable(m_lua);
	
	// numLeds constant
	lua_pushnumber(m_lua, CONFIG_LEDSTRIP_LED_NUMBER);
	lua_setfield(m_lua, -2, "numLeds");
	
	// updateRate constant
	lua_pushnumber(m_lua, CONFIG_LEDSTRIP_UPDATE_RATE);
	lua_setfield(m_lua, -2, "updateRate");
	
	// setPixelHSV function
	lua_pushlightuserdata(m_lua, this);
	lua_pushcclosure(m_lua, Main::ApiSetPixelRGB, 1);
	lua_setfield(m_lua, -2, "setPixelRGB");
	
	// setPixelHSV function
	lua_pushlightuserdata(m_lua, this);
	lua_pushcclosure(m_lua, Main::ApiSetPixelHSV, 1);
	lua_setfield(m_lua, -2, "setPixelHSV");
	
	// clear function
	lua_pushlightuserdata(m_lua, this);
	lua_pushcclosure(m_lua, Main::ApiClear, 1);
	lua_setfield(m_lua, -2, "clear");
	
	lua_setglobal(m_lua, "ledstrip");
	
	ESP_LOGI(TAG, "lua initialized");
	
	std::string program;
	if (loadProgram(program))
	{
		ESP_LOGI(TAG, "loaded program (%zu bytes)", program.length());
		executeProgram(program);
	}
	
	else
	{
		ESP_LOGI(TAG, "no saved program found; running default program");
		
		executeProgram(
			std::string_view(
				reinterpret_cast<const char*>(DEFAULT_LUA_BEGIN),
				reinterpret_cast<const char*>(DEFAULT_LUA_END)
			)
		);
	}
}

int Main::ApiSetPixelRGB(lua_State* lua)
{
	auto* instance = reinterpret_cast<Main*>(lua_touserdata(lua, lua_upvalueindex(1)));
	
	int index = luaL_checkinteger(lua, 1) - 1;
	if (!(0 <= index && index < CONFIG_LEDSTRIP_LED_NUMBER))
	{
		lua_pushnil(lua);
		lua_pushstring(lua, "pixel index out of bounds");
		
		return 2;
	}
	
	int r = luaL_checkinteger(lua, 2);
	int g = luaL_checkinteger(lua, 3);
	int b = luaL_checkinteger(lua, 4);
	
	ESP_ERROR_CHECK(led_strip_set_pixel(instance->m_ledstrip, index, r, g, b));
	
	lua_pushboolean(lua, true);
	return 1;
}

int Main::ApiSetPixelHSV(lua_State* lua)
{
	auto* instance = reinterpret_cast<Main*>(lua_touserdata(lua, lua_upvalueindex(1)));
	
	int index = luaL_checkinteger(lua, 1) - 1;
	if (!(0 <= index && index < CONFIG_LEDSTRIP_LED_NUMBER))
	{
		lua_pushnil(lua);
		lua_pushstring(lua, "pixel index out of bounds");
		
		return 2;
	}
	
	int h = luaL_checkinteger(lua, 2);
	int s = luaL_checkinteger(lua, 3);
	int v = luaL_checkinteger(lua, 4);
	
	ESP_ERROR_CHECK(led_strip_set_pixel_hsv(instance->m_ledstrip, index, h, s, v));
	
	lua_pushboolean(lua, true);
	return 1;
}

int Main::ApiClear(lua_State* lua)
{
	auto* instance = reinterpret_cast<Main*>(lua_touserdata(lua, lua_upvalueindex((1))));
	led_strip_clear(instance->m_ledstrip);
	
	lua_pushboolean(lua, true);
	return 1;
}

bool Main::executeProgram(std::string_view source, char* error_buffer /*= nullptr*/, size_t error_buffer_length /*= 0*/)
{
	if (luaL_loadbuffer(m_lua, source.data(), source.length(), "program") != LUA_OK)
	{
		const char* message = lua_tostring(m_lua, -1);
		
		ESP_LOGE(TAG, "program syntax error: %s", message);
		
		if (error_buffer)
			snprintf(error_buffer, error_buffer_length, "syntax error: %s", message);
		
		lua_pop(m_lua, 1);
		return false;
	}
	
	if (lua_pcall(m_lua, 0, 0, 0) != LUA_OK)
	{
		const char* message = lua_tostring(m_lua, -1);
		
		ESP_LOGE(TAG, "program runtime error: %s", message);
		
		if (error_buffer)
			snprintf(error_buffer, error_buffer_length, "runtime error: %s", message);
		
		lua_pop(m_lua, 1);
		return false;
	}
	
	ESP_LOGI(TAG, "program executed successfully");
	m_script_failure = false;
	m_script_start_time = esp_timer_get_time();
	
	return true;
}

void Main::saveProgram(std::optional<std::string_view> source /*= std::nullopt*/)
{
	nvs_handle_t nvs_handle = 0;
	ESP_ERROR_CHECK(nvs_open(TAG, NVS_READWRITE, &nvs_handle));
	
	if (source)
	{
		ESP_ERROR_CHECK(nvs_set_blob(nvs_handle, "program", source->data(), source->length()));
		ESP_LOGI(TAG, "program saved");
	}
	
	else
	{
		auto ret = nvs_erase_key(nvs_handle, "program");
		if (ret == ESP_ERR_NVS_NOT_FOUND)
			ESP_LOGI(TAG, "nothing to erase");
		
		else
		{
			ESP_ERROR_CHECK(ret);
			ESP_LOGI(TAG, "program erased");
		}
	}
	
	ESP_ERROR_CHECK(nvs_commit(nvs_handle));
	
	nvs_close(nvs_handle);
}

bool Main::loadProgram(std::string& program)
{
	nvs_handle_t nvs_handle = 0;
	
	auto ret = nvs_open(TAG, NVS_READONLY, &nvs_handle);
	if (ret == ESP_ERR_NVS_NOT_FOUND)
		return false;
	
	ESP_ERROR_CHECK(ret);
	
	size_t size = 0;
	
	ret = nvs_get_blob(nvs_handle, "program", nullptr, &size);
	if (ret == ESP_ERR_NVS_NOT_FOUND)
	{
		nvs_close(nvs_handle);
		return false;
	}
	
	ESP_ERROR_CHECK(ret);
	
	program.resize(size);
	
	ESP_ERROR_CHECK(nvs_get_blob(nvs_handle, "program", program.data(), &size));
	nvs_close(nvs_handle);
	
	return true;
}

//======================================== Ledstrip

void Main::initLedstrip()
{
	led_strip_config_t ledstrip_config = {};
	ledstrip_config.strip_gpio_num = static_cast<gpio_num_t>(CONFIG_LEDSTRIP_DATA_PIN);
	ledstrip_config.max_leds = CONFIG_LEDSTRIP_LED_NUMBER;
	ledstrip_config.led_model = LED_MODEL_WS2812;
	ledstrip_config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
	ledstrip_config.flags.invert_out = false;
	
	led_strip_rmt_config_t rmt_config = {};
	rmt_config.clk_src = RMT_CLK_SRC_DEFAULT;
	rmt_config.resolution_hz = 10'000'000;
	rmt_config.mem_block_symbols = 64;
	rmt_config.flags.with_dma = false;
	
	ESP_ERROR_CHECK(led_strip_new_rmt_device(&ledstrip_config, &rmt_config, &m_ledstrip));
	
	ESP_LOGI(TAG, "ledstrip initialized");
}

void Main::startLedstripHandler()
{
	xTaskCreate(
		[](void* arg) -> void
		{
			reinterpret_cast<Main*>(arg)->ledstripHandler();
		},
		"Ledstrip handler",
		4096,
		this,
		5,
		nullptr
	);
}

void Main::ledstripHandler()
{
	ESP_LOGI(TAG, "ledstrip handler started");
	
	while (true)
	{
		if (!m_script_failure)
		{
			lua_getglobal(m_lua, "update");
			lua_pushnumber(m_lua, static_cast<float>(esp_timer_get_time() - m_script_start_time) / 1'000'000);
			
			if (lua_pcall(m_lua, 1, 0, 0) != LUA_OK)
			{
				ESP_LOGE(TAG, "update script failed: %s", lua_tostring(m_lua, -1));
				lua_pop(m_lua, 1);
				
				m_script_failure = true;
			}
		}
		
		ESP_ERROR_CHECK(led_strip_refresh(m_ledstrip));
		vTaskDelay(pdMS_TO_TICKS(1000 / CONFIG_LEDSTRIP_UPDATE_RATE));
	}
}

//======================================== Serial

void Main::initSerial()
{
	usb_serial_jtag_driver_config_t config = {};
	config.rx_buffer_size = BUFFSIZE;
	config.tx_buffer_size = BUFFSIZE;

	ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&config));
	ESP_LOGI(TAG, "USB serial/JTAG driver initialized");
}

void Main::serialHandler()
{
	ESP_LOGI(TAG, "serial handler started");
	
	uint8_t buffer[BUFFSIZE] = {};
	while (true)
	{
		int len = usb_serial_jtag_read_bytes(buffer, BUFFSIZE, portMAX_DELAY);
		if (len)
		{
			m_packet.write(buffer, len);
			serialProcessData();
		}
	}
}

void Main::serialProcessData()
{
	while (m_pending_packet_length != 0 || m_packet.remain() >= 8)
	{
		if (!m_pending_packet_length)
		{
			auto offset = m_packet.offset();
			auto signature = m_packet.readUint32();
			
			if (signature != PACKET_SIGNATURE)
			{
				if (!m_last_signature_failure)
				{
					ESP_LOGE(
						TAG,
						"invalid signature received: 0x%08X",
						static_cast<unsigned>(signature)
					);
					
					m_last_signature_failure = true;
				}
				
				m_packet.seek(offset + 1, true);
				continue;
			}
			
			m_last_signature_failure = false;
			m_pending_packet_length = m_packet.readUint32();
		}
		
		if (m_packet.remain() >= m_pending_packet_length)
			serialProcessPacket();
		
		else
			return;
	}
}

// Packet structure:
// +-------------+------------------------+-------------------------------------------------+
// | Field name  | Type                   | Description                                     |
// +-------------+------------------------+-------------------------------------------------+
// | signature   | uint32                 | Packet signature, should be 'PIDR' (0x52444950) |
// +-------------+------------------------+-------------------------------------------------+
// | packet_size | uint32                 | Packet size (message_id + payload)              |
// +-------------+------------------------+-------------------------------------------------+
// | message_id  | uint8                  | Message ID                                      |
// +-------------+------------------------+-------------------------------------------------+
// | payload     | uint8[packet_size - 1] | Message payload                                 |
// +-------------+------------------------+-------------------------------------------------+
void Main::serialProcessPacket()
{
	ESP_LOGI(TAG, "received full packet (%zu bytes)", m_pending_packet_length);
	auto offset = m_packet.offset();
	auto message_id = static_cast<Message>(m_packet.readUint8());
	
	switch (message_id)
	{
		case Message::DEBUG_MESSAGE:
			serialOnDebugMessage();
			break;
			
		case Message::EXECUTE_PROGRAM:
			serialOnExecuteProgram();
			break;
			
		case Message::ERASE_PROGRAM:
			serialOnEraseProgram();
			break;
			
		case Message::PRINT_SAVED_PROGRAM:
			serialOnPrintSavedProgram();
			break;
			
		case Message::REBOOT:
			serialOnReboot();
			break;
			
		default:
			ESP_LOGW(TAG, "unknown message id received: 0x%02X", static_cast<unsigned>(message_id));
			break;
			
	}
	
	m_packet.seek(offset + m_pending_packet_length, true);
	m_packet.discard();
	m_pending_packet_length = 0;
}

// Debug message (id 0x00) payload structure:
// +------------+--------+------------------------+
// | Field name | Type   | Description            |
// +------------+--------+------------------------+
// | message    | string | The message to display |
// +------------+--------+------------------------+
void Main::serialOnDebugMessage()
{
	auto text = m_packet.readString();
	ESP_LOGI(TAG, "received debug message: '%.*s'", text.length(), text.data());
}

// Execute program message (id 0x01) payload structure:
// +------------+--------+------------------------------------------------------+
// | Field name | Type   | Description                                          |
// +------------+--------+------------------------------------------------------+
// | save       | bool   | Defines whether the program should be saved to flash |
// +------------+--------+------------------------------------------------------+
// | source     | string | Program source code                                  |
// +------------+--------+------------------------------------------------------+
void Main::serialOnExecuteProgram()
{
	auto save = m_packet.readBoolean();
	auto source = m_packet.readString();
	auto [size, units] = FormatBytes(source.size());
	
	ESP_LOGI(
		TAG,
		"received program (%.2f %s), save %srequested",
		size,
		units,
		save
			? ""
			: "not "
	);
	
	char error[512] = "";
	if (!executeProgram(source, error, sizeof(error)))
		return;
	
	if (save)
		saveProgram(source);
}

// Erase program message (id 0x02) does not require any payload
void Main::serialOnEraseProgram()
{
	ESP_LOGI(TAG, "received program erase request");
	saveProgram(std::nullopt);
}

// Print saved program message (id 0x03) does not require any payload
void Main::serialOnPrintSavedProgram()
{
	ESP_LOGI(TAG, "received saved program listing request");
	
	std::string program;
	if (!loadProgram(program))
	{
		ESP_LOGE(TAG, "no program is currently saved");
		return;
	}
	
	auto [size, units] = FormatBytes(program.size());
	ESP_LOGI(TAG, "currently saved program source code (%.2f %s):", size, units);
	printf("%.*s\n", program.length(), program.data());
}

// Reboot message (id 0x04) does not require any payload
void Main::serialOnReboot()
{
	ESP_LOGI(TAG, "received reboot request; restarting...");
	esp_restart();
}

//========================================

void Main::run()
{
	initNVS();
	initLua();
	initLedstrip();
	initSerial();
	
	startLedstripHandler();
	serialHandler();
}

//========================================

extern "C" void app_main()
{
	Main instance;
	instance.run();
}

//========================================