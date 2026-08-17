#include "App.h"
#include "screen.h"
#include "config.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>

int main(int argc, char ** argv)
{
    /* Optional rotation: -r 90 / --rotate 270, or LVGL_ROTATE env var.
     * Defaults to DEFAULT_ROTATION from config.h (0 = panel native). */
    int rotation = DEFAULT_ROTATION;
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--rotate") == 0)
            && i + 1 < argc) {
            rotation = atoi(argv[++i]);
        }
    }
    const char * env = getenv("LVGL_ROTATE");
    if (env) rotation = atoi(env);
    rotation %= 360;
    if (rotation < 0) rotation += 360;

    g_screen.rotation = rotation;

    fprintf(stderr, "[main] rotation=%d\n", rotation);

    App app;
    if (!app.init())
        return 1;
    app.run();
    return 0;
}
