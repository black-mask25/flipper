#include "f_hal_nfc_i.h"

#include <lib/drivers/st25r3916.h>

#include <furi.h>
#include <furi_hal_spi.h>

#define TAG "FHalNfc"

static FuriMutex* f_hal_nfc_mutex = NULL;

static FHalNfcError f_hal_nfc_turn_on_osc(FuriHalSpiBusHandle* handle) {
    FHalNfcError error = FHalNfcErrorNone;

    if(!st25r3916_check_reg(
           handle,
           ST25R3916_REG_OP_CONTROL,
           ST25R3916_REG_OP_CONTROL_en,
           ST25R3916_REG_OP_CONTROL_en)) {
        st25r3916_mask_irq(handle, ~ST25R3916_IRQ_MASK_OSC);
        st25r3916_set_reg_bits(handle, ST25R3916_REG_OP_CONTROL, ST25R3916_REG_OP_CONTROL_en);
        f_hal_nfc_event_wait_for_specific_irq(handle, ST25R3916_IRQ_MASK_OSC, 10);
    }
    // Disable IRQs
    st25r3916_mask_irq(handle, ST25R3916_IRQ_MASK_ALL);

    bool osc_on = st25r3916_check_reg(
        handle,
        ST25R3916_REG_AUX_DISPLAY,
        ST25R3916_REG_AUX_DISPLAY_osc_ok,
        ST25R3916_REG_AUX_DISPLAY_osc_ok);
    if(!osc_on) {
        error = FHalNfcErrorOscillator;
    }

    return error;
}

FHalNfcError f_hal_nfc_is_hal_ready() {
    FHalNfcError error = FHalNfcErrorNone;

    do {
        error = f_hal_nfc_acquire();
        if(error != FHalNfcErrorNone) break;

        FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;
        uint8_t chip_id = 0;
        st25r3916_read_reg(handle, ST25R3916_REG_IC_IDENTITY, &chip_id);
        if((chip_id & ST25R3916_REG_IC_IDENTITY_ic_type_mask) !=
           ST25R3916_REG_IC_IDENTITY_ic_type_st25r3916) {
            FURI_LOG_E(TAG, "Wrong chip id");
            error = FHalNfcErrorCommunication;
        }

        f_hal_nfc_release();
    } while(false);

    return error;
}

