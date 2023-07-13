#include <f_hal_nfc_i.h>

#include <furi.h>
#include <lib/drivers/st25r3916_reg.h>
#include <lib/drivers/st25r3916.h>
#include <digital_signal/presets/nfc/iso14443_3a_signal.h>
#include <furi_hal_resources.h>

#define TAG "FuriHalNfcA"

static Iso14443_3aSignal* iso14443_3a_signal = NULL;

FHalNfcError f_hal_nfca_send_short_frame(FHalNfcaShortFrame frame) {
    FHalNfcError error = FHalNfcErrorNone;

    FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    // Disable crc check
    st25r3916_set_reg_bits(handle, ST25R3916_REG_AUX, ST25R3916_REG_AUX_no_crc_rx);
    st25r3916_change_reg_bits(
        handle,
        ST25R3916_REG_ISO14443A_NFC,
        (ST25R3916_REG_ISO14443A_NFC_no_tx_par | ST25R3916_REG_ISO14443A_NFC_no_rx_par),
        (ST25R3916_REG_ISO14443A_NFC_no_tx_par_off | ST25R3916_REG_ISO14443A_NFC_no_rx_par_off));

    st25r3916_write_reg(handle, ST25R3916_REG_NUM_TX_BYTES2, 0);
    uint32_t interrupts =
        (ST25R3916_IRQ_MASK_FWL | ST25R3916_IRQ_MASK_TXE | ST25R3916_IRQ_MASK_RXS |
         ST25R3916_IRQ_MASK_RXE | ST25R3916_IRQ_MASK_PAR | ST25R3916_IRQ_MASK_CRC |
         ST25R3916_IRQ_MASK_ERR1 | ST25R3916_IRQ_MASK_ERR2 | ST25R3916_IRQ_MASK_NRE);
    // Clear interrupts
    st25r3916_get_irq(handle);
    // Enable interrupts
    st25r3916_mask_irq(handle, ~interrupts);
    if(frame == FHalNfcaShortFrameAllReq) {
        st25r3916_direct_cmd(handle, ST25R3916_CMD_TRANSMIT_REQA);
    } else {
        st25r3916_direct_cmd(handle, ST25R3916_CMD_TRANSMIT_WUPA);
    }

    return error;
}

FHalNfcError f_hal_nfca_send_sdd_frame(const uint8_t* tx_data, size_t tx_bits) {
    FHalNfcError error = FHalNfcErrorNone;
    // TODO Set anticollision parameters
    error = f_hal_nfc_poller_tx(tx_data, tx_bits);

    return error;
}

FHalNfcError f_hal_nfca_receive_sdd_frame(uint8_t* rx_data, size_t rx_data_size, size_t* rx_bits) {
    FHalNfcError error = FHalNfcErrorNone;
    UNUSED(rx_data);
    UNUSED(rx_bits);
    UNUSED(rx_data_size);

    error = f_hal_nfc_poller_rx(rx_data, rx_data_size, rx_bits);
    // TODO reset anticollision parameters here

    return error;
}

FHalNfcError
    furi_hal_nfca_set_col_res_data(uint8_t* uid, uint8_t uid_len, uint8_t* atqa, uint8_t sak) {
    furi_assert(uid);
    furi_assert(atqa);
    UNUSED(uid_len);
    UNUSED(sak);
    FHalNfcError error = FHalNfcErrorNone;

    FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    // Set 4 or 7 bytes UID
    if(uid_len == 4) {
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_AUX,
            ST25R3916_REG_AUX_nfc_id_mask,
            ST25R3916_REG_AUX_nfc_id_4bytes);
    } else {
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_AUX,
            ST25R3916_REG_AUX_nfc_id_mask,
            ST25R3916_REG_AUX_nfc_id_7bytes);
    }
    // Write PT Memory
    uint8_t pt_memory[15] = {};
    memcpy(pt_memory, uid, uid_len);
    pt_memory[10] = atqa[0];
    pt_memory[11] = atqa[1];
    if(uid_len == 4) {
        pt_memory[12] = sak & ~0x04;
    } else {
        pt_memory[12] = 0x04;
    }
    pt_memory[13] = sak & ~0x04;
    pt_memory[14] = sak & ~0x04;

    st25r3916_write_pta_mem(handle, pt_memory, sizeof(pt_memory));

    return error;
}

// TODO change this

FHalNfcError f_hal_nfca_listener_init() {
    furi_check(iso14443_3a_signal == NULL);

    iso14443_3a_signal = iso14443_3a_signal_alloc(&gpio_spi_r_mosi);

    return FHalNfcErrorNone;
}

FHalNfcError f_hal_nfca_listener_deinit() {
    if(iso14443_3a_signal) {
        iso14443_3a_signal_free(iso14443_3a_signal);
        iso14443_3a_signal = NULL;
    }

    return FHalNfcErrorNone;
}

FHalNfcError f_hal_nfca_listener_tx_custom_parity(
    const uint8_t* tx_data,
    const bool* tx_parity,
    size_t tx_bits) {
    furi_assert(tx_data);
    furi_assert(tx_parity);

    furi_assert(iso14443_3a_signal);

    FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    st25r3916_direct_cmd(handle, ST25R3916_CMD_TRANSPARENT_MODE);
    // Reconfigure gpio for Transparent mode
    furi_hal_spi_bus_handle_deinit(&furi_hal_spi_bus_handle_nfc);

    // Send signal
    iso14443_3a_signal_tx(iso14443_3a_signal, tx_data, tx_parity, tx_bits);

    // Exit transparent mode
    furi_hal_gpio_write(&gpio_spi_r_mosi, false);

    // Configure gpio back to SPI and exit transparent
    furi_hal_spi_bus_handle_init(&furi_hal_spi_bus_handle_nfc);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_UNMASK_RECEIVE_DATA);

    // TODO handle field off
    return FHalNfcErrorNone;
}
