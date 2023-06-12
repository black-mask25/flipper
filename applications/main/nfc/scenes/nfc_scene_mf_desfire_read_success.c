#include "../nfc_app_i.h"

void nfc_scene_mf_desfire_read_success_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    furi_assert(context);
    NfcApp* nfc = context;

    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(nfc->view_dispatcher, result);
    }
}

void nfc_scene_mf_desfire_read_success_on_enter(void* context) {
    NfcApp* nfc = context;

    // Setup view
    const MfDesfireData* data = nfc_dev_get_protocol_data(nfc->nfc_dev, NfcProtocolTypeMfDesfire);

    Widget* widget = nfc->widget;
    FuriString* temp_str;

    temp_str = furi_string_alloc_printf(
        "\e#%s\n", nfc_dev_get_protocol_name(nfc->nfc_dev, NfcProtocolNameTypeFull));
    furi_string_cat_printf(temp_str, "UID:");

    size_t uid_len;
    const uint8_t* uid = nfc_dev_get_uid(nfc->nfc_dev, &uid_len);
    for(size_t i = 0; i < uid_len; i++) {
        furi_string_cat_printf(temp_str, " %02X", uid[i]);
    }

    uint32_t bytes_total = 1UL << (data->version.sw_storage >> 1);
    uint32_t bytes_free = data->free_memory.bytes_free;
    furi_string_cat_printf(temp_str, "\n%lu", bytes_total);
    if(data->version.sw_storage & 1) {
        furi_string_push_back(temp_str, '+');
    }
    furi_string_cat_printf(temp_str, " bytes, %lu bytes free\n", bytes_free);

    uint16_t n_files = 0;
    uint16_t n_apps = data->applications.count;

    for(size_t i = 0; i < n_apps; ++i) {
        n_files += data->applications.data[i].files.count;
    }
    furi_string_cat_printf(temp_str, "%d Application", n_apps);
    if(n_apps != 1) {
        furi_string_push_back(temp_str, 's');
    }
    furi_string_cat_printf(temp_str, ", %d file", n_files);
    if(n_files != 1) {
        furi_string_push_back(temp_str, 's');
    }

    widget_add_text_scroll_element(widget, 0, 0, 128, 52, furi_string_get_cstr(temp_str));
    furi_string_free(temp_str);

    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Retry", nfc_scene_mf_desfire_read_success_widget_callback, nfc);
    widget_add_button_element(
        widget, GuiButtonTypeRight, "More", nfc_scene_mf_desfire_read_success_widget_callback, nfc);

    notification_message_block(nfc->notifications, &sequence_set_green_255);
    view_dispatcher_switch_to_view(nfc->view_dispatcher, NfcViewWidget);
}

bool nfc_scene_mf_desfire_read_success_on_event(void* context, SceneManagerEvent event) {
    NfcApp* nfc = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            scene_manager_next_scene(nfc->scene_manager, NfcSceneRetryConfirm);
            consumed = true;
        } else if(event.event == GuiButtonTypeRight) {
            scene_manager_next_scene(nfc->scene_manager, NfcSceneMfDesfireMenu);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_next_scene(nfc->scene_manager, NfcSceneExitConfirm);
        consumed = true;
    }

    return consumed;
}

void nfc_scene_mf_desfire_read_success_on_exit(void* context) {
    NfcApp* nfc = context;

    notification_message_block(nfc->notifications, &sequence_reset_green);

    // Clear view
    widget_reset(nfc->widget);
}
