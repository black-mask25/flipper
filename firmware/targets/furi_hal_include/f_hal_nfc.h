#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define F_HAL_NFC_TIMER_OFFSET_FC (500)

#define F_HAL_NFC_EVENT_WAIT_FOREVER (0xFFFFFFFFU)

typedef enum {
    FHalNfcEventOscOn = (1U << 0),
    FHalNfcEventFieldOn = (1U << 1),
    FHalNfcEventFieldOff = (1U << 2),
    FHalNfcEventListenerActive = (1U << 3),
    FHalNfcEventListenerActiveA = (1U << 4),
    FHalNfcEventTxStart = (1U << 5),
    FHalNfcEventTxEnd = (1U << 6),
    FHalNfcEventRxStart = (1U << 7),
    FHalNfcEventRxEnd = (1U << 8),
    FHalNfcEventCollision = (1U << 9),
    FHalNfcEventTimerFwtExpired = (1U << 10),
    FHalNfcEventTimerBlockTxExpired = (1U << 11),
    FHalNfcEventTimeout = (1U << 12),
    FHalNfcEventAbortRequest = (1U << 13),
} FHalNfcEvent;

typedef enum {
    FHalNfcErrorNone,
    FHalNfcErrorBusy,
    FHalNfcErrorChipCommunication,
    FHalNfcErrorCommunication,
    FHalNfcErrorOscillator,
    FHalNfcErrorIsrTimeout,
    FHalNfcErrorCommunicationTimeout,
    FHalNfcErrorBufferOverflow,
} FHalNfcError;

typedef enum {
    FHalNfcModeIso14443aPoller,
    FHalNfcModeIso14443aListener,

    FHalNfcModeIso14443bPoller,
    FHalNfcModeIso14443bListener,

    FHalNfcModeFelicaPoller,
    FHalNfcModeNfcfListener,

    FHalNfcModeIso15693Poller,
    FHalNfcModeIso15693Listener,

    FHalNfcModeNum,
} FHalNfcMode;

typedef enum {
    FHalNfcBitrate26p48,
    FHalNfcBitrate106,
} FHalNfcBitrate;

typedef enum {
    FHalNfcaShortFrameAllReq,
    FHalNfcaShortFrameSensReq,
} FHalNfcaShortFrame;

/**
 * @brief Nfc HAL initialization
 * 
 * @return FHalNfcError 
 */
FHalNfcError f_hal_nfc_init();

FHalNfcError f_hal_nfc_is_hal_ready();

/**
 * @brief Start Nfc hardware low power mode
 * 
 * @return FHalNfcError 
 */
FHalNfcError f_hal_nfc_low_power_mode_start();

/**
 * @brief Stop Nfc hardware low power mode
 * 
 * @return FHalNfcError 
 */
FHalNfcError f_hal_nfc_low_power_mode_stop();

/**
 * @brief Configure Nfc HAL mode
 * 
 * @param mode          FHalNfcMode instance
 * @param bitrate       FHalNfcBitrate instance
 * 
 * @return FHalNfcError 
 */
FHalNfcError f_hal_nfc_set_mode(FHalNfcMode mode, FHalNfcBitrate bitrate);

FHalNfcError f_hal_nfc_reset_mode();

/**
 * @brief Turn on field in poller mode
 * 
 * @return FHalNfcError 
 */
FHalNfcError f_hal_nfc_poller_field_on();

FHalNfcError f_hal_nfc_acquire();

FHalNfcError f_hal_nfc_release();

FHalNfcError f_hal_nfc_event_start();

FHalNfcError f_hal_nfc_poller_tx(const uint8_t* tx_data, size_t tx_bits);

FHalNfcError f_hal_nfc_poller_tx_custom_parity(const uint8_t* tx_data, size_t tx_bits);

FHalNfcError f_hal_nfc_poller_rx(uint8_t* rx_data, size_t rx_data_size, size_t* rx_bits);

FHalNfcError f_hal_nfc_listener_tx(const uint8_t* tx_data, size_t tx_bits);

FHalNfcError f_hal_nfc_trx_reset();

FHalNfcError f_hal_nfc_listen_start();

FHalNfcError f_hal_nfc_listen_reset();

FHalNfcError f_hal_nfc_listener_sleep();

FHalNfcError f_hal_nfc_listener_disable_auto_col_res();

FHalNfcError f_hal_nfc_abort();

FHalNfcEvent f_hal_nfc_wait_event(uint32_t timeout_ms);

void f_hal_nfc_timer_fwt_start(uint32_t time_fc);

void f_hal_nfc_timer_fwt_stop();

void f_hal_nfc_timer_block_tx_start(uint32_t time_fc);

void f_hal_nfc_timer_block_tx_start_us(uint32_t time_us);

void f_hal_nfc_timer_block_tx_stop();

bool f_hal_nfc_timer_block_tx_is_running();

void f_hal_nfc_set_mask_receive_timer(uint32_t time_fc);

/******************* NFCA specific API *******************/

FHalNfcError f_hal_nfca_send_short_frame(FHalNfcaShortFrame frame);

FHalNfcError f_hal_nfca_send_sdd_frame(const uint8_t* tx_data, size_t tx_bits);

FHalNfcError f_hal_nfca_receive_sdd_frame(uint8_t* rx_data, size_t rx_data_size, size_t* rx_bits);

// TODO virtual methods?
FHalNfcError f_hal_nfca_listener_init();

FHalNfcError f_hal_nfca_listener_deinit();

FHalNfcError
    furi_hal_nfca_set_col_res_data(uint8_t* uid, uint8_t uid_len, uint8_t* atqa, uint8_t sak);

FHalNfcError f_hal_nfca_listener_tx_custom_parity(
    const uint8_t* tx_data,
    const bool* tx_parity,
    size_t tx_bits);

#ifdef __cplusplus
}
#endif
