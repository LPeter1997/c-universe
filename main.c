#include <stdio.h>
#define LIQUID_C_IMPLEMENTATION
#include "liquid_c.h"

int main() {
    LiquidValue v1 = LiquidValue_new_array();
    LiquidValue_array_append(&v1, LiquidValue_new_int(5));
    LiquidValue_array_append(&v1, LiquidValue_new_bool(true));
    LiquidValue_array_append(&v1, LiquidValue_new_int(3));
    LiquidValue str = LiquidValue_to_string(&v1);
    puts(LiquidValue_get_string(&str));
    return 0;
}
