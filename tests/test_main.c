#include <stdio.h>

/* Test suite declarations */
extern void run_data_tests(void);
extern void run_storage_tests(void);

int main(void) {
    printf("=== hacklog test suite ===\n\n");

    run_data_tests();
    run_storage_tests();

    printf("=== ALL TESTS PASSED ===\n");
    return 0;
}
