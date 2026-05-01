#pragma once

#include <array>
#include <mutex>
#include <memory>

#include <FreeRTOS/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_system.h>
#include <esp_log.h>

#include <driver/usb_serial_jtag.h>

#include <nvs_flash.h>

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include <led_strip.h>
#include <led_strip_rmt.h>

#include <buffer.hpp>

//========================================

constexpr size_t   BUFFSIZE         = 1024;
const     uint32_t PACKET_SIGNATURE = *reinterpret_cast<const uint32_t*>("PIDR");

enum class Message: uint8_t
{
	DEBUG_MESSAGE   = 0,
	EXECUTE_PROGRAM = 1,
	ERASE_PROGRAM   = 2,
	REBOOT          = 3
};

//========================================

class Main
{
public:
	friend class Client;
	
	Main() = default;

	void run();

private:
	static int ApiSetPixelRGB(lua_State* lua);
	static int ApiSetPixelHSV(lua_State* lua);
	static int ApiClear      (lua_State* lua);
	
	bool executeProgram(std::string_view source, char* error_buffer = nullptr, size_t error_buffer_length = 0);
	void saveProgram(std::optional<std::string_view> source = std::nullopt);
	bool loadProgram(std::string& program);
	
	void startLedstripHandler();
	void ledstripHandler();
	
	void serialHandler();
	void serialProcessData();
	void serialProcessPacket();
	
	void serialOnDebugMessage();
	void serialOnExecuteProgram();
	void serialOnEraseProgram();
	void serialOnReboot();
	
	void initNVS();
	void initLua();
	void initLedstrip();
	void initSerial();
	
	lua_State* m_lua = nullptr;
	bool m_script_failure = false;
	int64_t m_script_start_time = 0;

	led_strip_handle_t m_ledstrip = nullptr;
	
	Buffer m_packet;
	size_t m_pending_packet_length = 0;
	bool m_last_signature_failure = false;
	
};

//========================================