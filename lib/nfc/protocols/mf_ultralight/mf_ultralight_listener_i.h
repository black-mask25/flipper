#pragma once

#include "mf_ultralight_listener.h"
#include <lib/nfc/protocols/iso14443_3a/iso14443_3a_listener.h>
#include <nfc/protocols/nfc_generic_event.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MfUltralightListenerAuthStateIdle,
    MfUltralightListenerAuthStateSuccess,
} MfUltralightListenerAuthState;

typedef enum {
    MfUltraligthListenerStateIdle,
} MfUltraligthListenerState;

typedef enum {
    MfUltralightCommandNotFound,
    MfUltralightCommandProcessed,
    MfUltralightCommandProcessedSilent,
    MfUltralightCommandNotProcessedNAK,
    MfUltralightCommandNotProcessedSilent,
} MfUltralightCommand;

typedef MfUltralightCommand (
    *MfUltralightListenerCommandCallback)(MfUltralightListener* instance, BitBuffer* buf);

typedef uint8_t MfUltralightListenerCompositeCommandData;

typedef struct {
    MfUltralightListenerCompositeCommandData data;
    MfUltralightListenerCommandCallback callback;
} MfUltralightListenerCompositeCommandContext;

typedef struct {
    uint8_t enabled;
    uint8_t ascii_offset;
    uint8_t ascii_end;
    uint8_t mirror_last_page;
    MfUltralightMirrorConf actual_mode;
    FuriString* ascii_mirror_data;
} MfUltralightMirrorMode;

struct MfUltralightListener {
    Iso14443_3aListener* iso14443_3a_listener;
    MfUltralightListenerAuthState auth_state;
    MfUltraligthListenerState state;

    MfUltralightData* data;
    BitBuffer* tx_buffer;
    MfUltralightFeatureSupport features;
    MfUltralightConfigPages* config;

    NfcGenericEvent generic_event;
    MfUltralightListenerEvent mfu_event;
    MfUltralightListenerEventData mfu_event_data;
    NfcGenericCallback callback;
    uint8_t sector;
    bool single_counter_increased;
    MfUltralightMirrorMode mirror;
    MfUltralightListenerCompositeCommandContext composite_cmd;
    void* context;
};

void mf_ultralight_single_counter_try_increase(MfUltralightListener* instance);
void mf_ultralight_single_counter_try_to_unlock(
    MfUltralightListener* instance,
    Iso14443_3aListenerEventType type);

void mf_ultraligt_mirror_prepare_emulation(MfUltralightListener* instance);
void mf_ultralight_mirror_read_prepare(uint8_t start_page, MfUltralightListener* instance);
void mf_ultralight_mirror_read_handler(
    uint8_t mirror_page_num,
    uint8_t* dest,
    MfUltralightListener* instance);

void mf_ultralight_composite_command_set_next(
    MfUltralightListener* instance,
    const MfUltralightListenerCommandCallback handler);
void mf_ultralight_composite_command_reset(MfUltralightListener* instance);
bool mf_ultralight_composite_command_in_progress(MfUltralightListener* instance);
MfUltralightCommand
    mf_ultralight_composite_command_run(MfUltralightListener* instance, BitBuffer* buffer);

bool mf_ultralight_is_i2c_tag(MfUltralightType type);
bool mf_ultralight_i2c_validate_pages(
    uint16_t start_page,
    uint16_t end_page,
    MfUltralightListener* instance);

uint16_t
    mf_ultralight_i2c_provide_page_by_requested(uint16_t page, MfUltralightListener* instance);
#ifdef __cplusplus
}
#endif
