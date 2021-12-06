#include <furi-hal-bt.h>
#include <ble.h>
#include <stm32wbxx.h>
#include <shci.h>
#include <cmsis_os2.h>

#include <furi-hal-version.h>
#include <furi-hal-bt-hid.h>
#include <furi-hal-bt-serial.h>
#include "battery_service.h"

#include <furi.h>

#define TAG "FuriHalBt"

#define FURI_HAL_BT_DEFAULT_MAC_ADDR {0x6c, 0x7a, 0xd8, 0xac, 0x57, 0x72}

osMutexId_t furi_hal_bt_core2_mtx = NULL;

typedef void (*FuriHalBtProfileStart)(void);
typedef void (*FuriHalBtProfileStop)(void);

typedef struct {
    FuriHalBtProfileStart start;
    FuriHalBtProfileStart stop;
    GapConfig config;
    uint16_t appearance_char;
    uint16_t advertise_service_uuid;
} FuriHalBtProfileConfig;

FuriHalBtProfileConfig profile_config[FuriHalBtProfileNumber] = {
    [FuriHalBtProfileSerial] = {
        .start = furi_hal_bt_serial_start,
        .stop = furi_hal_bt_serial_stop,
        .config = {
            .adv_service_uuid = 0x3080,
            .appearance_char = 0x8600,
            .bonding_mode = true,
            .pairing_method = GapPairingPinCodeShow,
            .mac_address = FURI_HAL_BT_DEFAULT_MAC_ADDR,
        },
    },
    [FuriHalBtProfileHidKeyboard] = {
        .start = furi_hal_bt_hid_start,
        .stop = furi_hal_bt_hid_stop,
        .config = {
            .adv_service_uuid = HUMAN_INTERFACE_DEVICE_SERVICE_UUID,
            .appearance_char = GAP_APPEARANCE_KEYBOARD,
            .bonding_mode = true,
            .pairing_method = GapPairingPinCodeVerifyYesNo,
            .mac_address = FURI_HAL_BT_DEFAULT_MAC_ADDR,
        },
    }
};
FuriHalBtProfileConfig* current_profile = NULL;

void furi_hal_bt_init() {
    furi_hal_bt_core2_mtx = osMutexNew(NULL);
    furi_assert(furi_hal_bt_core2_mtx);

    // Explicitly tell that we are in charge of CLK48 domain
    if(!HAL_HSEM_IsSemTaken(CFG_HW_CLK48_CONFIG_SEMID)) {
        HAL_HSEM_FastTake(CFG_HW_CLK48_CONFIG_SEMID);
    }

    // Start Core2
    ble_glue_init();
}

void furi_hal_bt_lock_core2() {
    furi_assert(furi_hal_bt_core2_mtx);
    furi_check(osMutexAcquire(furi_hal_bt_core2_mtx, osWaitForever) == osOK);
}

void furi_hal_bt_unlock_core2() {
    furi_assert(furi_hal_bt_core2_mtx);
    furi_check(osMutexRelease(furi_hal_bt_core2_mtx) == osOK);
}

static bool furi_hal_bt_start_core2() {
    furi_assert(furi_hal_bt_core2_mtx);

    osMutexAcquire(furi_hal_bt_core2_mtx, osWaitForever);
    // Explicitly tell that we are in charge of CLK48 domain
    if(!HAL_HSEM_IsSemTaken(CFG_HW_CLK48_CONFIG_SEMID)) {
        HAL_HSEM_FastTake(CFG_HW_CLK48_CONFIG_SEMID);
    }
    // Start Core2
    bool ret = ble_glue_start();
    osMutexRelease(furi_hal_bt_core2_mtx);

    return ret;
}

static bool furi_hal_bt_set_mac_address(FuriHalBtProfileConfig* profile_config) {
    bool res = false;
    uint32_t udn = LL_FLASH_GetUDN();
    if(udn != 0xFFFFFFFF) {
        uint32_t company_id = LL_FLASH_GetSTCompanyID();
        uint32_t device_id = LL_FLASH_GetDeviceID();
        profile_config->config.mac_address[0] = (uint8_t)(udn & 0x000000FF);
        profile_config->config.mac_address[1] = (uint8_t)((udn & 0x0000FF00) >> 8 );
        profile_config->config.mac_address[2] = (uint8_t)((udn & 0x00FF0000) >> 16 );
        profile_config->config.mac_address[3] = (uint8_t)device_id;
        profile_config->config.mac_address[4] = (uint8_t)(company_id & 0x000000FF);
        profile_config->config.mac_address[5] = (uint8_t)((company_id & 0x0000FF00) >> 8);
        res = true;
    }
    return res;
}

bool furi_hal_bt_start_app(FuriHalBtProfile profile, BleEventCallback event_cb, void* context) {
    furi_assert(event_cb);
    furi_assert(profile < FuriHalBtProfileNumber);
    bool ret = true;

    do {
        // Start 2nd core
        ret = furi_hal_bt_start_core2();
        if(!ret) {
            ble_app_kill_thread();
            FURI_LOG_E(TAG, "Failed to start 2nd core");
            break;
        }
        // Set mac address
        furi_hal_bt_set_mac_address(&profile_config[profile]);
        // Configure GAP
        GapConfig config = profile_config[profile].config;
        if(profile == FuriHalBtProfileSerial) {
            config.adv_service_uuid |= furi_hal_version_get_hw_color();
        } else if(profile == FuriHalBtProfileHidKeyboard) {
            // Change MAC address for HID profile
            config.mac_address[2]++;
        }
        ret = gap_init(&config, event_cb, context);
        if(!ret) {
            gap_kill_thread();
            FURI_LOG_E(TAG, "Failed to init GAP");
            break;
        }
        // Start selected profile services
        profile_config[profile].start();
    } while(false);
    current_profile = &profile_config[profile];

    return ret;
}

