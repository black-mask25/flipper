#include "nfca_poller_i.h"

#include <furi.h>

#define TAG "NFCA"

static NfcaError nfca_poller_process_error(NfcError error) {
    NfcaError ret = NfcaErrorNone;
    if(error == NfcErrorNone) {
        ret = NfcaErrorNone;
    } else if(error == NfcErrorTimeout) {
        ret = NfcaErrorTimeout;
    } else {
        ret = NfcaErrorNotPresent;
    }
    return ret;
}

static NfcaError nfca_poller_prepare_trx(NfcaPoller* instance) {
    NfcaError ret = NfcaErrorNone;

    if(instance->state == NfcaPollerStateIdle) {
        ret = nfca_poller_async_activate(instance, NULL);
    }

    return ret;
}

static NfcaError nfca_poller_standart_frame_exchange(
    NfcaPoller* instance,
    const BitBuffer* tx_buffer,
    BitBuffer* rx_buffer,
    uint32_t fwt) {
    furi_assert(instance);
    furi_assert(tx_buffer);
    furi_assert(rx_buffer);

    uint16_t tx_bytes = bit_buffer_get_size_bytes(tx_buffer);
    furi_assert(tx_bytes <= bit_buffer_get_capacity_bytes(instance->tx_buffer) - 2);

    bit_buffer_copy(instance->tx_buffer, tx_buffer);
    nfca_append_crc(instance->tx_buffer);
    NfcaError ret = NfcaErrorNone;

    do {
        NfcError error = nfc_trx(instance->nfc, instance->tx_buffer, instance->rx_buffer, fwt);
        if(error != NfcErrorNone) {
            ret = nfca_poller_process_error(error);
            break;
        }

        bit_buffer_copy(rx_buffer, instance->rx_buffer);
        if(!nfca_check_crc(instance->rx_buffer)) {
            ret = NfcaErrorWrongCrc;
            break;
        }

        nfca_trim_crc(rx_buffer);
    } while(false);

    return ret;
}

NfcaError nfca_poller_config(NfcaPoller* instance) {
    furi_assert(instance);
    furi_assert(instance->tx_buffer == NULL);
    furi_assert(instance->rx_buffer == NULL);

    instance->tx_buffer = bit_buffer_alloc(NFCA_POLLER_MAX_BUFFER_SIZE);
    instance->rx_buffer = bit_buffer_alloc(NFCA_POLLER_MAX_BUFFER_SIZE);

    nfc_config(instance->nfc, NfcModeNfcaPoller);
    nfc_set_guard_time_us(instance->nfc, NFCA_GUARD_TIME_US);
    nfc_set_fdt_poll_fc(instance->nfc, NFCA_FDT_POLL_FC);
    nfc_set_fdt_poll_poll_us(instance->nfc, NFCA_POLL_POLL_MIN_US);
    instance->config_state = NfcaPollerConfigStateDone;

    return NfcaErrorNone;
}

NfcaError nfca_poller_reset(NfcaPoller* instance) {
    furi_assert(instance);
    furi_assert(instance->tx_buffer);
    furi_assert(instance->rx_buffer);

    instance->callback = NULL;
    instance->context = NULL;
    memset(&instance->col_res, 0, sizeof(NfcaPollerColRes));

    bit_buffer_free(instance->tx_buffer);
    instance->tx_buffer = NULL;
    bit_buffer_free(instance->rx_buffer);
    instance->rx_buffer = NULL;

    instance->config_state = NfcaPollerConfigStateIdle;
    instance->state = NfcaPollerStateIdle;

    return NfcaErrorNone;
}

NfcaError nfca_poller_check_presence(NfcaPoller* instance) {
    furi_assert(instance);
    furi_assert(instance->nfc);

    NfcError error = NfcErrorNone;
    NfcaError ret = NfcaErrorNone;
    do {
        error = nfc_iso13444a_short_frame(
            instance->nfc, NfcIso14443aShortFrameSensReq, instance->rx_buffer, NFCA_FDT_LISTEN_FC);
        if(error != NfcErrorNone) {
            ret = nfca_poller_process_error(error);
            break;
        }
        if(bit_buffer_get_size_bytes(instance->rx_buffer) != sizeof(instance->col_res.sens_resp)) {
            ret = NfcaErrorCommunication;
            break;
        }
    } while(false);

    return ret;
}

