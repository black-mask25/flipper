#include "bt_hid_tiktok.h"
#include "../hid.h"
#include <gui/elements.h>

#include "hid_icons.h"

#define TAG "BtHidApp"

struct BtHidTikTok {
    View* view;
    Hid* hid;
};

typedef struct {
    bool left_pressed;
    bool up_pressed;
    bool right_pressed;
    bool down_pressed;
    bool ok_pressed;
    bool connected;
    bool is_cursor_set;
} BtHidTikTokModel;

static void bt_hid_tiktok_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);
    BtHidTikTokModel* model = context;

    // Header
    if(model->connected) {
        canvas_draw_icon(canvas, 0, 0, &I_Ble_connected_15x15);
    } else {
        canvas_draw_icon(canvas, 0, 0, &I_Ble_disconnected_15x15);
    }
    canvas_set_font(canvas, FontPrimary);
    elements_multiline_text_aligned(canvas, 17, 3, AlignLeft, AlignTop, "TikTok");
    canvas_set_font(canvas, FontSecondary);

    // Keypad circles
    canvas_draw_icon(canvas, 76, 8, &I_Circles_47x47);

    // Up
    if(model->up_pressed) {
        canvas_set_bitmap_mode(canvas, 1);
        canvas_draw_icon(canvas, 93, 9, &I_Pressed_Button_13x13);
        canvas_set_bitmap_mode(canvas, 0);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_icon(canvas, 96, 11, &I_Arr_up_7x9);
    canvas_set_color(canvas, ColorBlack);

    // Down
    if(model->down_pressed) {
        canvas_set_bitmap_mode(canvas, 1);
        canvas_draw_icon(canvas, 93, 41, &I_Pressed_Button_13x13);
        canvas_set_bitmap_mode(canvas, 0);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_icon(canvas, 96, 44, &I_Arr_dwn_7x9);
    canvas_set_color(canvas, ColorBlack);

    // Left
    if(model->left_pressed) {
        canvas_set_bitmap_mode(canvas, 1);
        canvas_draw_icon(canvas, 77, 25, &I_Pressed_Button_13x13);
        canvas_set_bitmap_mode(canvas, 0);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_icon(canvas, 81, 29, &I_Voldwn_6x6);
    canvas_set_color(canvas, ColorBlack);

    // Right
    if(model->right_pressed) {
        canvas_set_bitmap_mode(canvas, 1);
        canvas_draw_icon(canvas, 109, 25, &I_Pressed_Button_13x13);
        canvas_set_bitmap_mode(canvas, 0);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_icon(canvas, 111, 29, &I_Volup_8x6);
    canvas_set_color(canvas, ColorBlack);

    // Ok
    if(model->ok_pressed) {
        canvas_draw_icon(canvas, 91, 23, &I_Like_pressed_17x17);
    } else {
        canvas_draw_icon(canvas, 94, 27, &I_Like_def_11x9);
    }
    // Exit
    canvas_draw_icon(canvas, 0, 54, &I_Pin_back_arrow_10x8);
    canvas_set_font(canvas, FontSecondary);
    elements_multiline_text_aligned(canvas, 13, 62, AlignLeft, AlignBottom, "Hold to exit");
}

static void bt_hid_tiktok_reset_cursor(BtHidTikTok* bt_hid_tiktok) {
    // Set cursor to the phone's left up corner
    // Delays to guarantee one packet per connection interval
    for(size_t i = 0; i < 8; i++) {
        hid_hal_mouse_move(bt_hid_tiktok->hid, -127, -127);
        furi_delay_ms(50);
    }
    // Move cursor from the corner
    hid_hal_mouse_move(bt_hid_tiktok->hid, 20, 120);
    furi_delay_ms(50);
}

static void bt_hid_tiktok_process_press(
    BtHidTikTok* bt_hid_tiktok,
    BtHidTikTokModel* model,
    InputEvent* event) {
    if(event->key == InputKeyUp) {
        model->up_pressed = true;
    } else if(event->key == InputKeyDown) {
        model->down_pressed = true;
    } else if(event->key == InputKeyLeft) {
        model->left_pressed = true;
        hid_hal_consumer_key_press(bt_hid_tiktok->hid, HID_CONSUMER_VOLUME_DECREMENT);
    } else if(event->key == InputKeyRight) {
        model->right_pressed = true;
        hid_hal_consumer_key_press(bt_hid_tiktok->hid, HID_CONSUMER_VOLUME_INCREMENT);
    } else if(event->key == InputKeyOk) {
        model->ok_pressed = true;
    }
}

static void bt_hid_tiktok_process_release(
    BtHidTikTok* bt_hid_tiktok,
    BtHidTikTokModel* model,
    InputEvent* event) {
    if(event->key == InputKeyUp) {
        model->up_pressed = false;
    } else if(event->key == InputKeyDown) {
        model->down_pressed = false;
    } else if(event->key == InputKeyLeft) {
        model->left_pressed = false;
        hid_hal_consumer_key_release(bt_hid_tiktok->hid, HID_CONSUMER_VOLUME_DECREMENT);
    } else if(event->key == InputKeyRight) {
        model->right_pressed = false;
        hid_hal_consumer_key_release(bt_hid_tiktok->hid, HID_CONSUMER_VOLUME_INCREMENT);
    } else if(event->key == InputKeyOk) {
        model->ok_pressed = false;
    }
}

static bool bt_hid_tiktok_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    BtHidTikTok* bt_hid_tiktok = context;
    bool consumed = false;

    with_view_model(
        bt_hid_tiktok->view,
        BtHidTikTokModel * model,
        {
            if(event->type == InputTypePress) {
                bt_hid_tiktok_process_press(bt_hid_tiktok, model, event);
                if(model->connected && !model->is_cursor_set) {
                    bt_hid_tiktok_reset_cursor(bt_hid_tiktok);
                    model->is_cursor_set = true;
                }
                consumed = true;
            } else if(event->type == InputTypeRelease) {
                bt_hid_tiktok_process_release(bt_hid_tiktok, model, event);
                consumed = true;
            } else if(event->type == InputTypeShort) {
                if(event->key == InputKeyOk) {
                    hid_hal_mouse_press(bt_hid_tiktok->hid, HID_MOUSE_BTN_LEFT);
                    furi_delay_ms(50);
                    hid_hal_mouse_release(bt_hid_tiktok->hid, HID_MOUSE_BTN_LEFT);
                    furi_delay_ms(50);
                    hid_hal_mouse_press(bt_hid_tiktok->hid, HID_MOUSE_BTN_LEFT);
                    furi_delay_ms(50);
                    hid_hal_mouse_release(bt_hid_tiktok->hid, HID_MOUSE_BTN_LEFT);
                    consumed = true;
                } else if(event->key == InputKeyUp) {
                    // Emulate up swipe
                    hid_hal_mouse_scroll(bt_hid_tiktok->hid, -6);
                    hid_hal_mouse_scroll(bt_hid_tiktok->hid, -12);
                    hid_hal_mouse_scroll(bt_hid_tiktok->hid, -19);
                    hid_hal_mouse_scroll(bt_hid_tiktok->hid, -12);
                    hid_hal_mouse_scroll(bt_hid_tiktok->hid, -6);
                    consumed = true;
                } else if(event->key == InputKeyDown) {
                    // Emulate down swipe
                    hid_hal_mouse_scroll(bt_hid_tiktok->hid, 6);
                    hid_hal_mouse_scroll(bt_hid_tiktok->hid, 12);
                    hid_hal_mouse_scroll(bt_hid_tiktok->hid, 19);
                    hid_hal_mouse_scroll(bt_hid_tiktok->hid, 12);
                    hid_hal_mouse_scroll(bt_hid_tiktok->hid, 6);
                    consumed = true;
                } else if(event->key == InputKeyBack) {
                    hid_hal_consumer_key_release_all(bt_hid_tiktok->hid);
                    consumed = true;
                }
            } else if(event->type == InputTypeLong) {
                if(event->key == InputKeyBack) {
                    hid_hal_consumer_key_release_all(bt_hid_tiktok->hid);
                    model->is_cursor_set = false;
                    consumed = false;
                }
            }
        },
        true);

    return consumed;
}

BtHidTikTok* bt_hid_tiktok_alloc(Hid* bt_hid) {
    BtHidTikTok* bt_hid_tiktok = malloc(sizeof(BtHidTikTok));
    bt_hid_tiktok->hid = bt_hid;
    bt_hid_tiktok->view = view_alloc();
    view_set_context(bt_hid_tiktok->view, bt_hid_tiktok);
    view_allocate_model(bt_hid_tiktok->view, ViewModelTypeLocking, sizeof(BtHidTikTokModel));
    view_set_draw_callback(bt_hid_tiktok->view, bt_hid_tiktok_draw_callback);
    view_set_input_callback(bt_hid_tiktok->view, bt_hid_tiktok_input_callback);

    return bt_hid_tiktok;
}

void bt_hid_tiktok_free(BtHidTikTok* bt_hid_tiktok) {
    furi_assert(bt_hid_tiktok);
    view_free(bt_hid_tiktok->view);
    free(bt_hid_tiktok);
}

View* bt_hid_tiktok_get_view(BtHidTikTok* bt_hid_tiktok) {
    furi_assert(bt_hid_tiktok);
    return bt_hid_tiktok->view;
}

void bt_hid_tiktok_set_connected_status(BtHidTikTok* bt_hid_tiktok, bool connected) {
    furi_assert(bt_hid_tiktok);
    with_view_model(
        bt_hid_tiktok->view,
        BtHidTikTokModel * model,
        {
            model->connected = connected;
            model->is_cursor_set = false;
        },
        true);
}
