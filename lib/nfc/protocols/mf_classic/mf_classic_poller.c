#include "mf_classic_poller_i.h"

#include <furi.h>

#define TAG "MfClassicPoller"

#define MF_CLASSIC_MAX_BUFF_SIZE (64)

typedef MfClassicPollerCommand (*MfClassicPollerReadHandler)(MfClassicPoller* instance);

static NfcaPollerCommand mf_classic_process_command(MfClassicPollerCommand command) {
    NfcaPollerCommand ret = NfcaPollerCommandContinue;

    if(command == MfClassicPollerCommandContinue) {
        ret = NfcaPollerCommandContinue;
    } else if(command == MfClassicPollerCommandContinue) {
        ret = NfcaPollerCommandContinue;
    } else if(command == MfClassicPollerCommandReset) {
        ret = NfcaPollerCommandReset;
    } else if(command == MfClassicPollerCommandStop) {
        ret = NfcaPollerCommandStop;
    } else {
        furi_crash("Unknown command");
    }

    return ret;
}

MfClassicPoller* mf_classic_poller_alloc(NfcaPoller* nfca_poller) {
    furi_assert(nfca_poller);

    MfClassicPoller* instance = malloc(sizeof(MfClassicPoller));
    instance->nfca_poller = nfca_poller;

    return instance;
}

void mf_classic_poller_free(MfClassicPoller* instance) {
    furi_assert(instance);

    free(instance);
}

const MfClassicData* mf_classic_poller_get_data(MfClassicPoller* instance) {
    furi_assert(instance);

    return instance->data;
}

MfClassicError mf_classic_poller_start(
    MfClassicPoller* instance,
    NfcaPollerEventCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(callback);
    furi_assert(instance->nfca_poller);
    furi_assert(instance->session_state == MfClassicPollerSessionStateIdle);

    instance->data = mf_classic_alloc();
    instance->crypto = crypto1_alloc();
    instance->tx_plain_buffer = bit_buffer_alloc(MF_CLASSIC_MAX_BUFF_SIZE);
    instance->tx_encrypted_buffer = bit_buffer_alloc(MF_CLASSIC_MAX_BUFF_SIZE);
    instance->rx_plain_buffer = bit_buffer_alloc(MF_CLASSIC_MAX_BUFF_SIZE);
    instance->rx_encrypted_buffer = bit_buffer_alloc(MF_CLASSIC_MAX_BUFF_SIZE);

    instance->session_state = MfClassicPollerSessionStateActive;
    nfca_poller_start(instance->nfca_poller, callback, context);

    return MfClassicErrorNone;
}

MfClassicPollerCommand mf_classic_poller_handler_idle(MfClassicPoller* instance) {
    nfca_copy(instance->data->nfca_data, nfca_poller_get_data(instance->nfca_poller));
    MfClassicPollerCommand command = MfClassicPollerCommandContinue;
    MfClassicPollerEvent event = {};

    if(mf_classic_detect_protocol(instance->data->nfca_data, &instance->data->type)) {
        if(instance->card_state == MfClassicCardStateNotDetected) {
            instance->card_state = MfClassicCardStateDetected;
            event.type = MfClassicPollerEventTypeCardDetected;
            command = instance->callback(event, instance->context);
        }
        instance->state = instance->prev_state;
    }

    return command;
}

MfClassicPollerCommand mf_classic_poller_handler_start(MfClassicPoller* instance) {
    MfClassicPollerCommand command = MfClassicPollerCommandContinue;

    instance->sectors_read = 0;
    instance->sectors_total = mf_classic_get_total_sectors_num(instance->data->type);
    instance->event_data.start_data.type = instance->data->type;
    MfClassicPollerEvent event = {
        .type = MfClassicPollerEventTypeStart,
        .data = &instance->event_data,
    };
    command = instance->callback(event, instance->context);
    instance->prev_state = MfClassicPollerStateStart;
    instance->state = MfClassicPollerStateNewSector;

    return command;
}

