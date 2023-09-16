#include <stdalign.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../src/region.h"

#define CHECK_ALLOC(p) do {                                            \
        if (!p) {                                                      \
            fprintf(stderr, "Allocation failed line %d\n", __LINE__);  \
            exit(1);                                                   \
        }                                                              \
    } while (0)

int main(void) {
    struct region *region = new_region(alignof(max_align_t));
    CHECK_ALLOC(region);
    int *a = region_alloc(region, sizeof *a);
    CHECK_ALLOC(a);
    *a = 42;
    char *s = region_alloc(region, 3);
    CHECK_ALLOC(s);
    memcpy(s, "Hi", 3);
    double *x = region_alloc(region, sizeof *x);
    CHECK_ALLOC(x);
    long double *y = region_alloc(region, sizeof *y);
    CHECK_ALLOC(y);
    *x = 2.7182813284;
    *y = *x * 3.141592653;

    printf("a = %d, s = '%s', x = %f, y = %Lf\n\n", *a, s, *x, *y);

    for (struct region *r = region; r != NULL; r = r->next) {
        printf("%p ->\n\t{.next = %p, .size = %zu, .alloc_count = %zu, .bytes = %p...}\n\n",
               (void *)r, (void *)r->next, r->size, r->alloc_count, (void *)r->bytes);
    }

    kill_region(region);
}
