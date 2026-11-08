#include "test.h"

int main(void)
{
    int failures = 0;
    failures += test_release_manifest();
    failures += test_slot_swap();
    failures += test_log();
    failures += test_log_file();
    if (failures == 0) {
        (void)printf("All unit tests passed.\n");
    }
    return failures;
}
