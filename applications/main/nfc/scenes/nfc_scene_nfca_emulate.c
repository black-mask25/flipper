#include "../nfc_app_i.h"

#define NFC_SCENE_NFCA_EMULATE_LOG_SIZE_MAX (200)

enum {
    NfcSceneNfcaEmulateStateWidget,
    NfcSceneNfcaEmulateStateTextBox,
};

NfcCommand nfc_scene_nfca_emulate_worker_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.protocol == NfcProtocolIso14443_3a);
    furi_assert(event.data);

    NfcApp* nfc = context;
    Iso14443_3aListenerEvent* iso14443_3a_event = event.data;

    if(iso14443_3a_event->type == Iso14443_3aListenerEventTypeReceivedStandardFrame) {
        furi_string_cat_printf(nfc->text_box_store, "R:");
        for(size_t i = 0; i < bit_buffer_get_size_bytes(iso14443_3a_event->data->buffer); i++) {
            furi_string_cat_printf(
                nfc->text_box_store,
                " %02X",
                bit_buffer_get_byte(iso14443_3a_event->data->buffer, i));
        }
        furi_string_cat_printf(nfc->text_box_store, "\n");
        view_dispatcher_send_custom_event(nfc->view_dispatcher, NfcCustomEventWorkerUpdate);
    }

    return NfcCommandContinue;
}

void nfc_scene_nfca_emulate_widget_callback(GuiButtonType result, InputType type, void* context) {
    furi_assert(context);
    NfcApp* nfc = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(nfc->view_dispatcher, result);
    }
}

void nfc_nfca_emulate_textbox_callback(void* context) {
    furi_assert(context);
    NfcApp* nfc = context;
    view_dispatcher_send_custom_event(nfc->view_dispatcher, NfcCustomEventViewExit);
}

// Add widget with device name or inform that data received
static void nfc_scene_nfca_emulate_widget_config(NfcApp* nfc, bool data_received) {
    const Iso14443_3aData* data = nfc_device_get_data(nfc->nfc_device, NfcProtocolIso14443_3a);
    Widget* widget = nfc->widget;
    widget_reset(widget);
    FuriString* info_str;
    info_str = furi_string_alloc();

    widget_add_icon_element(widget, 0, 3, &I_NFC_dolphin_emulation_47x61);
    widget_add_string_element(widget, 57, 13, AlignLeft, AlignTop, FontPrimary, "Emulating UID");
    for(uint8_t i = 0; i < data->uid_len; i++) {
        furi_string_cat_printf(info_str, "%02X ", data->uid[i]);
    }
    furi_string_trim(info_str);
    widget_add_text_box_element(
        widget, 57, 28, 67, 25, AlignCenter, AlignTop, furi_string_get_cstr(info_str), true);
    furi_string_free(info_str);
    if(data_received) {
        widget_add_button_element(
            widget, GuiButtonTypeCenter, "Log", nfc_scene_nfca_emulate_widget_callback, nfc);
    }
}

void nfc_scene_nfca_emulate_on_enter(void* context) {
    NfcApp* nfc = context;

    // Setup Widget
    nfc_scene_nfca_emulate_widget_config(nfc, false);
    // Setup TextBox
    TextBox* text_box = nfc->text_box;
    text_box_set_font(text_box, TextBoxFontHex);
    text_box_set_focus(text_box, TextBoxFocusEnd);
    furi_string_reset(nfc->text_box_store);

    const NfcDeviceData* data = nfc_device_get_data(nfc->nfc_device, NfcProtocolIso14443_3a);
    nfc->listener = nfc_listener_alloc(nfc->nfc, NfcProtocolIso14443_3a, data);
    nfc_listener_start(nfc->listener, nfc_scene_nfca_emulate_worker_callback, nfc);

    // Set Widget state and view
    scene_manager_set_scene_state(
        nfc->scene_manager, NfcSceneNfcaEmulate, NfcSceneNfcaEmulateStateWidget);
    view_dispatcher_switch_to_view(nfc->view_dispatcher, NfcViewWidget);

    nfc_blink_emulate_start(nfc);
}

bool nfc_scene_nfca_emulate_on_event(void* context, SceneManagerEvent event) {
    NfcApp* nfc = context;
    uint32_t state = scene_manager_get_scene_state(nfc->scene_manager, NfcSceneNfcaEmulate);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcCustomEventWorkerUpdate) {
            // Add data button to widget if data is received for the first time
            if(furi_string_size(nfc->text_box_store)) {
                nfc_scene_nfca_emulate_widget_config(nfc, true);
            }
            // Update TextBox data
            text_box_set_text(nfc->text_box, furi_string_get_cstr(nfc->text_box_store));
            consumed = true;
        } else if(event.event == GuiButtonTypeCenter && state == NfcSceneNfcaEmulateStateWidget) {
            view_dispatcher_switch_to_view(nfc->view_dispatcher, NfcViewTextBox);
            scene_manager_set_scene_state(
                nfc->scene_manager, NfcSceneNfcaEmulate, NfcSceneNfcaEmulateStateTextBox);
            consumed = true;
        } else if(event.event == NfcCustomEventViewExit && state == NfcSceneNfcaEmulateStateTextBox) {
            view_dispatcher_switch_to_view(nfc->view_dispatcher, NfcViewWidget);
            scene_manager_set_scene_state(
                nfc->scene_manager, NfcSceneNfcaEmulate, NfcSceneNfcaEmulateStateWidget);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        if(state == NfcSceneNfcaEmulateStateTextBox) {
            view_dispatcher_switch_to_view(nfc->view_dispatcher, NfcViewWidget);
            scene_manager_set_scene_state(
                nfc->scene_manager, NfcSceneNfcaEmulate, NfcSceneNfcaEmulateStateWidget);
            consumed = true;
        }
    }

    return consumed;
}

void nfc_scene_nfca_emulate_on_exit(void* context) {
    NfcApp* nfc = context;

    nfc_listener_stop(nfc->listener);
    nfc_listener_free(nfc->listener);

    // Clear view
    widget_reset(nfc->widget);
    text_box_reset(nfc->text_box);
    furi_string_reset(nfc->text_box_store);

    nfc_blink_stop(nfc);
}
