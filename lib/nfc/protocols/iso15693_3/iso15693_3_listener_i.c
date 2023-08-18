#include "iso15693_3_listener_i.h"

#include <nfc/helpers/iso13239_crc.h>

#define TAG "Iso15693_3Listener"

typedef Iso15693_3Error (*Iso15693_3RequestHandler)(
    Iso15693_3Listener* instance,
    const uint8_t* data,
    size_t data_size,
    uint8_t flags);

static Iso15693_3Error iso15693_3_listener_inventory_handler(
    Iso15693_3Listener* instance,
    const uint8_t* data,
    size_t data_size,
    uint8_t flags) {
    Iso15693_3Error error = Iso15693_3ErrorNone;

    do {
        const bool afi_flag = flags & ISO15693_3_REQ_FLAG_T5_AFI_PRESENT;
        const size_t data_size_min = sizeof(uint8_t) * (afi_flag ? 2 : 1);

        if(data_size < data_size_min) {
            error = Iso15693_3ErrorFormat;
            break;
        }

        if(afi_flag) {
            const uint8_t afi = *data++;
            // When AFI flag is set, ignore non-matching requests
            if(afi != instance->data->system_info.afi) break;
        }

        const uint8_t mask_len = *data++;
        const size_t data_size_required = data_size_min + mask_len;

        if(data_size != data_size_required) {
            error = Iso15693_3ErrorFormat;
            break;
        }

        if(mask_len != 0) {
            // TODO: Take mask_len and mask_value into account (if present)
        }

        bit_buffer_append_byte(instance->tx_buffer, instance->data->system_info.dsfid); // DSFID
        iso15693_3_append_uid(instance->data, instance->tx_buffer); // UID
    } while(false);

    instance->session_state.no_reply = (error != Iso15693_3ErrorNone);
    return error;
}

static Iso15693_3Error iso15693_3_listener_stay_quiet_handler(
    Iso15693_3Listener* instance,
    const uint8_t* data,
    size_t data_size,
    uint8_t flags) {
    UNUSED(data);
    UNUSED(data_size);
    UNUSED(flags);

    instance->state = Iso15693_3ListenerStateQuiet;
    instance->session_state.no_reply = true;
    return Iso15693_3ErrorNone;
}

static Iso15693_3Error iso15693_3_listener_read_block_handler(
    Iso15693_3Listener* instance,
    const uint8_t* data,
    size_t data_size,
    uint8_t flags) {
    Iso15693_3Error error = Iso15693_3ErrorNone;

    do {
        typedef struct {
            uint8_t block_num;
        } Iso15693_3ReadBlockRequestLayout;

        const Iso15693_3ReadBlockRequestLayout* request =
            (const Iso15693_3ReadBlockRequestLayout*)data;

        if(data_size != sizeof(Iso15693_3ReadBlockRequestLayout)) {
            error = Iso15693_3ErrorFormat;
            break;
        }

        const uint32_t block_index = request->block_num;
        const uint32_t block_count_max = instance->data->system_info.block_count;

        if(block_index >= block_count_max) {
            error = Iso15693_3ErrorInternal;
            break;
        }

        if(flags & ISO15693_3_REQ_FLAG_T4_OPTION) {
            iso15693_3_append_block_security(
                instance->data, block_index, instance->tx_buffer); // Block security (optional)
        }

        iso15693_3_append_block(instance->data, block_index, instance->tx_buffer); // Block data
    } while(false);

    return error;
}

