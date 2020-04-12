#pragma once

typedef struct
{
  char *data;
  uint32_t capacity;
  uint32_t used;
} buffer_t;

static void buffer_init(buffer_t *buf, uint32_t capacity) {
  buf->data = (char *)(malloc(capacity));
  buf->capacity = capacity;
  buf->used = 0;
}

static void buffer_reserve(buffer_t *buf, uint32_t new_capacity) {
  if (buf->capacity >= new_capacity) {
    return;
  }

  char *tmp = (char *)(realloc(buf->data, new_capacity));
  if (tmp == NULL) {
    exit(1);
  }

  buf->data = tmp;
  buf->capacity = new_capacity;
}

static uint32_t max_uint32_t(uint32_t a, uint32_t b) {
  return a > b ? a : b;
}

static void buffer_put(buffer_t *buf, const char *data, uint32_t data_size) {
  if (data_size == 0) {
    return;
  }

  if (buf->used + data_size > buf->capacity) {
    uint32_t new_capacity = max_uint32_t(2 * buf->capacity, buf->used + data_size);
    buffer_reserve(buf, new_capacity);
  }

  for (uint32_t i = 0; i < data_size; i++) {
    buf->data[buf->used + i] = data[i];
  }

  buf->used += data_size;
}

static void buffer_put_until(buffer_t *buf, const char *data, const char *data_end) {
  bool valid = data <= data_end;
  if (!valid) {
    return;
  }

  uint32_t data_size = (uint32_t)(data_end - data);
  if (buf->used + data_size > buf->capacity) {
    uint32_t new_capacity = max_uint32_t(2 * buf->capacity, buf->used + data_size);
    buffer_reserve(buf, new_capacity);
  }

  for (const char *c = data; c < data_end; c++) {
    buf->data[buf->used] = *c;
    buf->used++;
  }
}

static void buffer_put_until_escaping_comma(buffer_t *buf, const char *data, const char *data_end) {
  bool valid = data <= data_end;
  if (!valid) {
    return;
  }

  uint32_t data_size = (uint32_t)(data_end - data);
  if (buf->used + data_size > buf->capacity) {
    uint32_t new_capacity = max_uint32_t(2 * buf->capacity, buf->used + data_size);
    buffer_reserve(buf, new_capacity);
  }

  for (const char *c = data; c < data_end; c++) {
    if ((*c == '\\') && (*(c + 1) == ',')) {
      buf->data[buf->used] = ',';
      buf->used++;
      c++;
    } else {
      buf->data[buf->used] = *c;
      buf->used++;
    }
  }
}

static void buffer_put_string(buffer_t *buf, const char *data) {
  for (uint32_t i = 0; data[i] != '\0'; i++) {
    if (buf->used == buf->capacity) {
      buffer_reserve(buf, 2 * buf->capacity);
    }
    buf->data[buf->used] = data[i];
    buf->used++;
  }
}

static void buffer_shrink_to(buffer_t *buf, const char *to) {
  bool valid = (buf->data <= to) && (to <= buf->data + buf->used);
  if (!valid) {
    return;
  }

  buf->used = (uint32_t)(to - buf->data);
}

static void buffer_clear(buffer_t *buf) {
  buf->used = 0;
}

static void buffer_copy(buffer_t *buf1, buffer_t *buf2) {
  buffer_put(buf1, buf2->data, buf2->used);
}

static void buffer_read_all(buffer_t *buf, FILE *f) {
  uint32_t read_size = 1024 * 4;

  while (true) {
    buffer_reserve(buf, buf->used + read_size);

    size_t size_just_read = fread(buf->data + buf->used, 1, read_size, f);
    buf->used += (uint32_t)(size_just_read);
    bool input_consumed = (size_just_read < read_size);
    if (input_consumed) {
      break;
    }
    read_size *= 2;
  }
}
