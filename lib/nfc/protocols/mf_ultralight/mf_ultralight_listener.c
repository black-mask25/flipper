#include "mf_ultralight_listener_i.h"
#include "mf_ultralight_listener_defs.h"

#include <lib/nfc/protocols/iso14443_3a/iso14443_3a_listener_i.h>

#include <furi.h>

#define TAG "MfUltralightListener"

#define MF_ULTRALIGHT_LISTENER_MAX_TX_BUFF_SIZE (32)

typedef enum {
    MfUltralightListenerAccessTypeRead,
    MfUltralightListenerAccessTypeWrite,
} MfUltralightListenerAccessType;

typedef bool (*MfUltralightListenerCommandCallback)(MfUltralightListener* instance, BitBuffer* buf);

typedef struct {
    uint8_t cmd;
    size_t cmd_len_bits;
    MfUltralightListenerCommandCallback callback;
} MfUltralightListenerCmdHandler;

static bool mf_ultralight_listener_check_access(
    MfUltralightListener* instance,
    uint8_t start_page,
    MfUltralightListenerAccessType access_type) {
    bool access_success = false;
    bool is_write_op = (access_type == MfUltralightListenerAccessTypeWrite);

    do {
        if((instance->features & MfUltralightFeatureSupportAuthentication) == 0) {
            access_success = true;
            break;
        }
        if(instance->auth_state != MfUltralightListenerAuthStateSuccess) {
            if((instance->config->auth0 <= start_page) &&
               (instance->config->access.prot || is_write_op)) {
                break;
            }
        }
        if(instance->config->access.cfglck && is_write_op) {
            uint16_t config_page_start = instance->data->pages_total - 4;
            if((start_page == config_page_start) || (start_page == config_page_start + 1)) {
                break;
            }
        }

        access_success = true;
    } while(false);

    return access_success;
}

static void mf_ultralight_listener_send_short_resp(MfUltralightListener* instance, uint8_t data) {
    furi_assert(instance->tx_buffer);

    bit_buffer_set_size(instance->tx_buffer, 4);
    bit_buffer_set_byte(instance->tx_buffer, 0, data);
    iso14443_3a_listener_tx(instance->iso14443_3a_listener, instance->tx_buffer);
};

static bool
    mf_ultralight_listener_read_page_handler(MfUltralightListener* instance, BitBuffer* buffer) {
    bool command_processed = false;
    uint8_t start_page = bit_buffer_get_byte(buffer, 1);
    uint16_t pages_total = instance->data->pages_total;
    MfUltralightPageReadCommandData read_cmd_data = {};

    if(pages_total < start_page) {
        mf_ultralight_listener_send_short_resp(instance, MF_ULTRALIGHT_CMD_NACK);
        instance->state = MfUltraligthListenerStateIdle;
        instance->auth_state = MfUltralightListenerAuthStateIdle;
    } else if(!mf_ultralight_listener_check_access(
                  instance, start_page, MfUltralightListenerAccessTypeRead)) {
        mf_ultralight_listener_send_short_resp(instance, MF_ULTRALIGHT_CMD_NACK);
        instance->state = MfUltraligthListenerStateIdle;
        instance->auth_state = MfUltralightListenerAuthStateIdle;
    } else {
        uint16_t config_page = mf_ultralight_get_config_page_num(instance->data->type);
        for(size_t i = 0; i < 4; i++) {
            bool hide_data =
                ((config_page != 0) && ((i == config_page + 1U) || (i == config_page + 2U)));
            if(hide_data) {
                memset(read_cmd_data.page[i].data, 0, sizeof(MfUltralightPage));
            } else {
                read_cmd_data.page[i] = instance->data->page[(start_page + i) % pages_total];
            }
        }
        bit_buffer_copy_bytes(
            instance->tx_buffer,
            (uint8_t*)&read_cmd_data,
            sizeof(MfUltralightPageReadCommandData));
        iso14443_3a_listener_send_standard_frame(
            instance->iso14443_3a_listener, instance->tx_buffer);
    }
    command_processed = true;

    return command_processed;
}

static bool
    mf_ultralight_listener_write_page_handler(MfUltralightListener* instance, BitBuffer* buffer) {
    bool command_processed = false;
    uint8_t start_page = bit_buffer_get_byte(buffer, 1);
    uint16_t pages_total = instance->data->pages_total;

    if(pages_total < start_page) {
        mf_ultralight_listener_send_short_resp(instance, MF_ULTRALIGHT_CMD_NACK);
        instance->state = MfUltraligthListenerStateIdle;
        instance->auth_state = MfUltralightListenerAuthStateIdle;
    } else if(!mf_ultralight_listener_check_access(
                  instance, start_page, MfUltralightListenerAccessTypeWrite)) {
        mf_ultralight_listener_send_short_resp(instance, MF_ULTRALIGHT_CMD_NACK);
        instance->state = MfUltraligthListenerStateIdle;
        instance->auth_state = MfUltralightListenerAuthStateIdle;
    } else {
        const uint8_t* rx_data = bit_buffer_get_data(buffer);
        memcpy(instance->data->page[start_page].data, &rx_data[2], sizeof(MfUltralightPage));
        mf_ultralight_listener_send_short_resp(instance, MF_ULTRALIGHT_CMD_ACK);
    }
    command_processed = true;

    return command_processed;
}