NfcaError nfca_poller_halt(NfcaPoller* instance) {
    furi_assert(instance);
    furi_assert(instance->nfc);
    furi_assert(instance->tx_buffer);

    uint8_t halt_cmd[2] = {0x50, 0x00};
    bit_buffer_copy_bytes(instance->tx_buffer, halt_cmd, sizeof(halt_cmd));

    nfca_poller_standart_frame_exchange(
        instance, instance->tx_buffer, instance->rx_buffer, NFCA_FDT_LISTEN_FC);

    instance->state = NfcaPollerStateIdle;
    return NfcaErrorNone;
}

NfcaError nfca_poller_async_activate(NfcaPoller* instance, NfcaData* nfca_data) {
    furi_assert(instance);
    furi_assert(instance->nfc);
    furi_assert(instance->tx_buffer);
    furi_assert(instance->rx_buffer);

    // Reset Nfca poller state
    memset(&instance->col_res, 0, sizeof(instance->col_res));
    memset(instance->data, 0, sizeof(NfcaData));
    bit_buffer_reset(instance->tx_buffer);
    bit_buffer_reset(instance->rx_buffer);

    // Halt if necessary
    if(instance->state != NfcaPollerStateIdle) {
        nfca_poller_halt(instance);
        instance->state = NfcaPollerStateIdle;
    }

    NfcError error = NfcErrorNone;
    NfcaError ret = NfcaErrorNone;

    bool activated = false;
    do {
        error = nfc_iso13444a_short_frame(
            instance->nfc, NfcIso14443aShortFrameSensReq, instance->rx_buffer, NFCA_FDT_LISTEN_FC);
        if(error != NfcErrorNone) {
            ret = NfcaErrorNotPresent;
            break;
        }
        if(bit_buffer_get_size_bytes(instance->rx_buffer) != sizeof(instance->col_res.sens_resp)) {
            FURI_LOG_W(TAG, "Wrong sens response size");
            ret = NfcaErrorCommunication;
            break;
        }
        bit_buffer_write_bytes(
            instance->rx_buffer,
            &instance->col_res.sens_resp,
            sizeof(instance->col_res.sens_resp));
        memcpy(
            instance->data->atqa,
            &instance->col_res.sens_resp,
            sizeof(instance->col_res.sel_resp));

        instance->state = NfcaPollerStateColResInProgress;
        instance->col_res.cascade_level = 0;
        instance->col_res.state = NfcaPollerColResStateStateNewCascade;

        while(instance->state == NfcaPollerStateColResInProgress) {
            if(instance->col_res.state == NfcaPollerColResStateStateNewCascade) {
                bit_buffer_set_size_bytes(instance->tx_buffer, 2);
                bit_buffer_set_byte(
                    instance->tx_buffer, 0, NFCA_POLLER_SEL_CMD(instance->col_res.cascade_level));
                bit_buffer_set_byte(instance->tx_buffer, 1, NFCA_POLLER_SEL_PAR(2, 0));
                error = nfc_iso13444a_sdd_frame(
                    instance->nfc, instance->tx_buffer, instance->rx_buffer, NFCA_FDT_LISTEN_FC);
                if(error != NfcErrorNone) {
                    FURI_LOG_E(TAG, "Sdd request failed: %d", error);
                    instance->state = NfcaPollerStateColResFailed;
                    ret = NfcaErrorColResFailed;
                    break;
                }
                if(bit_buffer_get_size_bytes(instance->rx_buffer) != 5) {
                    FURI_LOG_E(TAG, "Sdd response wrong length");
                    instance->state = NfcaPollerStateColResFailed;
                    ret = NfcaErrorColResFailed;
                    break;
                }
                // TODO BCC check here
                bit_buffer_write_bytes(
                    instance->rx_buffer, &instance->col_res.sdd_resp, sizeof(NfcaSddResp));
                instance->col_res.state = NfcaPollerColResStateStateSelectCascade;
            } else if(instance->col_res.state == NfcaPollerColResStateStateSelectCascade) {
                instance->col_res.sel_req.sel_cmd =
                    NFCA_POLLER_SEL_CMD(instance->col_res.cascade_level);
                instance->col_res.sel_req.sel_par = NFCA_POLLER_SEL_PAR(7, 0);
                memcpy(
                    instance->col_res.sel_req.nfcid,
                    instance->col_res.sdd_resp.nfcid,
                    sizeof(instance->col_res.sdd_resp.nfcid));
                instance->col_res.sel_req.bcc = instance->col_res.sdd_resp.bss;
                bit_buffer_copy_bytes(
                    instance->tx_buffer,
                    (uint8_t*)&instance->col_res.sel_req,
                    sizeof(instance->col_res.sel_req));
                ret = nfca_poller_send_standart_frame(
                    instance, instance->tx_buffer, instance->rx_buffer, NFCA_FDT_LISTEN_FC);
                if(ret != NfcaErrorNone) {
                    FURI_LOG_E(TAG, "Sel request failed: %d", ret);
                    instance->state = NfcaPollerStateColResFailed;
                    ret = NfcaErrorColResFailed;
                    break;
                }
                if(bit_buffer_get_size_bytes(instance->rx_buffer) !=
                   sizeof(instance->col_res.sel_resp)) {
                    FURI_LOG_E(TAG, "Sel response wrong length");
                    instance->state = NfcaPollerStateColResFailed;
                    ret = NfcaErrorColResFailed;
                    break;
                }
                bit_buffer_write_bytes(
                    instance->rx_buffer,
                    &instance->col_res.sel_resp,
                    sizeof(instance->col_res.sel_resp));
                FURI_LOG_T(TAG, "Sel resp: %02X", instance->col_res.sel_resp.sak);
                if(instance->col_res.sel_req.nfcid[0] == NFCA_POLLER_SDD_CL) {
                    // Copy part of UID
                    memcpy(
                        &instance->data->uid[instance->data->uid_len],
                        &instance->col_res.sel_req.nfcid[1],
                        3);
                    instance->data->uid_len += 3;
                    instance->col_res.cascade_level++;
                    instance->col_res.state = NfcaPollerColResStateStateNewCascade;
                } else {
                    FURI_LOG_T(TAG, "Col resolution complete");
                    instance->data->sak = instance->col_res.sel_resp.sak;
                    memcpy(
                        &instance->data->uid[instance->data->uid_len],
                        &instance->col_res.sel_req.nfcid[0],
                        4);
                    instance->data->uid_len += 4;
                    instance->col_res.state = NfcaPollerColResStateStateSuccess;
                    instance->state = NfcaPollerStateActivated;
                }
            }
        }

        activated = (instance->state == NfcaPollerStateActivated);
    } while(false);

    if(activated && nfca_data) {
        *nfca_data = *instance->data;
    }

    return ret;
}