bool furi_hal_bt_change_app(FuriHalBtProfile profile, BleEventCallback event_cb, void* context) {
    furi_assert(event_cb);
    furi_assert(profile < FuriHalBtProfileNumber);
    bool ret = true;

    FURI_LOG_I(TAG, "Stop current profile services");
    current_profile->stop();
    FURI_LOG_I(TAG, "Disconnect and stop advertising");
    furi_hal_bt_stop_advertising();
    FURI_LOG_I(TAG, "Shutdow 2nd core");
    LL_C2_PWR_SetPowerMode(LL_PWR_MODE_SHUTDOWN);
    FURI_LOG_I(TAG, "Stop BLE related RTOS threads");
    gap_kill_thread();
    ble_app_kill_thread();
    // TODO change delay to event
    osDelay(200);
    FURI_LOG_I(TAG, "Reset SHCI");
    SHCI_C2_Reinit();
    ble_glue_kill_thread();
    FURI_LOG_I(TAG, "Start BT initialization");
    furi_hal_bt_init();
    ret = furi_hal_bt_start_app(profile, event_cb, context);
    if(ret) {
        current_profile = &profile_config[profile];
    }
    return ret;
}

void furi_hal_bt_start_advertising() {
    if(gap_get_state() == GapStateIdle) {
        gap_start_advertising();
    }
}

void furi_hal_bt_stop_advertising() {
    if(furi_hal_bt_is_active()) {
        gap_stop_advertising();
        while(furi_hal_bt_is_active()) {
            osDelay(1);
        }
    }
}

void furi_hal_bt_update_battery_level(uint8_t battery_level) {
    if(battery_svc_is_started()) {
        battery_svc_update_level(battery_level);
    }
}

void furi_hal_bt_get_key_storage_buff(uint8_t** key_buff_addr, uint16_t* key_buff_size) {
    ble_app_get_key_storage_buff(key_buff_addr, key_buff_size);
}

void furi_hal_bt_set_key_storage_change_callback(BleGlueKeyStorageChangedCallback callback, void* context) {
    furi_assert(callback);
    ble_glue_set_key_storage_changed_callback(callback, context);
}

void furi_hal_bt_nvm_sram_sem_acquire() {
    while(HAL_HSEM_FastTake(CFG_HW_BLE_NVM_SRAM_SEMID) != HAL_OK) {
        osDelay(1);
    }
}

void furi_hal_bt_nvm_sram_sem_release() {
    HAL_HSEM_Release(CFG_HW_BLE_NVM_SRAM_SEMID, 0);
}

void furi_hal_bt_dump_state(string_t buffer) {
    if (furi_hal_bt_is_alive()) {
        uint8_t HCI_Version;
        uint16_t HCI_Revision;
        uint8_t LMP_PAL_Version;
        uint16_t Manufacturer_Name;
        uint16_t LMP_PAL_Subversion;

        tBleStatus ret = hci_read_local_version_information(
            &HCI_Version, &HCI_Revision, &LMP_PAL_Version, &Manufacturer_Name, &LMP_PAL_Subversion
        );

        string_cat_printf(buffer,
            "Ret: %d, HCI_Version: %d, HCI_Revision: %d, LMP_PAL_Version: %d, Manufacturer_Name: %d, LMP_PAL_Subversion: %d",
            ret, HCI_Version, HCI_Revision, LMP_PAL_Version, Manufacturer_Name, LMP_PAL_Subversion
        );
    } else {
        string_cat_printf(buffer, "BLE not ready");
    }
}

bool furi_hal_bt_is_alive() {
    return ble_glue_is_alive();
}

bool furi_hal_bt_is_active() {
    return gap_get_state() > GapStateIdle;
}

void furi_hal_bt_start_tone_tx(uint8_t channel, uint8_t power) {
    aci_hal_set_tx_power_level(0, power);
    aci_hal_tone_start(channel, 0);
}

void furi_hal_bt_stop_tone_tx() {
    aci_hal_tone_stop();
}

void furi_hal_bt_start_packet_tx(uint8_t channel, uint8_t pattern, uint8_t datarate) {
    hci_le_enhanced_transmitter_test(channel, 0x25, pattern, datarate);
}

void furi_hal_bt_start_packet_rx(uint8_t channel, uint8_t datarate) {
    hci_le_enhanced_receiver_test(channel, datarate, 0);
}

uint16_t furi_hal_bt_stop_packet_test() {
    uint16_t num_of_packets = 0;
    hci_le_test_end(&num_of_packets);
    return num_of_packets;
}

void furi_hal_bt_start_rx(uint8_t channel) {
    aci_hal_rx_start(channel);
}

float furi_hal_bt_get_rssi() {
    float val;
    uint8_t rssi_raw[3];

    if (aci_hal_read_raw_rssi(rssi_raw) != BLE_STATUS_SUCCESS) {
        return 0.0f;
    }

    // Some ST magic with rssi
    uint8_t agc = rssi_raw[2] & 0xFF;
    int rssi = (((int)rssi_raw[1] << 8) & 0xFF00) + (rssi_raw[0] & 0xFF);
    if(rssi == 0 || agc > 11) {
        val = -127.0;
    } else {
        val = agc * 6.0f - 127.0f;
        while(rssi > 30) {
            val += 6.0;
            rssi >>=1;
        }
        val += (417 * rssi + 18080) >> 10;
    }
    return val;
}

uint32_t furi_hal_bt_get_transmitted_packets() {
    uint32_t packets = 0;
    aci_hal_le_tx_test_packet_number(&packets);
    return packets;
}

void furi_hal_bt_stop_rx() {
    aci_hal_rx_stop();
}
