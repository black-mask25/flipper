#include "../nfc_app_i.h"

void nfc_scene_mf_ultralight_emulate_on_enter(void* context) {
    NfcApp* nfc = context;

    // Setup view
    const MfUltralightData* data = nfc_device_get_data(nfc->nfc_device, NfcProtocolMfUltralight);
    const MfUltralightType type = data->type;

    bool is_ultralight = (type == MfUltralightTypeUL11) || (type == MfUltralightTypeUL21) ||
                         (type == MfUltralightTypeUnknown);
    Popup* popup = nfc->popup;
    popup_set_header(popup, "Emulating", 67, 13, AlignLeft, AlignTop);
    if(!furi_string_empty(nfc->file_name)) {
        nfc_text_store_set(nfc, "%s", furi_string_get_cstr(nfc->file_name));
    } else if(is_ultralight) {
        nfc_text_store_set(nfc, "MIFARE\nUltralight");
    } else {
        nfc_text_store_set(nfc, "MIFARE\nNTAG");
    }
    popup_set_icon(popup, 0, 3, &I_NFC_dolphin_emulation_47x61);
    popup_set_text(popup, nfc->text_store, 90, 28, AlignCenter, AlignTop);

    // Setup and start worker
    view_dispatcher_switch_to_view(nfc->view_dispatcher, NfcViewPopup);
    nfc->listener = nfc_listener_alloc(nfc->nfc, NfcProtocolMfUltralight, data);
    nfc_listener_start(nfc->listener, NULL, NULL);

    nfc_blink_emulate_start(nfc);
}

bool nfc_scene_mf_ultralight_emulate_on_event(void* context, SceneManagerEvent event) {
    NfcApp* nfc = context;
    UNUSED(nfc);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        // MfUltralightData* mfu_data_after_emulation = nfc_listener_get_data(nfc->listener, NfcProtocolMfUltralight);
        // Check if data changed and save in shadow file
        // FIXME: A comparison method?
        // if(memcmp(
        //        mfu_data_after_emulation,
        //        &nfc->nfc_dev_data.mf_ul_data,
        //        sizeof(MfUltralightData)) != 0) {
        //     // Save shadow file
        //     if(!furi_string_empty(nfc->file_name)) {
        //         nfc_save_shadow_file(nfc);
        //     }
        // }
        // mf_ultralight_free(mfu_data_after_emulation);
        consumed = false;
    }
    return consumed;
}

void nfc_scene_mf_ultralight_emulate_on_exit(void* context) {
    NfcApp* nfc = context;

    nfc_listener_stop(nfc->listener);
    nfc_listener_free(nfc->listener);

    // Clear view
    popup_reset(nfc->popup);

    nfc_blink_stop(nfc);
}
