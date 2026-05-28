#pragma once

#include "ducky/core.hpp"

namespace dky {

void writeTGA(const char* path, int w, int h, unsigned char* data);
void saveState(const char* path, const TransformND& t);
bool loadState(const char* path, TransformND& t);

} // namespace dky
