#pragma once

#include "Defines.h"

// Reads the entire file at `path` into a single heap-allocated,
// NUL-terminated buffer (text mode — fine for shader sources, configs,
// anything that isn't binary). Returns NULL on failure (missing file,
// read error) and logs why.
//
// Caller owns the returned buffer and must free() it.
char* file_read_to_string(const char* path);
