#include "FileIO.h"

char* file_read_to_string(const char* path)
{
    FILE* file = fopen(path, "rb");
    if (!file)
    {
        LOG_ERROR("Could not open file: %s\n", path);
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0)
    {
        LOG_ERROR("Could not seek in file: %s\n", path);
        fclose(file);
        return NULL;
    }

    long size = ftell(file);
    if (size < 0)
    {
        LOG_ERROR("Could not determine size of file: %s\n", path);
        fclose(file);
        return NULL;
    }

    rewind(file);

    char* buffer = malloc((size_t)size + 1);
    if (!buffer)
        FATAL("Out of memory reading file: %s", path);

    size_t read_count = fread(buffer, 1, (size_t)size, file);
    fclose(file);

    if (read_count != (size_t)size)
    {
        LOG_ERROR("Short read on file: %s\n", path);
        free(buffer);
        return NULL;
    }

    buffer[size] = '\0';
    return buffer;
}