FHalNfcError f_hal_nfc_init() {
    furi_assert(f_hal_nfc_mutex == NULL);

    f_hal_nfc_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    FHalNfcError error = FHalNfcErrorNone;

    f_hal_nfc_event_init();
    f_hal_nfc_event_start();

    do {
        error = f_hal_nfc_acquire();
        if(error != FHalNfcErrorNone) break;

        FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;
        // Set default state
        st25r3916_direct_cmd(handle, ST25R3916_CMD_SET_DEFAULT);
        // Increase IO driver strength of MISO and IRQ
        st25r3916_write_reg(handle, ST25R3916_REG_IO_CONF2, ST25R3916_REG_IO_CONF2_io_drv_lvl);
        // Check chip ID
        uint8_t chip_id = 0;
        st25r3916_read_reg(handle, ST25R3916_REG_IC_IDENTITY, &chip_id);
        if((chip_id & ST25R3916_REG_IC_IDENTITY_ic_type_mask) !=
           ST25R3916_REG_IC_IDENTITY_ic_type_st25r3916) {
            FURI_LOG_E(TAG, "Wrong chip id");
            error = FHalNfcErrorCommunication;
        }
        // Clear interrupts
        st25r3916_get_irq(handle);
        // Mask all interrupts
        st25r3916_mask_irq(handle, ST25R3916_IRQ_MASK_ALL);
        // Enable interrupts
        f_hal_nfc_init_gpio_isr();
        // Disable internal overheat protection
        st25r3916_change_test_reg_bits(handle, 0x04, 0x10, 0x10);

        error = f_hal_nfc_turn_on_osc(handle);
        if(error != FHalNfcErrorNone) break;

        // Measure voltage
        // Set measure power supply voltage source
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_REGULATOR_CONTROL,
            ST25R3916_REG_REGULATOR_CONTROL_mpsv_mask,
            ST25R3916_REG_REGULATOR_CONTROL_mpsv_vdd);
        // Enable timer and interrupt register
        st25r3916_mask_irq(handle, ~ST25R3916_IRQ_MASK_DCT);
        st25r3916_direct_cmd(handle, ST25R3916_CMD_MEASURE_VDD);
        f_hal_nfc_event_wait_for_specific_irq(handle, ST25R3916_IRQ_MASK_DCT, 100);
        st25r3916_mask_irq(handle, ST25R3916_IRQ_MASK_ALL);
        uint8_t ad_res = 0;
        st25r3916_read_reg(handle, ST25R3916_REG_AD_RESULT, &ad_res);
        uint16_t mV = ((uint16_t)ad_res) * 23U;
        mV += (((((uint16_t)ad_res) * 4U) + 5U) / 10U);

        if(mV < 3600) {
            st25r3916_change_reg_bits(
                handle,
                ST25R3916_REG_IO_CONF2,
                ST25R3916_REG_IO_CONF2_sup3V,
                ST25R3916_REG_IO_CONF2_sup3V_3V);
        } else {
            st25r3916_change_reg_bits(
                handle,
                ST25R3916_REG_IO_CONF2,
                ST25R3916_REG_IO_CONF2_sup3V,
                ST25R3916_REG_IO_CONF2_sup3V_5V);
        }

        // Disable MCU CLK
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_IO_CONF1,
            ST25R3916_REG_IO_CONF1_out_cl_mask | ST25R3916_REG_IO_CONF1_lf_clk_off,
            0x07);
        // Disable MISO pull-down
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_IO_CONF2,
            ST25R3916_REG_IO_CONF2_miso_pd1 | ST25R3916_REG_IO_CONF2_miso_pd2,
            0x00);
        // Set tx driver resistance to 1 Om
        st25r3916_change_reg_bits(
            handle, ST25R3916_REG_TX_DRIVER, ST25R3916_REG_TX_DRIVER_d_res_mask, 0x00);
        // Use minimum non-overlap
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_RES_AM_MOD,
            ST25R3916_REG_RES_AM_MOD_fa3_f,
            ST25R3916_REG_RES_AM_MOD_fa3_f);

        // Set activation threashold
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_FIELD_THRESHOLD_ACTV,
            ST25R3916_REG_FIELD_THRESHOLD_ACTV_trg_mask,
            ST25R3916_REG_FIELD_THRESHOLD_ACTV_trg_105mV);
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_FIELD_THRESHOLD_ACTV,
            ST25R3916_REG_FIELD_THRESHOLD_ACTV_rfe_mask,
            ST25R3916_REG_FIELD_THRESHOLD_ACTV_rfe_105mV);
        // Set deactivation threashold
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_FIELD_THRESHOLD_DEACTV,
            ST25R3916_REG_FIELD_THRESHOLD_DEACTV_trg_mask,
            ST25R3916_REG_FIELD_THRESHOLD_DEACTV_trg_75mV);
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_FIELD_THRESHOLD_DEACTV,
            ST25R3916_REG_FIELD_THRESHOLD_DEACTV_rfe_mask,
            ST25R3916_REG_FIELD_THRESHOLD_DEACTV_rfe_75mV);
        // Enable external load modulation
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_AUX_MOD,
            ST25R3916_REG_AUX_MOD_lm_ext,
            ST25R3916_REG_AUX_MOD_lm_ext);
        // Enable internal load modulation
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_AUX_MOD,
            ST25R3916_REG_AUX_MOD_lm_dri,
            ST25R3916_REG_AUX_MOD_lm_dri);
        // Adjust FDT
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_PASSIVE_TARGET,
            ST25R3916_REG_PASSIVE_TARGET_fdel_mask,
            (5U << ST25R3916_REG_PASSIVE_TARGET_fdel_shift));
        // Reduce RFO resistance in Modulated state
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_PT_MOD,
            ST25R3916_REG_PT_MOD_ptm_res_mask | ST25R3916_REG_PT_MOD_pt_res_mask,
            0x0f);
        // Enable RX start on first 4 bits
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_EMD_SUP_CONF,
            ST25R3916_REG_EMD_SUP_CONF_rx_start_emv,
            ST25R3916_REG_EMD_SUP_CONF_rx_start_emv_on);
        // Set antena tunning
        st25r3916_change_reg_bits(handle, ST25R3916_REG_ANT_TUNE_A, 0xff, 0x82);
        st25r3916_change_reg_bits(handle, ST25R3916_REG_ANT_TUNE_B, 0xff, 0x82);
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_OP_CONTROL,
            ST25R3916_REG_OP_CONTROL_en_fd_mask,
            ST25R3916_REG_OP_CONTROL_en_fd_auto_efd);

        // Perform calibration
        if(st25r3916_check_reg(
               handle,
               ST25R3916_REG_REGULATOR_CONTROL,
               ST25R3916_REG_REGULATOR_CONTROL_reg_s,
               0x00)) {
            FURI_LOG_I(TAG, "Adjusting regulators");
            // Reset logic
            st25r3916_set_reg_bits(
                handle, ST25R3916_REG_REGULATOR_CONTROL, ST25R3916_REG_REGULATOR_CONTROL_reg_s);
            st25r3916_clear_reg_bits(
                handle, ST25R3916_REG_REGULATOR_CONTROL, ST25R3916_REG_REGULATOR_CONTROL_reg_s);
            st25r3916_direct_cmd(handle, ST25R3916_CMD_ADJUST_REGULATORS);
            furi_delay_ms(6);
        }

        f_hal_nfc_low_power_mode_start();
        f_hal_nfc_release();
    } while(false);

    return error;
}

