#include <stdio.h>

__declspec(dllexport)
void greet(const char *greetee) {
    printf("Hello, %s! Greetings from C!!\n", greetee);
}