MfClassicPollerCommand mf_classic_poller_handler_new_sector(MfClassicPoller* instance) {
    MfClassicPollerCommand command = MfClassicPollerCommandContinue;
    MfClassicPollerEvent event = {};

    if(instance->read_mode == MfClassicReadModeKeyReuse) {
        instance->key_reuse_sector++;
        if(instance->key_reuse_sector == instance->sectors_total) {
            instance->read_mode = MfClassicReadModeDictAttack;
            event.type = MfClassicPollerEventTypeKeyAttackStop;
            command = instance->callback(event, instance->context);
        } else if(mf_classic_is_sector_read(instance->data, instance->sectors_read)) {
            event.type = MfClassicPollerEventTypeKeyAttackNextSector;
            command = instance->callback(event, instance->context);
        } else {
            instance->state = MfClassicPollerStateAuthKeyA;
            event.type = MfClassicPollerEventTypeKeyAttackNextSector;
            command = instance->callback(event, instance->context);
        }
    } else {
        if(instance->sectors_read == instance->sectors_total) {
            instance->state = MfClassicPollerStateReadComplete;
        } else if(mf_classic_is_sector_read(instance->data, instance->sectors_read)) {
            instance->sectors_read++;
            event.type = MfClassicPollerEventTypeNewSector;
            command = instance->callback(event, instance->context);
        } else {
            instance->state = MfClassicPollerStateRequestKey;
        }
    }
    instance->prev_state = instance->state;

    return command;
}

MfClassicPollerCommand mf_classic_poller_handler_request_key(MfClassicPoller* instance) {
    MfClassicPollerCommand command = MfClassicPollerCommandContinue;

    instance->event_data.key_request_data.sector_num = instance->sectors_read;
    MfClassicPollerEvent event = {
        .type = MfClassicPollerEventTypeRequestKey,
        .data = &instance->event_data,
    };
    command = instance->callback(event, instance->context);
    if(instance->event_data.key_request_data.key_provided) {
        instance->current_key = instance->event_data.key_request_data.key;
        instance->state = MfClassicPollerStateAuthKeyA;
    } else {
        instance->state = MfClassicPollerStateNewSector;
    }

    return command;
}

MfClassicPollerCommand mf_classic_poller_handler_auth_a(MfClassicPoller* instance) {
    MfClassicPollerCommand command = MfClassicPollerCommandContinue;
    MfClassicPollerEvent event = {};

    uint8_t sector = 0;
    if(instance->read_mode == MfClassicReadModeKeyReuse) {
        sector = instance->key_reuse_sector;
    } else {
        sector = instance->sectors_read;
    }

    if(mf_classic_is_key_found(instance->data, sector, MfClassicKeyTypeA)) {
        instance->prev_state = instance->state;
        instance->state = MfClassicPollerStateAuthKeyB;
    } else {
        uint8_t block = mf_classic_get_first_block_num_of_sector(sector);
        uint64_t key = nfc_util_bytes2num(instance->current_key.data, 6);
        FURI_LOG_D(TAG, "Auth to block %d with key A: %06llx", block, key);

        MfClassicError error = mf_classic_async_auth(
            instance, block, &instance->current_key, MfClassicKeyTypeA, NULL);
        if(error == MfClassicErrorNone) {
            FURI_LOG_I(TAG, "Key A found");
            mf_classic_set_key_found(instance->data, sector, MfClassicKeyTypeA, key);
            if(instance->read_mode != MfClassicReadModeKeyReuse) {
                if(sector < instance->sectors_total - 1) {
                    instance->read_mode = MfClassicReadModeKeyReuse;
                    instance->key_reuse_sector = sector;
                    event.type = MfClassicPollerEventTypeKeyAttackStart;
                    instance->event_data.key_attack_data.start_sector = instance->key_reuse_sector;
                    event.data = &instance->event_data;
                    command = instance->callback(event, instance->context);
                }
            }
            event.type = MfClassicPollerEventTypeFoundKeyA;
            command = instance->callback(event, instance->context);
            instance->prev_state = instance->state;
            instance->state = MfClassicPollerStateReadSector;
        } else {
            if(instance->read_mode == MfClassicReadModeKeyReuse) {
                instance->state = MfClassicPollerStateAuthKeyB;
            } else {
                instance->state = MfClassicPollerStateRequestKey;
            }
        }
    }

    return command;
}

