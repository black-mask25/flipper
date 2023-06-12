#pragma once

#include <lib/nfc/helpers/bit_buffer.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NFCA_MAX_UID_SIZE (10U)

#define NFCA_GUARD_TIME_US (5000)
#define NFCA_FDT_POLL_FC (1620)
#define NFCA_FDT_LISTEN_FC (1172)
#define NFCA_POLLER_MASK_RX_FS ((NFCA_FDT_LISTEN_FC) / 2)
#define NFCA_POLL_POLL_MIN_US (1100)

typedef enum {
    NfcaErrorNone,
    NfcaErrorNotPresent,
    NfcaErrorColResFailed,
    NfcaErrorBufferOverflow,
    NfcaErrorCommunication,
    NfcaErrorFieldOff,
    NfcaErrorWrongCrc,
    NfcaErrorTimeout,
} NfcaError;

typedef struct {
    uint8_t sens_resp[2];
} NfcaSensResp;

typedef struct {
    uint8_t sel_cmd;
    uint8_t sel_par;
    uint8_t data[4]; // max data bit is 32
} NfcaSddReq;

typedef struct {
    uint8_t nfcid[4];
    uint8_t bss;
} NfcaSddResp;

typedef struct {
    uint8_t sel_cmd;
    uint8_t sel_par;
    uint8_t nfcid[4];
    uint8_t bcc;
} NfcaSelReq;

typedef struct {
    uint8_t sak;
} NfcaSelResp;

typedef struct {
    uint8_t sak;
} NfcaRats;

typedef struct {
    uint8_t uid[NFCA_MAX_UID_SIZE];
    uint8_t uid_len;
    uint8_t atqa[2];
    uint8_t sak;
    NfcaRats rats;
} NfcaData;

uint32_t nfca_get_cuid(NfcaData* nfca_data);

void nfca_append_crc(BitBuffer* buffer);

bool nfca_check_crc(const BitBuffer* buf);

void nfca_trim_crc(BitBuffer* buf);

#ifdef __cplusplus
}
#endif
