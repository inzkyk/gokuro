#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wcast-align"
#pragma clang diagnostic ignored "-Wshadow"
#endif

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#if __clang__
#pragma clang diagnostic pop
#endif

#ifndef __cplusplus
typedef uint8_t bool;
#define true 1
#define false 0
#endif

#define lengthof(x) ((sizeof((x))) / (sizeof((x)[0])))

#include "buffer.h"

// a =? b + X
static bool begin_with(const char *a, const char *b) {
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

typedef struct {
  uint32_t is_valid;
  uint32_t body_offset;
} macro_def;

typedef struct {
  uint64_t key; // 64 bit due to alignment issue.
  macro_def value;
} macro_hash_t;

static uint64_t hash(void *data_begin, void *data_end)
{
  uint64_t hash = 1099511628211u;
  for (unsigned char *c = (unsigned char *)(data_begin); c < (unsigned char *)data_end; c++)
    {
      hash = hash * 33 + *c;
    }
  return hash;
}

static void gokuro(FILE *in, FILE *out) {
#ifdef _WIN32
  // output "\n" instead of "\r\n".
  int oldmode = _setmode(_fileno(out), _O_BINARY);
  if (oldmode == -1) {
    fprintf(stderr, "_setmode() failed.\n");
    return;
  }
#endif

  int result = setvbuf(out, NULL, _IOFBF, 1024 * 4);
  if (result != 0) {
    fprintf(stderr, "setvbuf() failed.\n");
    return;
  }

  result = setvbuf(in, NULL, _IOFBF, 1024 * 4);
  if (result != 0) {
    fprintf(stderr, "setvbuf() failed.\n");
    return;
  }

  const uint32_t buffer_initial_size = 1024 * 4;

  buffer_t input_buf = {0};
  buffer_init(&input_buf, buffer_initial_size);
  if (input_buf.data == NULL) {
    fprintf(stderr, "memory allocation failed.\n");
    return;
  }

  buffer_t line_buf = {0};
  buffer_init(&line_buf, buffer_initial_size);
  if (line_buf.data == NULL) {
    fprintf(stderr, "memory allocation failed.\n");
    return;
  }

  buffer_t temp_buf = {0};
  buffer_init(&temp_buf, buffer_initial_size);
  if (temp_buf.data == NULL) {
    fprintf(stderr, "memory allocation failed.\n");
    return;
  }

  buffer_t global_macro_bodies = {0};
  buffer_init(&global_macro_bodies, buffer_initial_size);
  if (global_macro_bodies.data == NULL) {
    fprintf(stderr, "memory allocation failed.\n");
    return;
  }

  buffer_t local_macro_bodies = {0};
  buffer_init(&local_macro_bodies, buffer_initial_size);
  if (local_macro_bodies.data == NULL) {
    fprintf(stderr, "memory allocation failed.\n");
    return;
  }

  time_t t = time(NULL);
  stbds_rand_seed((uint32_t)(t));

  macro_hash_t *global_macros = NULL;
  macro_hash_t *local_macros = NULL;

  buffer_read_all(&input_buf, in);
  if (input_buf.used == 0) {
    // input is empty.
    return;
  }
  if (*(input_buf.data + input_buf.used - 1) != '\n') {
    // normalize the input.
    buffer_put(&input_buf, "\n", 1);
  }
  buffer_put(&input_buf, "\0", 1);

  char *input_index = input_buf.data;
  while (true) {
    // read next line to line_buf.
    {
      char *c = input_index;
      if (*c == '\0') {
        break;
      }
      while (true) {
        if (*c == '\n') {
          break;
        }
        c++;
      }

      buffer_clear(&line_buf);
      buffer_put_until(&line_buf, input_index, c);
      buffer_put(&line_buf, "\0", 1);
      input_index = c + 1;
    }

    char *line_begin = line_buf.data;
    char *line_end = line_buf.data + line_buf.used - 1; // 1 = strlen("\0")

    // global macro definition.
    if (begin_with(line_begin, "#+MACRO: ")) {
      // This line may define a global macro.
      const uint32_t name_offset = 9; // 9 = strlen("#+MACRO: ")

      if (*(line_begin + name_offset) == '\0') {
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
          if (*c == '\0') {
            // This line does not define a global macro.
            break;
          }
          c++;
        }

        if (name_end == NULL) {
          fputs(line_begin, out);
          fputs("\n", out);
          continue;
        }
      }

      buffer_put_string(&global_macro_bodies, name_end + 1);
      buffer_put(&global_macro_bodies, "", 1);

      uint32_t body_length = (uint32_t)(line_end - name_end) - 1; // 1 = strlen(" ")
      macro_def m = {true, global_macro_bodies.used - body_length - 1}; // 1 = strlen("\0")
      uint64_t macro_name_hash = hash(name_begin, name_end);
      hmput(global_macros, macro_name_hash, m);

      fputs(line_begin, out);
      fputs("\n", out);
      continue;
    }

    // local macro definition.
    bool local_macro_defined = false;
    if (begin_with(line_begin, "#+")) {
      char *name_begin = line_begin + 2; // 2 = strlen("#+")
      char *name_end = NULL;
      {
        char *c = name_begin;
        while(*c != '\0') {
          if (*c == ':') {
            name_end = c;
            break;
          }
          c++;
        }
        if (name_end == NULL) {
          // this line does not define a local macro.
          fputs(line_begin, out);
          fputs("\n", out);
          continue;
        }
      }

      local_macro_defined = true;
      buffer_put_until(&local_macro_bodies, name_end + 2, line_end); // 2 = strlen(": ")
      buffer_put(&local_macro_bodies, "", 1);

      uint32_t body_length = (uint32_t)(line_end - name_end) - 2; // 1 = strlen(": ")
      macro_def m = {true, local_macro_bodies.used - body_length - 1}; // 1 = strlen("\0")
      uint64_t macro_name_hash = hash(name_begin, name_end);
      hmput(local_macros, macro_name_hash, m);
    }

    // macro expansion
    while (true) {
      const uint32_t bbb_offset = 3; // 3 = strlen("{{{") or strlen("}}}")
      line_begin = line_buf.data;
      line_end = line_buf.data + line_buf.used - 1; // 1 = strlen("\0")

      char *macro_begin = NULL;
      char *macro_end = NULL;
      { // find the last macro call.
        char *c = line_end; // *c == "\0"
        while (true) {
          if (c < line_begin) {
            break;
          }
          if (*c == '{') {
            c--;
            if (*c == '{') {
              c--;
              if (*c == '{') {
                macro_begin = c;
                break;
              }
            }
          } else if (*c == '}') {
            c--;
            if (*c == '}') {
              c--;
              if (*c == '}') {
                while (*(c - 1) == '}') {
                  c--;
                }
                macro_end = c + 3;
                c--;
                continue;
              }
            }
          } else {
            c--;
          }
        }

        if (macro_begin == NULL || macro_end == NULL) {
          // no macro to expand.
          break;
        }
      }

      char *name_begin = macro_begin + bbb_offset;
      char *name_end = NULL;
      bool isConstant;
      { // get the length of the macro name.
        char *c = name_begin;
        while (true) {
          if (*c == '(') {
            isConstant = false;
            break;
          }

          if (*c == '}') {
            isConstant = true;
            break;
          }
          c++;
        }
        name_end = c;
      }

      // lookup the definition of the macro.
      uint64_t name_hash = hash(name_begin, name_end);
      char* macro_body_data = local_macro_bodies.data;
      macro_def m = hmget(local_macros, name_hash);
      if (!m.is_valid) {
        m = hmget(global_macros, name_hash);
        macro_body_data = global_macro_bodies.data;
      }

      if (!m.is_valid) {
        // the macro is undefined.
        buffer_shrink_to(&line_buf, macro_begin);
        buffer_put_until(&line_buf, macro_end, line_end + 1); // 1 = strlen("\0")
        continue;
      }

      if (isConstant) {
        // temp_buf = line[macro_end:]
        buffer_clear(&temp_buf);
        buffer_put_until(&temp_buf, macro_end, line_end + 1); // 1 = strlen("\0")

        // line = line[:macro_begin] + macro_body + line[maro_end:]
        buffer_shrink_to(&line_buf, macro_begin);
        buffer_put_string(&line_buf, macro_body_data + m.body_offset);
        buffer_copy(&line_buf, &temp_buf);
      } else {
        // expand the macro on temp_buf (we cannot modify line_buf because args depends on it).

        // parse the arguments.
        char *args[9] = {0}; // 9 = max bumber of the positions of arguments  ($1...$9)
        args[0] = name_end + 1; // 1 = strlen("(")
        uint32_t currentIndex = 1;
        for (char *c = args[0]; c < macro_end; c++) {
          if (currentIndex == lengthof(args)) {
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

        buffer_clear(&temp_buf);
        buffer_put_until(&temp_buf, line_begin, macro_begin);
        char *writeFrom = macro_body_data + m.body_offset;
        char *c = macro_body_data + m.body_offset;
        while (true) {
          if (*c == '\0') {
            buffer_put_until(&temp_buf, writeFrom, c);
            break;
          }
          if (*c == '$') {
            // If *c != '\0', we can read *(c + 1).
            if ('0' <= *(c + 1) && *(c + 1) <= '9') {
              buffer_put_until(&temp_buf, writeFrom, c);
              uint32_t argIndex = (uint32_t)(*(c + 1) - '0');
              if (argIndex == 0) {
                // The replacement is the whole text in the brackets.
                char *until = macro_end - bbb_offset - 1; // 1 = strlen(")")
                buffer_put_until(&temp_buf, args[0], until);
              } else if (args[argIndex - 1] != NULL) {
                 if ((argIndex == 9) || args[argIndex] == NULL) {
                   // last argument
                   char *until = macro_end - bbb_offset - 1; // 1 = strlen(")")
                   buffer_put_until_escaping_comma(&temp_buf, args[argIndex - 1], until);
                } else {
                   char *until = args[argIndex] - 1; // 1 = strlen(",")
                   buffer_put_until_escaping_comma(&temp_buf, args[argIndex - 1], until);
                }
              }
              c += 2;
              writeFrom = c;
              continue;
            }
          }
          c++;
        }

        // copy the expanded line to line_buf.
        buffer_put_until(&temp_buf, macro_end, line_end + 1); // 1 = strlen("\0")
        buffer_clear(&line_buf);
        buffer_copy(&line_buf, &temp_buf);
      }
    }
    fputs(line_begin, out);
    fputs("\n", out);
    if (!local_macro_defined) {
      hmfree(local_macros);
      local_macros = NULL;
    }
  }

  free(global_macro_bodies.data);
  free(local_macro_bodies.data);
  free(line_buf.data);
  free(temp_buf.data);
  hmfree(global_macros);
  fflush(out);
}

int main() {
  gokuro(stdin, stdout);
}
