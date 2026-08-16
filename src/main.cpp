#include "App.h"

int main(void)
{
    App app;
    if (!app.init())
        return 1;
    app.run();
    return 0;
}