static bool
    mf_ultralight_listener_read_version_handler(MfUltralightListener* instance, BitBuffer* buffer) {
    UNUSED(buffer);

    bool command_processed = false;

    if((instance->features & MfUltralightFeatureSupportReadVersion)) {
        bit_buffer_copy_bytes(
            instance->tx_buffer, (uint8_t*)&instance->data->version, sizeof(MfUltralightVersion));
        iso14443_3a_listener_send_standard_frame(
            instance->iso14443_3a_listener, instance->tx_buffer);
    } else {
        iso14443_3a_listener_sleep(instance->iso14443_3a_listener);
        instance->state = MfUltraligthListenerStateIdle;
    }
    command_processed = true;

    return command_processed;
}

static bool mf_ultralight_listener_read_signature_handler(
    MfUltralightListener* instance,
    BitBuffer* buffer) {
    UNUSED(buffer);

    bool command_processed = false;

    if((instance->features & MfUltralightFeatureSupportReadSignature)) {
        bit_buffer_copy_bytes(
            instance->tx_buffer, instance->data->signature.data, sizeof(MfUltralightSignature));
        iso14443_3a_listener_send_standard_frame(
            instance->iso14443_3a_listener, instance->tx_buffer);
    } else {
        iso14443_3a_listener_sleep(instance->iso14443_3a_listener);
        instance->state = MfUltraligthListenerStateIdle;
    }
    command_processed = true;

    return command_processed;
}

static bool
    mf_ultralight_listener_read_counter_handler(MfUltralightListener* instance, BitBuffer* buffer) {
    bool command_processed = false;

    do {
        uint8_t counter_num = bit_buffer_get_byte(buffer, 1);
        if((instance->features & MfUltralightFeatureSupportReadCounter) == 0) break;
        if(instance->features & MfUltralightFeatureSupportSingleCounter) {
            if(counter_num != 2) {
                break;
            }
        }
        if(instance->config) {
            if(!instance->config->access.nfc_cnt_en) {
                break;
            }
            if(instance->config->access.nfc_cnt_pwd_prot) {
                if(instance->auth_state != MfUltralightListenerAuthStateSuccess) {
                    break;
                }
            }
        }
        if(counter_num > 2) break;
        uint8_t cnt_value[3] = {
            (instance->data->counter[counter_num].counter >> 0) & 0xff,
            (instance->data->counter[counter_num].counter >> 8) & 0xff,
            (instance->data->counter[counter_num].counter >> 16) & 0xff,
        };
        bit_buffer_copy_bytes(instance->tx_buffer, cnt_value, sizeof(cnt_value));
        iso14443_3a_listener_send_standard_frame(
            instance->iso14443_3a_listener, instance->tx_buffer);
        command_processed = true;
    } while(false);

    return command_processed;
}

static bool mf_ultralight_listener_check_tearing_handler(
    MfUltralightListener* instance,
    BitBuffer* buffer) {
    bool command_processed = false;

    do {
        uint8_t tearing_flag_num = bit_buffer_get_byte(buffer, 1);
        if((instance->features & MfUltralightFeatureSupportCheckTearingFlag) == 0) break;
        if(tearing_flag_num > 2) break;
        bit_buffer_set_size_bytes(instance->tx_buffer, 1);
        bit_buffer_set_byte(instance->tx_buffer, 0, instance->data->tearing_flag->data[0]);
        iso14443_3a_listener_send_standard_frame(
            instance->iso14443_3a_listener, instance->tx_buffer);
        command_processed = true;
    } while(false);

    return command_processed;
}

static bool
    mf_ultralight_listener_auth_handler(MfUltralightListener* instance, BitBuffer* buffer) {
    bool command_processed = false;

    do {
        if((instance->features & MfUltralightFeatureSupportAuthentication) == 0) break;

        const uint8_t* rx_data = bit_buffer_get_data(buffer);
        MfUltralightAuthPassword password = {};
        memcpy(password.data, &rx_data[1], sizeof(MfUltralightAuthPassword));
        if(instance->callback) {
            instance->mfu_event_data.password = password;
            instance->mfu_event.type = MfUltralightListenerEventTypeAuth;
            instance->callback(instance->generic_event, instance->context);
        }
        if(password.pass != instance->config->password.pass) break;

        bit_buffer_copy_bytes(
            instance->tx_buffer, instance->config->pack.data, sizeof(MfUltralightAuthPack));
        instance->auth_state = MfUltralightListenerAuthStateSuccess;
        iso14443_3a_listener_send_standard_frame(
            instance->iso14443_3a_listener, instance->tx_buffer);

        command_processed = true;
    } while(false);

    return command_processed;
}

