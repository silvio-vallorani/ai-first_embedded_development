#ifndef AIED_RELEASE_MANIFEST_H
#define AIED_RELEASE_MANIFEST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AIED_RELEASE_ID_SIZE 16U
#define AIED_SHA256_SIZE     32U
#define AIED_MANIFEST_MAGIC  0x41494544UL

typedef enum { AIED_CORE_CM33 = 0, AIED_CORE_CM7 = 1 } aied_core_t;

typedef struct {
    uint32_t image_size;
    uint8_t sha256[AIED_SHA256_SIZE];
    uint32_t security_version;
} aied_image_descriptor_t;

typedef struct {
    uint32_t magic;
    uint16_t schema_version;
    uint16_t reserved;
    uint8_t release_id[AIED_RELEASE_ID_SIZE];
    uint32_t hardware_compatibility;
    uint32_t minimum_bootloader_version;
    aied_image_descriptor_t cm33;
    aied_image_descriptor_t cm7;
    uint8_t signature[64];
} aied_release_manifest_t;

typedef bool (*aied_signature_verify_fn)(const aied_release_manifest_t *manifest, void *context);

/** @brief Target-side acceptance policy for a release manifest. */
typedef struct {
    uint32_t expected_hardware;        /**< Hardware compatibility identifier of the target. */
    uint32_t bootloader_version;       /**< Version of the running bootloader. */
    uint32_t minimum_security_version; /**< Anti-rollback floor read from the hardware counter. */
    size_t cm33_capacity;              /**< Maximum CM33 image area in bytes. */
    size_t cm7_capacity;               /**< Maximum CM7 image area in bytes. */
} aied_manifest_policy_t;

/**
 * @brief Validate the structural and policy fields of a paired release manifest.
 * @note The anti-rollback floor must come from a hardware-backed monotonic counter;
 *       the manifest comparison alone is not rollback protection (ADR-0008).
 * @param manifest Manifest to validate.
 * @param policy Target acceptance policy.
 * @param verify_signature Signature verification callback.
 * @param context Opaque callback context.
 * @return true when the manifest is structurally valid, accepted by the policy and its
 *         signature is accepted.
 */
bool aied_manifest_validate(const aied_release_manifest_t *manifest, const aied_manifest_policy_t *policy,
                            aied_signature_verify_fn verify_signature, void *context);

#endif
