#include "mf_classic.h"
#include "mf_classic_render.h"

#include <nfc/protocols/mf_classic/mf_classic_poller.h>

#include "nfc/nfc_app_i.h"

#include "../nfc_protocol_support_gui_common.h"

#define TAG "MfClassicApp"

enum {
    SubmenuIndexDetectReader = SubmenuIndexCommonMax,
    SubmenuIndexWrite,
    SubmenuIndexUpdate,
};

static void nfc_scene_info_on_enter_mf_classic(NfcApp* instance) {
    const NfcDevice* device = instance->nfc_device;
    const MfClassicData* data = nfc_device_get_data(device, NfcProtocolMfClassic);

    FuriString* temp_str = furi_string_alloc();
    furi_string_cat_printf(
        temp_str, "\e#%s\n", nfc_device_get_name(device, NfcDeviceNameTypeFull));
    nfc_render_mf_classic_info(data, NfcProtocolFormatTypeFull, temp_str);

    widget_add_text_scroll_element(
        instance->widget, 0, 0, 128, 52, furi_string_get_cstr(temp_str));

    widget_add_button_element(
        instance->widget,
        GuiButtonTypeRight,
        "More",
        nfc_protocol_support_common_widget_callback,
        instance);

    furi_string_free(temp_str);
}

static NfcCommand nfc_scene_read_poller_callback_mf_classic(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolMfClassic);

    NfcApp* instance = context;
    const MfClassicPollerEvent* mfc_event = event.data;
    NfcCommand command = NfcCommandContinue;

    if(mfc_event->type == MfClassicPollerEventTypeRequestMode) {
        nfc_device_set_data(
            instance->nfc_device, NfcProtocolMfClassic, nfc_poller_get_data(instance->poller));
        size_t uid_len = 0;
        const uint8_t* uid = nfc_device_get_uid(instance->nfc_device, &uid_len);
        if(mf_classic_key_cache_load(instance->mfc_key_cache, uid, uid_len)) {
            FURI_LOG_I(TAG, "Key cache found");
            mfc_event->data->poller_mode.mode = MfClassicPollerModeRead;
        } else {
            FURI_LOG_I(TAG, "Key cache not found");
            view_dispatcher_send_custom_event(
                instance->view_dispatcher, NfcCustomEventPollerIncomplete);
            command = NfcCommandStop;
        }
    } else if(mfc_event->type == MfClassicPollerEventTypeRequestReadSector) {
        uint8_t sector_num = 0;
        MfClassicKey key = {};
        MfClassicKeyType key_type = MfClassicKeyTypeA;
        if(mf_classic_key_cahce_get_next_key(
               instance->mfc_key_cache, &sector_num, &key, &key_type)) {
            mfc_event->data->read_sector_request_data.sector_num = sector_num;
            mfc_event->data->read_sector_request_data.key = key;
            mfc_event->data->read_sector_request_data.key_type = key_type;
            mfc_event->data->read_sector_request_data.key_provided = true;
        } else {
            mfc_event->data->read_sector_request_data.key_provided = false;
        }
    } else if(mfc_event->type == MfClassicPollerEventTypeSuccess) {
        nfc_device_set_data(
            instance->nfc_device, NfcProtocolMfClassic, nfc_poller_get_data(instance->poller));
        const MfClassicData* mfc_data =
            nfc_device_get_data(instance->nfc_device, NfcProtocolMfClassic);
        NfcCustomEvent custom_event = mf_classic_is_card_read(mfc_data) ?
                                          NfcCustomEventPollerSuccess :
                                          NfcCustomEventPollerIncomplete;
        view_dispatcher_send_custom_event(instance->view_dispatcher, custom_event);
        command = NfcCommandStop;
    }

    return command;
}

static void nfc_scene_read_on_enter_mf_classic(NfcApp* instance) {
    mf_classic_key_cache_reset(instance->mfc_key_cache);
    nfc_poller_start(instance->poller, nfc_scene_read_poller_callback_mf_classic, instance);
}

static bool nfc_scene_read_on_event_mf_classic(NfcApp* instance, uint32_t event) {
    if(event == NfcCustomEventPollerIncomplete) {
        const MfClassicData* mfc_data = nfc_poller_get_data(instance->poller);
        if(mf_classic_is_card_read(mfc_data)) {
            view_dispatcher_send_custom_event(
                instance->view_dispatcher, NfcCustomEventPollerSuccess);
        } else {
            scene_manager_next_scene(instance->scene_manager, NfcSceneMfClassicDictAttack);
        }
    }

    return true;
}

