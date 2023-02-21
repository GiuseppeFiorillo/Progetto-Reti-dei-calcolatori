#ifndef GREEN_PASS_GREEN_PASS_H
#define GREEN_PASS_GREEN_PASS_H

#include <time.h>

#define TESSERA_LENGTH 16
#define WRITE_GP 0
#define CHECK_GP 1
#define VALIDATION_GP 2

struct GreenPass {
    char tessera_sanitaria[TESSERA_LENGTH + 1];
    time_t valid_from;
    time_t valid_until;
    int service;
};

#endif //GREEN_PASS_GREEN_PASS_H
