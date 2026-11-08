#include "release_manifest.h"

static bool is_all_zero(const uint8_t *data, size_t length)
{
    for (size_t i = 0U; i < length; i++) {
        if (data[i] != 0U) {
            return false;
        }
    }
    return true;
}

static bool image_is_acceptable(const aied_image_descriptor_t *image,
                                size_t capacity,
                                uint32_t minimum_security_version)
{
    return (image->image_size != 0U) && ((size_t)image->image_size <= capacity) &&
           !is_all_zero(image->sha256, AIED_SHA256_SIZE) &&
           (image->security_version >= minimum_security_version);
}

bool aied_manifest_validate(const aied_release_manifest_t *manifest,
                            const aied_manifest_policy_t *policy,
                            aied_signature_verify_fn verify_signature,
                            void *context)
{
    if ((manifest == NULL) || (policy == NULL) || (verify_signature == NULL)) {
        return false;
    }

    if ((manifest->magic != AIED_MANIFEST_MAGIC) || (manifest->schema_version != 1U) ||
        is_all_zero(manifest->release_id, AIED_RELEASE_ID_SIZE)) {
        return false;
    }

    if ((manifest->hardware_compatibility != policy->expected_hardware) ||
        (manifest->minimum_bootloader_version > policy->bootloader_version)) {
        return false;
    }

    if (!image_is_acceptable(&manifest->cm33, policy->cm33_capacity,
                             policy->minimum_security_version) ||
        !image_is_acceptable(&manifest->cm7, policy->cm7_capacity,
                             policy->minimum_security_version)) {
        return false;
    }

    return verify_signature(manifest, context);
}