NfcaError nfca_poller_txrx_custom_parity(
    NfcaPoller* instance,
    const BitBuffer* tx_buffer,
    BitBuffer* rx_buffer,
    uint32_t fwt) {
    furi_assert(instance);
    furi_assert(tx_buffer);
    furi_assert(rx_buffer);

    NfcaError ret = NfcaErrorNone;
    NfcError error = NfcErrorNone;

    do {
        ret = nfca_poller_prepare_trx(instance);
        if(ret != NfcaErrorNone) break;

        error = nfc_trx_custom_parity(instance->nfc, tx_buffer, rx_buffer, fwt);
        if(error != NfcErrorNone) {
            ret = nfca_poller_process_error(error);
            break;
        }
    } while(false);

    return ret;
}

NfcaError nfca_poller_txrx(
    NfcaPoller* instance,
    const BitBuffer* tx_buffer,
    BitBuffer* rx_buffer,
    uint32_t fwt) {
    furi_assert(instance);
    furi_assert(tx_buffer);
    furi_assert(rx_buffer);

    NfcaError ret = NfcaErrorNone;
    NfcError error = NfcErrorNone;

    do {
        ret = nfca_poller_prepare_trx(instance);
        if(ret != NfcaErrorNone) break;

        error = nfc_trx(instance->nfc, tx_buffer, rx_buffer, fwt);
        if(error != NfcErrorNone) {
            ret = nfca_poller_process_error(error);
            break;
        }
    } while(false);

    return ret;
}

NfcaError nfca_poller_send_standart_frame(
    NfcaPoller* instance,
    const BitBuffer* tx_buffer,
    BitBuffer* rx_buffer,
    uint32_t fwt) {
    furi_assert(instance);
    furi_assert(tx_buffer);
    furi_assert(rx_buffer);

    NfcaError ret = NfcaErrorNone;

    do {
        ret = nfca_poller_prepare_trx(instance);
        if(ret != NfcaErrorNone) break;

        ret = nfca_poller_standart_frame_exchange(instance, tx_buffer, rx_buffer, fwt);
        if(ret != NfcaErrorNone) break;
    } while(false);

    return ret;
}
