#include "nfc_listener.h"

#include <nfc/protocols/nfc_listener_defs.h>

#include <furi.h>

typedef struct NfcListenerListElement {
    NfcProtocol protocol;
    NfcGenericInstance* listener;
    const NfcListenerBase* listener_api;
    struct NfcListenerListElement* child;
    struct NfcListenerListElement* parent;
} NfcListenerListElement;

typedef struct {
    NfcListenerListElement* head;
    NfcListenerListElement* tail;
} NfcListenerList;

struct NfcListener {
    NfcProtocol protocol;
    Nfc* nfc;
    NfcListenerList list;
};

static void nfc_listener_list_alloc(NfcListener* instance, NfcDeviceData* data) {
    instance->list.head = malloc(sizeof(NfcListenerListElement));
    instance->list.head->protocol = instance->protocol;
    instance->list.head->listener_api = nfc_listeners_api[instance->protocol];
    instance->list.head->child = NULL;
    instance->list.tail = instance->list.head;

    // Build linked list
    do {
        NfcProtocol parent_protocol = nfc_protocol_get_parent(instance->list.head->protocol);
        if(parent_protocol == NfcProtocolInvalid) break;

        NfcListenerListElement* parent = malloc(sizeof(NfcListenerListElement));
        parent->protocol = parent_protocol;
        parent->listener_api = nfc_listeners_api[parent_protocol];
        parent->child = instance->list.head;
        instance->list.head->parent = parent;

        instance->list.head = parent;
    } while(true);

    // Allocate listener instances
    NfcListenerListElement* iter = instance->list.head;
    iter->listener = iter->listener_api->alloc(instance->nfc);

    do {
        if(iter->child == NULL) break;
        iter->child->listener = iter->child->listener_api->alloc(iter->listener);
        iter->listener_api->set_callback(
            iter->listener, iter->child->listener_api->run, iter->child->listener);

        iter = iter->child;
    } while(true);

    // Set data for each listener
    iter = instance->list.tail;
    iter->listener_api->set_data(iter->listener, data);

    do {
        if(iter == instance->list.head) break;

        const NfcDeviceData* base_data = iter->listener_api->get_base_data(iter->listener);
        iter->parent->listener_api->set_data(iter->parent->listener, base_data);
        iter = iter->parent;
    } while(false);
}

static void nfc_listener_list_free(NfcListener* instance) {
    do {
        instance->list.head->listener_api->free(instance->list.head->listener);
        NfcListenerListElement* child = instance->list.head->child;
        free(instance->list.head);
        if(child == NULL) break;
        instance->list.head = child;
    } while(true);
}

NfcListener* nfc_listener_alloc(Nfc* nfc, NfcProtocol protocol, NfcDeviceData* data) {
    furi_assert(nfc);
    furi_assert(protocol < NfcProtocolNum);
    furi_assert(data);

    NfcListener* instance = malloc(sizeof(NfcListener));
    instance->nfc = nfc;
    instance->protocol = protocol;
    nfc_listener_list_alloc(instance, data);

    return instance;
}

void nfc_listener_free(NfcListener* instance) {
    furi_assert(instance);

    nfc_listener_list_free(instance);
    free(instance);
}

NfcCommand nfc_listener_start_callback(NfcEvent event, void* context) {
    furi_assert(context);

    NfcListener* instance = context;
    furi_assert(instance->list.head);

    NfcCommand command = NfcCommandContinue;
    NfcGenericEvent generic_event = {
        .protocol = NfcProtocolInvalid,
        .poller = instance->nfc,
        .data = &event,
    };

    NfcListenerListElement* head_listener = instance->list.head;
    command = head_listener->listener_api->run(generic_event, head_listener->listener);

    return command;
}

void nfc_listener_start(NfcListener* instance) {
    furi_assert(instance);

    nfc_start_listener(instance->nfc, nfc_listener_start_callback, instance);
}

void nfc_listener_stop(NfcListener* instance) {
    furi_assert(instance);

    nfc_listener_abort(instance->nfc);
}

const NfcDeviceData* nfc_listener_get_data(NfcListener* instance) {
    furi_assert(instance);

    NfcListenerListElement* tail_element = instance->list.tail;
    return tail_element->listener_api->get_base_data(tail_element->listener);
}
