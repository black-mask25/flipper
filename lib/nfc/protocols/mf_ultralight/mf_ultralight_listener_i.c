#include "mf_ultralight_listener_i.h"

#include <furi.h>

#define MF_ULTRALIGHT_I2C_PAGE_IN_BOUNDS(page, start, end) (page >= start && page <= end)

#define MF_ULTRALIGHT_I2C_PAGE_ON_SESSION_REG(page) \
    MF_ULTRALIGHT_I2C_PAGE_IN_BOUNDS(page, 0x00EC, 0x00ED)

#define MF_ULTRALIGHT_I2C_PAGE_ON_MIRRORED_SESSION_REG(page) \
    MF_ULTRALIGHT_I2C_PAGE_IN_BOUNDS(page, 0x00F8, 0x00F9)

static MfUltralightMirrorConf mf_ultralight_mirror_check_mode(
    const MfUltralightConfigPages* const config,
    const MfUltralightListenerAuthState auth_state) {
    MfUltralightMirrorConf mirror_mode = config->mirror.mirror_conf;

    if(mirror_mode == MfUltralightMirrorNone || mirror_mode == MfUltralightMirrorUid)
        return mirror_mode;

    if(!config->access.nfc_cnt_en ||
       (config->access.nfc_cnt_pwd_prot && auth_state != MfUltralightListenerAuthStateSuccess)) {
        mirror_mode = mirror_mode == MfUltralightMirrorCounter ? MfUltralightMirrorNone :
                                                                 MfUltralightMirrorUid;
    }
    return mirror_mode;
}

static bool mf_ultralight_mirror_check_boundaries(MfUltralightListener* instance) {
    const MfUltralightConfigPages* const conf = instance->config;

    uint8_t last_user_page = mf_ultralight_get_config_page_num(instance->data->type) - 2;

    uint8_t max_page_offset = 0;
    uint8_t max_byte_offset = 2;

    MfUltralightMirrorConf mode = mf_ultralight_mirror_check_mode(conf, instance->auth_state);

    bool result = false;
    bool again = false;
    do {
        if(mode == MfUltralightMirrorNone) {
            break;
        } else if(mode == MfUltralightMirrorUid) {
            max_page_offset = 3;
        } else if(mode == MfUltralightMirrorCounter) {
            max_page_offset = 1;
        } else if(mode == MfUltralightMirrorUidCounter) {
            max_page_offset = 5;
            max_byte_offset = 3;
        }

        instance->mirror.actual_mode = mode;

        if(conf->mirror_page <= 3) break;
        if(conf->mirror_page < last_user_page - max_page_offset) {
            result = true;
            break;
        }
        if(conf->mirror_page == last_user_page - max_page_offset) {
            result = (conf->mirror.mirror_byte <= max_byte_offset);
            break;
        }

        if(conf->mirror_page > last_user_page - max_page_offset &&
           mode == MfUltralightMirrorUidCounter) {
            mode = MfUltralightMirrorUid;
            again = true;
        }
    } while(again);

    return result;
}

static bool mf_ultralight_mirror_enabled(MfUltralightListener* instance) {
    bool mirror_enabled = false;
    if(mf_ultralight_support_feature(instance->features, MfUltralightFeatureSupportAsciiMirror) &&
       (instance->config != NULL) && mf_ultralight_mirror_check_boundaries(instance)) {
        mirror_enabled = true;
    }
    instance->mirror.enabled = mirror_enabled;
    return instance->mirror.enabled;
}

static uint8_t mf_ultralight_get_mirror_data_size(MfUltralightMirrorConf mode) {
    switch(mode) {
    case MfUltralightMirrorUid:
        return 14;
    case MfUltralightMirrorCounter:
        return 6;
    case MfUltralightMirrorUidCounter:
        return 21;
    default:
        return 0;
    }
}

static uint8_t mf_ultralight_get_mirror_last_page(MfUltralightListener* instance) {
    uint8_t strSize = mf_ultralight_get_mirror_data_size(instance->mirror.actual_mode);
    return (instance->config->mirror_page + 1U + strSize / 4);
}

static uint8_t mf_ultralight_get_ascii_offset(uint8_t start_page, MfUltralightListener* instance) {
    uint8_t start_offset = 0;
    if(instance->config->mirror.mirror_conf == MfUltralightMirrorCounter) start_offset = 15;

    uint8_t ascii_offset = start_offset;

    if(start_page > instance->config->mirror_page)
        ascii_offset = (start_page - instance->config->mirror_page) * 4 -
                       instance->config->mirror.mirror_byte + start_offset;

    return ascii_offset;
}

static uint8_t mf_ultralight_get_ascii_end(MfUltralightMirrorConf mode) {
    return (mode == MfUltralightMirrorUid) ? 14 : 21;
}

static uint8_t mf_ultralight_get_byte_offset(
    uint8_t current_page,
    const MfUltralightConfigPages* const config) {
    return (current_page > config->mirror_page) ? 0 : config->mirror.mirror_byte;
}

