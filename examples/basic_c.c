#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "astrakv/kv.h"

int main() {
    astrakv_options_t opts = {0};
    astrakv_t *kv = astrakv_open(&opts);
    if (!kv) { printf("FAIL: open\n"); return 1; }

    uint8_t key[] = "hello";
    uint8_t val[] = "world";

    if (astrakv_put(kv, key, 5, val, 5) != 0) { printf("FAIL: put\n"); goto out; }

    uint8_t *out_val = NULL;
    size_t out_len   = 0;
    if (astrakv_get(kv, key, 5, &out_val, &out_len) != 0) { printf("FAIL: get\n"); goto out; }

    printf("got: %.*s\n", (int)out_len, out_val);
    free(out_val);

    if (astrakv_exists(kv, key, 5) != 0) { printf("FAIL: exists\n"); goto out; }
    if (astrakv_del(kv, key, 5) != 0)    { printf("FAIL: del\n"); goto out; }
    if (astrakv_exists(kv, key, 5) == 0) { printf("FAIL: exists after del\n"); goto out; }

    printf("OK\n");
out:
    astrakv_close(kv);
    return 0;
}
