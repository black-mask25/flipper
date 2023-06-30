#include "mf_desfire_i.h"

#include <furi.h>

#define MF_DESFIRE_PROTOCOL_NAME "Mifare DESfire"
#define MF_DESFIRE_DEVICE_NAME "Mifare DESfire"

const NfcDeviceBase nfc_device_mf_desfire = {
    .protocol_name = MF_DESFIRE_PROTOCOL_NAME,
    .alloc = (NfcDeviceAlloc)mf_desfire_alloc,
    .free = (NfcDeviceFree)mf_desfire_free,
    .reset = (NfcDeviceReset)mf_desfire_reset,
    .copy = (NfcDeviceCopy)mf_desfire_copy,
    .verify = (NfcDeviceVerify)mf_desfire_verify,
    .load = (NfcDeviceLoad)mf_desfire_load,
    .save = (NfcDeviceSave)mf_desfire_save,
    .is_equal = (NfcDeviceEqual)mf_desfire_is_equal,
    .get_name = (NfcDeviceGetName)mf_desfire_get_device_name,
    .get_uid = (NfcDeviceGetUid)mf_desfire_get_uid,
    .get_base_data = (NfcDeviceGetBaseData)mf_desfire_get_base_data,
};

MfDesfireData* mf_desfire_alloc() {
    MfDesfireData* data = malloc(sizeof(MfDesfireData));
    data->iso14443_4a_data = iso14443_4a_alloc();
    data->master_key_versions = simple_array_alloc(&mf_desfire_key_version_array_config);
    data->application_ids = simple_array_alloc(&mf_desfire_app_id_array_config);
    data->applications = simple_array_alloc(&mf_desfire_application_array_config);

    return data;
}

void mf_desfire_free(MfDesfireData* data) {
    furi_assert(data);

    mf_desfire_reset(data);
    simple_array_free(data->applications);
    simple_array_free(data->application_ids);
    simple_array_free(data->master_key_versions);
    iso14443_4a_free(data->iso14443_4a_data);
    free(data);
}

void mf_desfire_reset(MfDesfireData* data) {
    furi_assert(data);

    iso14443_4a_reset(data->iso14443_4a_data);

    memset(&data->version, 0, sizeof(MfDesfireVersion));
    memset(&data->free_memory, 0, sizeof(MfDesfireFreeMemory));

    simple_array_reset(data->master_key_versions);
    simple_array_reset(data->application_ids);
    simple_array_reset(data->applications);
}

void mf_desfire_copy(MfDesfireData* data, const MfDesfireData* other) {
    furi_assert(data);
    furi_assert(other);

    mf_desfire_reset(data);

    iso14443_4a_copy(data->iso14443_4a_data, other->iso14443_4a_data);

    data->version = other->version;
    data->free_memory = other->free_memory;
    data->master_key_settings = other->master_key_settings;

    simple_array_copy(data->master_key_versions, other->master_key_versions);
    simple_array_copy(data->application_ids, other->application_ids);
    simple_array_copy(data->applications, other->applications);
}

bool mf_desfire_verify(MfDesfireData* data, const FuriString* device_type) {
    UNUSED(data);
    return furi_string_equal_str(device_type, "Mifare Desfire");
}

bool mf_desfire_load(MfDesfireData* data, FlipperFormat* ff, uint32_t version) {
    UNUSED(data);
    UNUSED(ff);
    UNUSED(version);

    // TODO: Implementation
    return false;
}

bool mf_desfire_save(const MfDesfireData* data, FlipperFormat* ff, uint32_t version) {
    UNUSED(data);
    UNUSED(ff);
    UNUSED(version);

    // TODO: Implementation
    return false;
}

bool mf_desfire_is_equal(const MfDesfireData* data, const MfDesfireData* other) {
    furi_assert(data);
    furi_assert(other);

    // TODO: Complete equality method
    return iso14443_4a_is_equal(data->iso14443_4a_data, other->iso14443_4a_data);
}

const char* mf_desfire_get_device_name(const MfDesfireData* data, NfcDeviceNameType name_type) {
    UNUSED(data);
    UNUSED(name_type);
    return MF_DESFIRE_DEVICE_NAME;
}

const uint8_t* mf_desfire_get_uid(const MfDesfireData* data, size_t* uid_len) {
    furi_assert(data);

    return iso14443_4a_get_uid(data->iso14443_4a_data, uid_len);
}

const Iso14443_4aData* mf_desfire_get_base_data(const MfDesfireData* data) {
    furi_assert(data);

    return data->iso14443_4a_data;
}
