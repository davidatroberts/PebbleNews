#include <string.h>

#include "list_window.h"

#include "modules/global.h"

static Window *l_window;
static MenuLayer *menu_layer;

#define NUM_ITEMS 20
#define SCROLL_MENU_ITEM_WAIT_TIMER 100
#define MENU_CHARS_VISIBLE 10

typedef enum {
    AppKeyJSReady = 0,
    AppKeyNumItems,
    AppKeyItemID,
    AppKeyItemTitle
} AppKeys;

// news items
static int max_quantity;
static int id_index = 0;
static int title_index = 0;
static int id_data[NUM_ITEMS];
static char *titles[NUM_ITEMS];

// ready to send requests
static bool js_ready;

// data to be passed for scrolling
ScrollData scroll_data = {
    .menu_reloading_to_scroll = false,
    .scrolling_still_required = false,
    .menu_scroll_offset = 0
};

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
    // make sure it's ok to send/receive from phone
    if (!js_ready) {
        Tuple *ready_tuple = dict_find(iter, AppKeyJSReady);
        if (ready_tuple) {
            js_ready = true;
            APP_LOG(APP_LOG_LEVEL_INFO, "Received js_ready");
        }
        return;
    }

    // read the first pair in tuple
    Tuple *t = dict_read_first(iter);
    int key = t->key;

    while (t!=NULL) {
        if (key==AppKeyNumItems) {
            max_quantity = (int)t->value->int32;
        }

        if ((id_index < max_quantity) || (title_index < max_quantity)) {
            // set item info if limit not reached
            switch (key) {
                case AppKeyItemID:
                    // set the ID
                    id_data[id_index] = (int)t->value->int32;
                    id_index++;
                    break;
                case AppKeyItemTitle:
                    // set the title
                    titles[title_index] = malloc((strlen(
                        t->value->cstring)*sizeof(char))+1);
                    snprintf(titles[title_index],
                        (strlen(t->value->cstring)*sizeof(char))+1,
                        "%s", t->value->cstring);

                    APP_LOG(APP_LOG_LEVEL_INFO, titles[title_index]);
                    title_index++;
                    break;
            }

            if (id_index == title_index) {
                menu_layer_reload_data(menu_layer);
            }
        }

        // read next tuple
        t = dict_read_next(iter);
    }
}


static void inbox_dropped_handler(AppMessageResult reason,
    void *context) {

}

static uint16_t menu_get_num_sections_callback(MenuLayer *menu_layer,
    void *data) {

    return 1;
}

static uint16_t menu_get_num_rows_callback(MenuLayer *menu_layer,
    uint16_t selection_index, void *data) {
    return id_index;
}

static void get_item_text(ScrollData *scroll_data, int row, char **text) {
    MenuIndex menu_index = menu_layer_get_selected_index(menu_layer);

    // only scroll on currently selected row
    if (*text && (row == menu_index.row)) {
        // calculate scolling
        int len = strlen(*text);
        if(len - MENU_CHARS_VISIBLE - scroll_data->menu_scroll_offset > 0) {
            *text += scroll_data->menu_scroll_offset;
            scroll_data->scrolling_still_required = true;
        }
    }
}

static void menu_draw_row_callback(GContext *ctx, const Layer *cell_layer,
    MenuIndex *cell_index, void *data) {
    ScrollData *scroll_data = (ScrollData*)data;

    // allocate memory for headline text
    char *title_copy = malloc(64*sizeof(char));
    char *orig_address = title_copy;

    // copy text from titles and calculate scrolling if needed
    strcpy(title_copy, titles[cell_index->row]);
    get_item_text(scroll_data, cell_index->row, &title_copy);

    // draw the cell
    switch (cell_index->section) {
        case 0:
            menu_cell_basic_draw(ctx, cell_layer, title_copy, "", NULL);
            break;
    }

    free(orig_address);
}

static void menu_select_click_handler(MenuLayer *menu_layer,
    MenuIndex *cell_index, void *data) {

}

static void scroll_menu_callback(void* data) {
    ScrollData *scroll_data = (ScrollData*)data;

    scroll_data->scroll_timer = NULL;
    scroll_data->menu_scroll_offset++;
    if (!scroll_data->scrolling_still_required) {
        return;
    }

    MenuIndex menu_index = menu_layer_get_selected_index(menu_layer);
    if(menu_index.row != 0) {
        scroll_data->menu_reloading_to_scroll = true;
    }

    scroll_data->scrolling_still_required = false;
    menu_layer_reload_data(menu_layer);
    scroll_data->scroll_timer = app_timer_register(SCROLL_MENU_ITEM_WAIT_TIMER,
        scroll_menu_callback, scroll_data);
}

static void initiate_menu_scroll_timer(ScrollData *scroll_data) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Initiated menu scroll timer");

    bool create_timer = true;
    scroll_data->scrolling_still_required = true;
    scroll_data->menu_scroll_offset = 0;
    scroll_data->menu_reloading_to_scroll = false;

    if (scroll_data->scroll_timer) {
        APP_LOG(APP_LOG_LEVEL_INFO, "Rescheduling timer");
        create_timer = !app_timer_reschedule(scroll_data->scroll_timer,
            SCROLL_MENU_ITEM_WAIT_TIMER);
    }
    if (create_timer) {
        APP_LOG(APP_LOG_LEVEL_INFO, "Creating timer");
        scroll_data->scroll_timer = app_timer_register(
            SCROLL_MENU_ITEM_WAIT_TIMER, scroll_menu_callback, scroll_data);
    }
}

static void menu_selection_changed(MenuLayer *menu_layer, MenuIndex new_index,
    MenuIndex old_index, void *context) {
    ScrollData *scroll_data = (ScrollData*)context;

    if (!scroll_data->menu_reloading_to_scroll) {
        initiate_menu_scroll_timer(scroll_data);
    }
    else {
        scroll_data->menu_reloading_to_scroll = false;
    }
}



static void window_load(Window *window) {
    // network setup
    app_message_register_inbox_received(inbox_received_handler);
    app_message_register_inbox_dropped(inbox_dropped_handler);
    app_message_open(256, 256);

    // root window setup
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);

    // menulayer setup
    menu_layer = menu_layer_create(bounds);
    menu_layer_set_click_config_onto_window(menu_layer, window);

    menu_layer_set_callbacks(menu_layer, &scroll_data, (MenuLayerCallbacks) {
        .get_num_sections = menu_get_num_sections_callback,
        .get_num_rows = menu_get_num_rows_callback,
        .draw_row = menu_draw_row_callback,
        .select_click = menu_select_click_handler,
        .selection_changed = menu_selection_changed
    });

    // set menu colours
    menu_layer_set_normal_colors(menu_layer, GColorLightGray, GColorBlack);
    menu_layer_set_highlight_colors(menu_layer, GColorOrange, GColorBlack);

    // add to the window
    layer_add_child(window_layer, menu_layer_get_layer(menu_layer));
}

static void window_unload(Window *window) {
    // free the data here
    for (int i=0; i<max_quantity; i++) {
        if (titles[i]!=NULL) {
            free(titles[i]);
        }
    }

    menu_layer_destroy(menu_layer);
    window_destroy(l_window);
}

void list_window_push() {
    if (!l_window) {
        l_window = window_create();
        window_set_window_handlers(l_window, (WindowHandlers) {
            .load = window_load,
            .unload = window_unload,
        });
    }

    window_stack_push(l_window, true);
}