static Iso15693_3Error iso15693_3_listener_write_block_handler(
    Iso15693_3Listener* instance,
    const uint8_t* data,
    size_t data_size,
    uint8_t flags) {
    Iso15693_3Error error = Iso15693_3ErrorNone;

    do {
        typedef struct {
            uint8_t block_num;
            uint8_t block_data[];
        } Iso15693_3WriteBlockRequestLayout;

        const Iso15693_3WriteBlockRequestLayout* request =
            (const Iso15693_3WriteBlockRequestLayout*)data;

        instance->session_state.wait_for_eof = flags & ISO15693_3_REQ_FLAG_T4_OPTION;

        if(data_size <= sizeof(Iso15693_3WriteBlockRequestLayout)) {
            error = Iso15693_3ErrorFormat;
            break;
        }

        const uint32_t block_index = request->block_num;
        const uint32_t block_count_max = instance->data->system_info.block_count;
        const uint32_t block_size_max = instance->data->system_info.block_size;
        const size_t block_size_received = data_size - sizeof(Iso15693_3WriteBlockRequestLayout);

        if(block_index >= block_count_max) {
            error = Iso15693_3ErrorInternal;
            break;
        } else if(block_size_received != block_size_max) {
            error = Iso15693_3ErrorInternal;
            break;
        } else if(iso15693_3_is_block_locked(instance->data, block_index)) {
            error = Iso15693_3ErrorInternal;
            break;
        }

        iso15693_3_set_block_data(
            instance->data, block_index, request->block_data, block_size_received);
    } while(false);

    return error;
}

static Iso15693_3Error iso15693_3_listener_lock_block_handler(
    Iso15693_3Listener* instance,
    const uint8_t* data,
    size_t data_size,
    uint8_t flags) {
    Iso15693_3Error error = Iso15693_3ErrorNone;

    do {
        typedef struct {
            uint8_t block_num;
        } Iso15693_3LockBlockRequestLayout;

        const Iso15693_3LockBlockRequestLayout* request =
            (const Iso15693_3LockBlockRequestLayout*)data;

        instance->session_state.wait_for_eof = flags & ISO15693_3_REQ_FLAG_T4_OPTION;

        if(data_size != sizeof(Iso15693_3LockBlockRequestLayout)) {
            error = Iso15693_3ErrorFormat;
            break;
        }

        const uint32_t block_index = request->block_num;
        const uint32_t block_count_max = instance->data->system_info.block_count;

        if(block_index >= block_count_max) {
            error = Iso15693_3ErrorInternal;
            break;
        } else if(iso15693_3_is_block_locked(instance->data, block_index)) {
            error = Iso15693_3ErrorInternal;
            break;
        }

        iso15693_3_set_block_locked(instance->data, block_index, true);
    } while(false);

    return error;
}

static Iso15693_3Error iso15693_3_listener_read_multi_blocks_handler(
    Iso15693_3Listener* instance,
    const uint8_t* data,
    size_t data_size,
    uint8_t flags) {
    Iso15693_3Error error = Iso15693_3ErrorNone;

    do {
        typedef struct {
            uint8_t first_block_num;
            uint8_t block_count;
        } Iso15693_3ReadMultiBlocksRequestLayout;

        const Iso15693_3ReadMultiBlocksRequestLayout* request =
            (const Iso15693_3ReadMultiBlocksRequestLayout*)data;

        if(data_size != sizeof(Iso15693_3ReadMultiBlocksRequestLayout)) {
            error = Iso15693_3ErrorFormat;
            break;
        }

        const uint32_t block_index_start = request->first_block_num;
        const uint32_t block_index_end = block_index_start + request->block_count;

        const uint32_t block_count = request->block_count + 1;
        const uint32_t block_count_max = instance->data->system_info.block_count;
        const uint32_t block_count_available = block_count_max - block_index_start;

        if(block_count > block_count_available) {
            error = Iso15693_3ErrorInternal;
            break;
        }

        for(uint32_t i = block_index_start; i <= block_index_end; ++i) {
            if(flags & ISO15693_3_REQ_FLAG_T4_OPTION) {
                iso15693_3_append_block_security(
                    instance->data, i, instance->tx_buffer); // Block security (optional)
            }
            iso15693_3_append_block(instance->data, i, instance->tx_buffer); // Block data
        }
    } while(false);

    return error;
}

