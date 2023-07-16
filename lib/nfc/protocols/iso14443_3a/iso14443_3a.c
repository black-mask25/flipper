#include "iso14443_3a.h"

#include <furi.h>
#include <nfc/nfc_common.h>

#define ISO14443_3A_CRC_INIT (0x6363)
#define ISO14443_3A_PROTOCOL_NAME_LEGACY "UID"
#define ISO14443_3A_PROTOCOL_NAME "ISO14443-3A"
#define ISO14443_3A_DEVICE_NAME "Unknown ISO14443-3A Tag"

#define ISO14443_3A_UID_KEY "UID"
#define ISO14443_3A_ATQA_KEY "ATQA"
#define ISO14443_3A_SAK_KEY "SAK"

const NfcDeviceBase nfc_device_iso14443_3a = {
    .protocol_name = ISO14443_3A_PROTOCOL_NAME,
    .alloc = (NfcDeviceAlloc)iso14443_3a_alloc,
    .free = (NfcDeviceFree)iso14443_3a_free,
    .reset = (NfcDeviceReset)iso14443_3a_reset,
    .copy = (NfcDeviceCopy)iso14443_3a_copy,
    .verify = (NfcDeviceVerify)iso14443_3a_verify,
    .load = (NfcDeviceLoad)iso14443_3a_load,
    .save = (NfcDeviceSave)iso14443_3a_save,
    .is_equal = (NfcDeviceEqual)iso14443_3a_is_equal,
    .get_name = (NfcDeviceGetName)iso14443_3a_get_device_name,
    .get_uid = (NfcDeviceGetUid)iso14443_3a_get_uid,
    .get_base_data = (NfcDeviceGetBaseData)iso14443_3a_get_base_data,
};

Iso14443_3aData* iso14443_3a_alloc() {
    Iso14443_3aData* data = malloc(sizeof(Iso14443_3aData));
    return data;
}

void iso14443_3a_free(Iso14443_3aData* data) {
    furi_assert(data);

    free(data);
}

void iso14443_3a_reset(Iso14443_3aData* data) {
    furi_assert(data);

    UNUSED(data);
}

void iso14443_3a_copy(Iso14443_3aData* data, const Iso14443_3aData* other) {
    furi_assert(data);
    furi_assert(other);

    *data = *other;
}

bool iso14443_3a_verify(Iso14443_3aData* data, const FuriString* device_type) {
    UNUSED(data);
    return furi_string_equal(device_type, ISO14443_3A_PROTOCOL_NAME_LEGACY);
}

bool iso14443_3a_load(Iso14443_3aData* data, FlipperFormat* ff, uint32_t version) {
    furi_assert(data);

    bool parsed = false;

    do {
        // TODO: Load UID in nfc_device.c
        // if(version < NFC_UNIFIED_FORMAT_VERSION) {
        uint32_t uid_len = 0;
        if(!flipper_format_get_value_count(ff, ISO14443_3A_UID_KEY, &uid_len)) break;
        if(!(uid_len == 4 || uid_len == 7)) break;

        data->uid_len = uid_len;

        if(!flipper_format_read_hex(ff, ISO14443_3A_UID_KEY, data->uid, data->uid_len)) break;
        // }

        // Common to all format versions
        if(!flipper_format_read_hex(ff, ISO14443_3A_ATQA_KEY, data->atqa, 2)) break;
        if(!flipper_format_read_hex(ff, ISO14443_3A_SAK_KEY, &data->sak, 1)) break;

        if(version > NFC_LSB_ATQA_FORMAT_VERSION) {
            // Swap ATQA bytes for newer versions
            const uint8_t tmp = data->atqa[0];
            data->atqa[0] = data->atqa[1];
            data->atqa[1] = tmp;
        }

        parsed = true;
    } while(false);

    return parsed;
}