static bool f_hal_nfc_is_mine() {
    return (furi_mutex_get_owner(f_hal_nfc_mutex) == furi_thread_get_current_id());
}

FHalNfcError f_hal_nfc_acquire() {
    furi_check(f_hal_nfc_mutex);

    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_nfc);

    FHalNfcError error = FHalNfcErrorNone;
    if(furi_mutex_acquire(f_hal_nfc_mutex, 100) != FuriStatusOk) {
        furi_hal_spi_release(&furi_hal_spi_bus_handle_nfc);
        error = FHalNfcErrorBusy;
    }

    return error;
}

FHalNfcError f_hal_nfc_release() {
    furi_check(f_hal_nfc_mutex);
    furi_check(f_hal_nfc_is_mine());
    furi_check(furi_mutex_release(f_hal_nfc_mutex) == FuriStatusOk);

    furi_hal_spi_release(&furi_hal_spi_bus_handle_nfc);

    return FHalNfcErrorNone;
}

FHalNfcError f_hal_nfc_low_power_mode_start() {
    FHalNfcError error = FHalNfcErrorNone;
    FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    st25r3916_direct_cmd(handle, ST25R3916_CMD_STOP);
    st25r3916_clear_reg_bits(
        handle,
        ST25R3916_REG_OP_CONTROL,
        (ST25R3916_REG_OP_CONTROL_en | ST25R3916_REG_OP_CONTROL_rx_en |
         ST25R3916_REG_OP_CONTROL_wu | ST25R3916_REG_OP_CONTROL_tx_en |
         ST25R3916_REG_OP_CONTROL_en_fd_mask));
    f_hal_nfc_deinit_gpio_isr();
    f_hal_nfc_timers_deinit();

    return error;
}

FHalNfcError f_hal_nfc_low_power_mode_stop() {
    FHalNfcError error = FHalNfcErrorNone;
    FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    do {
        f_hal_nfc_init_gpio_isr();
        f_hal_nfc_timers_init();
        error = f_hal_nfc_turn_on_osc(handle);
        if(error != FHalNfcErrorNone) break;
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_OP_CONTROL,
            ST25R3916_REG_OP_CONTROL_en_fd_mask,
            ST25R3916_REG_OP_CONTROL_en_fd_auto_efd);

    } while(false);

    return error;
}

