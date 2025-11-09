// src/main.c
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#ifdef _WIN32
#include <sys/stat.h>
/* Define POSIX-style permission bits for Windows (dummy placeholders) */
#ifndef S_IRUSR
#define S_IRUSR _S_IREAD
#endif
#ifndef S_IWUSR
#define S_IWUSR _S_IWRITE
#endif
#ifndef S_IXUSR
#define S_IXUSR 0
#endif
#ifndef S_IRGRP
#define S_IRGRP 0
#endif
#ifndef S_IWGRP
#define S_IWGRP 0
#endif
#ifndef S_IXGRP
#define S_IXGRP 0
#endif
#ifndef S_IROTH
#define S_IROTH 0
#endif
#ifndef S_IWOTH
#define S_IWOTH 0
#endif
#ifndef S_IXOTH
#define S_IXOTH 0
#endif
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

void pause_console() {
    printf("Press Enter to continue...");
    int c = getchar();
    (void)c;
}

/* ---------- Utilities ---------- */
void print_header(const char *t) {
    printf("\n=== %s ===\n", t);
}

int is_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

/* print permissions like rwxr-xr-- */
void print_perms(mode_t m) {
    char p[11];
    p[10] = '\0'; // null terminator

    p[0] = S_ISDIR(m) ? 'd' : '-';
    p[1] = (m & S_IRUSR) ? 'r' : '-';
    p[2] = (m & S_IWUSR) ? 'w' : '-';
    p[3] = (m & S_IXUSR) ? 'x' : '-';
    p[4] = (m & S_IRGRP) ? 'r' : '-';
    p[5] = (m & S_IWGRP) ? 'w' : '-';
    p[6] = (m & S_IXGRP) ? 'x' : '-';
    p[7] = (m & S_IROTH) ? 'r' : '-';
    p[8] = (m & S_IWOTH) ? 'w' : '-';
    p[9] = (m & S_IXOTH) ? 'x' : '-';

    printf("%s", p);
}

/* ---------- Feature: list directory ---------- */
void list_dir(const char *path) {
    DIR *d = opendir(path);
    if (!d) {
        perror("opendir");
        return;
    }
    struct dirent *entry;
    char full[PATH_MAX];
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        snprintf(full, sizeof(full), "%s/%s", path, entry->d_name);
        struct stat st;
        if (stat(full, &st) == 0) {
            print_perms(st.st_mode);
            printf(" %8lld ", (long long)st.st_size);
            char tbuf[64];
            struct tm *tm = localtime(&st.st_mtime);
            strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M", tm);
            printf("%s %s", tbuf, entry->d_name);
            if (S_ISDIR(st.st_mode)) printf("/");
            printf("\n");
        } else {
            printf("?????????? %s\n", entry->d_name);
        }
    }
    closedir(d);
}

/* ---------- Feature: show file info ---------- */
void show_info(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        perror("stat");
        return;
    }
    printf("Path: %s\n", path);
    printf("Size: %lld bytes\n", (long long)st.st_size);
    printf("Type: %s\n", S_ISDIR(st.st_mode) ? "directory" : "file");
    printf("Permissions: ");
    print_perms(st.st_mode);
    printf("\n");
    char tbuf[64];
    struct tm *tm = localtime(&st.st_mtime);
    strftime(tbuf, sizeof(tbuf), "%c", tm);
    printf("Modified: %s\n", tbuf);
}

/* ---------- Feature: change directory ---------- */
void do_cd(const char *path) {
    if (chdir(path) != 0) {
        perror("chdir");
        return;
    }
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) printf("Now in: %s\n", cwd);
}

/* ---------- Feature: copy file ---------- */
int copy_file(const char *src, const char *dst) {
    int infd = open(src, O_RDONLY);
    if (infd < 0) { perror("open src"); return 1; }
    int outfd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (outfd < 0) { perror("open dst"); close(infd); return 1; }
    char buf[8192];
    ssize_t n;
    while ((n = read(infd, buf, sizeof(buf))) > 0) {
        ssize_t w = write(outfd, buf, n);
        if (w != n) { perror("write"); close(infd); close(outfd); return 1; }
    }
    if (n < 0) perror("read");
    close(infd); close(outfd);
    return 0;
}

/* ---------- Feature: move (rename) ---------- */
int move_file(const char *src, const char *dst) {
    if (rename(src, dst) == 0) return 0;
    // fallback: copy then remove
    if (copy_file(src, dst) == 0) {
        if (remove(src) == 0) return 0;
    }
    return 1;
}

/* ---------- Feature: delete file ---------- */
int delete_path(const char *path) {
    if (is_dir(path)) {
        fprintf(stderr, "delete: directory deletion not implemented (use system rm -r or implement recursion)\n");
        return 1;
    } else {
        if (remove(path) != 0) { perror("remove"); return 1; }
    }
    return 0;
}