static void nfc_scene_read_menu_on_enter_mf_classic(NfcApp* instance) {
    Submenu* submenu = instance->submenu;
    const MfClassicData* data = nfc_device_get_data(instance->nfc_device, NfcProtocolMfClassic);

    if(!mf_classic_is_card_read(data)) {
        submenu_add_item(
            submenu,
            "Detect Reader",
            SubmenuIndexDetectReader,
            nfc_protocol_support_common_submenu_callback,
            instance);
    }
}

static void nfc_scene_read_success_on_enter_mf_classic(NfcApp* instance) {
    const NfcDevice* device = instance->nfc_device;
    const MfClassicData* data = nfc_device_get_data(device, NfcProtocolMfClassic);

    FuriString* temp_str = furi_string_alloc();
    furi_string_cat_printf(
        temp_str, "\e#%s\n", nfc_device_get_name(device, NfcDeviceNameTypeFull));
    nfc_render_mf_classic_info(data, NfcProtocolFormatTypeShort, temp_str);

    widget_add_text_scroll_element(
        instance->widget, 0, 0, 128, 52, furi_string_get_cstr(temp_str));

    furi_string_free(temp_str);
}

static void nfc_scene_saved_menu_on_enter_mf_classic(NfcApp* instance) {
    Submenu* submenu = instance->submenu;
    const MfClassicData* data = nfc_device_get_data(instance->nfc_device, NfcProtocolMfClassic);

    if(!mf_classic_is_card_read(data)) {
        submenu_add_item(
            submenu,
            "Detect Reader",
            SubmenuIndexDetectReader,
            nfc_protocol_support_common_submenu_callback,
            instance);
    }
    submenu_add_item(
        submenu,
        "Write to Initial Card",
        SubmenuIndexWrite,
        nfc_protocol_support_common_submenu_callback,
        instance);
    submenu_add_item(
        submenu,
        "Update from Initial Card",
        SubmenuIndexUpdate,
        nfc_protocol_support_common_submenu_callback,
        instance);
}

static void nfc_scene_emulate_on_enter_mf_classic(NfcApp* instance) {
    const MfClassicData* data = nfc_device_get_data(instance->nfc_device, NfcProtocolMfClassic);
    instance->listener = nfc_listener_alloc(instance->nfc, NfcProtocolMfClassic, data);
    nfc_listener_start(instance->listener, NULL, NULL);
}

static bool nfc_scene_info_on_event_mf_classic(NfcApp* instance, uint32_t event) {
    if(event == GuiButtonTypeRight) {
        scene_manager_next_scene(instance->scene_manager, NfcSceneNotImplemented);
        return true;
    }

    return false;
}

static bool nfc_scene_read_menu_on_event_mf_classic(NfcApp* instance, uint32_t event) {
    if(event == SubmenuIndexDetectReader) {
        scene_manager_next_scene(instance->scene_manager, NfcSceneNotImplemented);
        dolphin_deed(DolphinDeedNfcDetectReader);
        return true;
    }

    return false;
}

static bool nfc_scene_saved_menu_on_event_mf_classic(NfcApp* instance, uint32_t event) {
    bool consumed = false;

    if(event == SubmenuIndexDetectReader) {
        scene_manager_next_scene(instance->scene_manager, NfcSceneMfClassicDetectReader);
        consumed = true;
    } else if(event == SubmenuIndexWrite) {
        scene_manager_next_scene(instance->scene_manager, NfcSceneMfClassicWriteInitial);
        consumed = true;
    } else if(event == SubmenuIndexUpdate) {
        scene_manager_next_scene(instance->scene_manager, NfcSceneMfClassicUpdateInitial);
        consumed = true;
    }

    return consumed;
}

const NfcProtocolSupportBase nfc_protocol_support_mf_classic = {
    .features = NfcProtocolFeatureEmulateFull,

    .scene_info =
        {
            .on_enter = nfc_scene_info_on_enter_mf_classic,
            .on_event = nfc_scene_info_on_event_mf_classic,
        },
    .scene_read =
        {
            .on_enter = nfc_scene_read_on_enter_mf_classic,
            .on_event = nfc_scene_read_on_event_mf_classic,
        },
    .scene_read_menu =
        {
            .on_enter = nfc_scene_read_menu_on_enter_mf_classic,
            .on_event = nfc_scene_read_menu_on_event_mf_classic,
        },
    .scene_read_success =
        {
            .on_enter = nfc_scene_read_success_on_enter_mf_classic,
            .on_event = NULL,
        },
    .scene_saved_menu =
        {
            .on_enter = nfc_scene_saved_menu_on_enter_mf_classic,
            .on_event = nfc_scene_saved_menu_on_event_mf_classic,
        },
    .scene_emulate =
        {
            .on_enter = nfc_scene_emulate_on_enter_mf_classic,
            .on_event = NULL,
        },
};
