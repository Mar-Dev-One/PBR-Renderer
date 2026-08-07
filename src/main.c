#include "Core/Defines.h"

#include "App.h"

int main(void)
{
    App app = {
        .name = "First app"
    };
    run(&app);
    terminate(&app);

    return 0;
}