MfClassicPollerCommand mf_classic_poller_handler_auth_b(MfClassicPoller* instance) {
    MfClassicPollerCommand command = MfClassicPollerCommandContinue;
    MfClassicPollerEvent event = {};

    uint8_t sector = 0;
    if(instance->read_mode == MfClassicReadModeKeyReuse) {
        sector = instance->key_reuse_sector;
    } else {
        sector = instance->sectors_read;
    }

    if(mf_classic_is_key_found(instance->data, sector, MfClassicKeyTypeB)) {
        instance->state = MfClassicPollerStateNewSector;
    } else {
        uint8_t block = mf_classic_get_first_block_num_of_sector(sector);
        uint64_t key = nfc_util_bytes2num(instance->current_key.data, 6);
        FURI_LOG_D(TAG, "Auth to block %d with key B: %06llx", block, key);

        MfClassicError error = mf_classic_async_auth(
            instance, block, &instance->current_key, MfClassicKeyTypeB, NULL);
        if(error == MfClassicErrorNone) {
            FURI_LOG_D(TAG, "Key B found");
            event.type = MfClassicPollerEventTypeFoundKeyB;
            mf_classic_set_key_found(instance->data, sector, MfClassicKeyTypeB, key);
            if(instance->read_mode != MfClassicReadModeKeyReuse) {
                if(sector < instance->sectors_total - 1) {
                    instance->read_mode = MfClassicReadModeKeyReuse;
                    instance->key_reuse_sector = sector;

                    event.type = MfClassicPollerEventTypeKeyAttackStart;
                    instance->event_data.key_attack_data.start_sector = instance->key_reuse_sector;
                    event.data = &instance->event_data;
                    command = instance->callback(event, instance->context);
                }
            }
            command = instance->callback(event, instance->context);
            instance->state = MfClassicPollerStateReadSector;
        } else {
            if(instance->read_mode == MfClassicReadModeKeyReuse) {
                instance->state = MfClassicPollerStateNewSector;
            } else {
                instance->state = MfClassicPollerStateRequestKey;
            }
        }
    }
    instance->prev_state = instance->state;

    return command;
}

MfClassicPollerCommand mf_classic_poller_handler_read_sector(MfClassicPoller* instance) {
    MfClassicPollerCommand command = MfClassicPollerCommandContinue;

    uint8_t block_start = mf_classic_get_first_block_num_of_sector(instance->sectors_read);
    uint8_t total_blocks = mf_classic_get_blocks_num_in_sector(instance->sectors_read);
    MfClassicBlock block = {};

    for(size_t i = block_start; i < block_start + total_blocks; i++) {
        FURI_LOG_D(TAG, "Reding block %d", i);
        if(mf_classic_is_block_read(instance->data, i)) continue;
        MfClassicError error = mf_classic_async_read_block(instance, i, &block);
        if(error == MfClassicErrorNone) {
            mf_classic_set_block_read(instance->data, i, &block);
        } else {
            FURI_LOG_D(TAG, "Failed to read %d block", i);
            break;
        }
    }

    instance->prev_state = instance->state;
    if(instance->prev_state == MfClassicPollerStateAuthKeyA) {
        instance->state = MfClassicPollerStateAuthKeyB;
    } else {
        instance->state = MfClassicPollerStateNewSector;
    }

    return command;
}

MfClassicPollerCommand mf_classic_poller_handler_read_complete(MfClassicPoller* instance) {
    MfClassicPollerCommand command = MfClassicPollerCommandContinue;
    MfClassicPollerEvent event = {.type = MfClassicPollerEventTypeReadComplete};
    command = instance->callback(event, instance->context);
    if(command == MfClassicPollerCommandRestart) {
        instance->state = MfClassicPollerStateStart;
    }

    return command;
}

