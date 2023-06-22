#include "../nfc_app_i.h"

void nfc_scene_mf_ultralight_read_success_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    furi_assert(context);
    NfcApp* nfc = context;

    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(nfc->view_dispatcher, result);
    }
}

void nfc_scene_mf_ultralight_read_success_on_enter(void* context) {
    NfcApp* nfc = context;

    // Setup view
    const MfUltralightData* data =
        nfc_dev_get_protocol_data(nfc->nfc_dev, NfcProtocolTypeMfUltralight);
    Widget* widget = nfc->widget;

    FuriString* temp_str;

    temp_str = furi_string_alloc_printf(
        "\e#%s\n", nfc_dev_get_device_name(nfc->nfc_dev, NfcProtocolNameTypeFull));
    furi_string_cat_printf(temp_str, "UID:");
    for(size_t i = 0; i < data->nfca_data->uid_len; i++) {
        furi_string_cat_printf(temp_str, " %02X", data->nfca_data->uid[i]);
    }
    furi_string_cat_printf(temp_str, "\nPages Read: %d/%d", data->pages_read, data->pages_total);
    if(data->pages_read != data->pages_total) {
        furi_string_cat_printf(temp_str, "\nPassword-protected pages!");
    }

    widget_add_text_scroll_element(widget, 0, 0, 128, 52, furi_string_get_cstr(temp_str));
    furi_string_free(temp_str);

    widget_add_button_element(
        widget,
        GuiButtonTypeLeft,
        "Retry",
        nfc_scene_mf_ultralight_read_success_widget_callback,
        nfc);
    widget_add_button_element(
        widget,
        GuiButtonTypeRight,
        "More",
        nfc_scene_mf_ultralight_read_success_widget_callback,
        nfc);

    notification_message_block(nfc->notifications, &sequence_set_green_255);
    view_dispatcher_switch_to_view(nfc->view_dispatcher, NfcViewWidget);
}

bool nfc_scene_mf_ultralight_read_success_on_event(void* context, SceneManagerEvent event) {
    NfcApp* nfc = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            scene_manager_next_scene(nfc->scene_manager, NfcSceneRetryConfirm);
            consumed = true;
        } else if(event.event == GuiButtonTypeRight) {
            scene_manager_next_scene(nfc->scene_manager, NfcSceneMfUltralightMenu);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_next_scene(nfc->scene_manager, NfcSceneExitConfirm);
        consumed = true;
    }

    return consumed;
}

void nfc_scene_mf_ultralight_read_success_on_exit(void* context) {
    NfcApp* nfc = context;

    notification_message_block(nfc->notifications, &sequence_reset_green);

    // Clear view
    widget_reset(nfc->widget);
}
