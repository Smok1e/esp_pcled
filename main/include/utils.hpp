#pragma once

#include <cerrno>
#include <cstring>
#include <utility>

#include "esp_log.h"

//========================================

void DumpHex(const void* data, size_t length);
std::pair<float, const char*> FormatBytes(size_t bytes);

//========================================