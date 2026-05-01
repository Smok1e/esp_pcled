#include <buffer.hpp>

#include "esp_log.h"

//========================================

static const char* TAG = "buffer";

//========================================

void Buffer::write(const uint8_t* data, size_t size)
{
	m_data.insert(m_data.end(), data, data + size);
}

void Buffer::writeUint8(uint8_t value)
{
	write(&value, sizeof(uint8_t));
}

void Buffer::writeUint16(uint16_t value)
{
	write(reinterpret_cast<const uint8_t*>(&value), sizeof(uint16_t));
}

void Buffer::writeUint32(uint32_t value)
{
	write(reinterpret_cast<const uint8_t*>(&value), sizeof(uint32_t));
}

void Buffer::writeString(std::string_view string)
{
	writeUint32(string.length());
	write(reinterpret_cast<const uint8_t*>(string.data()), string.length());
}

void Buffer::writeBoolean(bool value)
{
	writeUint8(value);
}

//========================================

void Buffer::read(uint8_t* buffer, size_t size)
{
	if (remain() < size)
	{
		ESP_LOGE(TAG, "attempt to read %zu bytes, while only %zu bytes remain in the buffer; aborting", size, remain());
		abort();
	}
	
	std::copy(m_data.begin() + m_offset, m_data.begin() + m_offset + size, buffer);
	seek(size);
}

uint8_t Buffer::readUint8()
{
	uint8_t value;
	read(&value, sizeof(uint8_t));
	
	return value;
}

uint16_t Buffer::readUint16()
{
	uint16_t value;
	read(reinterpret_cast<uint8_t*>(&value), sizeof(uint16_t));
	
	return value;
}

uint32_t Buffer::readUint32()
{
	uint32_t value;
	read(reinterpret_cast<uint8_t*>(&value), sizeof(uint32_t));
	
	return value;
}

std::string_view Buffer::readString()
{
	auto length = readUint32();
	
	return std::string_view(
		reinterpret_cast<const char*>(m_data.data()) + m_offset,
		length
	);
}

bool Buffer::readBoolean()
{
	return readUint8();
}

//========================================

size_t Buffer::offset()
{
	return m_offset;
}

size_t Buffer::remain()
{
	return m_data.size() - m_offset;
}

void Buffer::seek(size_t offset, bool absolute /*= false*/)
{
	if (absolute)
		m_offset = offset;
	
	else
		m_offset += offset;
}

void Buffer::discard()
{
	m_data.erase(m_data.begin(), m_data.begin() + m_offset);
	m_offset = 0;
}

void Buffer::clear()
{
	m_data.clear();
	m_offset = 0;
}

const uint8_t* Buffer::data()
{
	return m_data.data();
}

size_t Buffer::size()
{
	return m_data.size();
}

//========================================