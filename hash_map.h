#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
  uint32_t *keys;
  uint32_t *items;
  uint32_t size;
  uint32_t capacity;
} hash_map_t;

static void hash_map_init(hash_map_t *hm, uint32_t initial_capacity) {
  hm->keys = (uint32_t *)malloc(sizeof(uint32_t) * initial_capacity);
  hm->items = (uint32_t *)malloc(sizeof(uint32_t) * initial_capacity);
  hm->size = 0;
  hm->capacity = initial_capacity;

  if ((hm->keys == NULL) || (hm->items == NULL)) {
    fprintf(stderr, "realloc() failed\n");
    return;
  }

  for (uint32_t i = 0; i < initial_capacity; i++) {
    hm->keys[i] = UINT32_MAX;
    hm->items[i] = UINT32_MAX;
  }
}

static void hash_map_put(hash_map_t *hm, uint32_t key, uint32_t item) {
  uint32_t idx = key % hm->capacity;
  while (true) {
    if (hm->keys[idx] == UINT32_MAX) {
      hm->size++;
      break;
    }
    if (hm->keys[idx] == key) {
      break;
    }
    idx = (idx + 1) % hm->capacity;
  }

  hm->keys[idx] = key;
  hm->items[idx] = item;

  if (2 * hm->size < hm->capacity) {
    return;
  }

  // enlarge buffer.
  uint32_t old_capacity = hm->capacity;
  uint32_t new_capacity = hm->capacity * 2;
  uint32_t *temp1 = (uint32_t *)(realloc(hm->keys, sizeof(uint32_t) * new_capacity));
  uint32_t *temp2 = (uint32_t *)(realloc(hm->items, sizeof(uint32_t) * new_capacity));
  if ((temp1 == NULL) || (temp2 == NULL)) {
    fprintf(stderr, "realloc() failed\n");
    return;
  }
  hm->keys = temp1;
  hm->items = temp2;
  hm->capacity = new_capacity;

  // rehash all existing items.
  // this operation takes O(n) time.
  uint32_t *key_buf = (uint32_t *)(malloc(sizeof(uint32_t) * old_capacity));
  uint32_t *item_buf = (uint32_t *)(malloc(sizeof(uint32_t) * old_capacity));
  memcpy(key_buf, hm->keys, sizeof(uint32_t) * old_capacity);
  memcpy(item_buf, hm->items, sizeof(uint32_t) * old_capacity);

  // clear data.
  for (uint32_t i = 0; i < new_capacity; i++) {
    hm->keys[i] = UINT32_MAX;
    hm->items[i] = UINT32_MAX;
  }

  for (uint32_t old_idx = 0; old_idx < old_capacity; old_idx++) {
    if (key_buf[old_idx] == UINT32_MAX) {
      // this slot is empty.
      continue;
    }

    uint32_t new_idx = key_buf[old_idx] % new_capacity;
    while (true) {
      if (hm->keys[new_idx] == UINT32_MAX) {
        break;
      }
      new_idx = (new_idx + 1) % new_capacity;
    }
    hm->keys[new_idx] = key_buf[old_idx];
    hm->items[new_idx] = item_buf[old_idx];
  }
  free(key_buf);
  free(item_buf);
}

static uint32_t hash_map_get(hash_map_t *hm, uint32_t key) {
  uint32_t idx = key % hm->capacity;
  while (true) {
    if (hm->keys[idx] == key) {
      return hm->items[idx];
    }
    if (hm->keys[idx] == UINT32_MAX) {
      return UINT32_MAX;
    }
    idx = (idx + 1) % hm->capacity;
  }
}

static void hash_map_clear(hash_map_t *hm) {
  if (hm->size == 0) {
    return;
  }

  for (uint32_t i = 0; i < hm->capacity; i++) {
    hm->keys[i] = UINT32_MAX;
    hm->items[i] = UINT32_MAX;
  }
  hm->size = 0;
}

static void hash_map_free(hash_map_t *hm) {
  free(hm->keys);
  free(hm->items);
}
