#include "iso14443_3a_i.h"
#include "iso14443_3a_render.h"

#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>

#include "../nfc_protocol_support_gui_common.h"
#include "../../../nfc_app_i.h"

static void nfc_scene_info_on_enter_iso14443_3a(NfcApp* instance) {
    const NfcDevice* device = instance->nfc_device;
    const Iso14443_3aData* data = nfc_device_get_data(device, NfcProtocolIso14443_3a);

    FuriString* temp_str = furi_string_alloc();
    furi_string_cat_printf(
        temp_str, "\e#%s\n", nfc_device_get_name(device, NfcDeviceNameTypeFull));
    nfc_render_iso14443_3a_info(data, NfcProtocolFormatTypeFull, temp_str);

    widget_add_text_scroll_element(
        instance->widget, 0, 0, 128, 64, furi_string_get_cstr(temp_str));

    furi_string_free(temp_str);
}

static NfcCommand
    nfc_scene_read_poller_callback_iso14443_3a(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolIso14443_3a);

    NfcApp* instance = context;
    const Iso14443_3aPollerEvent* iso14443_3a_event = event.data;

    if(iso14443_3a_event->type == Iso14443_3aPollerEventTypeReady) {
        nfc_device_set_data(
            instance->nfc_device, NfcProtocolIso14443_3a, nfc_poller_get_data(instance->poller));
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcCustomEventReadHandlerSuccess);
        return NfcCommandStop;
    }

    return NfcCommandContinue;
}

static void nfc_scene_read_on_enter_iso14443_3a(NfcApp* instance) {
    nfc_poller_start(instance->poller, nfc_scene_read_poller_callback_iso14443_3a, instance);
}

static void nfc_scene_read_menu_on_enter_iso14443_3a(NfcApp* instance) {
    UNUSED(instance);
}

static void nfc_scene_read_success_on_enter_iso14443_3a(NfcApp* instance) {
    const NfcDevice* device = instance->nfc_device;
    const Iso14443_3aData* data = nfc_device_get_data(device, NfcProtocolIso14443_3a);

    FuriString* temp_str = furi_string_alloc();
    furi_string_cat_printf(
        temp_str, "\e#%s\n", nfc_device_get_name(device, NfcDeviceNameTypeFull));
    nfc_render_iso14443_3a_info(data, NfcProtocolFormatTypeShort, temp_str);

    widget_add_text_scroll_element(
        instance->widget, 0, 0, 128, 52, furi_string_get_cstr(temp_str));

    furi_string_free(temp_str);
}

static void nfc_scene_saved_menu_on_enter_iso14443_3a(NfcApp* instance) {
    UNUSED(instance);
}

static bool nfc_scene_info_on_event_iso14443_3a(NfcApp* instance, uint32_t event) {
    if(event == GuiButtonTypeRight) {
        scene_manager_next_scene(instance->scene_manager, NfcSceneNotImplemented);
        return true;
    }

    return false;
}

static bool nfc_scene_read_menu_on_event_iso14443_3a(NfcApp* instance, uint32_t event) {
    if(event == SubmenuIndexCommonEmulate) {
        scene_manager_next_scene(instance->scene_manager, NfcSceneNfcaEmulate);
        return true;
    }

    return false;
}

bool nfc_scene_saved_menu_on_event_iso14443_3a_common(NfcApp* instance, uint32_t event) {
    switch(event) {
    case SubmenuIndexCommonEmulate:
        scene_manager_next_scene(instance->scene_manager, NfcSceneNfcaEmulate);
        return true;
    case SubmenuIndexCommonEdit:
        scene_manager_next_scene(instance->scene_manager, NfcSceneSetUid);
        return true;
    default:
        return false;
    }
}

static bool nfc_scene_saved_menu_on_event_iso14443_3a(NfcApp* instance, uint32_t event) {
    return nfc_scene_saved_menu_on_event_iso14443_3a_common(instance, event);
}

const NfcProtocolSupportBase nfc_protocol_support_iso14443_3a = {
    .features = NfcProtocolFeatureEmulateUid | NfcProtocolFeatureEditUid,

    .scene_info =
        {
            .on_enter = nfc_scene_info_on_enter_iso14443_3a,
            .on_event = nfc_scene_info_on_event_iso14443_3a,
        },
    .scene_read =
        {
            .on_enter = nfc_scene_read_on_enter_iso14443_3a,
            .on_event = NULL,
        },
    .scene_read_menu =
        {
            .on_enter = nfc_scene_read_menu_on_enter_iso14443_3a,
            .on_event = nfc_scene_read_menu_on_event_iso14443_3a,
        },
    .scene_read_success =
        {
            .on_enter = nfc_scene_read_success_on_enter_iso14443_3a,
            .on_event = NULL,
        },
    .scene_saved_menu =
        {
            .on_enter = nfc_scene_saved_menu_on_enter_iso14443_3a,
            .on_event = nfc_scene_saved_menu_on_event_iso14443_3a,
        },
};
