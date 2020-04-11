#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#include "buffer.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wcast-align"
#pragma clang diagnostic ignored "-Wshadow"
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#pragma clang diagnostic pop

static bool begin_with(const char *a, const char *b) {
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

static bool end_with(const char *a, const char c) {
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

typedef struct {
  size_t name_offset; // redundant
  size_t name_length; // redundant
  size_t body_offset;
  size_t body_length; // redundant
} macro_def;

typedef struct {
  size_t key;
  macro_def value;
} macro_hash_t;

// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
static size_t fnv1_hash_64(const char *byte, size_t length)
{
  size_t hash = 1099511628211u;
  for (size_t i = 0; i < length; i++)
    {
      hash *= 14695981039346656037u;
      hash ^= (size_t)(byte[i]);
    }
  return hash;
}

int main() {
  const size_t buffer_initial_size = 4;
  buffer_t line_buf = {0};
  buffer_init(&line_buf, buffer_initial_size);
  if (line_buf.data == NULL) {
    return 1;
  }

  buffer_t temp_buf = {0};
  buffer_init(&temp_buf, buffer_initial_size);
  if (temp_buf.data == NULL) {
    return 1;
  }

  buffer_t macro_names = {0};
  buffer_init(&macro_names, buffer_initial_size);
  if (macro_names.data == NULL) {
    return 1;
  }

  buffer_t macro_bodies = {0};
  buffer_init(&macro_bodies, buffer_initial_size);
  if (macro_bodies.data == NULL) {
    return 1;
  }

  time_t t = time(NULL);
  stbds_rand_seed((size_t)(t));

  macro_hash_t *macros = NULL;

  while (true) {
    buffer_get_line(&line_buf, stdin);
    if (line_buf.capacity == 0) {
      break;
    }

    if (!end_with(line_buf.data, '\n')) {
      // This line is the last line of the input.
      buffer_put(&line_buf, "\n", 1);
    }

    if (begin_with(line_buf.data, "#+MACRO: ")) {
      // This line may define a macro.
      const size_t line_length = line_buf.used - 2;
      const size_t name_offset = 9; // 9 = strlen("#+MACRO: ")
      const size_t invalid = line_length + 1;

      if (line_buf.data[name_offset] == '\n') {
        break;
      }

      size_t name_length = 0;
      while (true) {
        if (line_buf.data[name_offset + name_length] == ' ') {
          break;
        }
        if (line_buf.data[name_offset + name_length] == '\n') {
          // This line does not define a macro.
          name_length = invalid;
          break;
        }
        name_length++;
      }

      if (name_length == invalid) {
        fprintf(stdout, "%s", line_buf.data);
        continue;
      }

      buffer_put(&macro_names, line_buf.data + name_offset, name_length);
      buffer_put(&macro_names, "", 1);

      size_t body_offset = name_offset + name_length + 1; // 1 = strlen(" ")
      size_t body_length = line_length - body_offset;
      buffer_put(&macro_bodies, line_buf.data + body_offset, body_length);
      buffer_put(&macro_bodies, "", 1);

      macro_def m = {macro_names.used - 1 - name_length, name_length, macro_bodies.used - 1 - body_length, body_length};
      size_t macro_name_hash = fnv1_hash_64(line_buf.data + name_offset, name_length);
      hmput(macros, macro_name_hash, m);

      fprintf(stdout, "%s", line_buf.data);
      continue;
    }

    // macro expansion
    while (true) {
      const size_t line_length = line_buf.used - 2;
      const size_t bbb_offset = 3; // 3 = strlen("{{{") or strlen("}}}")
      const size_t invalid_index = line_length + 1;
      size_t macro_begin = invalid_index;
      size_t macro_end = invalid_index;
      if (true) {
        // find the last macro call.
        size_t i = line_length;
        while (true) {
          if (i < bbb_offset - 1) {
            break;
          }
          if (line_buf.data[i] == '{') {
            i--;
            if (line_buf.data[i] == '{') {
              i--;
              if (line_buf.data[i] == '{') {
                macro_begin = i;
                break;
              }
            }
          } else if (line_buf.data[i] == '}') {
            i--;
            if (line_buf.data[i] == '}') {
              i--;
              if (line_buf.data[i] == '}') {
                if (i == 0) {
                  macro_end = invalid_index;
                  break;
                }
                macro_end = i + 3;
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

      size_t name_sep = macro_begin;
      while (true) {
        if (line_buf.data[name_sep] == '(' || line_buf.data[name_sep] == '}') {
          break;
        }
        name_sep++;
      }
      const char *macro_name = line_buf.data + macro_begin + bbb_offset;
      size_t name_length = name_sep - macro_begin - bbb_offset;
      size_t macro_name_hash = fnv1_hash_64(macro_name, name_length);
      macro_def m = hmget(macros, macro_name_hash); // m may be a zero value.

      if (m.name_length == 0) {
        // the macro is undefined.
        line_buf.used = macro_begin;
        size_t line_rest_size = line_length - macro_end + 2; // 2 = strlen("\n\0")
        buffer_put(&line_buf, line_buf.data + macro_end, line_rest_size);
        continue;
      }

      bool isConstant = (macro_end - macro_begin - 2 * bbb_offset) == name_length;
      if (isConstant) {
        // temp_buf = line[macro_end:]
        temp_buf.used = 0;
        buffer_put(&temp_buf, line_buf.data + macro_end, line_length - macro_end + 2); // 2 = strlen("\n\0")

        // line = line[:macro_begin] + macro_body + line[maro_end:]
        line_buf.used = macro_begin;
        buffer_put(&line_buf, macro_bodies.data + m.body_offset, m.body_length);
        buffer_copy(&line_buf, &temp_buf);
      } else if (!isConstant) {
        // parse the arguments.
        size_t argOffsets[11] = {0};
        argOffsets[0] = macro_begin + bbb_offset + m.name_length + 1; // 1 = strlen("(")
        argOffsets[1] = macro_begin + bbb_offset + m.name_length + 1; // 1 = strlen("(")
        size_t currentIndex = 2;
        for (size_t i = argOffsets[1]; i < macro_end - bbb_offset; i++) {
          if (currentIndex == 10) {
            break;
          }
          if (line_buf.data[i] == ',') {
            argOffsets[currentIndex] = i + 1;
            currentIndex++;
          }
        }
        argOffsets[currentIndex] = macro_end - bbb_offset;

        // Expand the macro on temp_buf (we cannot modify line_buf because argOffsets depends on it).
        temp_buf.used = 0;
        size_t writeFrom = m.body_offset;
        size_t writeUntil = m.body_offset;
        while (true) {
          if (macro_bodies.data[writeUntil] == '\0') {
            buffer_put(&temp_buf, macro_bodies.data + writeFrom, writeUntil - writeFrom);
            break;
          }
          if (macro_bodies.data[writeUntil] == '$') {
            // If macro_bodies.data[writeUntil] != '\n', we can read macro_bodies.data[writeUntil + 1].
            writeUntil++;
            if ('0' <= macro_bodies.data[writeUntil] && macro_bodies.data[writeUntil] <= '9') {
              size_t offset = 1; // 1 = strlen("$")
              buffer_put(&temp_buf, macro_bodies.data + writeFrom, writeUntil - writeFrom - offset);
              size_t argIndex = (size_t)(macro_bodies.data[writeUntil]) - '0';
              size_t writeSize = 0;
              if (argIndex == 0) {
                // The replacement is the whole text in the brackets.
                writeSize = macro_end - macro_begin - m.name_length - 2 * bbb_offset - 2; // 2 = strlen("()")
              } else {
                writeSize = argOffsets[argIndex + 1] - argOffsets[argIndex] - 1; // 1 = strlen(",") or strlen(")")
              }
              buffer_put(&temp_buf, line_buf.data + argOffsets[argIndex], writeSize);
              writeUntil++;
              writeFrom = writeUntil;
              continue;
            }
          }
          writeUntil++;
        }
        size_t macro_expanded_size = temp_buf.used;
        size_t line_rest_size = line_length - macro_end + 2; // 2 = strlen("\n\0")
        buffer_put(&temp_buf, line_buf.data + macro_end, line_rest_size);

        line_buf.used = macro_begin;
        buffer_put(&line_buf, temp_buf.data, macro_expanded_size);
        buffer_put(&line_buf, temp_buf.data + macro_expanded_size, line_rest_size);
      }
    }
    fprintf(stdout, "%s", line_buf.data);
  }

  free(macro_names.data);
  free(macro_bodies.data);
  free(line_buf.data);
  free(temp_buf.data);
  hmfree(macros);
  return 0;
}
