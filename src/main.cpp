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

    /* Screen capture (utils/lv_snapshot):
     *   --shot-dir <dir>       save <dir>/shot_000.png periodically
     *   --shot-period <sec>    capture period (default 5 s, with --shot-dir)
     *   --shot <file>          one capture after the UI settles (~2 s) */
    const char * shotDir  = nullptr;
    const char * shotPath = nullptr;
    int          shotPeriodSec = 5;

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--rotate") == 0)
            && i + 1 < argc) {
            rotation = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--shot-dir") == 0 && i + 1 < argc) {
            shotDir = argv[++i];
        }
        else if (strcmp(argv[i], "--shot-period") == 0 && i + 1 < argc) {
            shotPeriodSec = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--shot") == 0 && i + 1 < argc) {
            shotPath = argv[++i];
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

    if (shotDir)   app.startSnapshot(shotDir, shotPeriodSec);
    if (shotPath)  app.takeSnapshotOnce(shotPath);

    app.run();
    return 0;
}
