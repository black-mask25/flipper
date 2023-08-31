#include "iso14443_3b.h"

#include <furi.h>

#include <nfc/nfc_common.h>
#include <nfc/helpers/iso14443_crc.h>

#define ISO14443_3B_PROTOCOL_NAME "ISO14443-3B"
#define ISO14443_3B_DEVICE_NAME "ISO14443-3B (Unknown)"

#define ISO14443_3B_APP_DATA_KEY "Application data"
#define ISO14443_3B_PROTOCOL_INFO_KEY "Protocol info"

const NfcDeviceBase nfc_device_iso14443_3b = {
    .protocol_name = ISO14443_3B_PROTOCOL_NAME,
    .alloc = (NfcDeviceAlloc)iso14443_3b_alloc,
    .free = (NfcDeviceFree)iso14443_3b_free,
    .reset = (NfcDeviceReset)iso14443_3b_reset,
    .copy = (NfcDeviceCopy)iso14443_3b_copy,
    .verify = (NfcDeviceVerify)iso14443_3b_verify,
    .load = (NfcDeviceLoad)iso14443_3b_load,
    .save = (NfcDeviceSave)iso14443_3b_save,
    .is_equal = (NfcDeviceEqual)iso14443_3b_is_equal,
    .get_name = (NfcDeviceGetName)iso14443_3b_get_device_name,
    .get_uid = (NfcDeviceGetUid)iso14443_3b_get_uid,
    .set_uid = (NfcDeviceSetUid)iso14443_3b_set_uid,
    .get_base_data = (NfcDeviceGetBaseData)iso14443_3b_get_base_data,
};

Iso14443_3bData* iso14443_3b_alloc() {
    Iso14443_3bData* data = malloc(sizeof(Iso14443_3bData));
    return data;
}

void iso14443_3b_free(Iso14443_3bData* data) {
    furi_assert(data);

    free(data);
}

void iso14443_3b_reset(Iso14443_3bData* data) {
    memset(data, 0, sizeof(Iso14443_3bData));
}

void iso14443_3b_copy(Iso14443_3bData* data, const Iso14443_3bData* other) {
    furi_assert(data);
    furi_assert(other);

    *data = *other;
}

bool iso14443_3b_verify(Iso14443_3bData* data, const FuriString* device_type) {
    UNUSED(data);
    UNUSED(device_type);
    // TODO: How to distinguish from old ISO14443-3/4a?
    return false;
}

bool iso14443_3b_load(Iso14443_3bData* data, FlipperFormat* ff, uint32_t version) {
    furi_assert(data);

    bool parsed = false;

    do {
        if(version < NFC_UNIFIED_FORMAT_VERSION) break;

        if(!flipper_format_read_hex(
               ff, ISO14443_3B_APP_DATA_KEY, data->app_data, ISO14443_3B_APP_DATA_SIZE))
            break;
        if(!flipper_format_read_hex(
               ff,
               ISO14443_3B_PROTOCOL_INFO_KEY,
               data->protocol_info,
               ISO14443_3B_PROTOCOL_INFO_SIZE))
            break;

        parsed = true;
    } while(false);

    return parsed;
}

bool iso14443_3b_save(const Iso14443_3bData* data, FlipperFormat* ff) {
    furi_assert(data);

    bool saved = false;

    do {
        if(!flipper_format_write_comment_cstr(ff, ISO14443_3B_PROTOCOL_NAME " specific data"))
            break;
        if(!flipper_format_write_hex(
               ff, ISO14443_3B_APP_DATA_KEY, data->app_data, ISO14443_3B_APP_DATA_SIZE))
            break;
        if(!flipper_format_write_hex(
               ff,
               ISO14443_3B_PROTOCOL_INFO_KEY,
               data->protocol_info,
               ISO14443_3B_PROTOCOL_INFO_SIZE))
            break;
        saved = true;
    } while(false);

    return saved;
}

bool iso14443_3b_is_equal(const Iso14443_3bData* data, const Iso14443_3bData* other) {
    furi_assert(data);
    furi_assert(other);

    return memcmp(data, other, sizeof(Iso14443_3bData)) == 0;
}

const char* iso14443_3b_get_device_name(const Iso14443_3bData* data, NfcDeviceNameType name_type) {
    UNUSED(data);
    UNUSED(name_type);

    return ISO14443_3B_DEVICE_NAME;
}

const uint8_t* iso14443_3b_get_uid(const Iso14443_3bData* data, size_t* uid_len) {
    furi_assert(data);

    if(uid_len) {
        *uid_len = ISO14443_3B_UID_SIZE;
    }

    return data->uid;
}

bool iso14443_3b_set_uid(Iso14443_3bData* data, const uint8_t* uid, size_t uid_len) {
    furi_assert(data);

    const bool uid_valid = uid_len == ISO14443_3B_UID_SIZE;

    if(uid_valid) {
        memcpy(data->uid, uid, uid_len);
    }

    return uid_valid;
}

Iso14443_3bData* iso14443_3b_get_base_data(const Iso14443_3bData* data) {
    UNUSED(data);
    furi_crash("No base data");
}