static void f_hal_nfc_configure_poller_common(FuriHalSpiBusHandle* handle) {
    // Disable wake up
    st25r3916_clear_reg_bits(handle, ST25R3916_REG_OP_CONTROL, ST25R3916_REG_OP_CONTROL_wu);
    // Enable correlator
    st25r3916_change_reg_bits(
        handle,
        ST25R3916_REG_AUX,
        ST25R3916_REG_AUX_dis_corr,
        ST25R3916_REG_AUX_dis_corr_correlator);

    st25r3916_change_reg_bits(handle, ST25R3916_REG_ANT_TUNE_A, 0xff, 0x82);
    st25r3916_change_reg_bits(handle, ST25R3916_REG_ANT_TUNE_B, 0xFF, 0x82);

    st25r3916_write_reg(handle, ST25R3916_REG_OVERSHOOT_CONF1, 0x00);
    st25r3916_write_reg(handle, ST25R3916_REG_OVERSHOOT_CONF2, 0x00);
    st25r3916_write_reg(handle, ST25R3916_REG_UNDERSHOOT_CONF1, 0x00);
    st25r3916_write_reg(handle, ST25R3916_REG_UNDERSHOOT_CONF2, 0x00);
}

// TODO: Refactor this function to be more modular and readable
FHalNfcError f_hal_nfc_set_mode(FHalNfcMode mode, FHalNfcBitrate bitrate) {
    FHalNfcError error = FHalNfcErrorNone;
    FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    if(mode == FHalNfcModeIso14443aPoller || mode == FHalNfcModeIso14443aListener) {
        if(mode == FHalNfcModeIso14443aPoller) {
            // Poller configuration
            f_hal_nfc_configure_poller_common(handle);
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

        } else {
            // Listener configuration
            f_hal_nfca_listener_init();
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
                    ST25R3916_REG_PASSIVE_TARGET_d_ac_ap2p |
                    ST25R3916_REG_PASSIVE_TARGET_d_212_424_1r);

            st25r3916_write_reg(handle, ST25R3916_REG_MASK_RX_TIMER, 0x02);
        }

        if(bitrate == FHalNfcBitrate106) {
            // Bitrate-dependent NFC-A settings

            // 1st stage zero = 600kHz, 3rd stage zero = 200 kHz
            st25r3916_write_reg(handle, ST25R3916_REG_RX_CONF1, ST25R3916_REG_RX_CONF1_z600k);
            // AGC enabled, ratio 6:1, squelch after TX
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
        }

    } else if(mode == FHalNfcModeIso14443bPoller /* TODO: Listener support */) {
        f_hal_nfc_configure_poller_common(handle);
        // Enable ISO14443B mode, AM modulation
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_MODE,
            ST25R3916_REG_MODE_om_mask | ST25R3916_REG_MODE_tr_am,
            ST25R3916_REG_MODE_om_iso14443b | ST25R3916_REG_MODE_tr_am_am);

        // 10% ASK modulation
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_TX_DRIVER,
            ST25R3916_REG_TX_DRIVER_am_mod_mask,
            ST25R3916_REG_TX_DRIVER_am_mod_10percent);

        // Use regulator AM, resistive AM disabled
        st25r3916_clear_reg_bits(
            handle,
            ST25R3916_REG_AUX_MOD,
            ST25R3916_REG_AUX_MOD_dis_reg_am | ST25R3916_REG_AUX_MOD_res_am);

        // EGT = 0 etu
        // SOF = 10 etu LOW + 2 etu HIGH
        // EOF = 10 etu
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_ISO14443B_1,
            ST25R3916_REG_ISO14443B_1_egt_mask | ST25R3916_REG_ISO14443B_1_sof_mask |
                ST25R3916_REG_ISO14443B_1_eof,
            (0U << ST25R3916_REG_ISO14443B_1_egt_shift) | ST25R3916_REG_ISO14443B_1_sof_0_10etu |
                ST25R3916_REG_ISO14443B_1_sof_1_2etu | ST25R3916_REG_ISO14443B_1_eof_10etu);

        // TR1 = 80 / fs
        // B' mode off (no_sof & no_eof = 0)
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_ISO14443B_2,
            ST25R3916_REG_ISO14443B_2_tr1_mask | ST25R3916_REG_ISO14443B_2_no_sof |
                ST25R3916_REG_ISO14443B_2_no_eof,
            ST25R3916_REG_ISO14443B_2_tr1_80fs80fs);

        if(bitrate == FHalNfcBitrate106) {
            // Bitrate-dependent NFC-B settings

            // 1st stage zero = 60kHz, 3rd stage zero = 200 kHz
            st25r3916_write_reg(handle, ST25R3916_REG_RX_CONF1, ST25R3916_REG_RX_CONF1_h200);

            // Enable AGC
            // AGC Ratio 6
            // AGC algorithm with RESET (recommended for ISO14443-B)
            // AGC operation during complete receive period
            // Squelch ratio 6/3 (recommended for ISO14443-B)
            // Squelch automatic activation on TX end
            st25r3916_write_reg(
                handle,
                ST25R3916_REG_RX_CONF2,
                ST25R3916_REG_RX_CONF2_agc6_3 | ST25R3916_REG_RX_CONF2_agc_alg |
                    ST25R3916_REG_RX_CONF2_agc_m | ST25R3916_REG_RX_CONF2_agc_en |
                    ST25R3916_REG_RX_CONF2_pulz_61 | ST25R3916_REG_RX_CONF2_sqm_dyn);

            // HF operation, full gain on AM and PM channels
            st25r3916_write_reg(handle, ST25R3916_REG_RX_CONF3, 0x00);
            // No gain reduction on AM and PM channels
            st25r3916_write_reg(handle, ST25R3916_REG_RX_CONF4, 0x00);

            // Subcarrier end detector enabled
            // Subcarrier end detection level = 66%
            // BPSK start 33 pilot pulses
            // AM & PM summation before digitizing on
            st25r3916_write_reg(
                handle,
                ST25R3916_REG_CORR_CONF1,
                ST25R3916_REG_CORR_CONF1_corr_s0 | ST25R3916_REG_CORR_CONF1_corr_s1 |
                    ST25R3916_REG_CORR_CONF1_corr_s3 | ST25R3916_REG_CORR_CONF1_corr_s4);
            // Sleep mode disable, 424kHz mode off
            st25r3916_write_reg(handle, ST25R3916_REG_CORR_CONF2, 0x00);
        }

    } else if(mode == FHalNfcModeIso15693Poller || mode == FHalNfcModeIso15693Listener) {
        if(mode == FHalNfcModeIso15693Poller) {
            // Poller configuration
            f_hal_nfc_configure_poller_common(handle);
            // Enable Subcarrier Stream mode, OOK modulation
            st25r3916_change_reg_bits(
                handle,
                ST25R3916_REG_MODE,
                ST25R3916_REG_MODE_om_mask | ST25R3916_REG_MODE_tr_am,
                ST25R3916_REG_MODE_om_subcarrier_stream | ST25R3916_REG_MODE_tr_am_ook);

            // Subcarrier 424 kHz mode
            // 8 sub-carrier pulses in report period
            st25r3916_write_reg(
                handle,
                ST25R3916_REG_STREAM_MODE,
                ST25R3916_REG_STREAM_MODE_scf_sc424 | ST25R3916_REG_STREAM_MODE_stx_106 |
                    ST25R3916_REG_STREAM_MODE_scp_8pulses);

            // Use regulator AM, resistive AM disabled
            st25r3916_clear_reg_bits(
                handle,
                ST25R3916_REG_AUX_MOD,
                ST25R3916_REG_AUX_MOD_dis_reg_am | ST25R3916_REG_AUX_MOD_res_am);

        } else {
            // Listener configuration
            f_hal_nfca_listener_init();
            // TODO: Implement listener config
        }

        if(bitrate == FHalNfcBitrate26p48) {
            // Bitrate-dependent NFC-V settings

            // 1st stage zero = 12 kHz, 3rd stage zero = 80 kHz, low-pass = 600 kHz
            st25r3916_write_reg(
                handle,
                ST25R3916_REG_RX_CONF1,
                ST25R3916_REG_RX_CONF1_z12k | ST25R3916_REG_RX_CONF1_h80 |
                    ST25R3916_REG_RX_CONF1_lp_600khz);

            // Enable AGC
            // AGC Ratio 6
            // AGC algorithm with RESET (recommended for ISO15693)
            // AGC operation during complete receive period
            // Squelch automatic activation on TX end
            st25r3916_write_reg(
                handle,
                ST25R3916_REG_RX_CONF2,
                ST25R3916_REG_RX_CONF2_agc6_3 | ST25R3916_REG_RX_CONF2_agc_m |
                    ST25R3916_REG_RX_CONF2_agc_en | ST25R3916_REG_RX_CONF2_sqm_dyn);

            // HF operation, full gain on AM and PM channels
            st25r3916_write_reg(handle, ST25R3916_REG_RX_CONF3, 0x00);
            // No gain reduction on AM and PM channels
            st25r3916_write_reg(handle, ST25R3916_REG_RX_CONF4, 0x00);

            // Collision detection level 53%
            // AM & PM summation before digitizing on
            st25r3916_write_reg(
                handle,
                ST25R3916_REG_CORR_CONF1,
                ST25R3916_REG_CORR_CONF1_corr_s0 | ST25R3916_REG_CORR_CONF1_corr_s1 |
                    ST25R3916_REG_CORR_CONF1_corr_s4);
            // 424 kHz subcarrier stream mode on
            st25r3916_write_reg(
                handle, ST25R3916_REG_CORR_CONF2, ST25R3916_REG_CORR_CONF2_corr_s8);
        }
    } else if(mode == FHalNfcModeFelicaPoller) {
        f_hal_nfc_configure_poller_common(handle);
        // Enable Felica mode, AM modulation
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_MODE,
            ST25R3916_REG_MODE_om_mask | ST25R3916_REG_MODE_tr_am,
            ST25R3916_REG_MODE_om_felica | ST25R3916_REG_MODE_tr_am_am);

        // 10% ASK modulation
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_TX_DRIVER,
            ST25R3916_REG_TX_DRIVER_am_mod_mask,
            ST25R3916_REG_TX_DRIVER_am_mod_10percent);

        // Use regulator AM, resistive AM disabled
        st25r3916_clear_reg_bits(
            handle,
            ST25R3916_REG_AUX_MOD,
            ST25R3916_REG_AUX_MOD_dis_reg_am | ST25R3916_REG_AUX_MOD_res_am);

        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_BIT_RATE,
            ST25R3916_REG_BIT_RATE_txrate_mask | ST25R3916_REG_BIT_RATE_rxrate_mask,
            ST25R3916_REG_BIT_RATE_txrate_212 | ST25R3916_REG_BIT_RATE_rxrate_212);

        // Receive configuration
        st25r3916_write_reg(
            handle,
            ST25R3916_REG_RX_CONF1,
            ST25R3916_REG_RX_CONF1_lp0 | ST25R3916_REG_RX_CONF1_hz_12_80khz);

        // Correlator setup
        st25r3916_write_reg(
            handle,
            ST25R3916_REG_CORR_CONF1,
            ST25R3916_REG_CORR_CONF1_corr_s6 | ST25R3916_REG_CORR_CONF1_corr_s4 |
                ST25R3916_REG_CORR_CONF1_corr_s3);
    }

    return error;
}

