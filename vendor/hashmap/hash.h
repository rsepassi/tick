// Standalone hash functions extracted from hashmap.c
// Contains implementations of SipHash, MurmurHash3, and xxHash3

#ifndef HASHMAP_HASH_H
#define HASHMAP_HASH_H

#include <stdint.h>
#include <stddef.h>

// hashmap_sip returns a hash value for `data` using SipHash-2-4.
uint64_t hashmap_sip(const void *data, size_t len, uint64_t seed0,
    uint64_t seed1);

// hashmap_murmur returns a hash value for `data` using Murmur3_86_128.
uint64_t hashmap_murmur(const void *data, size_t len, uint64_t seed0,
    uint64_t seed1);

// hashmap_xxhash3 returns a hash value for `data` using xxHash3.
uint64_t hashmap_xxhash3(const void *data, size_t len, uint64_t seed0,
    uint64_t seed1);

#endif // HASHMAP_HASH_H
