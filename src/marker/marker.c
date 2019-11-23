#include "../mipssim.h"
#include "../parser.h"
#include "../mipssim.c"
#include "../memory_hierarchy.c"

static inline void marking_after_clock_cycle()
{
    printf("clock-cycle marker was called\n");
}

static inline void marking_at_the_end()
{
    printf("end-of-program marker was called\n");
}
