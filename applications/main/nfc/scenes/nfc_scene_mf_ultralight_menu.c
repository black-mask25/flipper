#include "../nfc_app_i.h"
#include <dolphin/dolphin.h>

enum SubmenuIndex {
    SubmenuIndexUnlock,
    SubmenuIndexSave,
    SubmenuIndexEmulate,
    SubmenuIndexInfo,
};

void nfc_scene_mf_ultralight_menu_submenu_callback(void* context, uint32_t index) {
    NfcApp* nfc = context;

    view_dispatcher_send_custom_event(nfc->view_dispatcher, index);
}

void nfc_scene_mf_ultralight_menu_on_enter(void* context) {
    NfcApp* nfc = context;
    Submenu* submenu = nfc->submenu;

    const MfUltralightData* data = nfc_device_get_data(nfc->nfc_device, NfcProtocolMfUltralight);

    if(!mf_ultralight_is_all_data_read(data)) {
        submenu_add_item(
            submenu,
            "Unlock",
            SubmenuIndexUnlock,
            nfc_scene_mf_ultralight_menu_submenu_callback,
            nfc);
    }
    submenu_add_item(
        submenu, "Save", SubmenuIndexSave, nfc_scene_mf_ultralight_menu_submenu_callback, nfc);
    submenu_add_item(
        submenu,
        "Emulate",
        SubmenuIndexEmulate,
        nfc_scene_mf_ultralight_menu_submenu_callback,
        nfc);
    submenu_add_item(
        submenu, "Info", SubmenuIndexInfo, nfc_scene_mf_ultralight_menu_submenu_callback, nfc);

    submenu_set_selected_item(
        nfc->submenu, scene_manager_get_scene_state(nfc->scene_manager, NfcSceneMfUltralightMenu));

    view_dispatcher_switch_to_view(nfc->view_dispatcher, NfcViewMenu);
}

bool nfc_scene_mf_ultralight_menu_on_event(void* context, SceneManagerEvent event) {
    NfcApp* nfc = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexSave) {
            scene_manager_next_scene(nfc->scene_manager, NfcSceneSaveName);
            consumed = true;
        } else if(event.event == SubmenuIndexEmulate) {
            scene_manager_next_scene(nfc->scene_manager, NfcSceneMfUltralightEmulate);
            // if(scene_manager_has_previous_scene(nfc->scene_manager, NfcSceneSetType)) {
            // dolphin_deed(DolphinDeedNfcAddEmulate);
            // } else {
            // dolphin_deed(DolphinDeedNfcEmulate);
            // }
            consumed = true;
        } else if(event.event == SubmenuIndexUnlock) {
            scene_manager_next_scene(nfc->scene_manager, NfcSceneMfUltralightUnlockMenu);
            consumed = true;
        } else if(event.event == SubmenuIndexInfo) {
            scene_manager_next_scene(nfc->scene_manager, NfcSceneInfo);
            consumed = true;
        }
        scene_manager_set_scene_state(nfc->scene_manager, NfcSceneMfUltralightMenu, event.event);

    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_previous_scene(nfc->scene_manager);
    }

    return consumed;
}

void nfc_scene_mf_ultralight_menu_on_exit(void* context) {
    NfcApp* nfc = context;

    // Clear view
    submenu_reset(nfc->submenu);
}
