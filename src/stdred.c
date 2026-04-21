#ifdef __APPLE__
  #define _DARWIN_C_SOURCE
#else
  #define _DEFAULT_SOURCE
#endif

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#ifdef __APPLE__
  #include <util.h>
#else
  #include <pty.h>
#endif

static const char *default_color = "\x1b[31m";
static const char *reset_color = "\x1b[0m";

static volatile sig_atomic_t pending_sigint = 0;
static volatile sig_atomic_t pending_sigterm = 0;
static volatile sig_atomic_t pending_sigquit = 0;
static volatile sig_atomic_t pending_sigwinch = 0;
static volatile sig_atomic_t child_pgid = -1;
static const char *shell = "zsh";

static void signal_handler(int sig) {
  switch (sig) {
    case SIGINT:
      pending_sigint = 1;
      break;
    case SIGTERM:
      pending_sigterm = 1;
      break;
    case SIGQUIT:
      pending_sigquit = 1;
      break;
    case SIGWINCH:
      pending_sigwinch = 1;
      break;
  }
}

static void usage(FILE *stream, const char *argv0) {
  fprintf(stream, "usage: %s --line <command>\n", argv0);
}

static int write_all(int fd, const char *buf, size_t len) {
  while (len > 0) {
    ssize_t written = write(fd, buf, len);
    if (written < 0) {
      if (errno == EINTR) continue;
      return -1;
    }

    buf += written;
    len -= (size_t)written;
  }

  return 0;
}

static void sync_winsize(int pty_fd) {
  struct winsize ws;

  if (ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) == -1) return;
  ioctl(pty_fd, TIOCSWINSZ, &ws);
}

static void forward_signal(int sig) {
  if (child_pgid > 0) kill(-child_pgid, sig);
}

static void flush_pending_signals(int pty_fd) {
  if (pending_sigwinch) {
    pending_sigwinch = 0;
    if (pty_fd >= 0) sync_winsize(pty_fd);
    forward_signal(SIGWINCH);
  }

  if (pending_sigint) {
    pending_sigint = 0;
    forward_signal(SIGINT);
  }

  if (pending_sigterm) {
    pending_sigterm = 0;
    forward_signal(SIGTERM);
  }

  if (pending_sigquit) {
    pending_sigquit = 0;
    forward_signal(SIGQUIT);
  }
}

static int install_handler(int sig) {
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = signal_handler;
  sigemptyset(&action.sa_mask);
  return sigaction(sig, &action, NULL);
}

static int set_foreground_pgrp(int tty_fd, pid_t pgid) {
  struct sigaction ignore_action;
  struct sigaction previous_action;

  memset(&ignore_action, 0, sizeof(ignore_action));
  ignore_action.sa_handler = SIG_IGN;
  sigemptyset(&ignore_action.sa_mask);

  if (sigaction(SIGTTOU, &ignore_action, &previous_action) == -1) return -1;

  int result = tcsetpgrp(tty_fd, pgid);
  int saved_errno = errno;

  sigaction(SIGTTOU, &previous_action, NULL);
  errno = saved_errno;
  return result;
}

static int child_exit_code(int status) {
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
  return 1;
}

static int prepare_zdotdir(char *template_dir, size_t size) {
  int written = snprintf(template_dir, size, "/tmp/stdred-zdotdir-XXXXXX");
  if (written < 0 || (size_t)written >= size) {
    errno = ENAMETOOLONG;
    return -1;
  }

  if (mkdtemp(template_dir) == NULL) return -1;

  char zshrc_path[PATH_MAX];
  written = snprintf(zshrc_path, sizeof(zshrc_path), "%s/.zshrc", template_dir);
  if (written < 0 || (size_t)written >= sizeof(zshrc_path)) {
    errno = ENAMETOOLONG;
    return -1;
  }

  FILE *zshrc = fopen(zshrc_path, "w");
  if (zshrc == NULL) return -1;
  fclose(zshrc);
  return 0;
}

