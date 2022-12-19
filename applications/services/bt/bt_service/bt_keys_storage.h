#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "bt_keys_filename.h"

typedef struct BtKeysStorage BtKeysStorage;

BtKeysStorage* bt_keys_storage_alloc();

void bt_keys_storage_free(BtKeysStorage* instance);

void bt_keys_storage_set_file_path(BtKeysStorage* instance, const char* path);

void bt_keys_storage_set_ram_params(BtKeysStorage* instance, uint8_t* buff, uint16_t size);

bool bt_keys_storage_load(BtKeysStorage* instance);

bool bt_keys_storage_update(BtKeysStorage* instance, uint8_t* start_addr, uint32_t size);

bool bt_keys_storage_delete(BtKeysStorage* instance);
