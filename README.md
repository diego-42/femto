# Femto

Femto is a terminal text editor like [vim](https://github.com/vim/vim).

# Quick Start

```bash
$ ./build.sh
$ ./femto femto.c
```

# Usage

Femto have two modes, navigation mode and edit mode.

## navigation mode

Navigation mode is where you can navigate to entire file.

| keys    | action                               |
|---------|--------------------------------------|
| f       | move cursor down                     |
| d       | move cursor up                       |
| s       | move cursor left                     |
| a       | move cursor right                    |
| j       | move cursor to next empty line       |
| k       | move cursor to previous empty line   |
| shit+j  | move cursor to beginning of the file |
| shit+k  | move cursor to end of the file       |
| (space) | enter in edit mode                   |

## edito mode

Edit mode is where you can edit the entire file.

| keys                       | action                    |
|----------------------------|---------------------------|
| (esc)                      | exit of edit mode         |
| (any printable ascii char) | put this char in the file |