int main(int argc, char **argv) {
  const char *line = NULL;

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--line")) {
      if (++i >= argc) {
        usage(stderr, argv[0]);
        return 2;
      }
      line = argv[i];
    } else if (!strcmp(argv[i], "--help")) {
      usage(stdout, argv[0]);
      return 0;
    } else {
      usage(stderr, argv[0]);
      return 2;
    }
  }

  if (line == NULL) {
    usage(stderr, argv[0]);
    return 2;
  }

  int master_fd = -1;
  int slave_fd = -1;
  char zdotdir_template[PATH_MAX] = "";
  int tty_fd = isatty(STDIN_FILENO) ? STDIN_FILENO : -1;
  pid_t original_tty_pgid = -1;
  if (openpty(&master_fd, &slave_fd, NULL, NULL, NULL) == -1) {
    perror("openpty");
    return 1;
  }

  if (tty_fd >= 0) {
    original_tty_pgid = tcgetpgrp(tty_fd);
    if (original_tty_pgid == -1 && errno == ENOTTY) tty_fd = -1;
  }

  struct termios raw;
  if (tcgetattr(slave_fd, &raw) == 0) {
    cfmakeraw(&raw);
    tcsetattr(slave_fd, TCSANOW, &raw);
  }

  sync_winsize(slave_fd);

  if (prepare_zdotdir(zdotdir_template, sizeof(zdotdir_template)) == -1) {
    perror("mkdtemp");
    return 1;
  }

  if (install_handler(SIGINT) == -1 ||
      install_handler(SIGTERM) == -1 ||
      install_handler(SIGQUIT) == -1 ||
      install_handler(SIGWINCH) == -1) {
    perror("sigaction");
    return 1;
  }

  pid_t child = fork();
  if (child == -1) {
    perror("fork");
    return 1;
  }

  if (child == 0) {
    setpgid(0, 0);

    if (dup2(slave_fd, STDERR_FILENO) == -1) {
      perror("dup2");
      _exit(127);
    }

    close(master_fd);
    if (slave_fd > STDERR_FILENO) close(slave_fd);

    setenv("STDRED_ACTIVE", "1", 1);
    unsetenv("LD_PRELOAD");
    unsetenv("DYLD_INSERT_LIBRARIES");
    setenv("ZDOTDIR", zdotdir_template, 1);
    execlp(shell, shell, "-ic", line, (char *)NULL);
    perror("execlp");
    _exit(127);
  }

  child_pgid = child;
  setpgid(child, child);

  if (tty_fd >= 0 && set_foreground_pgrp(tty_fd, child) == -1) {
    perror("tcsetpgrp");
    return 1;
  }

  close(slave_fd);

  char buffer[4096];
  for (;;) {
    flush_pending_signals(master_fd);

    ssize_t count = read(master_fd, buffer, sizeof(buffer));
    if (count > 0) {
      if (write_all(STDERR_FILENO, default_color, strlen(default_color)) == -1 ||
          write_all(STDERR_FILENO, buffer, (size_t)count) == -1 ||
          write_all(STDERR_FILENO, reset_color, strlen(reset_color)) == -1) {
        perror("write");
        break;
      }
      continue;
    }

    if (count == 0) break;
    if (errno == EINTR) continue;
    if (errno == EIO) break;

    perror("read");
    break;
  }

  close(master_fd);

  int status = 0;
  while (waitpid(child, &status, 0) == -1) {
    if (errno == EINTR) {
      flush_pending_signals(-1);
      continue;
    }
    perror("waitpid");
    return 1;
  }

  if (tty_fd >= 0 && original_tty_pgid > 0 && set_foreground_pgrp(tty_fd, original_tty_pgid) == -1) {
    perror("tcsetpgrp");
    return 1;
  }

  char zshrc_path[PATH_MAX];
  int written = snprintf(zshrc_path, sizeof(zshrc_path), "%s/.zshrc", zdotdir_template);
  if (written > 0 && (size_t)written < sizeof(zshrc_path)) unlink(zshrc_path);
  rmdir(zdotdir_template);

  return child_exit_code(status);
}
