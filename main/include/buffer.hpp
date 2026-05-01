#pragma once

#include <vector>
#include <string_view>

//========================================

class Buffer
{
public:
	Buffer() = default;
	
	void write(const uint8_t* data, size_t size);
	void writeUint8(uint8_t value);
	void writeUint16(uint16_t value);
	void writeUint32(uint32_t value);
	void writeString(std::string_view string);
	void writeBoolean(bool value);
	
	void read(uint8_t* buffer, size_t size);
	uint8_t readUint8();
	uint16_t readUint16();
	uint32_t readUint32();
	std::string_view readString();
	bool readBoolean();
	
	size_t offset();
	size_t remain();
	void seek(size_t offset, bool absolute = false);
	void discard();
	void clear();
	
	const uint8_t* data();
	size_t size();
	
private:
	std::vector<uint8_t> m_data {};
	size_t m_offset = 0;
	
};

//========================================