bool iso14443_3a_save(const Iso14443_3aData* data, FlipperFormat* ff) {
    furi_assert(data);

    bool saved = false;

    do {
        // Save ATQA in MSB order for correct companion apps display
        const uint8_t atqa[2] = {data->atqa[1], data->atqa[0]};
        if(!flipper_format_write_comment_cstr(ff, ISO14443_3A_PROTOCOL_NAME " specific data"))
            break;

        // Write ATQA and SAK
        if(!flipper_format_write_hex(ff, ISO14443_3A_ATQA_KEY, atqa, 2)) break;
        if(!flipper_format_write_hex(ff, ISO14443_3A_SAK_KEY, &data->sak, 1)) break;
        saved = true;
    } while(false);

    return saved;
}

bool iso14443_3a_is_equal(const Iso14443_3aData* data, const Iso14443_3aData* other) {
    furi_assert(data);
    furi_assert(other);

    return memcmp(data, other, sizeof(Iso14443_3aData)) == 0;
}

const char* iso14443_3a_get_device_name(const Iso14443_3aData* data, NfcDeviceNameType name_type) {
    UNUSED(data);
    UNUSED(name_type);
    return ISO14443_3A_DEVICE_NAME;
}

const uint8_t* iso14443_3a_get_uid(const Iso14443_3aData* data, size_t* uid_len) {
    furi_assert(data);

    if(uid_len) {
        *uid_len = data->uid_len;
    }

    return data->uid;
}

const Iso14443_3aData* iso14443_3a_get_base_data(const Iso14443_3aData* data) {
    UNUSED(data);
    furi_crash("No base data");
}

static uint16_t iso14443_3a_get_crc(const uint8_t* buff, uint16_t len) {
    furi_assert(buff);
    furi_assert(len);

    uint16_t crc = ISO14443_3A_CRC_INIT;
    uint8_t byte = 0;

    for(uint8_t i = 0; i < len; i++) {
        byte = buff[i];
        byte ^= (uint8_t)(crc & 0xff);
        byte ^= byte << 4;
        crc = (crc >> 8) ^ (((uint16_t)byte) << 8) ^ (((uint16_t)byte) << 3) ^
              (((uint16_t)byte) >> 4);
    }

    return crc;
}

uint32_t iso14443_3a_get_cuid(const Iso14443_3aData* iso14443_3a_data) {
    furi_assert(iso14443_3a_data);

    uint32_t cuid = 0;
    const uint8_t* cuid_start = iso14443_3a_data->uid;
    if(iso14443_3a_data->uid_len == 7) {
        cuid_start = &iso14443_3a_data->uid[3];
    }
    cuid = (cuid_start[0] << 24) | (cuid_start[1] << 16) | (cuid_start[2] << 8) | (cuid_start[3]);

    return cuid;
}

void iso14443_3a_append_crc(BitBuffer* buffer) {
    furi_assert(buffer);

    const uint8_t* data = bit_buffer_get_data(buffer);
    size_t bytes = bit_buffer_get_size_bytes(buffer);

    uint16_t crc = iso14443_3a_get_crc(data, bytes);
    uint8_t crc_bytes[2] = {(uint8_t)crc, (uint8_t)(crc >> 8)};
    bit_buffer_append_bytes(buffer, crc_bytes, sizeof(crc_bytes));
}

bool iso14443_3a_check_crc(const BitBuffer* buf) {
    furi_assert(buf);

    bool crc_ok = false;
    do {
        const uint8_t* data = bit_buffer_get_data(buf);
        size_t bytes = bit_buffer_get_size_bytes(buf);
        if(bytes < 3) break;

        uint16_t crc_calc = iso14443_3a_get_crc(data, bytes - 2);
        uint8_t crc_start = bit_buffer_get_byte(buf, bytes - 2);
        uint8_t crc_end = bit_buffer_get_byte(buf, bytes - 1);
        uint16_t crc_received = (crc_end << 8) | crc_start;
        crc_ok = (crc_calc == crc_received);
    } while(false);

    return crc_ok;
}

void iso14443_3a_trim_crc(BitBuffer* buf) {
    furi_assert(buf);

    size_t bytes = bit_buffer_get_size_bytes(buf);
    furi_assert(bytes > 2);

    bit_buffer_set_size_bytes(buf, bytes - 2);
}
