#include "nfcb_poller.h"

#include <furi.h>

#define NFCB_POLLER_BUFER_MAX_SIZE (512)

typedef enum {
    NfcbPollerStateIdle,
    NfcbPollerColResInProgress,
    NfcbPollerColResFailed,
    NfcbPollerActivated,
} NfcbPollerState;

struct NfcbPoller {
    Nfc* nfc;
    NfcbPollerState state;
    NfcbData* data;
    NfcbPollerEventCallback callback;
    void* context;
};

NfcbPoller* nfcb_poller_alloc(Nfc* nfc) {
    NfcbPoller* instance = malloc(sizeof(NfcbPoller));
    instance->nfc = nfc;

    return instance;
}

void nfcb_poller_free(NfcbPoller* instance) {
    furi_assert(instance);

    free(instance);
}

static NfcCommand nfcb_poller_event_callback(NfcEvent event, void* context) {
    furi_assert(context);
    UNUSED(event);

    NfcbPoller* instance = context;
    furi_assert(instance->callback);

    return NfcCommandContinue;
}

NfcbError
    nfcb_poller_start(NfcbPoller* instance, NfcbPollerEventCallback callback, void* context) {
    furi_assert(instance);

    instance->callback = callback;
    instance->context = context;

    nfc_start_poller(instance->nfc, nfcb_poller_event_callback, instance);
    return NfcbErrorNone;
}

NfcbError nfcb_poller_get_data(NfcbPoller* instance, NfcbData* data) {
    furi_assert(instance);
    furi_assert(instance->data);

    *data = *instance->data;
    return NfcbErrorNone;
}

NfcbError nfcb_poller_reset(NfcbPoller* instance) {
    furi_assert(instance);
    furi_assert(instance->nfc);
    furi_assert(instance->data);

    nfc_stop(instance->nfc);

    instance->callback = NULL;
    instance->context = NULL;
    free(instance->data);
    instance->data = NULL;

    return NfcbErrorNone;
}

NfcbError nfcb_poller_config(NfcbPoller* instance) {
    furi_assert(instance);

    // instance->data = malloc(sizeof(NfcbData));

    nfc_config(instance->nfc, NfcModeNfcbPoller);
    nfc_set_guard_time_us(instance->nfc, NFCB_GUARD_TIME_US);
    nfc_set_fdt_poll_fc(instance->nfc, NFCB_FDT_POLL_FC);
    nfc_set_fdt_poll_poll_us(instance->nfc, NFCB_POLL_POLL_MIN_US);

    return NfcbErrorNone;
}

NfcbError nfcb_poller_activate(NfcbPoller* instance, NfcbData* data) {
    furi_assert(instance);
    furi_assert(data);

    return NfcbErrorTimeout;
}
