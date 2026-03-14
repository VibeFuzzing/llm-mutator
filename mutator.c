#include "afl-fuzz.h"
#include <stdint.h>
#include <stdlib.h>

typedef struct llm_mutator {
    afl_state_t *afl;
} llm_mutator_t;

llm_mutator_t *afl_custom_init(afl_state_t *afl, unsigned int seed) {
    srand(seed);

    llm_mutator_t *data = calloc(1, sizeof(llm_mutator_t));

    return data;
}

// currently a no-op
size_t afl_custom_fuzz(llm_mutator_t *data, uint8_t *buf, size_t buf_size, uint8_t **out_buf, uint8_t *add_buf, size_t add_buf_size, size_t max_size) {
    *out_buf = buf;

    return buf_size;
}
