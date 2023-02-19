#ifndef GREEN_PASS_GREEN_PASS_H
#define GREEN_PASS_GREEN_PASS_H

#include <time.h>

#define TESSERA_LENGTH 16

struct GreenPass {
    char tessera_sanitaria[TESSERA_LENGTH];
    time_t valid_from;
    time_t valid_until;
};

#endif //GREEN_PASS_GREEN_PASS_H
