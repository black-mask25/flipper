#include "../nfc_app_i.h"

// FIXME: Make it work
// void nfc_scene_set_atqa_byte_input_callback(void* context) {
//     NfcApp* nfc = context;
//
//     view_dispatcher_send_custom_event(nfc->view_dispatcher, NfcCustomEventByteInputDone);
// }

void nfc_scene_set_atqa_on_enter(void* context) {
    NfcApp* nfc = context;

    // Setup view
    ByteInput* byte_input = nfc->byte_input;
    byte_input_set_header_text(byte_input, "Enter ATQA in hex");
    // byte_input_set_result_callback(
    //     byte_input,
    //     nfc_scene_set_atqa_byte_input_callback,
    //     NULL,
    //     nfc,
    //     nfc->nfc_dev_data.nfca_data->atqa,
    //     2);
    // view_dispatcher_switch_to_view(nfc->view_dispatcher, NfcViewByteInput);
}

bool nfc_scene_set_atqa_on_event(void* context, SceneManagerEvent event) {
    NfcApp* nfc = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcCustomEventByteInputDone) {
            scene_manager_next_scene(nfc->scene_manager, NfcSceneSetUid);
            consumed = true;
        }
    }
    return consumed;
}

void nfc_scene_set_atqa_on_exit(void* context) {
    NfcApp* nfc = context;

    // Clear view
    byte_input_set_result_callback(nfc->byte_input, NULL, NULL, NULL, NULL, 0);
    byte_input_set_header_text(nfc->byte_input, "");
}
