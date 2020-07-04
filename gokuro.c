#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

#define PANIC() do { exit(1); } while(0)
#define BUFFER_INITIAL_SIZE 1024 * 4
#define MEMORY_MAX_SIZE 1024 * 1024 * 128

///// util functions
static inline bool begin_with(const char *a, const char *b) {
  uint32_t i = 0;
  while (true) {
    if (b[i] == '\0') {
      return true;
    }

    if (a[i] != b[i]) {
      return false;
    }

    i++;
  }
}

static inline bool is_digit(char c) {
  return '0' <= c && c <= '9';
}

static inline uint32_t hash_32(void *data_begin, void *data_end) {
  uint32_t hash = 2147483647u;
  for (unsigned char *c = (unsigned char *)(data_begin); c < (unsigned char *)data_end; c++) {
    hash = hash * 33 + *c;
  }
  return hash;
}
///// util functions end

///// hash_map begin
typedef struct {
  uint32_t *keys;
  uint32_t *items;
  uint32_t size;
  uint32_t capacity;
} hash_map_t;

static inline void hash_map_init(hash_map_t *hm, uint32_t initial_capacity) {
  if (2 * sizeof(uint32_t) * initial_capacity > MEMORY_MAX_SIZE) {
    fprintf(stderr, "too much memory requested.\n");
    PANIC();
  }

  hm->keys = (uint32_t *)malloc(sizeof(uint32_t) * initial_capacity);
  hm->items = (uint32_t *)malloc(sizeof(uint32_t) * initial_capacity);
  hm->size = 0;
  hm->capacity = initial_capacity;

  if ((hm->keys == NULL) || (hm->items == NULL)) {
    fprintf(stderr, "malloc() failed\n");
    PANIC();
  }

  for (uint32_t i = 0; i < initial_capacity; i++) {
    hm->keys[i] = UINT32_MAX;
    hm->items[i] = UINT32_MAX;
  }
}