static Iso15693_3Error iso15693_3_listener_write_multi_blocks_handler(
    Iso15693_3Listener* instance,
    const uint8_t* data,
    size_t data_size,
    uint8_t flags) {
    Iso15693_3Error error = Iso15693_3ErrorNone;

    do {
        typedef struct {
            uint8_t first_block_num;
            uint8_t block_count;
            uint8_t block_data[];
        } Iso15693_3WriteMultiBlocksRequestLayout;

        const Iso15693_3WriteMultiBlocksRequestLayout* request =
            (const Iso15693_3WriteMultiBlocksRequestLayout*)data;

        instance->session_state.wait_for_eof = flags & ISO15693_3_REQ_FLAG_T4_OPTION;

        if(data_size <= sizeof(Iso15693_3WriteMultiBlocksRequestLayout)) {
            error = Iso15693_3ErrorFormat;
            break;
        }

        const uint32_t block_index_start = request->first_block_num;
        const uint32_t block_index_end = block_index_start + request->block_count;

        const uint32_t block_count = request->block_count + 1;
        const uint32_t block_count_max = instance->data->system_info.block_count;
        const uint32_t block_count_available = block_count_max - block_index_start;

        const size_t block_data_size = data_size - sizeof(Iso15693_3WriteMultiBlocksRequestLayout);
        const size_t block_size = block_data_size / block_count;
        const size_t block_size_max = instance->data->system_info.block_size;

        if(block_count > block_count_available) {
            error = Iso15693_3ErrorInternal;
            break;
        } else if(block_size != block_size_max) {
            error = Iso15693_3ErrorInternal;
            break;
        }

        for(uint32_t i = block_index_start; i <= block_index_end; ++i) {
            if(iso15693_3_is_block_locked(instance->data, i)) {
                error = Iso15693_3ErrorInternal;
                break;
            }
        }

        if(error != Iso15693_3ErrorNone) break;

        for(uint32_t i = block_index_start; i < block_count + request->first_block_num; ++i) {
            const uint8_t* block_data = &request->block_data[block_size * i];
            iso15693_3_set_block_data(instance->data, i, block_data, block_size);
        }
    } while(false);

    return error;
}

static Iso15693_3Error iso15693_3_listener_select_handler(
    Iso15693_3Listener* instance,
    const uint8_t* data,
    size_t data_size,
    uint8_t flags) {
    UNUSED(data);
    UNUSED(data_size);

    Iso15693_3Error error = Iso15693_3ErrorNone;

    do {
        if(!(flags & ISO15693_3_REQ_FLAG_T4_ADDRESSED)) {
            instance->session_state.no_reply = true;
            error = Iso15693_3ErrorUnknown;
            break;
        }

        instance->state = Iso15693_3ListenerStateSelected;
    } while(false);

    return error;
}

static Iso15693_3Error iso15693_3_listener_reset_to_ready_handler(
    Iso15693_3Listener* instance,
    const uint8_t* data,
    size_t data_size,
    uint8_t flags) {
    UNUSED(data);
    UNUSED(data_size);
    UNUSED(flags);

    instance->state = Iso15693_3ListenerStateReady;
    return Iso15693_3ErrorNone;
}

static Iso15693_3Error iso15693_3_listener_write_afi_handler(
    Iso15693_3Listener* instance,
    const uint8_t* data,
    size_t data_size,
    uint8_t flags) {
    Iso15693_3Error error = Iso15693_3ErrorNone;

    do {
        typedef struct {
            uint8_t afi;
        } Iso15693_3WriteAfiRequestLayout;

        const Iso15693_3WriteAfiRequestLayout* request =
            (const Iso15693_3WriteAfiRequestLayout*)data;

        instance->session_state.wait_for_eof = flags & ISO15693_3_REQ_FLAG_T4_OPTION;

        if(data_size <= sizeof(Iso15693_3WriteAfiRequestLayout)) {
            error = Iso15693_3ErrorFormat;
            break;
        } else if(instance->data->system_info.flags & ISO15693_3_SYSINFO_LOCK_AFI) {
            error = Iso15693_3ErrorInternal;
            break;
        }

        instance->data->system_info.afi = request->afi;
    } while(false);

    return error;
}

static Iso15693_3Error iso15693_3_listener_lock_afi_handler(
    Iso15693_3Listener* instance,
    const uint8_t* data,
    size_t data_size,
    uint8_t flags) {
    UNUSED(data);
    UNUSED(data_size);

    Iso15693_3Error error = Iso15693_3ErrorNone;

    do {
        instance->session_state.wait_for_eof = flags & ISO15693_3_REQ_FLAG_T4_OPTION;

        if(instance->data->system_info.flags & ISO15693_3_SYSINFO_LOCK_AFI) {
            error = Iso15693_3ErrorInternal;
            break;
        }

        instance->data->system_info.flags |= ISO15693_3_SYSINFO_LOCK_AFI;
    } while(false);

    return error;
}

