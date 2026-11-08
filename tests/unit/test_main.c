#include "test.h"

int main(void)
{
    int failures = 0;
    failures += test_release_manifest();
    failures += test_slot_swap();
    if (failures == 0) {
        (void)printf("All unit tests passed.\n");
    }
    return failures;
}
