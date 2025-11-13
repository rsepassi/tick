// Copyright 2020 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct hashmap;

// Param `elsize` is the size of each element in the tree. Every element that
// is inserted, deleted, or retrieved will be this size.
// Param `cap` is the default lower capacity of the hashmap. Setting this to
// zero will default to 16.
// Params `seed0` and `seed1` are optional seed values that are passed to the 
// following `hash` function. These can be any value you wish but it's often 
// best to use randomly generated values.
// Param `hash` is a function that generates a hash value for an item. It's
// important that you provide a good hash function, otherwise it will perform
// poorly or be vulnerable to Denial-of-service attacks. This implementation
// comes with two helper functions `hashmap_sip()` and `hashmap_murmur()`.
// Param `compare` is a function that compares items in the tree. See the 
// qsort stdlib function for an example of how this function works.
// The hashmap must be freed with hashmap_free(). 
// Param `elfree` is a function that frees a specific item. This should be NULL
// unless you're storing some kind of reference data in the hash.
struct hashmap *hashmap_new_with_allocator(
    void *(*malloc)(void *ctx, size_t),
    void *(*realloc)(void *ctx, void *, size_t),
    void (*free)(void *ctx, void*),
    void *allocator_ctx,
    size_t elsize,
    size_t cap, uint64_t seed0, uint64_t seed1,
    uint64_t (*hash)(const void *item, uint64_t seed0, uint64_t seed1),
    int (*compare)(const void *a, const void *b, void *udata),
    void (*elfree)(void *ctx, void *item),
    void *udata);

void hashmap_free(struct hashmap *map);
void hashmap_clear(struct hashmap *map, bool update_cap);
size_t hashmap_count(const struct hashmap *map);
bool hashmap_oom(struct hashmap *map);
const void *hashmap_get(const struct hashmap *map, const void *item);
const void *hashmap_set(struct hashmap *map, const void *item);
const void *hashmap_delete(struct hashmap *map, const void *item);
const void *hashmap_probe(struct hashmap *map, uint64_t position);
bool hashmap_scan(struct hashmap *map, bool (*iter)(const void *item, void *udata), void *udata);
bool hashmap_iter(struct hashmap *map, size_t *i, void **item);

uint64_t hashmap_sip(const void *data, size_t len, uint64_t seed0, uint64_t seed1);
uint64_t hashmap_murmur(const void *data, size_t len, uint64_t seed0, uint64_t seed1);
uint64_t hashmap_xxhash3(const void *data, size_t len, uint64_t seed0, uint64_t seed1);

const void *hashmap_get_with_hash(const struct hashmap *map, const void *key, uint64_t hash);
const void *hashmap_delete_with_hash(struct hashmap *map, const void *key, uint64_t hash);
const void *hashmap_set_with_hash(struct hashmap *map, const void *item, uint64_t hash);
void hashmap_set_grow_by_power(struct hashmap *map, size_t power);
void hashmap_set_load_factor(struct hashmap *map, double load_factor);


#endif  // HASHMAP_H
