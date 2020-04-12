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

static void buffer_free(buffer_t *buf) {
  free(buf->data);
  buf->data = NULL;
  buf->used = 0;
  buf->capacity = 0;
}

static void buffer_ensure_capacity(buffer_t *buf, uint32_t capacity) {
  if (buf->capacity >= capacity) {
    return;
  }

  uint32_t new_capacity = 2 * capacity;
  char *tmp = (char *)(realloc(buf->data, new_capacity));
  if (tmp == NULL) {
    fprintf(stderr, "memory allocation failed");
    exit(1);
  }

  buf->data = tmp;
  buf->capacity = new_capacity;
}

static void buffer_put(buffer_t *buf, const char *data, uint32_t data_size) {
  if (data_size == 0) {
    return;
  }

  buffer_ensure_capacity(buf, buf->used + data_size);

  for (uint32_t i = 0; i < data_size; i++) {
    buf->data[buf->used + i] = data[i];
  }

  buf->used += data_size;
}

static void buffer_put_char(buffer_t *buf, char c) {
  buffer_ensure_capacity(buf, buf->used + 1);
  buf->data[buf->used] = c;
  buf->used++;
}

static void buffer_put_until_ptr(buffer_t *buf, const char *data, const char *data_end) {
  bool valid = data <= data_end;
  if (!valid) {
    return;
  }

  uint32_t data_size = (uint32_t)(data_end - data);
  buffer_ensure_capacity(buf, buf->used + data_size);

  for (const char *c = data; c < data_end; c++) {
    buf->data[buf->used] = *c;
    buf->used++;
  }
}

static void buffer_put_until_ptr_escaping_comma(buffer_t *buf, const char *data, const char *data_end) {
  bool valid = data <= data_end;
  if (!valid) {
    return;
  }

  uint32_t max_data_size = (uint32_t)(data_end - data);
  buffer_ensure_capacity(buf, buf->used + max_data_size);

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

static void buffer_put_until_char(buffer_t *buf, const char *data, char c) {
  for (uint32_t i = 0; data[i] != c; i++) {
    if (buf->used == buf->capacity) {
      buffer_ensure_capacity(buf, 2 * buf->capacity);
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
    buffer_ensure_capacity(buf, buf->used + read_size);

    size_t size_just_read = fread(buf->data + buf->used, 1, read_size, f);
    buf->used += (uint32_t)(size_just_read);
    bool input_consumed = (size_just_read < read_size);
    if (input_consumed) {
      break;
    }
    read_size *= 2;
  }
}