FHalNfcError f_hal_nfc_reset_mode() {
    FHalNfcError error = FHalNfcErrorNone;
    FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    st25r3916_direct_cmd(handle, ST25R3916_CMD_STOP);
    // Set default value in mode register
    st25r3916_write_reg(handle, ST25R3916_REG_MODE, ST25R3916_REG_MODE_om0);
    st25r3916_write_reg(handle, ST25R3916_REG_STREAM_MODE, 0);

    st25r3916_clear_reg_bits(handle, ST25R3916_REG_AUX, ST25R3916_REG_AUX_no_crc_rx);
    st25r3916_clear_reg_bits(
        handle,
        ST25R3916_REG_BIT_RATE,
        ST25R3916_REG_BIT_RATE_txrate_mask | ST25R3916_REG_BIT_RATE_rxrate_mask);

    // Write default values
    st25r3916_write_reg(handle, ST25R3916_REG_RX_CONF1, 0);
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_RX_CONF2,
        ST25R3916_REG_RX_CONF2_sqm_dyn | ST25R3916_REG_RX_CONF2_agc_en |
            ST25R3916_REG_RX_CONF2_agc_m);

    st25r3916_write_reg(
        handle,
        ST25R3916_REG_CORR_CONF1,
        ST25R3916_REG_CORR_CONF1_corr_s7 | ST25R3916_REG_CORR_CONF1_corr_s4 |
            ST25R3916_REG_CORR_CONF1_corr_s1 | ST25R3916_REG_CORR_CONF1_corr_s0);
    st25r3916_write_reg(handle, ST25R3916_REG_CORR_CONF2, 0);

    f_hal_nfca_listener_deinit();

    return error;
}

