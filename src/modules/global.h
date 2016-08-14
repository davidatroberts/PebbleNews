#include <pebble.h>

#ifndef GLOBAL
#define GLOBAL

typedef struct {
    bool menu_reloading_to_scroll;
    bool scrolling_still_required;
    int menu_scroll_offset;
    AppTimer *scroll_timer;
} ScrollData;

#endif
