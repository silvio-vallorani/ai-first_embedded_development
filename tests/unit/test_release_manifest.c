#include "test.h"
#include "release_manifest.h"

#include <string.h>

static bool signature_accept(const aied_release_manifest_t *manifest, void *context)
{
    (void)manifest;
    (void)context;
    return true;
}

int test_release_manifest(void)
{
    aied_release_manifest_t manifest = {0};
    manifest.magic = AIED_MANIFEST_MAGIC;
    manifest.schema_version = 1U;
    manifest.hardware_compatibility = 0x11890001U;
    manifest.minimum_bootloader_version = 2U;
    manifest.cm33.image_size = 128U;
    manifest.cm7.image_size = 512U;
    (void)memset(manifest.cm33.sha256, 0xA5, sizeof(manifest.cm33.sha256));
    (void)memset(manifest.cm7.sha256, 0x5A, sizeof(manifest.cm7.sha256));

    TEST_ASSERT_TRUE(aied_manifest_validate(&manifest, 0x11890001U, 2U, 1024U, 2048U, signature_accept, NULL));
    TEST_ASSERT_FALSE(aied_manifest_validate(&manifest, 0x11890002U, 2U, 1024U, 2048U, signature_accept, NULL));
    TEST_ASSERT_FALSE(aied_manifest_validate(&manifest, 0x11890001U, 1U, 1024U, 2048U, signature_accept, NULL));
    TEST_ASSERT_FALSE(aied_manifest_validate(&manifest, 0x11890001U, 2U, 64U, 2048U, signature_accept, NULL));
    TEST_ASSERT_FALSE(aied_manifest_validate(&manifest, 0x11890001U, 2U, 1024U, 2048U, NULL, NULL));
    return 0;
}