FHalNfcError f_hal_nfc_poller_field_on() {
    FHalNfcError error = FHalNfcErrorNone;
    FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    if(!st25r3916_check_reg(
           handle,
           ST25R3916_REG_OP_CONTROL,
           ST25R3916_REG_OP_CONTROL_tx_en,
           ST25R3916_REG_OP_CONTROL_tx_en)) {
        // Set min guard time
        st25r3916_write_reg(handle, ST25R3916_REG_FIELD_ON_GT, 0);
        // st25r3916_direct_cmd(handle, ST25R3916_CMD_INITIAL_RF_COLLISION);
        // Enable tx rx
        st25r3916_set_reg_bits(
            handle,
            ST25R3916_REG_OP_CONTROL,
            (ST25R3916_REG_OP_CONTROL_rx_en | ST25R3916_REG_OP_CONTROL_tx_en));
    }

    return error;
}

FHalNfcError f_hal_nfc_poller_tx_custom_parity(const uint8_t* tx_data, size_t tx_bits) {
    furi_assert(tx_data);

    // TODO common code for f_hal_nfc_poller_tx

    FHalNfcError err = FHalNfcErrorNone;
    FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    // Prepare tx
    st25r3916_direct_cmd(handle, ST25R3916_CMD_CLEAR_FIFO);
    st25r3916_clear_reg_bits(
        handle, ST25R3916_REG_TIMER_EMV_CONTROL, ST25R3916_REG_TIMER_EMV_CONTROL_nrt_emv);
    st25r3916_change_reg_bits(
        handle,
        ST25R3916_REG_ISO14443A_NFC,
        (ST25R3916_REG_ISO14443A_NFC_no_tx_par | ST25R3916_REG_ISO14443A_NFC_no_rx_par),
        (ST25R3916_REG_ISO14443A_NFC_no_tx_par | ST25R3916_REG_ISO14443A_NFC_no_rx_par));
    uint32_t interrupts =
        (ST25R3916_IRQ_MASK_FWL | ST25R3916_IRQ_MASK_TXE | ST25R3916_IRQ_MASK_RXS |
         ST25R3916_IRQ_MASK_RXE | ST25R3916_IRQ_MASK_PAR | ST25R3916_IRQ_MASK_CRC |
         ST25R3916_IRQ_MASK_ERR1 | ST25R3916_IRQ_MASK_ERR2 | ST25R3916_IRQ_MASK_NRE);
    // Clear interrupts
    st25r3916_get_irq(handle);
    // Enable interrupts
    st25r3916_mask_irq(handle, ~interrupts);

    st25r3916_write_fifo(handle, tx_data, tx_bits);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_TRANSMIT_WITHOUT_CRC);
    return err;
}

