#include "test.h"
#include "release_manifest.h"

#include <string.h>

static bool signature_accept(const aied_release_manifest_t *manifest, void *context)
{
    (void)manifest;
    (void)context;
    return true;
}

static bool signature_reject(const aied_release_manifest_t *manifest, void *context)
{
    (void)manifest;
    (void)context;
    return false;
}

static const aied_manifest_policy_t base_policy = {
    .expected_hardware = 0x11890001U,
    .bootloader_version = 2U,
    .minimum_security_version = 3U,
    .cm33_capacity = 1024U,
    .cm7_capacity = 2048U
};

static aied_release_manifest_t valid_manifest(void)
{
    aied_release_manifest_t manifest = {0};
    manifest.magic = AIED_MANIFEST_MAGIC;
    manifest.schema_version = 1U;
    manifest.release_id[0] = 0x01U;
    manifest.hardware_compatibility = 0x11890001U;
    manifest.minimum_bootloader_version = 2U;
    manifest.cm33.image_size = 128U;
    manifest.cm33.security_version = 3U;
    manifest.cm7.image_size = 512U;
    manifest.cm7.security_version = 4U;
    (void)memset(manifest.cm33.sha256, 0xA5, sizeof(manifest.cm33.sha256));
    (void)memset(manifest.cm7.sha256, 0x5A, sizeof(manifest.cm7.sha256));
    return manifest;
}

static bool validate(const aied_release_manifest_t *manifest, const aied_manifest_policy_t *policy)
{
    return aied_manifest_validate(manifest, policy, signature_accept, NULL);
}

int test_release_manifest(void)
{
    aied_release_manifest_t manifest = valid_manifest();
    aied_manifest_policy_t policy = base_policy;

    TEST_ASSERT_TRUE(validate(&manifest, &policy));
    TEST_ASSERT_FALSE(validate(NULL, &policy));
    TEST_ASSERT_FALSE(validate(&manifest, NULL));
    TEST_ASSERT_FALSE(aied_manifest_validate(&manifest, &policy, NULL, NULL));
    TEST_ASSERT_FALSE(aied_manifest_validate(&manifest, &policy, signature_reject, NULL));

    policy.expected_hardware = 0x11890002U;
    TEST_ASSERT_FALSE(validate(&manifest, &policy));
    policy = base_policy;
    policy.bootloader_version = 1U;
    TEST_ASSERT_FALSE(validate(&manifest, &policy));
    policy = base_policy;
    policy.cm33_capacity = 64U;
    TEST_ASSERT_FALSE(validate(&manifest, &policy));
    policy = base_policy;
    policy.cm7_capacity = 511U;
    TEST_ASSERT_FALSE(validate(&manifest, &policy));
    policy = base_policy;
    policy.cm7_capacity = 512U;
    TEST_ASSERT_TRUE(validate(&manifest, &policy));

    /* Anti-rollback floor applies to each image of the pair. */
    policy = base_policy;
    policy.minimum_security_version = 4U;
    TEST_ASSERT_FALSE(validate(&manifest, &policy));
    manifest.cm33.security_version = 4U;
    TEST_ASSERT_TRUE(validate(&manifest, &policy));

    policy = base_policy;
    manifest = valid_manifest();
    manifest.magic ^= 1U;
    TEST_ASSERT_FALSE(validate(&manifest, &policy));
    manifest = valid_manifest();
    manifest.schema_version = 2U;
    TEST_ASSERT_FALSE(validate(&manifest, &policy));
    manifest = valid_manifest();
    manifest.release_id[0] = 0U;
    TEST_ASSERT_FALSE(validate(&manifest, &policy));
    manifest = valid_manifest();
    manifest.cm7.image_size = 0U;
    TEST_ASSERT_FALSE(validate(&manifest, &policy));
    manifest = valid_manifest();
    (void)memset(manifest.cm33.sha256, 0, sizeof(manifest.cm33.sha256));
    TEST_ASSERT_FALSE(validate(&manifest, &policy));
    manifest = valid_manifest();
    (void)memset(manifest.cm7.sha256, 0, sizeof(manifest.cm7.sha256));
    TEST_ASSERT_FALSE(validate(&manifest, &policy));
    return 0;
}