static inline void hash_map_put(hash_map_t *hm, uint32_t key, uint32_t item) {
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
  if (2 * sizeof(uint32_t) * new_capacity > MEMORY_MAX_SIZE) {
    fprintf(stderr, "too much memory requested\n");
    PANIC();
  }
  uint32_t *temp1 = (uint32_t *)(realloc(hm->keys, sizeof(uint32_t) * new_capacity));
  uint32_t *temp2 = (uint32_t *)(realloc(hm->items, sizeof(uint32_t) * new_capacity));
  if ((temp1 == NULL) || (temp2 == NULL)) {
    fprintf(stderr, "realloc() failed\n");
    PANIC();
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

static inline uint32_t hash_map_get(hash_map_t *hm, uint32_t key) {
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

static inline void hash_map_clear(hash_map_t *hm) {
  if (hm->size == 0) {
    return;
  }

  for (uint32_t i = 0; i < hm->capacity; i++) {
    hm->keys[i] = UINT32_MAX;
    hm->items[i] = UINT32_MAX;
  }
  hm->size = 0;
}

static inline void hash_map_free(hash_map_t *hm) {
  free(hm->keys);
  free(hm->items);
}
///// hash_map end

///// buffer begin
typedef struct {
  char *data;
  uint32_t capacity;
  uint32_t used;
} buffer_t;

static inline void buffer_free(buffer_t *buf) {
  free(buf->data);
  buf->data = NULL;
  buf->used = 0;
  buf->capacity = 0;
}

static inline void buffer_reserve(buffer_t *buf, uint32_t capacity) {
  if (buf->capacity >= capacity) {
    return;
  }

  uint32_t new_capacity = 2 * capacity;
  if (new_capacity > MEMORY_MAX_SIZE) {
    fprintf(stderr, "too much memory requested.\n");
    PANIC();
  }
  char *tmp = (char *)(realloc(buf->data, new_capacity));
  if (tmp == NULL) {
    fprintf(stderr, "realloc() failed");
    PANIC();
  }

  buf->data = tmp;
  buf->capacity = new_capacity;
}

static inline void buffer_put(buffer_t *buf, const char *data, uint32_t data_size) {
  if (data_size == 0) {
    return;
  }

  buffer_reserve(buf, buf->used + data_size);

  for (uint32_t i = 0; i < data_size; i++) {
    buf->data[buf->used + i] = data[i];
  }

  buf->used += data_size;
}

static inline void buffer_put_char(buffer_t *buf, char c) {
  buffer_reserve(buf, buf->used + 1);
  buf->data[buf->used] = c;
  buf->used++;
}

static inline void buffer_put_until_ptr(buffer_t *buf, const char *data, const char *data_end) {
  bool valid = data <= data_end;
  if (!valid) {
    return;
  }

  uint32_t data_size = (uint32_t)(data_end - data);
  buffer_reserve(buf, buf->used + data_size);

  for (const char *c = data; c < data_end; c++) {
    buf->data[buf->used] = *c;
    buf->used++;
  }
}

static inline void buffer_put_until_ptr_escaping_comma(buffer_t *buf, const char *data, const char *data_end) {
  bool valid = data <= data_end;
  if (!valid) {
    return;
  }

  uint32_t max_data_size = (uint32_t)(data_end - data);
  buffer_reserve(buf, buf->used + max_data_size);

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

static inline void buffer_put_until_char(buffer_t *buf, const char *data, char c) {
  for (uint32_t i = 0; data[i] != c; i++) {
    if (buf->used == buf->capacity) {
      buffer_reserve(buf, 2 * buf->capacity);
    }
    buf->data[buf->used] = data[i];
    buf->used++;
  }
}

static inline void buffer_clear(buffer_t *buf) {
  buf->used = 0;
}

static inline void buffer_copy(buffer_t *buf1, buffer_t *buf2) {
  buffer_put(buf1, buf2->data, buf2->used);
}

static inline void buffer_read_all(buffer_t *buf, FILE *f) {
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

///// buffer end

static void gokuro(const char *input, buffer_t *output_buf) {
  hash_map_t global_macro_map = {0};
  hash_map_t local_macro_map = {0};
  hash_map_init(&global_macro_map, 128);
  hash_map_init(&local_macro_map, 128);

  buffer_t line_buf = {0};
  buffer_t temp_buf = {0};
  buffer_t global_macro_bodies = {0};
  buffer_t local_macro_bodies = {0};

  buffer_reserve(&line_buf, BUFFER_INITIAL_SIZE);
  buffer_reserve(&temp_buf, BUFFER_INITIAL_SIZE);
  buffer_reserve(&global_macro_bodies, BUFFER_INITIAL_SIZE);
  buffer_reserve(&local_macro_bodies, BUFFER_INITIAL_SIZE);

  // the mainloop.
  const char *input_index = input;
  while (true) {
    if (*input_index == '\0') {
      break;
    }

    // read next line to line_buf.
    buffer_clear(&line_buf);
    buffer_put_until_char(&line_buf, input_index, '\n');
    buffer_put_char(&line_buf, '\n');
    input_index += line_buf.used;

    enum LINE_TYPE {
      LINE_TYPE_NORMAL,
      LINE_TYPE_GLOBAL_MACRO_DEFINITION,
      LINE_TYPE_LOCAL_MACRO_DEFINITION,
    } line_type = LINE_TYPE_NORMAL;

    // macro_offset stores how far the macro expansion proceeds.
    // loop invariant at the begenning of the loop below:
    //   "line_buf does not have a macro from (line_end - macro_offset) to (line_end)."
    uint32_t macro_offset = 0;

    while (true) {
      // line_begin and line_end can not be precomputed because
      // line_buf gets modified in macro expansion.
      char *line_begin = line_buf.data;
      char *line_end = line_buf.data + line_buf.used - 1; // 1 = strlen("\n")

      // shortcut
      if (*line_begin != '#') {
        goto MACRO_EXPANSION;
      }

      // Does this line define a global macro?
      if (begin_with(line_begin, "#+MACRO: ")) {
        const uint32_t name_offset = 9; // 9 = strlen("#+MACRO: ")

        if (*(line_begin + name_offset) == '\n') {
          // This line does not define a global macro.
          break;
        }

        char *name_begin = line_begin + name_offset;
        char *name_end = NULL;
        { // read the name of the macro.
          char *c = name_begin;
          while (true) {
            if (*c == ' ') {
              name_end = c;
              break;
            }
            if (*c == '\n') {
              // The name is empty: this line does not define a global macro.
              break;
            }
            c++;
          }
        }

        if (name_end != NULL) {
          line_type = LINE_TYPE_GLOBAL_MACRO_DEFINITION;

          // save this macro.
          buffer_put_until_char(&global_macro_bodies, name_end + 1, '\n'); // 1 = strlen(" ")
          buffer_put_char(&global_macro_bodies, '\0');
          uint32_t body_length = (uint32_t)(line_end - name_end) - 1; // 1 = strlen(" ")
          uint32_t body_offset = global_macro_bodies.used - body_length - 1; // 1 = strlen("\0")
          uint32_t macro_name_hash = hash_32(name_begin, name_end);
          hash_map_put(&global_macro_map, macro_name_hash, body_offset);
          break;
        }
      }

      // Does this line define a local macro?
      if (begin_with(line_begin, "#+MACRO_LOCAL: ")) {
        char *name_begin = line_begin + 15; // 15 = strlen("#+MACRO_LOCAL: ")
        char *name_end = NULL;
        { // read the name of the macro.
          char *c = name_begin;
          while (true) {
            if (*c == ' ') {
              name_end = c;
              break;
            }
            if (*c == '\n') {
              // This line does not define a global macro.
              break;
            }
            c++;
          }
        }

        if (name_end != NULL) {
          line_type = LINE_TYPE_LOCAL_MACRO_DEFINITION;

          // save this macro.
          buffer_put_until_char(&local_macro_bodies, name_end + 1, '\n'); // 1 = strlen(" ")
          buffer_put_char(&local_macro_bodies, '\0');
          uint32_t body_length = (uint32_t)(line_end - name_end) - 1; // 1 = strlen(" ")
          uint32_t body_offset = local_macro_bodies.used - body_length - 1; // 1 = strlen("\0")
          uint32_t macro_name_hash = hash_32(name_begin, name_end);
          hash_map_put(&local_macro_map, macro_name_hash, body_offset);
          break;
        }
      }

MACRO_EXPANSION:;
      // expand the last macro (so that nested calls are treated properly)
      const uint32_t bbb_offset = 3; // 3 = strlen("{{{") or strlen("}}}")

      char *macro_begin = NULL;
      char *macro_end = NULL;
      { // find the last macro call.
        uint32_t num_consecutive_open = 0;
        uint32_t num_consecutive_close = 0;
        for (char *c = line_end - macro_offset; line_begin <= c; c--) {
          if (*c != '{' && *c != '}') {
            num_consecutive_open = 0;
            num_consecutive_close = 0;
          } else if (*c == '{') {
            num_consecutive_open++;
            num_consecutive_close = 0;
            if (num_consecutive_open == bbb_offset) {
              macro_begin = c;
              break;
            }
            continue;
          } else if (*c == '}') {
            num_consecutive_open = 0;
            num_consecutive_close++;
            if (num_consecutive_close >= bbb_offset) {
              if (macro_end == NULL) {
                macro_offset = (uint32_t)(line_end - c) - bbb_offset;
              }
              macro_end = c + bbb_offset;
            }
          }
        }

        if (macro_begin == NULL || macro_end == NULL) {
          // no (more) macro to expand.
          line_type = LINE_TYPE_NORMAL;
          break;
        }
      }

      // macro found
      char *name_begin = macro_begin + bbb_offset;
      char *name_end = NULL;
      bool is_constant;
      { // get the length of the macro name.
        char *c = name_begin;
        while (true) {
          if (*c == '(') {
            is_constant = false;
            break;
          }

          if (*c == '}') {
            is_constant = true;
            break;
          }
          c++;
        }
        name_end = c;
      }

      // lookup the definition of the macro.
      char* macro_body = NULL;
      {
        uint32_t name_hash = hash_32(name_begin, name_end);
        uint32_t body_offset = hash_map_get(&local_macro_map, name_hash);

        if (body_offset != UINT32_MAX) {
          macro_body = local_macro_bodies.data + body_offset;
        } else {
          body_offset = hash_map_get(&global_macro_map, name_hash);
          if (body_offset != UINT32_MAX) {
            macro_body = global_macro_bodies.data + body_offset;
          }
        }
      }

      if (macro_body == NULL) {
        // the macro is undefined: delete the call.
        line_buf.used = (uint32_t)(macro_begin - line_buf.data);
        buffer_put_until_char(&line_buf, macro_end, '\n');
        buffer_put_char(&line_buf, '\n');
        continue;
      }

      if (is_constant) {
        // temp_buf = line[macro_end:]
        buffer_clear(&temp_buf);
        buffer_put_until_char(&temp_buf, macro_end, '\n');

        // line = line[:macro_begin] + macro_body + line[maro_end:]
        line_buf.used = (uint32_t)(macro_begin - line_buf.data);
        buffer_put_until_char(&line_buf, macro_body, '\0');
        buffer_copy(&line_buf, &temp_buf);
        buffer_put_char(&line_buf, '\n');
        continue;
      }

      // macro with arguments.
      // parse the arguments.
      char *args[9] = {0}; // 9 = max bumber of the positions of arguments  ($1...$9)
      args[0] = name_end + 1; // 1 = strlen("(")
      uint32_t currentIndex = 1;
      for (char *c = args[0]; c < macro_end; c++) {
        if (currentIndex == 9) {
          break;
        }
        if (*c == ',') {
          args[currentIndex] = c + 1; // 1 = strlen(",")
          currentIndex++;
          continue;
        }
        if ((*c == '\\') && (*(c + 1) == ',')) {
          // escaped comma.
          c++;
          continue;
        }
      }

      // expand the macro on temp_buf (we cannot modify line_buf because args depends on it).
      buffer_clear(&temp_buf);
      char *write_from = macro_body;
      char *c = macro_body;
      while (true) {
        if (*c == '\0') {
          buffer_put_until_char(&temp_buf, write_from, '\0');
          break;
        }
        if (*c != '$') {
          c++;
          continue;
        }
        // If *c != '\0', we can read *(c + 1).
        c++;
        if (!is_digit(*c)) {
          continue;
        }

        // argument found in the macro body.
        buffer_put_until_ptr(&temp_buf, write_from, c - 1);
        write_from = c + 1;
        uint32_t arg_index = (uint32_t)(*c - '0');
        if (arg_index == 0) {
          // The replacement is the whole text in the brackets.
          char *until = macro_end - bbb_offset - 1; // 1 = strlen(")")
          buffer_put_until_ptr(&temp_buf, args[0], until);
        } else if (args[arg_index - 1] != NULL) {
          if ((arg_index == 9) || args[arg_index] == NULL) {
            // the last argument
            char *until = macro_end - bbb_offset - 1; // 1 = strlen(")")
            buffer_put_until_ptr_escaping_comma(&temp_buf, args[arg_index - 1], until);
          } else {
            char *until = args[arg_index] - 1; // 1 = strlen(",")
            buffer_put_until_ptr_escaping_comma(&temp_buf, args[arg_index - 1], until);
          }
        }
        c++;
      }

      // copy the expanded macro to line_buf.
      buffer_put_until_char(&temp_buf, macro_end, '\n');
      line_buf.used = (uint32_t)(macro_begin - line_begin);
      buffer_copy(&line_buf, &temp_buf);
      buffer_put_char(&line_buf, '\n');
    }

    if (line_type == LINE_TYPE_NORMAL) {
      buffer_copy(output_buf, &line_buf);
    }

    if (line_type != LINE_TYPE_LOCAL_MACRO_DEFINITION) {
      hash_map_clear(&local_macro_map);
    }
  }

  buffer_put_char(output_buf, '\0');
  buffer_free(&global_macro_bodies);
  buffer_free(&local_macro_bodies);
  hash_map_free(&global_macro_map);
  hash_map_free(&local_macro_map);
  buffer_free(&line_buf);
  buffer_free(&temp_buf);
}

static int init_io(FILE *in, FILE *out) {
#ifdef _WIN32
  // output "\n" instead of "\r\n".
  int oldmode = _setmode(_fileno(out), _O_BINARY);
  if (oldmode == -1) {
    fprintf(stderr, "_setmode() failed.\n");
    return 1;
  }
#endif

  int result = setvbuf(out, NULL, _IOFBF, 1024 * 4);
  if (result != 0) {
    fprintf(stderr, "setvbuf() failed.\n");
    return 1;
  }

  result = setvbuf(in, NULL, _IOFBF, 1024 * 4);
  if (result != 0) {
    fprintf(stderr, "setvbuf() failed.\n");
    return 1;
  }

  return 0;
}

int main() {
  if (init_io(stdin, stdout) != 0) {
    return 1;
  }

  buffer_t input_buf = {0};
  buffer_reserve(&input_buf, BUFFER_INITIAL_SIZE);
  buffer_read_all(&input_buf, stdin);

  if (input_buf.used == 0) {
    // nothing to output.
    return 0;
  }

  char last_char = *(input_buf.data + input_buf.used - 1);
  if (last_char != '\n') {
    // add a newline to input.
    buffer_put_char(&input_buf, '\n');
  }

  buffer_put_char(&input_buf, '\0');

  buffer_t output_buf = {0};
  buffer_reserve(&output_buf, input_buf.used);

  gokuro(input_buf.data, &output_buf);
  fputs(output_buf.data, stdout);
  fflush(stdout);

  buffer_free(&input_buf);
  buffer_free(&output_buf);
}
