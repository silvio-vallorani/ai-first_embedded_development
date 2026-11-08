#include "release_manifest.h"

#include <string.h>

bool aied_manifest_validate(const aied_release_manifest_t *manifest,
                            uint32_t expected_hardware,
                            uint32_t bootloader_version,
                            size_t cm33_capacity,
                            size_t cm7_capacity,
                            aied_signature_verify_fn verify_signature,
                            void *context)
{
    if ((manifest == NULL) || (verify_signature == NULL)) {
        return false;
    }

    if ((manifest->magic != AIED_MANIFEST_MAGIC) || (manifest->schema_version != 1U)) {
        return false;
    }

    if ((manifest->hardware_compatibility != expected_hardware) ||
        (manifest->minimum_bootloader_version > bootloader_version)) {
        return false;
    }

    if ((manifest->cm33.image_size == 0U) || (manifest->cm7.image_size == 0U) ||
        ((size_t)manifest->cm33.image_size > cm33_capacity) ||
        ((size_t)manifest->cm7.image_size > cm7_capacity)) {
        return false;
    }

    if ((memcmp(manifest->cm33.sha256, (const uint8_t[AIED_SHA256_SIZE]){0}, AIED_SHA256_SIZE) == 0) ||
        (memcmp(manifest->cm7.sha256, (const uint8_t[AIED_SHA256_SIZE]){0}, AIED_SHA256_SIZE) == 0)) {
        return false;
    }

    return verify_signature(manifest, context);
}