FHalNfcError f_hal_nfc_poller_tx(const uint8_t* tx_data, size_t tx_bits) {
    furi_assert(tx_data);

    FHalNfcError err = FHalNfcErrorNone;
    FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    // Prepare tx
    st25r3916_direct_cmd(handle, ST25R3916_CMD_CLEAR_FIFO);
    st25r3916_clear_reg_bits(
        handle, ST25R3916_REG_TIMER_EMV_CONTROL, ST25R3916_REG_TIMER_EMV_CONTROL_nrt_emv);
    st25r3916_change_reg_bits(
        handle,
        ST25R3916_REG_ISO14443A_NFC,
        (ST25R3916_REG_ISO14443A_NFC_no_tx_par | ST25R3916_REG_ISO14443A_NFC_no_rx_par),
        (ST25R3916_REG_ISO14443A_NFC_no_tx_par_off | ST25R3916_REG_ISO14443A_NFC_no_rx_par_off));
    uint32_t interrupts =
        (ST25R3916_IRQ_MASK_FWL | ST25R3916_IRQ_MASK_TXE | ST25R3916_IRQ_MASK_RXS |
         ST25R3916_IRQ_MASK_RXE | ST25R3916_IRQ_MASK_PAR | ST25R3916_IRQ_MASK_CRC |
         ST25R3916_IRQ_MASK_ERR1 | ST25R3916_IRQ_MASK_ERR2 | ST25R3916_IRQ_MASK_NRE);
    // Clear interrupts
    st25r3916_get_irq(handle);
    // Enable interrupts
    st25r3916_mask_irq(handle, ~interrupts);

    st25r3916_write_fifo(handle, tx_data, tx_bits);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_TRANSMIT_WITHOUT_CRC);

    return err;
}

