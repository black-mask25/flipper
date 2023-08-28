#include "../nfc_app_i.h"

#include <dolphin/dolphin.h>
#include <lib/nfc/protocols/mf_classic/mf_classic_poller.h>

#define TAG "NfcMfClassicDictAttack"

typedef enum {
    DictAttackStateUserDictInProgress,
    DictAttackStateSystemDictInProgress,
} DictAttackState;

NfcCommand nfc_dict_attack_worker_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.data);
    furi_assert(event.instance);
    furi_assert(event.protocol == NfcProtocolMfClassic);

    NfcCommand command = NfcCommandContinue;
    MfClassicPollerEvent* mfc_event = event.data;

    NfcApp* instance = context;
    if(mfc_event->type == MfClassicPollerEventTypeCardDetected) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, NfcCustomEventCardDetected);
    } else if(mfc_event->type == MfClassicPollerEventTypeCardLost) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, NfcCustomEventCardLost);
    } else if(mfc_event->type == MfClassicPollerEventTypeRequestMode) {
        const MfClassicData* mfc_data =
            nfc_device_get_data(instance->nfc_device, NfcProtocolMfClassic);
        mfc_event->data->poller_mode.mode = MfClassicPollerModeDictAttack;
        mfc_event->data->poller_mode.data = mfc_data;
        instance->mf_dict_context.sectors_total = mf_classic_get_total_sectors_num(mfc_data->type);
        mf_classic_get_read_sectors_and_keys(
            mfc_data,
            &instance->mf_dict_context.sectors_read,
            &instance->mf_dict_context.keys_found);
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcCustomEventDictAttackDataUpdate);
    } else if(mfc_event->type == MfClassicPollerEventTypeRequestKey) {
        MfClassicKey key = {};
        if(mf_dict_get_next_key(instance->mf_dict_context.dict, &key)) {
            mfc_event->data->key_request_data.key = key;
            mfc_event->data->key_request_data.key_provided = true;
            instance->mf_dict_context.dict_keys_current++;
            if(instance->mf_dict_context.dict_keys_current % 10 == 0) {
                view_dispatcher_send_custom_event(
                    instance->view_dispatcher, NfcCustomEventDictAttackDataUpdate);
            }
        } else {
            mfc_event->data->key_request_data.key_provided = false;
        }
    } else if(mfc_event->type == MfClassicPollerEventTypeDataUpdate) {
        MfClassicPollerEventDataUpdate* data_update = &mfc_event->data->data_update;
        instance->mf_dict_context.sectors_read = data_update->sectors_read;
        instance->mf_dict_context.keys_found = data_update->keys_found;
        instance->mf_dict_context.current_sector = data_update->current_sector;
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcCustomEventDictAttackDataUpdate);
    } else if(mfc_event->type == MfClassicPollerEventTypeNextSector) {
        mf_dict_rewind(instance->mf_dict_context.dict);
        instance->mf_dict_context.dict_keys_current = 0;
        instance->mf_dict_context.current_sector =
            mfc_event->data->next_sector_data.current_sector;
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcCustomEventDictAttackDataUpdate);
    } else if(mfc_event->type == MfClassicPollerEventTypeFoundKeyA) {
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcCustomEventDictAttackDataUpdate);
    } else if(mfc_event->type == MfClassicPollerEventTypeFoundKeyB) {
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcCustomEventDictAttackDataUpdate);
    } else if(mfc_event->type == MfClassicPollerEventTypeKeyAttackStart) {
        instance->mf_dict_context.key_attack_current_sector =
            mfc_event->data->key_attack_data.current_sector;
        instance->mf_dict_context.is_key_attack = true;
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcCustomEventDictAttackDataUpdate);
    } else if(mfc_event->type == MfClassicPollerEventTypeKeyAttackStop) {
        mf_dict_rewind(instance->mf_dict_context.dict);
        instance->mf_dict_context.is_key_attack = false;
        instance->mf_dict_context.dict_keys_current = 0;
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcCustomEventDictAttackDataUpdate);
    } else if(mfc_event->type == MfClassicPollerEventTypeSuccess) {
        const MfClassicData* mfc_data = nfc_poller_get_data(instance->poller);
        nfc_device_set_data(instance->nfc_device, NfcProtocolMfClassic, mfc_data);
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcCustomEventDictAttackComplete);
        command = NfcCommandStop;
    }

    return command;
}

