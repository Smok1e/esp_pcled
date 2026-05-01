#include <cstdlib>
#include <cmath>
#include <cstdio>

#include <utils.hpp>

//========================================

void DumpHex(const void* data, size_t length)
{
	constexpr size_t line_size = 16;
	const auto* bytes = reinterpret_cast<const char*>(data);
	
	for (size_t offset = 0; offset < length; offset += line_size)
	{
		printf("%04zu: ", offset);
		
		for (size_t i = 0; i < line_size; i++)
		{
			if (offset + i >= length)
			{
				printf("   ");
				continue;
			}
			
			printf("%02x ", bytes[offset + i]);
		}
		
		printf(" ");
		
		for (size_t i = 0; i < line_size && offset + i < length; i++)
		{
			auto byte = bytes[offset + i];
			printf(
				"%c",
				0x20 <= byte && byte <= 0x7E
					? byte
					: '.'
			);
		}
		
		printf("\n");
	}
}

std::pair<float, const char*> FormatBytes(size_t bytes)
{
	constexpr const char* units[] = {
		"bytes", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB"
	};
	
	unsigned power = floor(log(bytes) / log(1024));
	
	return {
		static_cast<float>(bytes) / pow(1024, power),
		units[power]
	};
}

//========================================