FHalNfcError f_hal_nfc_listener_tx(const uint8_t* tx_data, size_t tx_bits) {
    furi_assert(tx_data);

    FHalNfcError err = FHalNfcErrorNone;
    FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    st25r3916_direct_cmd(handle, ST25R3916_CMD_CLEAR_FIFO);

    st25r3916_write_fifo(handle, tx_data, tx_bits);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_TRANSMIT_WITHOUT_CRC);

    return err;
}

FHalNfcError f_hal_nfc_poller_rx(uint8_t* rx_data, size_t rx_data_size, size_t* rx_bits) {
    furi_assert(rx_data);
    furi_assert(rx_bits);

    FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;
    FHalNfcError error = FHalNfcErrorNone;

    if(!st25r3916_read_fifo(handle, rx_data, rx_data_size, rx_bits)) {
        error = FHalNfcErrorBufferOverflow;
    }

    return error;
}

FHalNfcError f_hal_nfc_trx_reset() {
    FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    st25r3916_direct_cmd(handle, ST25R3916_CMD_STOP);

    return FHalNfcErrorNone;
}

FHalNfcError f_hal_nfc_listen_start() {
    FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

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

    return FHalNfcErrorNone;
}

FHalNfcError f_hal_nfc_listen_reset() {
    FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    st25r3916_direct_cmd(handle, ST25R3916_CMD_UNMASK_RECEIVE_DATA);

    return FHalNfcErrorNone;
}

FHalNfcError f_hal_nfc_listener_sleep() {
    FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    // Enable auto collision resolution
    st25r3916_clear_reg_bits(
        handle, ST25R3916_REG_PASSIVE_TARGET, ST25R3916_REG_PASSIVE_TARGET_d_106_ac_a);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_STOP);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_GOTO_SLEEP);

    return FHalNfcErrorNone;
}

FHalNfcError f_hal_nfc_listener_disable_auto_col_res() {
    FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    st25r3916_set_reg_bits(
        handle, ST25R3916_REG_PASSIVE_TARGET, ST25R3916_REG_PASSIVE_TARGET_d_106_ac_a);

    return FHalNfcErrorNone;
}

void f_hal_nfc_set_mask_receive_timer(uint32_t time_fc) {
    FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    st25r3916_write_reg(handle, ST25R3916_REG_MASK_RX_TIMER, time_fc);
}
