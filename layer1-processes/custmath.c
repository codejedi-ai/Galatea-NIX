#include "custmath.h"
#include "rpi.h"
#include "util.h"

uint64_t min(uint64_t a, uint64_t b){
    if (a < b){
        return a;
    }
    return b;
}
uint64_t max(uint64_t a, uint64_t b){
    if (a > b){
        return a;
    }
    return b;
}