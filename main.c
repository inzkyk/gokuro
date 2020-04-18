#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

#ifdef __clang__
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

static bool is_digit(char c) {
  return '0' <= c && c <= '9';
}

typedef struct {
  uint32_t is_valid;

  // offset from {global|local}_macro_bodies.data (we cannot store
  // the pointer because realloc() may change the base pointer)
  uint32_t offset;
} macro_def;

typedef struct {
  uint64_t key; // key is 64 bit due to alignment requirement.
  macro_def value;
} macro_hash_t;

static uint64_t hash(void *data_begin, void *data_end) {
  uint64_t hash = 1099511628211u;
  for (unsigned char *c = (unsigned char *)(data_begin); c < (unsigned char *)data_end; c++) {
    hash = hash * 33 + *c;
  }
  return hash;
}

static void gokuro(FILE *in, FILE *out) {
  time_t t = time(NULL);
  stbds_rand_seed((uint32_t)(t));

  macro_hash_t *global_macros = NULL;
  macro_hash_t *local_macros = NULL;

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

  { // load input to input_buf
    buffer_read_all(&input_buf, in);
    if (input_buf.used == 0) {
      // nothing to output.
      return;
    }

    char last_char = *(input_buf.data + input_buf.used - 1);
    if (last_char != '\n') {
      // normalize the input.
      buffer_put_char(&input_buf, '\n');
    }
    buffer_put_char(&input_buf, '\0');
  }

  // the mainloop.
  char *input_index = input_buf.data;
  while (true) {
    if (*input_index == '\0') {
      break;
    }

    // read next line to line_buf.
    buffer_clear(&line_buf);
    buffer_put_until_char(&line_buf, input_index, '\n');
    buffer_put_char(&line_buf, '\0');
    input_index += line_buf.used;

    char *line_begin = line_buf.data;
    char *line_end = line_buf.data + line_buf.used - 1; // 1 = strlen("\0")

    // Does this line define a global macro?
    if (begin_with(line_begin, "#+MACRO: ")) {
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

      buffer_put_until_char(&global_macro_bodies, name_end + 1, '\0');
      buffer_put_char(&global_macro_bodies, '\0');

      uint32_t body_length = (uint32_t)(line_end - name_end) - 1; // 1 = strlen(" ")
      macro_def m = {true, global_macro_bodies.used - body_length - 1}; // 1 = strlen("\0")
      uint64_t macro_name_hash = hash(name_begin, name_end);
      hmput(global_macros, macro_name_hash, m);

      fputs(line_begin, out);
      fputs("\n", out);
      continue;
    }

    // Does this line define a local macro?
    bool local_macro_defined = false;
    if (begin_with(line_begin, "#+")) {
      char *name_begin = line_begin + 2; // 2 = strlen("#+")
      char *name_end = NULL;
      { // find ":"
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
      buffer_put_until_char(&local_macro_bodies, name_end + 2, '\0'); // 2 = strlen(": ")
      buffer_put_char(&local_macro_bodies, '\0');

      uint32_t body_length = (uint32_t)(line_end - name_end) - 2; // 1 = strlen(": ")
      macro_def m = {true, local_macro_bodies.used - body_length - 1}; // 1 = strlen("\0")
      uint64_t macro_name_hash = hash(name_begin, name_end);
      hmput(local_macros, macro_name_hash, m);

      fputs(line_begin, out);
      fputs("\n", out);
      continue;
    }

    // macro expansion
    uint32_t macro_offset = 0; // line_buf does not have a macro from (line_end - macro_offset) to (line_end).
    while (true) {
      const uint32_t bbb_offset = 3; // 3 = strlen("{{{") or strlen("}}}")

      // need to reasign line_begin and line_end because line_buf gets modified in macro expansion.
      line_begin = line_buf.data;
      line_end = line_buf.data + line_buf.used - 1; // 1 = strlen("\0")

      // expand the last macro call first, so that nested calls are treated properly.
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
      uint64_t name_hash = hash(name_begin, name_end);
      macro_def m = hmget(local_macros, name_hash);
      char* macro_body = local_macro_bodies.data + m.offset;
      if (!m.is_valid) {
        m = hmget(global_macros, name_hash);
        macro_body = global_macro_bodies.data + m.offset;
      }

      if (!m.is_valid) {
        // the macro is undefined.
        buffer_shrink_to(&line_buf, macro_begin);
        buffer_put_until_char(&line_buf, macro_end, '\0');
        buffer_put_char(&line_buf, '\0');
        continue;
      }

      if (is_constant) {
        // temp_buf = line[macro_end:]
        buffer_clear(&temp_buf);
        buffer_put_until_char(&temp_buf, macro_end, '\0');

        // line = line[:macro_begin] + macro_body + line[maro_end:]
        buffer_shrink_to(&line_buf, macro_begin);
        buffer_put_until_char(&line_buf, macro_body, '\0');
        buffer_copy(&line_buf, &temp_buf);
        buffer_put_char(&line_buf, '\0');
      } else {
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
        buffer_put_until_ptr(&temp_buf, line_begin, macro_begin);
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

        // copy the expanded line to line_buf.
        buffer_put_until_char(&temp_buf, macro_end, '\0'); // 1 = strlen("\0")
        buffer_clear(&line_buf);
        buffer_copy(&line_buf, &temp_buf);
        buffer_put_char(&line_buf, '\0');
      }
    }

    fputs(line_begin, out);
    fputs("\n", out);
    hmfree(local_macros);
    local_macros = NULL;
  }

  buffer_free(&global_macro_bodies);
  buffer_free(&local_macro_bodies);
  buffer_free(&input_buf);
  buffer_free(&line_buf);
  buffer_free(&temp_buf);
  hmfree(global_macros);
  fflush(out);
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

  gokuro(stdin, stdout);
}
