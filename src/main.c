#include <pebble.h>

#include "modules/global.h"
#include "windows/list_window.h"

static void init(void) {
    list_window_push();
}

static void deinit(void) {

}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