static const MfClassicPollerReadHandler
    mf_classic_poller_dict_attack_handler[MfClassicPollerStateNum] = {
        [MfClassicPollerStateIdle] = mf_classic_poller_handler_idle,
        [MfClassicPollerStateStart] = mf_classic_poller_handler_start,
        [MfClassicPollerStateNewSector] = mf_classic_poller_handler_new_sector,
        [MfClassicPollerStateRequestKey] = mf_classic_poller_handler_request_key,
        [MfClassicPollerStateAuthKeyA] = mf_classic_poller_handler_auth_a,
        [MfClassicPollerStateAuthKeyB] = mf_classic_poller_handler_auth_b,
        [MfClassicPollerStateReadSector] = mf_classic_poller_handler_read_sector,
        [MfClassicPollerStateReadComplete] = mf_classic_poller_handler_read_complete,
};

NfcaPollerCommand mf_classic_dict_attack_callback(NfcaPollerEvent event, void* context) {
    furi_assert(context);
    MfClassicPoller* instance = context;
    furi_assert(instance->callback);
    MfClassicPollerCommand command = MfClassicPollerCommandContinue;
    MfClassicPollerEvent mfc_event = {};

    furi_assert(instance->session_state != MfClassicPollerSessionStateIdle);

    if(instance->session_state == MfClassicPollerSessionStateStopRequest) {
        command = MfClassicPollerCommandStop;
    } else {
        if(event.type == NfcaPollerEventTypeReady) {
            command = mf_classic_poller_dict_attack_handler[instance->state](instance);
        } else if(event.type == NfcaPollerEventTypeError) {
            if(event.data.error == NfcaErrorNotPresent) {
                if(instance->card_state == MfClassicCardStateDetected) {
                    instance->card_state = MfClassicCardStateNotDetected;
                    mfc_event.type = MfClassicPollerEventTypeCardNotDetected;
                    command = instance->callback(mfc_event, instance->context);
                    instance->state = MfClassicPollerStateIdle;
                }
            }
        }
    }

    return mf_classic_process_command(command);
}

MfClassicError mf_classic_poller_dict_attack(
    MfClassicPoller* instance,
    MfClassicPollerCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(callback);

    instance->callback = callback;
    instance->context = context;
    instance->prev_state = MfClassicPollerStateStart;
    instance->state = MfClassicPollerStateIdle;

    return mf_classic_poller_start(instance, mf_classic_dict_attack_callback, instance);
}

MfClassicError mf_classic_poller_reset(MfClassicPoller* instance) {
    furi_assert(instance);
    furi_assert(instance->data);
    furi_assert(instance->nfca_poller);
    furi_assert(instance->tx_plain_buffer);
    furi_assert(instance->rx_plain_buffer);
    furi_assert(instance->tx_encrypted_buffer);
    furi_assert(instance->rx_encrypted_buffer);

    instance->auth_state = MfClassicAuthStateIdle;
    crypto1_free(instance->crypto);
    bit_buffer_free(instance->tx_plain_buffer);
    bit_buffer_free(instance->rx_plain_buffer);
    bit_buffer_free(instance->tx_encrypted_buffer);
    bit_buffer_free(instance->rx_encrypted_buffer);
    instance->callback = NULL;
    instance->context = NULL;

    return MfClassicErrorNone;
}

MfClassicError mf_classic_poller_stop(MfClassicPoller* instance) {
    furi_assert(instance);
    furi_assert(instance->nfca_poller);

    instance->session_state = MfClassicPollerSessionStateStopRequest;
    nfca_poller_stop(instance->nfca_poller);
    instance->session_state = MfClassicPollerSessionStateIdle;
    mf_classic_free(instance->data);

    return mf_classic_poller_reset(instance);
}
