#pragma once

#include "Defines.h"

// ASSETS_ROOT is injected by CMake as an absolute path to the assets/
// folder (see target_compile_definitions in CmakeLists.txt), so asset
// paths resolve correctly no matter what the process's working directory
// is (IDE "run" buttons, ctest, double-clicking the binary, etc).
//
// Falls back to a relative path if built outside that CMake target
// (e.g. a stray translation unit compiled by hand) so this header never
// hard-fails to compile.
#ifndef ASSETS_ROOT
#define ASSETS_ROOT "assets"
#endif

// Joins ASSETS_ROOT with `relative_path` (e.g. "shaders/basic.vert") into
// a heap-allocated, NUL-terminated string. Caller owns the result and
// must free() it.
char* asset_path(const char* relative_path);
