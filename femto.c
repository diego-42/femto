#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <termios.h>

#include <sys/ioctl.h>

#define EDITOR_INIT_LINE_CAP 1024

typedef struct Line {
  char *data;
  size_t size;
} Line;

typedef enum Mode {
  MODE_NORMAL = 0,
  MODE_EDIT,
} Mode;

char *mode_to_str(Mode mode) {
  switch (mode) {
    case MODE_NORMAL: return "normal";
    case MODE_EDIT:   return "edit";
    default:          return "unknown";
  }
}

typedef struct Cursor {
  int x;
  int y;
} Cursor;

typedef struct Editor {
  char *file_path;

  Line *lines;
  size_t l_size;
  size_t l_cap;

  Mode mode;

  Cursor cursor;
} Editor;

Editor *editor_create() {
  Editor *e = calloc(1, sizeof(Editor));

  return e;
}

void editor_free(Editor *e) {
  for (size_t i = 0; i < e->l_size; i++) {
    free(e->lines[i].data);
  }

  free(e->lines);
  free(e);
}

void editor_push_line(Editor *e, char *data, size_t size) {
  if (e->l_size >= e->l_cap) {
    e->l_cap = e->l_cap == 0 ? EDITOR_INIT_LINE_CAP : e->l_cap * 2;

    e->lines = realloc(e->lines, e->l_cap * sizeof(Line));
    if (e->lines == NULL) {
      fprintf(stderr, "[Error]: Could not expand the size of lines buffer.\n");
      exit(1);
    }
  }

  char *copy_data = malloc(size);
  memcpy(copy_data, data, size);

  Line line = {
    .data = copy_data,
    .size = size
  };

  e->lines[e->l_size++] = line;
}


void editor_insert_line(Editor *e, int cursor, char *data, size_t size) {
  if (e->l_size >= e->l_cap) {
    e->l_cap = e->l_cap == 0 ? EDITOR_INIT_LINE_CAP : e->l_cap * 2;

    e->lines = realloc(e->lines, e->l_cap * sizeof(Line));
    if (e->lines == NULL) {
      fprintf(stderr, "[Error]: Could not expand the size of lines buffer.\n");
      exit(1);
    }
  }

  memmove(&e->lines[cursor + 1], &e->lines[cursor], (e->l_size - cursor) * sizeof(Line));

  char *copy_data = malloc(size);
  memcpy(copy_data, data, size);

  Line line = {
    .data = copy_data,
    .size = size
  };

  e->lines[cursor] = line;
  e->l_size++;
}

void editor_update_line(Line *line, int cursor, char value) {
  line->data = realloc(line->data, line->size + 1);

  memmove(&line->data[cursor + 1], &line->data[cursor], line->size - cursor);

  line->data[cursor] = value;
  line->size++;
}

void editor_initialize(Editor *e, char *input_path) {
  FILE *f = fopen(input_path, "rb");

  if (f == NULL) {
    fprintf(stderr, "[Error]: Could not open the file '%s': %s\n", input_path, strerror(errno));
    exit(1);
  }
  
  char *data = NULL;
  size_t len = 0;
  ssize_t size;

  while ((size = getline(&data, &len, f)) != -1) {
    editor_push_line(e, data, size - 1);
  }

  if (data) free(data);

  fclose(f);
}

void editor_render_status_bar(Editor *e, int cols) {
  char *buffer = calloc(cols + 1, sizeof(char));
  int cursor = 0;

  cursor += sprintf(buffer + cursor, "[[ %s ]] ", mode_to_str(e->mode));
  cursor += sprintf(buffer + cursor, "%d:%d ", e->cursor.y + 1, e->cursor.x + 1);

  if (cursor + strlen(e->file_path) < (size_t)cols) {
    cursor += sprintf(buffer + cursor, e->file_path);
  } else {
    cursor += sprintf(buffer + cursor, "%.*s...", (cols - cursor) - 3, e->file_path);
  }

  while (cursor < cols) {
    buffer[cursor++] = ' ';
  }

  printf(buffer);

  free(buffer);
}

void editor_render(Editor *e) {
  struct winsize w;
  ioctl(fileno(stdout), TIOCGWINSZ, &w);

  int rows = w.ws_row;
  int cols = w.ws_col;

  printf("\033[2J");
  printf("\033[H");

  for (int i = 0; i < rows - 1; i++) {
    if (i < (int)e->l_size) {
      Line line = e->lines[i];

      size_t line_size = line.size > (size_t)cols ? (size_t)cols : line.size;

      for (size_t j = 0; j < line_size; j++) {
        putchar(line.data[j]);
      }

      printf("\n");

      continue;
    }

    printf("+");
    printf("\033[1G\033[1B");
  }

  printf("\033[7m");

  editor_render_status_bar(e, cols);

  printf("\033[27m");

  printf("\033[%d;%dH", e->cursor.y + 1, e->cursor.x + 1);
}

void editor_save_file(Editor *e) {
  FILE *f = fopen(e->file_path, "wb");

  for (size_t i = 0; i < e->l_size; i++) {
    Line line = e->lines[i];

    fprintf(f, "%*s\n", (int)line.size, line.data);
  }

  fclose(f);
}

int editor_normal_mode(Editor *e) {
  int quit = 0;

  char c = getchar();

  switch (c) {
    case 'q': quit = 1; break;
    case 'w':
      if (e->cursor.y > 0) e->cursor.y--;
      break;
    case 's':
      e->cursor.y++;
      break;
    case 'a':
      if (e->cursor.x > 0) e->cursor.x--;
      break;
    case 'd':
      e->cursor.x++;
      break;
    case 'e':
      e->mode = MODE_EDIT;
      break;
    case 'f':
      editor_save_file(e);
      break;
  }

  if (e->cursor.y > (int)e->l_size) e->cursor.y = e->l_size;

  if (e->cursor.y < (int)e->l_size) {
    Line line = e->lines[e->cursor.y];
    if (e->cursor.x > (int)line.size) e->cursor.x = line.size;
  } else e->cursor.x = 0;

  return quit;
}

void editor_edit_mode(Editor *e) {
  char c = getchar();

  if (c == 27) e->mode = MODE_NORMAL;
  else {
    if (c == '\n') {
      editor_insert_line(e, ++e->cursor.y, "", 0);
      e->cursor.x = 0;
      return;
    }

    editor_update_line(&e->lines[e->cursor.y], e->cursor.x, c);
    e->cursor.x++;
  }
}

void editor_run(Editor *e) {
  editor_render(e);

  int quit = 0;
  while (!quit) {
    editor_render(e);

    switch (e->mode) {
      case MODE_NORMAL:
        quit = editor_normal_mode(e);
        break;
      case MODE_EDIT:
        editor_edit_mode(e);
        break;
    }
  }

  printf("\033[2J");
}

void usage(FILE *fd, char *program) {
  fprintf(fd, "[Usage]: %s <input_file>\n", program);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    usage(stderr, argv[0]);

    fprintf(stderr, "[Error]: File path is not provided.\n");
    exit(1);
  }

  char *input_path = argv[1];

  Editor *editor = editor_create();
  editor->file_path = input_path;

  editor_initialize(editor, input_path);

  struct termios term;
  tcgetattr(fileno(stdin), &term);

  term.c_lflag &= ~ECHO;
  term.c_lflag &= ~ICANON;
  tcsetattr(fileno(stdin),  0, &term);

  editor_run(editor);

  term.c_lflag |= ECHO;
  term.c_lflag |= ICANON;
  tcsetattr(fileno(stdin),  0, &term);

  editor_free(editor);
  return 0;
}
