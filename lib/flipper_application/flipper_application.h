#pragma once

#include "application_manifest.h"
#include "elf/elf_api_interface.h"

#include <furi.h>
#include <storage/storage.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FlipperApplicationPreloadStatusSuccess = 0,
    FlipperApplicationPreloadStatusUnspecifiedError,
    FlipperApplicationPreloadStatusInvalidFile,
    FlipperApplicationPreloadStatusInvalidManifest,
    FlipperApplicationPreloadStatusApiMismatch,
    FlipperApplicationPreloadStatusTargetMismatch,
} FlipperApplicationPreloadStatus;

typedef enum {
    FlipperApplicationLoadStatusSuccess = 0,
    FlipperApplicationLoadStatusUnspecifiedError,
    FlipperApplicationLoadStatusNoFreeMemory,
    FlipperApplicationLoadStatusMissingImports,
} FlipperApplicationLoadStatus;

const char* flipper_application_preload_status_to_string(FlipperApplicationPreloadStatus status);
const char* flipper_application_load_status_to_string(FlipperApplicationLoadStatus status);

typedef struct FlipperApplication FlipperApplication;

typedef struct {
    const char* name;
    uint32_t address;
} FlipperApplicationMemoryMapEntry;

typedef struct {
    uint32_t mmap_entry_count;
    FlipperApplicationMemoryMapEntry* mmap_entries;
    uint32_t debug_link_size;
    uint8_t* debug_link;
} FlipperApplicationState;

FlipperApplication*
    flipper_application_alloc(Storage* storage, const ElfApiInterface* api_interface);

void flipper_application_free(FlipperApplication* app);

/* Parse headers, load manifest */
FlipperApplicationPreloadStatus
    flipper_application_preload(FlipperApplication* app, const char* path);

const FlipperApplicationManifest* flipper_application_get_manifest(FlipperApplication* app);

FlipperApplicationLoadStatus flipper_application_map_to_memory(FlipperApplication* app);

const FlipperApplicationState* flipper_application_get_state(FlipperApplication* app);

FuriThread* flipper_application_spawn(FlipperApplication* app, void* args);

FuriThread* flipper_application_get_thread(FlipperApplication* app);

void const* flipper_application_get_entry_address(FlipperApplication* app);

#ifdef __cplusplus
}
#endif