/* ---------- Feature: create file ---------- */
int create_file(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) { perror("fopen"); return 1; }
    fprintf(f, ""); // empty file
    fclose(f);
    return 0;
}

/* ---------- Feature: recursive search ---------- */
void search_recursive(const char *root, const char *pattern) {
    DIR *d = opendir(root);
    if (!d) return;
    struct dirent *e;
    char full[PATH_MAX];
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        snprintf(full, sizeof(full), "%s/%s", root, e->d_name);
        if (strstr(e->d_name, pattern) != NULL) {
            printf("FOUND: %s\n", full);
        }
        struct stat st;
        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
            search_recursive(full, pattern);
        }
    }
    closedir(d);
}

/* ---------- Feature: chmod (permissions) ---------- */
void change_perms(const char *path, mode_t mode) {
    if (chmod(path, mode) != 0) perror("chmod");
    else printf("Permissions updated.\n");
}

void print_help() {
    printf("\nCommands:\n");
    printf(" ls [path]             - list directory (default: .)\n");
    printf(" cd <path>             - change directory\n");
    printf(" info <path>           - show file information\n");
    printf(" copy <src> <dst>      - copy file\n");
    printf(" move <src> <dst>      - move/rename file\n");
    printf(" delete <path>         - delete file (no recursive dir delete)\n");
    printf(" create <path>         - create empty file\n");
    printf(" search <pattern> [p]  - search names (recursive) in path (default .)\n");
    printf(" chmod <oct> <path>    - change permissions (e.g., chmod 644 file)\n");
    printf(" pwd                   - print current directory\n");
    printf(" help                  - this help\n");
    printf(" exit                  - quit\n");
}

/* ---------- Main loop ---------- */
int main(int argc, char **argv) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) {
        // ok
    }
    print_header("Simple File Explorer (Assignment 1)");
    print_help();

    char line[1024];
    while (1) {
        if (getcwd(cwd, sizeof(cwd))) printf("\n%s> ", cwd);
        else printf("\n?> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        // remove newline
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 0) continue;
        // tokenize
        char *args[16]; int ac = 0;
        char *tok = strtok(line, " ");
        while (tok && ac < 15) { args[ac++] = tok; tok = strtok(NULL, " "); }
        args[ac] = NULL;
        if (ac == 0) continue;
        if (strcmp(args[0], "ls") == 0) {
            const char *p = (ac > 1) ? args[1] : ".";
            list_dir(p);
        } else if (strcmp(args[0], "cd") == 0) {
            if (ac < 2) printf("cd: missing argument\n");
            else do_cd(args[1]);
        } else if (strcmp(args[0], "info") == 0) {
            if (ac < 2) printf("info: missing\n"); else show_info(args[1]);
        } else if (strcmp(args[0], "copy") == 0) {
            if (ac < 3) printf("copy: need src dst\n");
            else { if (copy_file(args[1], args[2]) == 0) printf("Copied.\n"); else printf("Copy failed.\n"); }
        } else if (strcmp(args[0], "move") == 0) {
            if (ac < 3) printf("move: need src dst\n");
            else { if (move_file(args[1], args[2]) == 0) printf("Moved.\n"); else printf("Move failed.\n"); }
        } else if (strcmp(args[0], "delete") == 0) {
            if (ac < 2) printf("delete: missing\n"); else { if (delete_path(args[1]) == 0) printf("Deleted.\n"); else printf("Delete failed.\n"); }
        } else if (strcmp(args[0], "create") == 0) {
            if (ac < 2) printf("create: missing\n"); else { if (create_file(args[1]) == 0) printf("Created.\n"); else printf("Create failed.\n"); }
        } else if (strcmp(args[0], "search") == 0) {
            if (ac < 2) printf("search: missing pattern\n");
            else {
                const char *path = (ac > 2) ? args[2] : ".";
                search_recursive(path, args[1]);
            }
        } else if (strcmp(args[0], "chmod") == 0) {
            if (ac < 3) printf("chmod: usage chmod 644 file\n");
            else {
                int mode = (int)strtol(args[1], NULL, 8);
                change_perms(args[2], (mode_t)mode);
            }
        } else if (strcmp(args[0], "pwd") == 0) {
            if (getcwd(cwd, sizeof(cwd))) printf("%s\n", cwd);
        } else if (strcmp(args[0], "help") == 0) {
            print_help();
        } else if (strcmp(args[0], "exit") == 0) {
            break;
        } else {
            printf("Unknown command: %s\n", args[0]);
        }
    }

    printf("Goodbye!\n");
    return 0;
}
