#pragma once

#include "Core/Defines.h"

typedef struct App
{
    char* name;
} App;


void run(App* app);
void terminate(App* app);