static Iso15693_3Error iso15693_3_listener_write_dsfid_handler(
    Iso15693_3Listener* instance,
    const uint8_t* data,
    size_t data_size,
    uint8_t flags) {
    Iso15693_3Error error = Iso15693_3ErrorNone;

    do {
        typedef struct {
            uint8_t dsfid;
        } Iso15693_3WriteDsfidRequestLayout;

        const Iso15693_3WriteDsfidRequestLayout* request =
            (const Iso15693_3WriteDsfidRequestLayout*)data;

        instance->session_state.wait_for_eof = flags & ISO15693_3_REQ_FLAG_T4_OPTION;

        if(data_size <= sizeof(Iso15693_3WriteDsfidRequestLayout)) {
            error = Iso15693_3ErrorFormat;
            break;
        } else if(instance->data->system_info.flags & ISO15693_3_SYSINFO_LOCK_DSFID) {
            error = Iso15693_3ErrorInternal;
            break;
        }

        instance->data->system_info.dsfid = request->dsfid;
    } while(false);

    return error;
}

static Iso15693_3Error iso15693_3_listener_lock_dsfid_handler(
    Iso15693_3Listener* instance,
    const uint8_t* data,
    size_t data_size,
    uint8_t flags) {
    UNUSED(data);
    UNUSED(data_size);

    Iso15693_3Error error = Iso15693_3ErrorNone;

    do {
        instance->session_state.wait_for_eof = flags & ISO15693_3_REQ_FLAG_T4_OPTION;

        if(instance->data->system_info.flags & ISO15693_3_SYSINFO_LOCK_DSFID) {
            error = Iso15693_3ErrorInternal;
            break;
        }

        instance->data->system_info.flags |= ISO15693_3_SYSINFO_LOCK_DSFID;
    } while(false);

    return error;
}

static Iso15693_3Error iso15693_3_listener_get_system_info_handler(
    Iso15693_3Listener* instance,
    const uint8_t* data,
    size_t data_size,
    uint8_t flags) {
    UNUSED(data);
    UNUSED(data_size);
    UNUSED(flags);

    Iso15693_3Error error = Iso15693_3ErrorNone;

    do {
        const uint8_t system_flags = instance->data->system_info.flags;
        bit_buffer_append_byte(instance->tx_buffer, system_flags); // System info flags

        iso15693_3_append_uid(instance->data, instance->tx_buffer); // UID

        if(system_flags & ISO15693_3_SYSINFO_FLAG_DSFID) {
            bit_buffer_append_byte(instance->tx_buffer, instance->data->system_info.dsfid);
        }
        if(system_flags & ISO15693_3_SYSINFO_FLAG_AFI) {
            bit_buffer_append_byte(instance->tx_buffer, instance->data->system_info.afi);
        }
        if(system_flags & ISO15693_3_SYSINFO_FLAG_MEMORY) {
            const uint8_t memory_info[2] = {
                instance->data->system_info.block_count - 1,
                instance->data->system_info.block_size - 1,
            };
            bit_buffer_append_bytes(instance->tx_buffer, memory_info, COUNT_OF(memory_info));
        }
        if(system_flags & ISO15693_3_SYSINFO_FLAG_IC_REF) {
            bit_buffer_append_byte(instance->tx_buffer, instance->data->system_info.ic_ref);
        }

    } while(false);

    return error;
}

