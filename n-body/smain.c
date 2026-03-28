#include <stdio.h>

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("incorrect argument count, expected 2 got %d", argc);
        return 1;
    }
}