void nfc_dict_attack_dict_attack_result_callback(DictAttackEvent event, void* context) {
    furi_assert(context);
    NfcApp* instance = context;

    if(event == DictAttackEventSkipPressed) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, NfcCustomEventDictAttackSkip);
    }
}

static void nfc_scene_mf_classic_dict_attack_update_view(NfcApp* instance) {
    NfcMfClassicDictAttackContext* mfc_dict = &instance->mf_dict_context;

    if(mfc_dict->is_key_attack) {
        dict_attack_set_key_attack(instance->dict_attack, mfc_dict->key_attack_current_sector);
    } else {
        dict_attack_reset_key_attack(instance->dict_attack);
        dict_attack_set_sectors_total(instance->dict_attack, mfc_dict->sectors_total);
        dict_attack_set_sectors_read(instance->dict_attack, mfc_dict->sectors_read);
        dict_attack_set_keys_found(instance->dict_attack, mfc_dict->keys_found);
        dict_attack_set_current_dict_key(instance->dict_attack, mfc_dict->dict_keys_current);
        dict_attack_set_current_sector(instance->dict_attack, mfc_dict->current_sector);
    }
}

static void nfc_scene_mf_classic_dict_attack_prepare_view(NfcApp* instance) {
    uint32_t state =
        scene_manager_get_scene_state(instance->scene_manager, NfcSceneMfClassicDictAttack);
    if(state == DictAttackStateUserDictInProgress) {
        do {
            if(!mf_dict_check_presence(MfDictTypeUser)) {
                state = DictAttackStateSystemDictInProgress;
                break;
            }

            instance->mf_dict_context.dict = mf_dict_alloc(MfDictTypeUser);
            if(mf_dict_get_total_keys(instance->mf_dict_context.dict) == 0) {
                mf_dict_free(instance->mf_dict_context.dict);
                state = DictAttackStateSystemDictInProgress;
                break;
            }

            dict_attack_set_header(instance->dict_attack, "MF Classic User Dictionary");
        } while(false);
    }
    if(state == DictAttackStateSystemDictInProgress) {
        instance->mf_dict_context.dict = mf_dict_alloc(MfDictTypeSystem);
        dict_attack_set_header(instance->dict_attack, "MF Classic System Dictionary");
    }

    instance->mf_dict_context.dict_keys_total =
        mf_dict_get_total_keys(instance->mf_dict_context.dict);
    dict_attack_set_total_dict_keys(
        instance->dict_attack, instance->mf_dict_context.dict_keys_total);
    instance->mf_dict_context.dict_keys_current = 0;

    dict_attack_set_callback(
        instance->dict_attack, nfc_dict_attack_dict_attack_result_callback, instance);
    nfc_scene_mf_classic_dict_attack_update_view(instance);

    scene_manager_set_scene_state(instance->scene_manager, NfcSceneMfClassicDictAttack, state);
}

void nfc_scene_mf_classic_dict_attack_on_enter(void* context) {
    NfcApp* instance = context;

    scene_manager_set_scene_state(
        instance->scene_manager, NfcSceneMfClassicDictAttack, DictAttackStateUserDictInProgress);
    nfc_scene_mf_classic_dict_attack_prepare_view(instance);
    dict_attack_set_card_state(instance->dict_attack, true);
    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcViewDictAttack);
    nfc_blink_read_start(instance);
    notification_message(instance->notifications, &sequence_display_backlight_enforce_on);

    instance->poller = nfc_poller_alloc(instance->nfc, NfcProtocolMfClassic);
    nfc_poller_start(instance->poller, nfc_dict_attack_worker_callback, instance);
}

