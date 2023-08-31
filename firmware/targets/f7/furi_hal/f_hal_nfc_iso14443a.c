#include <f_hal_nfc_i.h>

#include <furi.h>
#include <furi_hal_resources.h>

#include <digital_signal/presets/nfc/iso14443_3a_signal.h>

#define TAG "FuriHalIso14443a"

static Iso14443_3aSignal* iso14443_3a_signal = NULL;

static FHalNfcError f_hal_nfc_iso14443a_common_init(FuriHalSpiBusHandle* handle) {
    // Common NFC-A settings, 106 kbps

    // 1st stage zero = 600kHz, 3rd stage zero = 200 kHz
    st25r3916_write_reg(handle, ST25R3916_REG_RX_CONF1, ST25R3916_REG_RX_CONF1_z600k);
    // AGC enabled, ratio 3:1, squelch after TX
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_RX_CONF2,
        ST25R3916_REG_RX_CONF2_agc6_3 | ST25R3916_REG_RX_CONF2_agc_m |
            ST25R3916_REG_RX_CONF2_agc_en | ST25R3916_REG_RX_CONF2_sqm_dyn);
    // HF operation, full gain on AM and PM channels
    st25r3916_write_reg(handle, ST25R3916_REG_RX_CONF3, 0x00);
    // No gain reduction on AM and PM channels
    st25r3916_write_reg(handle, ST25R3916_REG_RX_CONF4, 0x00);
    // Correlator config
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_CORR_CONF1,
        ST25R3916_REG_CORR_CONF1_corr_s0 | ST25R3916_REG_CORR_CONF1_corr_s4 |
            ST25R3916_REG_CORR_CONF1_corr_s6);
    // Sleep mode disable, 424kHz mode off
    st25r3916_write_reg(handle, ST25R3916_REG_CORR_CONF2, 0x00);

    return FHalNfcErrorNone;
}

static FHalNfcError f_hal_nfc_iso14443a_poller_init(FuriHalSpiBusHandle* handle) {
    // Enable ISO14443A mode, OOK modulation
    st25r3916_change_reg_bits(
        handle,
        ST25R3916_REG_MODE,
        ST25R3916_REG_MODE_om_mask | ST25R3916_REG_MODE_tr_am,
        ST25R3916_REG_MODE_om_iso14443a | ST25R3916_REG_MODE_tr_am_ook);

    // Overshoot protection - is this necessary here?
    st25r3916_change_reg_bits(handle, ST25R3916_REG_OVERSHOOT_CONF1, 0xff, 0x40);
    st25r3916_change_reg_bits(handle, ST25R3916_REG_OVERSHOOT_CONF2, 0xff, 0x03);
    st25r3916_change_reg_bits(handle, ST25R3916_REG_UNDERSHOOT_CONF1, 0xff, 0x40);
    st25r3916_change_reg_bits(handle, ST25R3916_REG_UNDERSHOOT_CONF2, 0xff, 0x03);

    return f_hal_nfc_iso14443a_common_init(handle);
}

static FHalNfcError f_hal_nfc_iso14443a_poller_deinit(FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    return FHalNfcErrorNone;
}

static FHalNfcError f_hal_nfc_iso14443a_listener_init(FuriHalSpiBusHandle* handle) {
    furi_check(iso14443_3a_signal == NULL);
    iso14443_3a_signal = iso14443_3a_signal_alloc(&gpio_spi_r_mosi);

    st25r3916_write_reg(
        handle,
        ST25R3916_REG_OP_CONTROL,
        ST25R3916_REG_OP_CONTROL_en | ST25R3916_REG_OP_CONTROL_rx_en |
            ST25R3916_REG_OP_CONTROL_en_fd_auto_efd);
    st25r3916_write_reg(
        handle, ST25R3916_REG_MODE, ST25R3916_REG_MODE_targ_targ | ST25R3916_REG_MODE_om0);
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_PASSIVE_TARGET,
        ST25R3916_REG_PASSIVE_TARGET_fdel_2 | ST25R3916_REG_PASSIVE_TARGET_fdel_0 |
            ST25R3916_REG_PASSIVE_TARGET_d_ac_ap2p | ST25R3916_REG_PASSIVE_TARGET_d_212_424_1r);

    st25r3916_write_reg(handle, ST25R3916_REG_MASK_RX_TIMER, 0x02);

    st25r3916_direct_cmd(handle, ST25R3916_CMD_STOP);
    uint32_t interrupts =
        (/*ST25R3916_IRQ_MASK_FWL | ST25R3916_IRQ_MASK_TXE |*/ ST25R3916_IRQ_MASK_RXS /*|
         ST25R3916_IRQ_MASK_RXE | ST25R3916_IRQ_MASK_PAR | ST25R3916_IRQ_MASK_CRC |
         ST25R3916_IRQ_MASK_ERR1 | ST25R3916_IRQ_MASK_ERR2 | ST25R3916_IRQ_MASK_EON |
         ST25R3916_IRQ_MASK_EOF | ST25R3916_IRQ_MASK_WU_A_X | ST25R3916_IRQ_MASK_WU_A*/);
    // Clear interrupts
    // FURI_LOG_I("LISTEN START", "%lX", interrupts);
    st25r3916_get_irq(handle);
    // Enable interrupts
    st25r3916_mask_irq(handle, interrupts);
    // Enable auto collision resolution
    st25r3916_clear_reg_bits(
        handle, ST25R3916_REG_PASSIVE_TARGET, ST25R3916_REG_PASSIVE_TARGET_d_106_ac_a);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_GOTO_SENSE);

    return f_hal_nfc_iso14443a_common_init(handle);
}

static FHalNfcError f_hal_nfc_iso14443a_listener_deinit(FuriHalSpiBusHandle* handle) {
    UNUSED(handle);

    if(iso14443_3a_signal) {
        iso14443_3a_signal_free(iso14443_3a_signal);
        iso14443_3a_signal = NULL;
    }

    return FHalNfcErrorNone;
}

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

FHalNfcError f_hal_iso4443a_listener_tx(
    FuriHalSpiBusHandle* handle,
    const uint8_t* tx_data,
    size_t tx_bits) {
    FHalNfcError error = FHalNfcErrorNone;

    do {
        error = f_hal_nfc_common_fifo_tx(handle, tx_data, tx_bits);
        if(error != FHalNfcErrorNone) break;

        bool tx_end = f_hal_nfc_event_wait_for_specific_irq(handle, ST25R3916_IRQ_MASK_TXE, 10);
        if(!tx_end) {
            error = FHalNfcErrorCommunicationTimeout;
            break;
        }

    } while(false);

    return error;
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

const FHalNfcTechBase f_hal_nfc_iso14443a = {
    .poller =
        {
            .init = f_hal_nfc_iso14443a_poller_init,
            .deinit = f_hal_nfc_iso14443a_poller_deinit,
            .wait_event = f_hal_nfc_wait_event_common,
            .tx = f_hal_nfc_poller_tx_common,
            .rx = f_hal_nfc_common_fifo_rx,
        },

    .listener =
        {
            .init = f_hal_nfc_iso14443a_listener_init,
            .deinit = f_hal_nfc_iso14443a_listener_deinit,
            .wait_event = f_hal_nfc_wait_event_common,
            .rx_start = f_hal_nfc_common_listener_rx_start,
            .tx = f_hal_iso4443a_listener_tx,
            .rx = f_hal_nfc_common_fifo_rx,
        },
};
