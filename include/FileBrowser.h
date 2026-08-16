#pragma once

#include "lvgl.h"
#include <string>
#include <vector>

class App;   /* forward declaration; App::playFile() is called on selection */

/* One entry shown in the file-browser list. */
struct BrowserEntry {
    std::string name;
    std::string full;
    bool        isdir = false;
};

/*
 * A touch-friendly file browser / playlist.
 *
 * Starts at ROOT_DIR and only ever descends (never above the root). Each
 * directory is scanned lazily when opened (no slow recursive crawl at startup).
 * Folders are navigable; video files are selectable and trigger App::playFile().
 */
class FileBrowser {
public:
    FileBrowser(App & app, const char * root);

    void toggle();                 /* open if closed, close if open */
    void open();
    void close();
    bool isOpen() const { return overlay_ != nullptr; }

private:
    void render();                 /* (re)build the list for curDir_ */
    void onItem(int index);        /* index < 0 means ".. up" */

    bool isVideoExt(const char * name) const;

    static void itemCb(lv_event_t * e);
    static void closeCb(lv_event_t * e);

    App &              app_;
    std::string        curDir_;
    std::string        root_;
    lv_obj_t *         overlay_    = nullptr;
    lv_obj_t *         list_       = nullptr;
    lv_obj_t *         pathLabel_  = nullptr;
    std::vector<BrowserEntry> entries_;
};
