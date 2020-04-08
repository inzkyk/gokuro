#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#define BUF_MAX_SIZE (1024 * 1024 * 10)
#define MIN(a,b) (a) < (b) ? (a) :(b)
#define MAX(a,b) (a) > (b) ? (a) :(b)

size_t find_char(const char *buf, size_t size, char c) {
  for (size_t i = 0; i < size; i++) {
    if (buf[i] == c) {
      return i;
    }
  }
  return size + 1;
}

char *get_line(char *buf, size_t buf_size, FILE *f) {
  char *line = fgets(buf, buf_size, f);

  if (line == NULL) {
    return NULL;
  }

  size_t index_of_null = find_char(line, buf_size, '\0');
  bool line_too_long = (index_of_null == buf_size - 1) && (buf[index_of_null - 1] != '\n');
  while (line_too_long) {
    size_t old_buf_size = buf_size;
    buf_size = buf_size * 2;
    if (buf_size > BUF_MAX_SIZE) {
      return NULL;
    }

    char *tmp = realloc(buf, buf_size);
    if (tmp == NULL) {
      return NULL;
    }
    buf = tmp;
    line = tmp;

    size_t offset = old_buf_size - 1; // minus one for a null character.
    tmp = fgets(buf + offset, buf_size - offset, f);
    if (tmp == NULL) {
      break;
    }
    index_of_null += find_char(buf + offset, buf_size - offset, '\0');
    line_too_long = (index_of_null == buf_size - 1) && (buf[buf_size - 1] != '\n');
  }

  return line;
}

bool begin_with(char *a, char *b) {
  size_t i = 0;
  while (true) {
    if (a[i] == '\0' || b[i] == '\0') {
      return true;
    }

    if (a[i] != b[i]) {
      return false;
    }

    i++;
  }
}

bool end_width(char *a, char c) {
  size_t i = 0;
  bool just_read_c = false;
  while (true) {
    if (a[i] == '\0') {
      return just_read_c;
    }

    just_read_c = (a[i] == c);
    i++;
  }
}

typedef struct
{
  char *base;
  size_t capacity;
  size_t used;
} memory_pool_t;

void memory_pool_init(memory_pool_t *pool, size_t capacity) {
  pool->base = malloc(capacity);
  pool->capacity = capacity;
  pool->used = 0;
}

bool memory_pool_is_initialized(memory_pool_t *pool) {
  return pool->base != NULL;
}

void memory_pool_realloc(memory_pool_t *pool, size_t new_capacity) {
  char *tmp = realloc(pool->base, new_capacity);
  if (tmp == NULL) {
    free(pool->base);
    pool->base = NULL;
    return;
  }

  pool->base = tmp;
  pool->capacity = new_capacity;
  if (pool->capacity < pool->used) {
    pool->used = pool->capacity;
  }
}

void memory_pool_free(memory_pool_t *pool) {
  free(pool->base);
}

char *memory_pool_put(memory_pool_t *pool, char *buf, size_t buf_size) {
  if (pool->used + buf_size > pool->capacity) {
    size_t new_capacity = MAX(2 * pool->capacity, pool->used + buf_size);
    memory_pool_realloc(pool, new_capacity);
  }

  for (size_t i = 0; i < buf_size; i++) {
    pool->base[pool->used + i] = buf[i];
  }
  char *retval = pool->base + pool->used;
  pool->used += buf_size;
  return retval;
}

typedef struct {
  char *name;
  size_t name_length;
  char *body;
  size_t body_length;
} macro;

typedef struct {
  size_t key;
  macro value;
} macro_hash;

// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
size_t
fnv1_hash_64(const char *byte, size_t length)
{
  size_t hash = 1099511628211u;
  for (size_t i = 0; i < length; i++)
    {
      hash *= 14695981039346656037u;
      hash ^= byte[i];
    }
  return hash;
}

