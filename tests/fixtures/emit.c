#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int usage(const char *argv0) {
  fprintf(stderr, "usage: %s <mode>\n", argv0);
  return 2;
}

int main(int argc, char **argv) {
  if (argc != 2) return usage(argv[0]);

  if (!strcmp(argv[1], "write")) {
    puts("stdout-write");
    fflush(stdout);
    write(STDERR_FILENO, "stderr-write\n", 13);
    return 0;
  }

  if (!strcmp(argv[1], "fprintf")) {
    puts("stdout-fprintf");
    fflush(stdout);
    fprintf(stderr, "stderr-fprintf\n");
    return 0;
  }

  if (!strcmp(argv[1], "perror")) {
    puts("stdout-perror");
    fflush(stdout);
    errno = ENOENT;
    perror("stderr-perror");
    return 0;
  }

  if (!strcmp(argv[1], "isatty")) {
    printf("%d\n", isatty(STDERR_FILENO));
    fflush(stdout);
    return 0;
  }

  return usage(argv[0]);
}
