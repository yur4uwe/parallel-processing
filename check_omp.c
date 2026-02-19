#include <stdio.h>

int main() {
#if defined(_OPENMP)
	printf("OpenMP is supported and enabled. Version %d\n", _OPENMP);
#else		
	printf("OpenMP is not supported or not enabled.\n");
#endif
	return 0;
}