bool nfc_scene_mf_classic_dict_attack_on_event(void* context, SceneManagerEvent event) {
    NfcApp* instance = context;
    bool consumed = false;

    uint32_t state =
        scene_manager_get_scene_state(instance->scene_manager, NfcSceneMfClassicDictAttack);
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcCustomEventDictAttackComplete) {
            if(state == DictAttackStateUserDictInProgress) {
                nfc_poller_stop(instance->poller);
                nfc_poller_free(instance->poller);
                mf_dict_free(instance->mf_dict_context.dict);
                scene_manager_set_scene_state(
                    instance->scene_manager,
                    NfcSceneMfClassicDictAttack,
                    DictAttackStateSystemDictInProgress);
                nfc_scene_mf_classic_dict_attack_prepare_view(instance);
                instance->poller = nfc_poller_alloc(instance->nfc, NfcProtocolMfClassic);
                nfc_poller_start(instance->poller, nfc_dict_attack_worker_callback, instance);
                consumed = true;
            } else {
                notification_message(instance->notifications, &sequence_success);
                scene_manager_next_scene(instance->scene_manager, NfcSceneReadSuccess);
                dolphin_deed(DolphinDeedNfcReadSuccess);
                consumed = true;
            }
        } else if(event.event == NfcCustomEventCardDetected) {
            dict_attack_set_card_state(instance->dict_attack, true);
            consumed = true;
        } else if(event.event == NfcCustomEventCardLost) {
            dict_attack_set_card_state(instance->dict_attack, false);
            consumed = true;
        } else if(event.event == NfcCustomEventDictAttackDataUpdate) {
            nfc_scene_mf_classic_dict_attack_update_view(instance);
        } else if(event.event == NfcCustomEventDictAttackSkip) {
            const MfClassicData* mfc_data = nfc_poller_get_data(instance->poller);
            nfc_device_set_data(instance->nfc_device, NfcProtocolMfClassic, mfc_data);
            if(state == DictAttackStateUserDictInProgress) {
                nfc_poller_stop(instance->poller);
                nfc_poller_free(instance->poller);
                mf_dict_free(instance->mf_dict_context.dict);
                scene_manager_set_scene_state(
                    instance->scene_manager,
                    NfcSceneMfClassicDictAttack,
                    DictAttackStateSystemDictInProgress);
                nfc_scene_mf_classic_dict_attack_prepare_view(instance);
                instance->poller = nfc_poller_alloc(instance->nfc, NfcProtocolMfClassic);
                nfc_poller_start(instance->poller, nfc_dict_attack_worker_callback, instance);
                consumed = true;
            } else if(state == DictAttackStateSystemDictInProgress) {
                notification_message(instance->notifications, &sequence_success);
                scene_manager_next_scene(instance->scene_manager, NfcSceneReadSuccess);
                dolphin_deed(DolphinDeedNfcReadSuccess);
                consumed = true;
            }
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_next_scene(instance->scene_manager, NfcSceneExitConfirm);
        consumed = true;
    }
    return consumed;
}

void nfc_scene_mf_classic_dict_attack_on_exit(void* context) {
    NfcApp* instance = context;

    nfc_poller_stop(instance->poller);
    nfc_poller_free(instance->poller);

    dict_attack_reset(instance->dict_attack);
    scene_manager_set_scene_state(
        instance->scene_manager, NfcSceneMfClassicDictAttack, DictAttackStateUserDictInProgress);

    mf_dict_free(instance->mf_dict_context.dict);

    instance->mf_dict_context.current_sector = 0;
    instance->mf_dict_context.sectors_total = 0;
    instance->mf_dict_context.sectors_read = 0;
    instance->mf_dict_context.current_sector = 0;
    instance->mf_dict_context.keys_found = 0;
    instance->mf_dict_context.dict_keys_total = 0;
    instance->mf_dict_context.dict_keys_current = 0;
    instance->mf_dict_context.is_key_attack = false;
    instance->mf_dict_context.key_attack_current_sector = 0;

    nfc_blink_stop(instance);
    notification_message(instance->notifications, &sequence_display_backlight_enforce_auto);
}
