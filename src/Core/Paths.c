#include "Paths.h"

#include <string.h>

char* asset_path(const char* relative_path)
{
    size_t root_len = strlen(ASSETS_ROOT);
    size_t rel_len = strlen(relative_path);
    b8 needs_sep = root_len > 0 && ASSETS_ROOT[root_len - 1] != '/';

    size_t total = root_len + (needs_sep ? 1u : 0u) + rel_len + 1;

    char* path = malloc(total);
    if (!path)
        FATAL("Out of memory building asset path for: %s", relative_path);

    snprintf(path, total, "%s%s%s", ASSETS_ROOT, needs_sep ? "/" : "", relative_path);
    return path;
}
