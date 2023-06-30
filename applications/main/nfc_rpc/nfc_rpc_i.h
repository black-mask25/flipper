#pragma once

#include "nfc_rpc.h"

#include "furi.h"
#include "furi_hal.h"
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <rpc/rpc_app.h>

#include <lib/nanopb/pb_decode.h>
#include <lib/nanopb/pb_encode.h>

#include "assets/compiled/main.pb.h"
#include <m-dict.h>

#include <lib/nfc/nfc.h>
#include <lib/nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <lib/nfc/protocols/iso14443_3a/iso14443_3a_listener.h>
#include <lib/nfc/protocols/mf_ultralight/mf_ultralight_listener.h>

#include <nfc/nfc_listener.h>

typedef void (*NfcRpcHandler)(Nfc_Main* cmd, void* context);

DICT_DEF2(NfcRpcHandlerDict, pb_size_t, M_DEFAULT_OPLIST, NfcRpcHandler, M_POD_OPLIST)

typedef void (*NfcRpcHandlersAlloc)(void* context);
typedef void (*NfcRpcHandlersFree)(void* context);

typedef enum {
    NfcRpcViewMain,
} NfcRpcView;

typedef enum {
    NfcRpcCustomEventMessageReceived,
} NfcRpcCustomEvent;

typedef enum {
    NfcRpcEventDataExchange,
} NfcRpcEventType;

typedef struct {
    NfcRpcEventType type;
    const uint8_t* data;
    size_t data_size;
} NfcRpcEvent;

struct NfcRpc {
    Gui* gui;
    RpcAppSystem* rpc;
    FuriMessageQueue* queue;
    ViewDispatcher* view_dispatcher;
    View* view;
    NfcRpcHandlerDict_t handlers;

    Nfc* nfc;
    NfcListener* listener;
};

typedef struct {
    NfcRpcHandlersAlloc alloc;
    NfcRpcHandlersFree free;
} NfcRpcCallbacks;

void nfc_rpc_add_handler(NfcRpc* instance, pb_size_t message_tag, NfcRpcHandler handler);

void nfc_rpc_nfca_alloc(void* context);
void nfc_rpc_mf_ultralight_alloc(void* context);
void nfc_rpc_mf_classic_alloc(void* context);