int main() {
  size_t line_buf_size = 1024 * 4;
  char *line_buf = malloc(line_buf_size);
  if (line_buf == NULL) {
    return 1;
  }

  size_t temp_buf_size = 1024 * 4;
  char *temp_buf = malloc(temp_buf_size);
  if (line_buf == NULL) {
    return 1;
  }

  memory_pool_t macro_names = {0};
  memory_pool_init(&macro_names, 1024);
  if (!memory_pool_is_initialized(&macro_names)) {
    return 1;
  }

  memory_pool_t macro_values = {0};
  memory_pool_init(&macro_values, 1025 * 16);
  if (!memory_pool_is_initialized(&macro_values)) {
    return 1;
  }

  time_t t = time(NULL);
  stbds_rand_seed(t);

  macro_hash *macros = NULL;

  while (true) {
    char *line = get_line(line_buf, line_buf_size, stdin);
    if (line == NULL) {
      break;
    }

    if (!end_width(line, '\n')) {
      // This is the last line of the input.
      fprintf(stdout, "%s", line);
      break;
    }

    size_t line_length = 0;
    while (true) {
      if (line[line_length] == '\n') {
        break;
      }
      line_length++;
    }

    if (begin_with(line, "#+MACRO: ")) {
      // This line may define a macro.
      const size_t name_offset = 9; // 9 = strlen("#+MACRO: ")

      if (line[name_offset] == '\n') {
        break;
      }

      size_t macro_name_length = 0;
      while (true) {
        if (line[name_offset + macro_name_length] == ' ') {
          break;
        }
        if (line[name_offset + macro_name_length] == '\n') {
          // This line does not define a macro.
          macro_name_length = line_length + 1;
          break;
        }
        macro_name_length++;
      }

      if (macro_name_length == line_length + 1) {
        fprintf(stdout, "%s", line);
        continue;
      }

      char *macro_name = memory_pool_put(&macro_names, line + name_offset, macro_name_length);
      memory_pool_put(&macro_names, "", 1);
      size_t macro_name_hash = fnv1_hash_64(line + name_offset, macro_name_length);

      size_t value_offset = name_offset + macro_name_length + 1; // 1 = strlen(" ")
      size_t macro_value_length = line_length - value_offset;
      char *macro_value = memory_pool_put(&macro_values, line + value_offset, macro_value_length);
      memory_pool_put(&macro_values, "", 1);

      macro m = {macro_name, macro_name_length, macro_value, macro_value_length};
      hmput(macros, macro_name_hash, m);
    }

    // macro expansion
    while (true) {
      fprintf(stderr, "processing: %s\n", line);
      const size_t bbb_offset = 3; // 3 = strlen("{{{") or strlen("}}}")
      const size_t invalid_index = line_length + 1;
      size_t macro_begin = invalid_index;
      size_t macro_end = invalid_index;
      if (true) {
        // find the last macro call.
        size_t i = line_length;
        while (true) {
          fprintf(stderr, "i = %zu\n", i);
          if (i < bbb_offset - 1) {
            break;
          }
          if (line[i] == '{') {
            i--;
            if (line[i] == '{') {
              i--;
              if (line[i] == '{') {
                macro_begin = i;
                break;
              }
            }
          } else if (line[i] == '}') {
            i--;
            if (line[i] == '}') {
              i--;
              if (line[i] == '}') {
                macro_end = i + 3;
                if (i == 0) {
                  macro_end = invalid_index;
                  break;
                }
                i--;
                continue;
              }
            }
          } else {
            i--;
          }
        }
      }

      if (macro_begin == invalid_index || macro_end == invalid_index) {
        // no macro to expand.
        break;
      }

      bool isConstant = true;
      for (size_t i = macro_begin + bbb_offset; i < macro_end - bbb_offset; i++) {
        if (line[i] == '(') {
          isConstant = false;
          break;
        }
      }

      if (isConstant) {
        char *macro_name = line + macro_begin + bbb_offset;
        size_t macro_name_length = macro_end - macro_begin - 2 * bbb_offset;
        size_t macro_name_hash = fnv1_hash_64(macro_name, macro_name_length);
        macro m = hmget(macros, macro_name_hash);
        // m may be zero value, but that is not a problem in the following procedure.

        // temp_buf = line[macro_end:]
        if (temp_buf_size < line_length - macro_end) {
          temp_buf_size = line_length - macro_end;
          char *tmp = realloc(temp_buf, temp_buf_size);
          if (tmp == NULL) {
            return 1;
          }
          temp_buf = tmp;
        }
        for (size_t i = 0; i < line_length - macro_end; i++) {
          temp_buf[i] = line[macro_end + i];
        }

        // line = line[:macro_begin] + macro_body + line[maro_end:]
        size_t new_line_length = macro_begin + m.body_length + (line_length - macro_end);
        if (line_buf_size < new_line_length + 1) {
          char *tmp = realloc(temp_buf, new_line_length + 1);
          if (tmp == NULL) {
            return 1;
          }
          temp_buf = tmp;
        }
        for (size_t i = 0; i < m.body_length; i++) {
          line[macro_begin + i] = m.body[i];
        }
        for (size_t i = 0; i < line_length - macro_end; i++) {
          line[macro_begin + m.body_length + i] = temp_buf[i];
        }
        line[new_line_length] = '\n';
        line[new_line_length+1] = '\0';
        line_length = new_line_length;
      }
    }

    fprintf(stdout, "%s", line);
  }

  memory_pool_free(&macro_names);
  memory_pool_free(&macro_values);
  free(line_buf);
  free(temp_buf);
  return 0;
}