static Iso15693_3Error iso15693_3_listener_get_multi_blocks_security_handler(
    Iso15693_3Listener* instance,
    const uint8_t* data,
    size_t data_size,
    uint8_t flags) {
    UNUSED(flags);

    Iso15693_3Error error = Iso15693_3ErrorNone;

    do {
        typedef struct {
            uint8_t first_block_num;
            uint8_t block_count;
        } Iso15693_3GetMultiBlocksSecurityRequestLayout;

        const Iso15693_3GetMultiBlocksSecurityRequestLayout* request =
            (const Iso15693_3GetMultiBlocksSecurityRequestLayout*)data;

        if(data_size < sizeof(Iso15693_3GetMultiBlocksSecurityRequestLayout)) {
            error = Iso15693_3ErrorFormat;
            break;
        }

        const uint32_t block_index_start = request->first_block_num;
        const uint32_t block_index_end = block_index_start + request->block_count;

        const uint32_t block_count_max = instance->data->system_info.block_count;

        if(block_index_end >= block_count_max) {
            error = Iso15693_3ErrorInternal;
            break;
        }

        for(uint32_t i = block_index_start; i <= block_index_end; ++i) {
            bit_buffer_append_byte(
                instance->tx_buffer, iso15693_3_is_block_locked(instance->data, i) ? 1 : 0);
        }
    } while(false);

    return error;
}

static const Iso15693_3RequestHandler iso15693_3_request_handlers[] = {
    // Mandatory commands
    iso15693_3_listener_inventory_handler,
    iso15693_3_listener_stay_quiet_handler,
    // Optional commands
    iso15693_3_listener_read_block_handler,
    iso15693_3_listener_write_block_handler,
    iso15693_3_listener_lock_block_handler,
    iso15693_3_listener_read_multi_blocks_handler,
    iso15693_3_listener_write_multi_blocks_handler,
    iso15693_3_listener_select_handler,
    iso15693_3_listener_reset_to_ready_handler,
    iso15693_3_listener_write_afi_handler,
    iso15693_3_listener_lock_afi_handler,
    iso15693_3_listener_write_dsfid_handler,
    iso15693_3_listener_lock_dsfid_handler,
    iso15693_3_listener_get_system_info_handler,
    iso15693_3_listener_get_multi_blocks_security_handler,
};

static inline Iso15693_3Error iso15693_3_listener_handle_request(
    Iso15693_3Listener* instance,
    const uint8_t* data,
    size_t data_size,
    uint8_t command,
    uint8_t flags) {
    Iso15693_3Error error = Iso15693_3ErrorNone;

    do {
        uint8_t command_index;

        if(command < ISO15693_3_CMD_MANDATORY_RFU) {
            command_index = command - ISO15693_3_CMD_MANDATORY_START;
        } else if(command >= ISO15693_3_CMD_OPTIONAL_START && command < ISO15693_3_CMD_OPTIONAL_RFU) {
            command_index = command - ISO15693_3_CMD_MANDATORY_START -
                            ISO15693_3_CMD_OPTIONAL_START + ISO15693_3_CMD_MANDATORY_RFU;
        } else {
            error = Iso15693_3ErrorNotSupported;
            break;
        }

        bit_buffer_reset(instance->tx_buffer);
        bit_buffer_append_byte(instance->tx_buffer, ISO15693_3_RESP_FLAG_NONE);

        error = iso15693_3_request_handlers[command_index](instance, data, data_size, flags);

        Iso15693_3ListenerSessionState* session_state = &instance->session_state;

        // Several commands may not require an answer
        if(session_state->no_reply) {
            session_state->no_reply = false;
            error = Iso15693_3ErrorNone;
            break;
        }

        // TODO: Move it to a separate function
        if(error != Iso15693_3ErrorNone) {
            bit_buffer_reset(instance->tx_buffer);
            bit_buffer_append_byte(instance->tx_buffer, ISO15693_3_RESP_FLAG_ERROR);
            bit_buffer_append_byte(instance->tx_buffer, ISO15693_3_RESP_ERROR_UNKNOWN);
        }

        if(!session_state->wait_for_eof) {
            error = iso15693_3_listener_send_frame(instance, instance->tx_buffer);
        }

    } while(false);

    return error;
}

Iso15693_3Error iso15693_3_listener_ready(Iso15693_3Listener* instance) {
    furi_assert(instance);
    instance->state = Iso15693_3ListenerStateReady;
    return Iso15693_3ErrorNone;
}

Iso15693_3Error iso15693_3_listener_sleep(Iso15693_3Listener* instance) {
    furi_assert(instance);
    instance->state = Iso15693_3ListenerStateIdle;
    return Iso15693_3ErrorNone;
}

