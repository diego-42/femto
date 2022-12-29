#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/stat.h>

#define EDITOR_INIT_LINE_CAP 1024

typedef struct Line {
  char *data;
  size_t size;
} Line;

typedef enum Mode {
  MODE_NAVIGATION = 0,
  MODE_EDIT,
} Mode;

char *mode_to_str(Mode mode) {
  switch (mode) {
    case MODE_NAVIGATION: return "navigation";
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

  int offset_row;
  int offset_col;
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


void editor_insert_new_line(Editor *e) {
  if (e->l_size >= e->l_cap) {
    e->l_cap = e->l_cap == 0 ? EDITOR_INIT_LINE_CAP : e->l_cap * 2;

    e->lines = realloc(e->lines, e->l_cap * sizeof(Line));
    if (e->lines == NULL) {
      fprintf(stderr, "[Error]: Could not expand the size of lines buffer.\n");
      exit(1);
    }
  }

  memmove(&e->lines[e->cursor.y + e->offset_row + 2], &e->lines[e->cursor.y + e->offset_row + 1], (e->l_size - (e->cursor.y + e->offset_row)) * sizeof(Line));

  Line *current_line = &e->lines[e->cursor.y + e->offset_row];

  size_t new_line_size = current_line->size - (e->cursor.x + e->offset_col);

  current_line->size -= new_line_size;

  char *new_line_data = malloc(new_line_size);
  memcpy(new_line_data, &current_line->data[e->cursor.x + e->offset_col], new_line_size);

  Line new_line = {
    .data = new_line_data,
    .size = new_line_size
  };

  e->lines[e->cursor.y + e->offset_row + 1] = new_line;
  e->l_size++;
}

void editor_remove_line(Editor *e) {
  Line *current_line = &e->lines[e->cursor.y + e->offset_row];
  Line *previous_line = &e->lines[e->cursor.y + e->offset_row - 1];

  previous_line->data = realloc(previous_line->data, previous_line->size + current_line->size);
  memcpy(&previous_line->data[previous_line->size], current_line->data, current_line->size);
  previous_line->size += current_line->size;


  memmove(&e->lines[e->cursor.y + e->offset_row], &e->lines[e->cursor.y + e->offset_row + 1], (e->l_size - (e->cursor.y + e->offset_row)) * sizeof(Line));

  e->l_size--;
}

void editor_insert_char_line(Editor *e, char value) {
  Line *line = &e->lines[e->cursor.y + e->offset_row];

  line->data = realloc(line->data, line->size + 1);

  memmove(&line->data[e->cursor.x + e->offset_col + 1], &line->data[e->cursor.x + e->offset_col], line->size - (e->cursor.x + e->offset_col));

  line->data[e->cursor.x + e->offset_col] = value;
  line->size++;
}

void editor_remove_char_line(Editor *e) {
  Line *line = &e->lines[e->cursor.y + e->offset_row];

  memmove(&line->data[e->cursor.x + e->offset_col - 1], &line->data[e->cursor.x + e->offset_col], line->size - (e->cursor.x + e->offset_col));

  line->size--;
}

void editor_initialize(Editor *e) {
  struct stat buffer;
  if (stat(e->file_path, &buffer) < 0) {
    if (errno == ENOENT) {
      editor_push_line(e, "", 0);
      return;
    } else {
      fprintf(stderr, "[Error]: Could not determine if file exists\n");
      exit(1);
    }
  }

  if ((buffer.st_mode & S_IFMT) == S_IFREG) {
    fprintf(stderr, "[Error]: Could not open the file '%s': is not a regular file\n", e->file_path);
    exit(1);
  }

  FILE *f = fopen(e->file_path, "rb");

  if (f == NULL) {
    fprintf(stderr, "[Error]: Could not open the file '%s': %s\n", e->file_path, strerror(errno));
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
  cursor += sprintf(buffer + cursor, "%d:%d ", e->cursor.y + 1 + e->offset_row, e->cursor.x + e->offset_col + 1);

  if (cursor + strlen(e->file_path) < (size_t)cols) {
    cursor += sprintf(buffer + cursor, e->file_path);
  } else {
    cursor += sprintf(buffer + cursor, "%.*s...", (cols - cursor) - 3, e->file_path);
  }

  while (cursor < cols) {
    buffer[cursor++] = ' ';
  }

  fputs(buffer, stdout);

  free(buffer);
}

//TODO: Update render method and add buffer to print the entire screen once
void editor_render(Editor *e) {
  struct winsize w;
  ioctl(fileno(stdout), TIOCGWINSZ, &w);

  int rows = w.ws_row;
  int cols = w.ws_col;

  printf("\033[2J");
  printf("\033[H");

  for (int i = 0; i < rows - 1; i++) {
    if (i + e->offset_row < (int)e->l_size) {
      Line line = e->lines[i + e->offset_row];
      int local_offset_col = e->offset_col;
      int local_line_size = line.size;

      if (local_offset_col > local_line_size) local_offset_col = local_line_size;

      local_line_size -= local_offset_col;

      if (local_line_size > cols) local_line_size = cols;
      fwrite(line.data + local_offset_col, sizeof(*line.data), local_line_size, stdout);
    } else {
      fputc('+', stdout);
    }

    printf("\033[1G\033[1B");
  }

  printf("\033[7m");

  editor_render_status_bar(e, cols);

  printf("\033[27m");

  printf("\033[%d;%dH", e->cursor.y + 1, e->cursor.x + 1);

  fflush(stdout);
}

void editor_save_file(Editor *e) {
  FILE *f = fopen(e->file_path, "wb");

  for (size_t i = 0; i < e->l_size; i++) {
    Line line = e->lines[i];

    fwrite(line.data, sizeof(*line.data), line.size, f);
    fputc('\n', f);
  }

  fclose(f);
}

void editor_check_scroll(Editor *e) {
  struct winsize w;
  ioctl(fileno(stdout), TIOCGWINSZ, &w);

  while (e->cursor.y < 0) {
    e->cursor.y++;
    
    if (e->offset_row > 0) e->offset_row--;
  }

  while (e->cursor.y >= w.ws_row - 1) {
    e->cursor.y--;

    if (e->offset_row < (int)(e->l_size - w.ws_row + 1)) e->offset_row++;
  }


  while (e->cursor.x < 0) {
    e->cursor.x++;

    if (e->offset_col > 0) e->offset_col--;
  }

  Line line = e->lines[e->cursor.y + e->offset_row];

  while (e->cursor.x >= w.ws_col) {
    e->cursor.x--;

    if (e->offset_col < (int)(line.size - w.ws_col + 1)) e->offset_col++;
  }

}
int editor_navigation_mode(Editor *e) {
  int quit = 0;

  char seq[32];

  int seq_len = read(fileno(stdin), seq, 32);

  if (seq_len == 1) {
    char c = seq[0];
  
    switch (c) {
      case 'q': quit = 1; break;
      case 'd':
        e->cursor.y--;
        break;
      case 'f':
        e->cursor.y++;
        break;
      case 'a':
        e->cursor.x--;
        break;
      case 's':
        e->cursor.x++;
        break;
      case 'e':
        e->mode = MODE_EDIT;
        break;
      case 'r':
        editor_save_file(e);
        break;
    }
  }

  if (e->cursor.y > (int)e->l_size) e->cursor.y = e->l_size;

  if (e->cursor.y < (int)e->l_size) {
    Line line = e->lines[e->cursor.y + e->offset_row];
    while (e->cursor.x > (int)line.size) {
      if (e->offset_col > 0) e->offset_col--;
      else e->cursor.x--;
    }
  } else {
    e->cursor.x = 0;
    e->offset_col = 0;
  }

  editor_check_scroll(e);

  return quit;
}

void editor_edit_mode(Editor *e) {
  char seq[32];
  int seq_len = read(fileno(stdin), seq, 32);
  (void)seq_len;

  if (strcmp(seq, "\x7f") == 0) {
    if (e->cursor.x > 0) {
      editor_remove_char_line(e);

      if (e->offset_col > 0) e->offset_col--;
      else e->cursor.x--;
    } else {
      if (e->cursor.y > 0 || e->offset_row > 0) {
        int new_cursor_x = e->lines[e->cursor.y + e->offset_row - 1].size;

        editor_remove_line(e);
        e->cursor.y--;
        e->cursor.x = new_cursor_x;
      }
    }
  } else if (strcmp(seq, "\x1b") == 0) {
    e->mode = MODE_NAVIGATION;
  } else { 
    char c = seq[0];

    if (c == '\n') {
      editor_insert_new_line(e);
      e->cursor.y++;
      e->cursor.x = 0;
      e->offset_col = 0;
    } else {
      if (e->cursor.y + e->offset_row >= (int)e->l_size) editor_push_line(e, "", 0);

      editor_insert_char_line(e, c);
      e->cursor.x++;
    }
  }

  editor_check_scroll(e);
}

void editor_run(Editor *e) {
  editor_render(e);

  int quit = 0;
  while (!quit) {
    editor_render(e);

    switch (e->mode) {
      case MODE_NAVIGATION:
        quit = editor_navigation_mode(e);
        break;
      case MODE_EDIT:
        editor_edit_mode(e);
        break;
    }
  }

  printf("\033[2J\033[H");
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

  editor_initialize(editor);

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
