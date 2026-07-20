# libvibe benchmark

Proves the two headline properties of the single-header libvibe:

1. **Header-only** — `bench.c` builds with nothing but the header:
   ```sh
   cc -O2 -I.. bench.c -o bench -lm && ./bench
   ```
   No `vibe.c`, no `libvibe.a`, no link step. (`#define VIBE_IMPLEMENTATION`
   pulls the whole library in; `#define VIBE_STATIC` gives it internal linkage.)

2. **O(1) object access** — the lazy FNV-1a hash index turns large-object parse
   and lookup from O(n²) into O(n)/O(1).

## Measured (40,000-key flat object, `-O2`)

| Object index | Parse | 40k lookups |
|--------------|-------|-------------|
| **Hashed (default)** | ~30 ms | ~10 ms |
| Linear (old, `-DVIBE_OBJECT_HASH_THRESHOLD=100000000`) | ~3140 ms | ~3130 ms |

That's a **~100× parse** and **~300× lookup** speedup on large objects, with
zero cost for small ones (the index isn't allocated until an object crosses
`VIBE_OBJECT_HASH_THRESHOLD`, default 8 keys).

## A/B it yourself

```sh
cc -O2 -I.. bench.c -o bench -lm
cc -O2 -I.. -DVIBE_OBJECT_HASH_THRESHOLD=100000000 bench.c -o bench_linear -lm
./bench_linear   # simulates the old linear behaviour
./bench          # the shipped hashed index
```
