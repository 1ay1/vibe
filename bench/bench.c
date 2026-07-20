/*
 * bench.c — proves two things about the single-header libvibe:
 *   1. It compiles header-only: just #define VIBE_IMPLEMENTATION + #include.
 *   2. The lazy hash index makes large-object parse + lookup O(1), not O(n^2).
 *
 * Build (header-only, nothing to link):
 *   cc -O2 -I.. bench.c -o bench -lm && ./bench
 */
#define VIBE_IMPLEMENTATION
#define VIBE_STATIC          /* header-only: internal linkage, best inlining */
#include "vibe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(void) {
    printf("libvibe %s — single-header benchmark\n", vibe_version());

    /* Build a big flat object: N keys "kNNNN vNNNN". The old linear insert was
     * O(N^2); the hash index makes it O(N). */
    enum { N = 40000 };
    size_t cap = (size_t)N * 24;
    char *doc = (char *)malloc(cap);
    size_t off = 0;
    for (int i = 0; i < N; i++)
        off += (size_t)snprintf(doc + off, cap - off, "k%d %d\n", i, i);

    VibeParser *p = vibe_parser_new();
    VibeLimits lim = vibe_default_limits();
    lim.max_object_keys = N + 16;
    lim.max_document_size = cap;
    vibe_parser_set_limits(p, &lim);

    double t0 = now();
    VibeValue *root = vibe_parse_string(p, doc);
    double t1 = now();
    if (!root) { fprintf(stderr, "parse failed: %s\n", vibe_get_last_error(p).message); return 1; }

    /* Random lookups — each is a hash probe, not a scan. */
    char key[32];
    long checksum = 0;
    double t2 = now();
    for (int i = 0; i < N; i++) {
        snprintf(key, sizeof key, "k%d", (i * 2654435761u) % N);
        checksum += vibe_get_int(vibe_get(root, key), NULL);
    }
    double t3 = now();

    printf("  keys parsed : %d\n", N);
    printf("  parse time  : %.1f ms  (%.2f M keys/s)\n",
           (t1 - t0) * 1e3, N / (t1 - t0) / 1e6);
    printf("  %d lookups  : %.1f ms  (%.2f M lookups/s)\n",
           N, (t3 - t2) * 1e3, N / (t3 - t2) / 1e6);
    printf("  checksum    : %ld\n", checksum);

    vibe_value_free(root);
    vibe_parser_free(p);
    free(doc);
    printf("OK (header-only, hashed objects)\n");
    return 0;
}
