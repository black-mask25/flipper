#pragma once

#include "mf_classic.h"
#include <lib/nfc/protocols/nfca/nfca_poller.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MfClassicPoller MfClassicPoller;

typedef enum {
    MfClassicPollerEventTypeStart,
    MfClassicPollerEventTypeRequestKey,
    MfClassicPollerEventTypeNewSector,
    MfClassicPollerEventTypeFoundKeyA,
    MfClassicPollerEventTypeFoundKeyB,
    MfClassicPollerEventTypeCardDetected,
    MfClassicPollerEventTypeCardNotDetected,
    MfClassicPollerEventTypeKeyAttackStart,
    MfClassicPollerEventTypeKeyAttackStop,
    MfClassicPollerEventTypeKeyAttackNextSector,
    MfClassicPollerEventTypeReadComplete,
} MfClassicPollerEventType;

typedef struct {
    MfClassicType type;
} MfClassicPollerEventDataStart;

typedef struct {
    uint8_t sector_num;
    MfClassicKey key;
    bool key_provided;
} MfClassicPollerEventDataKeyRequest;

typedef struct {
    uint8_t start_sector;
} MfClassicPollerEventKeyAttackData;

typedef union {
    MfClassicError error;
    MfClassicPollerEventDataStart start_data;
    MfClassicPollerEventDataKeyRequest key_request_data;
    MfClassicPollerEventKeyAttackData key_attack_data;
} MfClassicPollerEventData;

typedef struct {
    MfClassicPollerEventType type;
    MfClassicPollerEventData* data;
} MfClassicPollerEvent;

#ifdef __cplusplus
}
#endif
