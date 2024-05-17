#include "felica_listener.h"

#include <nfc/protocols/nfc_generic_event.h>

typedef enum {
    Felica_ListenerStateIdle,
    Felica_ListenerStateActivated,
} FelicaListenerState;

typedef struct {
    uint8_t length;
    FelicaCommandHeader header;
} FelicaListenerGenericRequest;

typedef struct {
    uint8_t length;
    FelicaCommandHeader header;
    FelicaBlockListElement list[];
} FelicaListenerRequest;

typedef FelicaListenerRequest FelicaListenerReadRequest;
typedef FelicaListenerRequest FelicaListenerWriteRequest;

typedef struct {
    FelicaBlockData blocks[2];
} FelicaListenerWriteBlockData;

typedef void (*FelicaCommandWriteBlockHandler)(
    FelicaListener* instance,
    const uint8_t block_number,
    const FelicaBlockData* data_block);

struct FelicaListener {
    Nfc* nfc;
    FelicaData* data;
    FelicaListenerState state;
    FelicaAuthentication auth;
    FelicaBlockData mc_shadow;
    bool rc_written;
    ///TODO: replace bools below woth one single bool operation_needs_mac
    bool write_with_mac;
    bool read_with_mac;
    BitBuffer* tx_buffer;
    BitBuffer* rx_buffer;

    NfcGenericEvent generic_event;
    NfcGenericCallback callback;
    void* context;
};

void felica_wcnt_increment(FelicaData* data);
bool felica_wcnt_check_warning_boundary(const FelicaData* data);
bool felica_wcnt_check_error_boundary(const FelicaData* data);
void felica_wcnt_post_process(FelicaData* data);

uint8_t felica_listener_get_block_index(uint8_t number);
bool felica_block_exists(uint8_t number);
bool felica_get_mc_bit(const FelicaListener* instance, uint8_t byte_index, uint8_t bit_number);
bool felica_block_requires_auth(
    const FelicaListener* instance,
    uint8_t command,
    uint8_t block_number);
bool felica_block_is_readonly(const FelicaListener* instance, uint8_t block_number);
bool felica_block_requires_mac(const FelicaListener* instance, uint8_t block_number);

FelicaCommandWriteBlockHandler felica_listener_get_write_block_handler(uint8_t block_number);

bool felica_listener_validate_write_request_and_set_sf(
    FelicaListener* instance,
    const FelicaListenerWriteRequest* const request,
    const FelicaListenerWriteBlockData* const data,
    FelicaListenerWriteCommandResponse* response);