static Iso15693_3Error iso15693_3_listener_process_nfc_error(NfcError error) {
    Iso15693_3Error ret = Iso15693_3ErrorNone;

    if(error == NfcErrorNone) {
        ret = Iso15693_3ErrorNone;
    } else if(error == NfcErrorTimeout) {
        ret = Iso15693_3ErrorTimeout;
    } else {
        ret = Iso15693_3ErrorFieldOff;
    }

    return ret;
}

Iso15693_3Error
    iso15693_3_listener_send_frame(Iso15693_3Listener* instance, const BitBuffer* tx_buffer) {
    furi_assert(instance);
    furi_assert(tx_buffer);

    bit_buffer_copy(instance->tx_buffer, tx_buffer);
    iso13239_crc_append(Iso13239CrcTypeDefault, instance->tx_buffer);

    NfcError error = nfc_listener_tx(instance->nfc, instance->tx_buffer);
    return iso15693_3_listener_process_nfc_error(error);
}

Iso15693_3Error
    iso15693_3_listener_process_request(Iso15693_3Listener* instance, const BitBuffer* rx_buffer) {
    Iso15693_3Error error = Iso15693_3ErrorNone;

    do {
        typedef struct {
            uint8_t flags;
            uint8_t command;
            uint8_t data[];
        } Iso15693_3RequestLayout;

        const size_t buf_size = bit_buffer_get_size_bytes(rx_buffer);
        const size_t buf_size_min = sizeof(Iso15693_3RequestLayout);

        if(buf_size < buf_size_min) {
            error = Iso15693_3ErrorFormat;
            break;
        }

        const Iso15693_3RequestLayout* request =
            (const Iso15693_3RequestLayout*)bit_buffer_get_data(rx_buffer);

        const bool inventory_flag = request->flags & ISO15693_3_REQ_FLAG_INVENTORY_T5;

        if(!inventory_flag) {
            const bool selected_mode = request->flags & ISO15693_3_REQ_FLAG_T4_SELECTED;
            const bool addressed_mode = request->flags & ISO15693_3_REQ_FLAG_T4_ADDRESSED;

            if(selected_mode && addressed_mode) {
                // A request mode can be either addressed or selected, but not both
                break;
            } else if(instance->state == Iso15693_3ListenerStateQuiet) {
                // If the card is quiet, ignore non-addressed commands
                if(!addressed_mode) break;
            } else if(instance->state != Iso15693_3ListenerStateSelected) {
                // If the card is not selected, ignore selected commands
                if(selected_mode) break;
            }

            const uint8_t* data;
            size_t data_size;

            if(addressed_mode) {
                // In addressed mode, UID must be included in each command
                const size_t buf_size_min_addr = buf_size_min + ISO15693_3_UID_SIZE;

                if(buf_size < buf_size_min_addr) {
                    error = Iso15693_3ErrorFormat;
                    break;
                } else if(!iso15693_3_is_equal_uid(instance->data, request->data)) {
                    // In addressed mode, ignore all commands with non-matching UID
                    if(instance->state == Iso15693_3ListenerStateSelected &&
                       request->command == ISO15693_3_CMD_SELECT) {
                        // Special case, reset to ready on reception of a
                        // SELECT command with non-matching UID
                        // TODO: Find a neater way to do this?
                        instance->state = Iso15693_3ListenerStateReady;
                    }
                    break;
                }

                data = &request->data[ISO15693_3_UID_SIZE];
                data_size = buf_size - buf_size_min_addr;

            } else {
                data = request->data;
                data_size = buf_size - buf_size_min;
            }

            error = iso15693_3_listener_handle_request(
                instance, data, data_size, request->command, request->flags);

        } else {
            // If the card is quiet, ignore INVENTORY commands
            if(instance->state == Iso15693_3ListenerStateQuiet) {
                break;
            }

            // Only the INVENTORY command is allowed with this flag set
            if(request->command != ISO15693_3_CMD_INVENTORY) {
                error = Iso15693_3ErrorUnknown;
                break;
            }

            error = iso15693_3_listener_handle_request(
                instance, request->data, buf_size - buf_size_min, request->command, request->flags);
        }

    } while(false);

    return error;
}
