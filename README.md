# File Explorer (Assignment 1)

Simple terminal file explorer implemented in C.

## Features
- `ls [path]` : list directory contents (permissions, size, mtime)
- `cd <path>` : change current directory
- `pwd` : print working directory
- `info <path>` : show file metadata
- `create <path>` : create empty file
- `copy <src> <dst>` : copy file
- `move <src> <dst>` : move/rename file
- `delete <path>` : delete file (no recursive directory delete in this version)
- `search <pattern> [path]` : recursive name search
- `chmod <octal> <path>` : change permission bits (Linux only)
- `help`, `exit`

## Build (Linux / WSL / MSYS2)
```bash
make