static void mf_ultraligt_format_mirror_data(
    FuriString* str,
    const uint8_t* const data,
    const uint8_t data_len) {
    for(uint8_t i = 0; i < data_len; i++) furi_string_cat_printf(str, "%02X", data[i]);
}

void mf_ultralight_mirror_read_prepare(uint8_t start_page, MfUltralightListener* instance) {
    if(mf_ultralight_mirror_enabled(instance)) {
        instance->mirror.ascii_offset = mf_ultralight_get_ascii_offset(start_page, instance);
        instance->mirror.ascii_end = mf_ultralight_get_ascii_end(instance->mirror.actual_mode);

        instance->mirror.mirror_last_page = mf_ultralight_get_mirror_last_page(instance);
    }
}

void mf_ultralight_mirror_read_handler(
    uint8_t mirror_page_num,
    uint8_t* dest,
    MfUltralightListener* instance) {
    if(instance->mirror.enabled && mirror_page_num >= instance->config->mirror_page &&
       mirror_page_num <= instance->mirror.mirror_last_page) {
        uint8_t byte_offset = mf_ultralight_get_byte_offset(mirror_page_num, instance->config);

        uint8_t ascii_offset = instance->mirror.ascii_offset;
        uint8_t ascii_end = instance->mirror.ascii_end;
        uint8_t* source = (uint8_t*)furi_string_get_cstr(instance->mirror.ascii_mirror_data);
        for(uint8_t j = byte_offset; (j < 4) && (ascii_offset < ascii_end); j++) {
            dest[j] = source[ascii_offset];
            ascii_offset++;
        }
        instance->mirror.ascii_offset = ascii_offset;
    }
}

void mf_ultraligt_mirror_prepare_emulation(MfUltralightListener* instance) {
    mf_ultraligt_format_mirror_data(
        instance->mirror.ascii_mirror_data,
        instance->data->iso14443_3a_data->uid,
        instance->data->iso14443_3a_data->uid_len);

    furi_string_push_back(instance->mirror.ascii_mirror_data, 'x');

    mf_ultraligt_format_mirror_data(
        instance->mirror.ascii_mirror_data,
        instance->data->counter[2].data,
        sizeof(instance->data->counter[2].data));
}

bool mf_ultralight_composite_command_in_progress(MfUltralightListener* instance) {
    return (instance->composite_cmd.callback != NULL);
}

MfUltralightCommand
    mf_ultralight_composite_command_run(MfUltralightListener* instance, BitBuffer* buffer) {
    MfUltralightCommand command = (instance->composite_cmd.callback)(instance, buffer);
    mf_ultralight_composite_command_reset(instance);
    return command;
}

void mf_ultralight_composite_command_reset(MfUltralightListener* instance) {
    instance->composite_cmd.callback = NULL;
    instance->composite_cmd.data = 0;
}

void mf_ultralight_composite_command_set_next(
    MfUltralightListener* instance,
    const MfUltralightListenerCommandCallback handler) {
    instance->composite_cmd.callback = handler;
}

void mf_ultralight_single_counter_try_increase(MfUltralightListener* instance) {
    if(mf_ultralight_support_feature(instance->features, MfUltralightFeatureSupportSingleCounter) &&
       instance->config->access.nfc_cnt_en && !instance->single_counter_increased) {
        instance->data->counter[2].counter++;
        instance->single_counter_increased = true;
    }
}

void mf_ultralight_single_counter_try_to_unlock(
    MfUltralightListener* instance,
    Iso14443_3aListenerEventType type) {
    if(mf_ultralight_support_feature(instance->features, MfUltralightFeatureSupportSingleCounter) &&
       type == Iso14443_3aListenerEventTypeFieldOff) {
        instance->single_counter_increased = false;
    }
}

static bool mf_ultralight_i2c_page_validator_for_sector0(
    uint16_t start_page,
    uint16_t end_page,
    MfUltralightType type) {
    UNUSED(type);
    bool valid = false;
    if(type == MfUltralightTypeNTAGI2CPlus1K || type == MfUltralightTypeNTAGI2CPlus2K) {
        if(start_page <= 0xE9 && end_page <= 0xE9) {
            valid = true;
        } else if(
            MF_ULTRALIGHT_I2C_PAGE_ON_SESSION_REG(start_page) &&
            MF_ULTRALIGHT_I2C_PAGE_ON_SESSION_REG(end_page)) {
            valid = true;
        }
    } else if(type == MfUltralightTypeNTAGI2C1K) {
        if((start_page <= 0xE2) || MF_ULTRALIGHT_I2C_PAGE_IN_BOUNDS(start_page, 0x00E8, 0x00E9)) {
            valid = true;
        }
    } else if(type == MfUltralightTypeNTAGI2C2K) {
        valid = (start_page <= 0xFF && end_page <= 0xFF);
    }

    return valid;
}

