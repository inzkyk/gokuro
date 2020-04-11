#pragma once

typedef struct
{
  char *data;
  size_t capacity;
  size_t used;
} buffer_t;

static void buffer_init(buffer_t *buf, size_t capacity) {
  buf->data = malloc(capacity);
  buf->capacity = capacity;
  buf->used = 0;
}

static void buffer_reserve(buffer_t *buf, size_t new_capacity) {
  if (buf->capacity >= new_capacity) {
    return;
  }

  char *tmp = realloc(buf->data, new_capacity);
  if (tmp == NULL) {
    exit(1);
  }

  buf->data = tmp;
  buf->capacity = new_capacity;
}

static size_t max_size_t(size_t a, size_t b) {
  return a > b ? a : b;
}

static void buffer_put(buffer_t *buf, const char *data, size_t data_size) {
  if (data_size == 0) {
    return;
  }

  if (buf->used + data_size > buf->capacity) {
    size_t new_capacity = max_size_t(2 * buf->capacity, buf->used + data_size);
    buffer_reserve(buf, new_capacity);
  }

  for (size_t i = 0; i < data_size; i++) {
    buf->data[buf->used + i] = data[i];
  }

  buf->used += data_size;
}

static size_t char_index(const char *data, size_t data_size, char c) {
  for (size_t i = 0; i < data_size; i++) {
    if (data[i] == c) {
      return i;
    }
  }
  return data_size + 1;
}

static void buffer_copy(buffer_t *buf1, buffer_t *buf2) {
  buffer_put(buf1, buf2->data, buf2->used);
}

static void buffer_get_line(buffer_t *buf, FILE *f) {
  char *result = fgets(buf->data, (int)(buf->capacity), f);
  if (result == NULL) {
    buf->capacity = 0;
    return;
  }
  buf->data = result;

  size_t index_of_null = char_index(buf->data, buf->capacity, '\0');
  bool buffer_too_small = (index_of_null == buf->capacity - 1) && (buf->data[index_of_null - 1] != '\n');
  while (buffer_too_small) {
    size_t old_buf_size = buf->capacity;
    buffer_reserve(buf, 2 * buf->capacity);
    size_t offset = old_buf_size - 1; // minus one for a null character.
    result = fgets(buf->data + offset, (int)(buf->capacity - offset), f);
    if (result == NULL) {
      exit(1);
    }

    index_of_null += char_index(buf->data + offset, buf->capacity - offset, '\0');
    buffer_too_small = (index_of_null == buf->capacity - 1) && (buf->data[buf->capacity - 1] != '\n');
  }

  buf->used = index_of_null + 1;
}
