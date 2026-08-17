#include "FileBrowser.h"
#include "App.h"
#include "config.h"
#include "screen.h"

#include <cctype>
#include <cstring>
#include <algorithm>
#include <utility>
#include <sys/stat.h>
#include <dirent.h>

static const char * kVideoExts[] = {
    "mp4","mkv","avi","mov","webm","ts","m4v","flv",
    "mpg","mpeg","3gp","wmv"
};

FileBrowser::FileBrowser(App & app, const char * root)
    : app_(app), curDir_(root), root_(root)
{
}

bool FileBrowser::isVideoExt(const char * name) const
{
    const char * dot = strrchr(name, '.');
    if (!dot) return false;
    const char * ext = dot + 1;
    char buf[16];
    for (int i = 0; ext[i] && i < (int)sizeof(buf) - 1; i++)
        buf[i] = (char)tolower((unsigned char)ext[i]);
    buf[strlen(ext)] = 0;
    for (size_t i = 0; i < sizeof(kVideoExts) / sizeof(kVideoExts[0]); i++)
        if (strcmp(buf, kVideoExts[i]) == 0) return true;
    return false;
}

void FileBrowser::toggle()
{
    if (overlay_) close();
    else          open();
}

void FileBrowser::open()
{
    if (overlay_) return;
    /* Overlay and widgets scale with the logical screen size. */
    const int W = g_screen.w;
    const int H = g_screen.h;

    overlay_ = lv_obj_create(lv_screen_active());
    lv_obj_set_size(overlay_, W, H);
    lv_obj_set_style_bg_color(overlay_, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_bg_opa(overlay_, LV_OPA_90, 0);
    lv_obj_clear_flag(overlay_, LV_OBJ_FLAG_SCROLLABLE);

    pathLabel_ = lv_label_create(overlay_);
    lv_obj_set_style_text_color(pathLabel_, lv_color_white(), 0);
    lv_obj_align(pathLabel_, LV_ALIGN_TOP_MID, 0, spct(H, 7));

    lv_obj_t * x = lv_button_create(overlay_);
    lv_obj_set_size(x, spct(W, 9), spct(H, 4));
    lv_obj_align(x, LV_ALIGN_TOP_RIGHT, -spct(W, 2), spct(H, 13) / 10);
    lv_obj_set_style_bg_color(x, lv_color_make(0x8E, 0x24, 0x24), 0);
    lv_obj_t * xl = lv_label_create(x);
    lv_label_set_text(xl, "X");
    lv_obj_center(xl);
    lv_obj_set_user_data(x, this);
    lv_obj_add_event_cb(x, closeCb, LV_EVENT_CLICKED, this);

    render();
}

void FileBrowser::close()
{
    if (overlay_) {
        lv_obj_delete(overlay_);
        overlay_ = nullptr;
        list_ = nullptr;
        pathLabel_ = nullptr;
    }
}

void FileBrowser::render()
{
    const int W = g_screen.w;
    const int H = g_screen.h;

    if (list_) { lv_obj_delete(list_); list_ = nullptr; }
    list_ = lv_list_create(overlay_);
    lv_obj_set_size(list_, spct(W, 93), spct(H, 79));
    lv_obj_align(list_, LV_ALIGN_BOTTOM_MID, 0, -spct(H, 16) / 10);
    lv_obj_set_user_data(list_, this);
    if (pathLabel_) lv_label_set_text(pathLabel_, curDir_.c_str());

    entries_.clear();

    /* ".. up" entry (hidden at the root). */
    if (curDir_ != root_) {
        lv_obj_t * b = lv_list_add_button(list_, (const void *)LV_SYMBOL_LEFT,
                                          ".. (上级目录)");
        lv_obj_set_user_data(b, (void *)(intptr_t)-1);
        lv_obj_add_event_cb(b, itemCb, LV_EVENT_CLICKED, this);
    }

    DIR * d = opendir(curDir_.c_str());
    if (d) {
        struct dirent * de;
        while ((de = readdir(d)) != nullptr) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
                continue;
            std::string full = curDir_ + "/" + de->d_name;
            struct stat st;
            if (stat(full.c_str(), &st) != 0) continue;
            bool isdir = S_ISDIR(st.st_mode);
            if (!isdir && !isVideoExt(de->d_name)) continue;  /* hide non-video */
            BrowserEntry e;
            e.name  = de->d_name;
            e.full  = full;
            e.isdir = isdir;
            entries_.push_back(std::move(e));
        }
        closedir(d);
    }

    /* folders first, then alphabetical */
    std::sort(entries_.begin(), entries_.end(),
              [](const BrowserEntry & a, const BrowserEntry & b) {
                  if (a.isdir != b.isdir) return a.isdir > b.isdir;
                  return a.name < b.name;
              });

    for (size_t i = 0; i < entries_.size(); i++) {
        const void * icon = entries_[i].isdir
            ? (const void *)LV_SYMBOL_DIRECTORY
            : (const void *)LV_SYMBOL_VIDEO;
        lv_obj_t * b = lv_list_add_button(list_, icon, entries_[i].name.c_str());
        lv_obj_set_user_data(b, (void *)(intptr_t)i);
        lv_obj_add_event_cb(b, itemCb, LV_EVENT_CLICKED, this);
    }
}

void FileBrowser::onItem(int index)
{
    if (index < 0) {            /* ".. up" */
        std::string tmp = curDir_;
        size_t sl = tmp.find_last_of('/');
        if (sl != std::string::npos) {
            tmp = (sl == 0) ? tmp.substr(0, 1) : tmp.substr(0, sl);
        }
        if (tmp.size() < root_.size() ||
            tmp.compare(0, root_.size(), root_) != 0)
            tmp = root_;
        curDir_ = tmp;
        render();
        return;
    }
    if (index >= (int)entries_.size()) return;
    const BrowserEntry & e = entries_[index];
    struct stat st;
    if (stat(e.full.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        curDir_ = e.full;
        render();
    } else {
        app_.playFile(e.full);
    }
}

void FileBrowser::itemCb(lv_event_t * e)
{
    lv_obj_t * btn = (lv_obj_t *)lv_event_get_target(e);
    FileBrowser * fb = (FileBrowser *)lv_obj_get_user_data(lv_obj_get_parent(btn));
    if (!fb) return;
    int idx = (int)(intptr_t)lv_obj_get_user_data(btn);
    fb->onItem(idx);
}

void FileBrowser::closeCb(lv_event_t * e)
{
    LV_UNUSED(e);
    FileBrowser * fb = (FileBrowser *)lv_obj_get_user_data(
        (lv_obj_t *)lv_event_get_target(e));
    if (fb) fb->close();
}
