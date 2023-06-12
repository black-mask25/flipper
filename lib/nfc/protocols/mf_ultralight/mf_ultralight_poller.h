#pragma once

#include "mf_ultralight.h"
#include <lib/nfc/protocols/nfca/nfca_poller.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MfUltralightPoller MfUltralightPoller;

typedef enum {
    MfUltralightPollerEventTypeAuthRequest,
    MfUltralightPollerEventTypeAuthSuccess,
    MfUltralightPollerEventTypeAuthFailed,
    MfUltralightPollerEventTypeReadSuccess,
    MfUltralightPollerEventTypeReadFailed,
} MfUltralightPollerEventType;

typedef struct {
    MfUltralightAuthPassword password;
    MfUltralightAuthPack pack;
    bool auth_success;
    bool skip_auth;
} MfUltralightPollerAuthContext;

typedef struct {
    union {
        MfUltralightPollerAuthContext auth_context;
        MfUltralightError error;
    };
} MfUltralightPollerEventData;

typedef struct {
    MfUltralightPollerEventType type;
    MfUltralightPollerEventData* data;
} MfUltralightPollerEvent;

typedef enum {
    MfUltralightPollerCommandContinue = NfcaPollerCommandContinue,
    MfUltralightPollerCommandReset = NfcaPollerCommandReset,
    MfUltralightPollerCommandStop = NfcaPollerCommandStop,
} MfUltralightPollerCommand;

typedef MfUltralightPollerCommand (
    *MfUltralightPollerCallback)(MfUltralightPollerEvent event, void* context);

MfUltralightPoller* mf_ultralight_poller_alloc(NfcaPoller* nfca_poller);

void mf_ultralight_poller_free(MfUltralightPoller* instance);

const MfUltralightData* mf_ultralight_poller_get_data(MfUltralightPoller* instance);

MfUltralightError mf_ultralight_poller_start(
    MfUltralightPoller* instance,
    NfcaPollerEventCallback callback,
    void* context);

MfUltralightError mf_ultralight_poller_read(
    MfUltralightPoller* instance,
    MfUltralightPollerCallback callback,
    void* context);

MfUltralightError mf_ultralight_poller_reset(MfUltralightPoller* instance);

MfUltralightError mf_ultralight_poller_stop(MfUltralightPoller* instance);

// Sync API

MfUltralightError mf_ultralight_poller_read_page(
    MfUltralightPoller* instance,
    uint16_t page,
    MfUltralightPage* data);

MfUltralightError mf_ultralight_poller_write_page(
    MfUltralightPoller* instance,
    uint16_t page,
    MfUltralightPage* data);

MfUltralightError
    mf_ultralight_poller_read_version(MfUltralightPoller* instance, MfUltralightVersion* data);

MfUltralightError
    mf_ultralight_poller_read_signature(MfUltralightPoller* instance, MfUltralightSignature* data);

MfUltralightError mf_ultralight_poller_read_counter(
    MfUltralightPoller* instance,
    uint8_t counter_num,
    MfUltralightCounter* data);

MfUltralightError mf_ultralight_poller_read_tearing_flag(
    MfUltralightPoller* instance,
    uint8_t flag_num,
    MfUltralightTearingFlag* data);

MfUltralightError
    mf_ultralight_poller_read_card(MfUltralightPoller* instance, MfUltralightData* data);

#ifdef __cplusplus
}
#endif
