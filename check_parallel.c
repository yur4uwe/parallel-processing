#include <stdio.h>
#include <omp.h>

int main(void) {
    printf("Parallel Processing Example\n");
    #pragma omp parallel
    {
        printf("current thread id: %d\n", omp_get_thread_num());
    }
    printf("Parallel Processing Completed\n");
    return 0;
}
