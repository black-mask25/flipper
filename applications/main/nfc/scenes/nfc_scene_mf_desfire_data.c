#include "../nfc_app_i.h"

#include "../helpers/format/nfc_mf_desfire_format.h"

enum {
    MifareDesfireDataStateMenu,
    MifareDesfireDataStateItem, // MUST be last, states >= this correspond with submenu index
};

enum SubmenuIndex {
    SubmenuIndexCardInfo,
    SubmenuIndexDynamic, // dynamic indexes start here
};

static void nfc_scene_mf_desfire_data_submenu_callback(void* context, uint32_t index) {
    NfcApp* nfc = (NfcApp*)context;

    view_dispatcher_send_custom_event(nfc->view_dispatcher, index);
}

void nfc_scene_mf_desfire_data_on_enter(void* context) {
    NfcApp* nfc = context;
    Submenu* submenu = nfc->submenu;

    const uint32_t state =
        scene_manager_get_scene_state(nfc->scene_manager, NfcSceneMfDesfireData);
    const MfDesfireData* data = nfc_dev_get_protocol_data(nfc->nfc_dev, NfcProtocolTypeMfDesfire);

    text_box_set_font(nfc->text_box, TextBoxFontHex);

    submenu_add_item(
        submenu,
        "Card info",
        SubmenuIndexCardInfo,
        nfc_scene_mf_desfire_data_submenu_callback,
        nfc);

    FuriString* label = furi_string_alloc();

    for(uint32_t i = 0; i < simple_array_get_count(data->application_ids); ++i) {
        // TODO: Make it more type safe
        const uint8_t* app_id = simple_array_cget(data->application_ids, i);
        furi_string_printf(label, "App %02x%02x%02x", app_id[0], app_id[1], app_id[2]);
        submenu_add_item(
            submenu,
            furi_string_get_cstr(label),
            i + SubmenuIndexDynamic,
            nfc_scene_mf_desfire_data_submenu_callback,
            nfc);
    }

    furi_string_free(label);

    if(state >= MifareDesfireDataStateItem) {
        submenu_set_selected_item(
            nfc->submenu, state - MifareDesfireDataStateItem + SubmenuIndexDynamic);
        scene_manager_set_scene_state(
            nfc->scene_manager, NfcSceneMfDesfireData, MifareDesfireDataStateMenu);
    }

    view_dispatcher_switch_to_view(nfc->view_dispatcher, NfcViewMenu);
}

bool nfc_scene_mf_desfire_data_on_event(void* context, SceneManagerEvent event) {
    NfcApp* nfc = context;
    bool consumed = false;

    const uint32_t state =
        scene_manager_get_scene_state(nfc->scene_manager, NfcSceneMfDesfireData);
    const MfDesfireData* data = nfc_dev_get_protocol_data(nfc->nfc_dev, NfcProtocolTypeMfDesfire);

    if(event.type == SceneManagerEventTypeCustom) {
        TextBox* text_box = nfc->text_box;
        furi_string_reset(nfc->text_box_store);

        if(event.event == SubmenuIndexCardInfo) {
            nfc_mf_desfire_format_data(data, nfc->text_box_store);
            text_box_set_text(text_box, furi_string_get_cstr(nfc->text_box_store));
            view_dispatcher_switch_to_view(nfc->view_dispatcher, NfcViewTextBox);
            scene_manager_set_scene_state(
                nfc->scene_manager,
                NfcSceneMfDesfireData,
                MifareDesfireDataStateItem + SubmenuIndexCardInfo);
            consumed = true;
        } else {
            const uint32_t index = event.event - SubmenuIndexDynamic;
            scene_manager_set_scene_state(
                nfc->scene_manager, NfcSceneMfDesfireData, MifareDesfireDataStateItem + index);
            scene_manager_set_scene_state(nfc->scene_manager, NfcSceneMfDesfireApp, index << 1);
            scene_manager_next_scene(nfc->scene_manager, NfcSceneMfDesfireApp);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        if(state >= MifareDesfireDataStateItem) {
            view_dispatcher_switch_to_view(nfc->view_dispatcher, NfcViewMenu);
            scene_manager_set_scene_state(
                nfc->scene_manager, NfcSceneMfDesfireData, MifareDesfireDataStateMenu);
            consumed = true;
        }
    }

    return consumed;
}

void nfc_scene_mf_desfire_data_on_exit(void* context) {
    NfcApp* nfc = context;

    // Clear views
    text_box_reset(nfc->text_box);
    furi_string_reset(nfc->text_box_store);
    submenu_reset(nfc->submenu);
}