static bool mf_ultralight_i2c_page_validator_for_sector1(
    uint16_t start_page,
    uint16_t end_page,
    MfUltralightType type) {
    bool valid = false;
    if(type == MfUltralightTypeNTAGI2CPlus2K) {
        valid = (start_page <= 0xFF && end_page <= 0xFF);
    } else if(type == MfUltralightTypeNTAGI2C2K) {
        valid =
            (MF_ULTRALIGHT_I2C_PAGE_IN_BOUNDS(start_page, 0x00E8, 0x00E9) || (start_page <= 0xE0));
    } else if(type == MfUltralightTypeNTAGI2C1K || type == MfUltralightTypeNTAGI2CPlus1K) {
        valid = false;
    }

    return valid;
}

static bool mf_ultralight_i2c_page_validator_for_sector2(
    uint16_t start_page,
    uint16_t end_page,
    MfUltralightType type) {
    UNUSED(start_page);
    UNUSED(end_page);
    UNUSED(type);
    return false;
}

static bool mf_ultralight_i2c_page_validator_for_sector3(
    uint16_t start_page,
    uint16_t end_page,
    MfUltralightType type) {
    UNUSED(type);
    UNUSED(end_page);
    return MF_ULTRALIGHT_I2C_PAGE_ON_MIRRORED_SESSION_REG(start_page);
}

typedef bool (
    *MfUltralightI2CValidator)(uint16_t start_page, uint16_t end_page, MfUltralightType type);

typedef uint16_t (*MfUltralightI2CPageProvider)(uint16_t page, MfUltralightType type);

const MfUltralightI2CValidator validation_methods[] = {
    mf_ultralight_i2c_page_validator_for_sector0,
    mf_ultralight_i2c_page_validator_for_sector1,
    mf_ultralight_i2c_page_validator_for_sector2,
    mf_ultralight_i2c_page_validator_for_sector3,
};

bool mf_ultralight_i2c_validate_pages(
    uint16_t start_page,
    uint16_t end_page,
    MfUltralightListener* instance) {
    bool valid = false;
    if(instance->sector < sizeof(validation_methods)) {
        MfUltralightI2CValidator validate = validation_methods[instance->sector];
        valid = validate(start_page, end_page, instance->data->type);
    }
    return valid;
}

bool mf_ultralight_is_i2c_tag(MfUltralightType type) {
    return type == MfUltralightTypeNTAGI2C1K || type == MfUltralightTypeNTAGI2C2K ||
           type == MfUltralightTypeNTAGI2CPlus1K || type == MfUltralightTypeNTAGI2CPlus2K;
}

static uint16_t mf_ultralight_i2c_page_provider_for_sector0(uint16_t page, MfUltralightType type) {
    uint8_t new_page = page;
    if(type == MfUltralightTypeNTAGI2CPlus1K || type == MfUltralightTypeNTAGI2CPlus2K) {
        if(page == 0x00EC) {
            new_page = 234;
        } else if(page == 0x00ED) {
            new_page = 235;
        }
    } else if(type == MfUltralightTypeNTAGI2C1K) {
        new_page = (page == 0x00E8) ? 232 : page;
        new_page = (page == 0x00E9) ? 233 : page;
    } else if(type == MfUltralightTypeNTAGI2C2K) {
        new_page = page;
    }
    return new_page;
}

static uint16_t mf_ultralight_i2c_page_provider_for_sector1(uint16_t page, MfUltralightType type) {
    UNUSED(type);
    uint16_t new_page = page;
    if(type == MfUltralightTypeNTAGI2CPlus2K) new_page = page + 236;
    if(type == MfUltralightTypeNTAGI2C2K) new_page = page + 255;
    return new_page;
}

static uint16_t mf_ultralight_i2c_page_provider_for_sector2(uint16_t page, MfUltralightType type) {
    UNUSED(type);
    return page;
}

static uint16_t mf_ultralight_i2c_page_provider_for_sector3(uint16_t page, MfUltralightType type) {
    uint16_t new_page = page;
    if(type == MfUltralightTypeNTAGI2CPlus1K || type == MfUltralightTypeNTAGI2CPlus2K) {
        if(page == 0x00F8) new_page = 234;
        if(page == 0x00F9) new_page = 235;
    } else if(type == MfUltralightTypeNTAGI2C1K || type == MfUltralightTypeNTAGI2C2K) {
        if(page == 0x00F8) new_page = (type == MfUltralightTypeNTAGI2C1K) ? 227 : 481;
        if(page == 0x00F9) new_page = (type == MfUltralightTypeNTAGI2C1K) ? 228 : 482;
    }
    return new_page;
}

const MfUltralightI2CPageProvider provider_methods[] = {
    mf_ultralight_i2c_page_provider_for_sector0,
    mf_ultralight_i2c_page_provider_for_sector1,
    mf_ultralight_i2c_page_provider_for_sector2,
    mf_ultralight_i2c_page_provider_for_sector3,
};

uint16_t
    mf_ultralight_i2c_provide_page_by_requested(uint16_t page, MfUltralightListener* instance) {
    uint16_t result = page;
    if(instance->sector < sizeof(provider_methods)) {
        MfUltralightI2CPageProvider provider = provider_methods[instance->sector];
        result = provider(page, instance->data->type);
    }
    return result;
}