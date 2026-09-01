#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "ejdb2.h"
#include "jqp.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size){
        char *new_str = (char *)malloc(size+1);
        if (new_str == NULL){
                return 0;
        }
        memcpy(new_str, data, size);
        new_str[size] = '\0';

        JQP_AUX *aux;
        int rc = jqp_aux_create(&aux, new_str);

        rc = jqp_parse(aux);
        jqp_aux_destroy(&aux);

        free(new_str);
        return 0;
}