static const MfUltralightListenerCmdHandler mf_ultralight_command[] = {
    {
        .cmd = MF_ULTRALIGHT_CMD_READ_PAGE,
        .cmd_len_bits = 2 * 8,
        .callback = mf_ultralight_listener_read_page_handler,
    },
    {
        .cmd = MF_ULTRALIGHT_CMD_WRITE_PAGE,
        .cmd_len_bits = 6 * 8,
        .callback = mf_ultralight_listener_write_page_handler,
    },
    {
        .cmd = MF_ULTRALIGHT_CMD_GET_VERSION,
        .cmd_len_bits = 8,
        .callback = mf_ultralight_listener_read_version_handler,
    },
    {
        .cmd = MF_ULTRALIGTH_CMD_READ_SIG,
        .cmd_len_bits = 2 * 8,
        .callback = mf_ultralight_listener_read_signature_handler,
    },
    {
        .cmd = MF_ULTRALIGHT_CMD_READ_CNT,
        .cmd_len_bits = 2 * 8,
        .callback = mf_ultralight_listener_read_counter_handler,
    },
    {
        .cmd = MF_ULTRALIGHT_CMD_CHECK_TEARING,
        .cmd_len_bits = 2 * 8,
        .callback = mf_ultralight_listener_check_tearing_handler,
    },
    {
        .cmd = MF_ULTRALIGHT_CMD_AUTH,
        .cmd_len_bits = 5 * 8,
        .callback = mf_ultralight_listener_auth_handler,
    },
};

static void mf_ultralight_listener_prepare_emulation(MfUltralightListener* instance) {
    MfUltralightData* data = instance->data;
    instance->features = mf_ultralight_get_feature_support_set(data->type);
    mf_ultralight_get_config_page(data, &instance->config);
}

MfUltralightListener* mf_ultralight_listener_alloc(
    Iso14443_3aListener* iso14443_3a_listener,
    const MfUltralightData* data) {
    furi_assert(iso14443_3a_listener);

    MfUltralightListener* instance = malloc(sizeof(MfUltralightListener));
    instance->iso14443_3a_listener = iso14443_3a_listener;
    instance->data = mf_ultralight_alloc();
    mf_ultralight_copy(instance->data, data);
    mf_ultralight_listener_prepare_emulation(instance);
    instance->tx_buffer = bit_buffer_alloc(MF_ULTRALIGHT_LISTENER_MAX_TX_BUFF_SIZE);

    instance->mfu_event.data = &instance->mfu_event_data;
    instance->generic_event.protocol = NfcProtocolMfUltralight;
    instance->generic_event.instance = instance;
    instance->generic_event.data = &instance->mfu_event;

    return instance;
}

void mf_ultralight_listener_free(MfUltralightListener* instance) {
    furi_assert(instance);
    furi_assert(instance->data);
    furi_assert(instance->tx_buffer);

    bit_buffer_free(instance->tx_buffer);
    mf_ultralight_free(instance->data);
    free(instance);
}

const MfUltralightData* mf_ultralight_listener_get_data(MfUltralightListener* instance) {
    furi_assert(instance);
    furi_assert(instance->data);

    return instance->data;
}

void mf_ultralight_listener_set_callback(
    MfUltralightListener* instance,
    NfcGenericCallback callback,
    void* context) {
    furi_assert(instance);

    instance->callback = callback;
    instance->context = context;
}

NfcCommand mf_ultralight_listener_run(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.protocol == NfcProtocolIso14443_3a);
    furi_assert(event.data);

    MfUltralightListener* instance = context;
    Iso14443_3aListenerEvent* iso14443_3a_event = event.data;
    BitBuffer* rx_buffer = iso14443_3a_event->data->buffer;
    NfcCommand command = NfcCommandContinue;

    if(iso14443_3a_event->type == Iso14443_3aListenerEventTypeReceivedStandardFrame) {
        bool cmd_processed = false;
        for(size_t i = 0; i < COUNT_OF(mf_ultralight_command); i++) {
            if(bit_buffer_get_size(rx_buffer) != mf_ultralight_command[i].cmd_len_bits) continue;
            if(bit_buffer_get_byte(rx_buffer, 0) != mf_ultralight_command[i].cmd) continue;
            cmd_processed = mf_ultralight_command[i].callback(instance, rx_buffer);
            if(cmd_processed) break;
        }
        if(!cmd_processed) {
            mf_ultralight_listener_send_short_resp(instance, MF_ULTRALIGHT_CMD_NACK);
            instance->state = MfUltraligthListenerStateIdle;
            instance->auth_state = MfUltralightListenerAuthStateIdle;
        }
    }

    return command;
}

const NfcListenerBase mf_ultralight_listener = {
    .alloc = (NfcListenerAlloc)mf_ultralight_listener_alloc,
    .free = (NfcListenerFree)mf_ultralight_listener_free,
    .get_data = (NfcListenerGetData)mf_ultralight_listener_get_data,
    .set_callback = (NfcListenerSetCallback)mf_ultralight_listener_set_callback,
    .run = (NfcListenerRun)mf_ultralight_listener_run,
};
