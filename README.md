# stdred

stderr in red, for interactive zsh.

## About

`stdred` works as a zsh plugin plus a PTY-backed helper instead of a
preload library. The plugin rewrites most interactive commands to run through
`stdred`, and the helper gives the child process a real TTY on `stderr`
so tools still see `isatty(2) == 1`.

This is a near-parity replacement for interactive zsh sessions, not a drop-in
replacement for every process launch path. Commands started outside that zsh
session are not affected.

## Installation

Build the helper:

```sh
make
```

Enable it in zsh:

```sh
source /absolute/path/to/stdred/stdred.plugin.zsh
```

The plugin expects `stdred` to be on your `PATH`. If you install with
`make install`, the helper is installed to `$(PREFIX)/bin` and the plugin to
`$(PREFIX)/share/stdred/stdred.plugin.zsh`.

### Homebrew

```sh
brew install --HEAD devnoname120/zsh-stdred/stdred
```

Then add this to your `.zshrc`:

```sh
source "$(brew --prefix stdred)/share/stdred/stdred.plugin.zsh"
```

## Usage

Once sourced, pressing Enter in interactive zsh rewrites most commands to:

```sh
stdred --line '<your original command>'
```

You can also call the helper directly:

```sh
stdred --line 'python3 -c "import os; os.write(2, b\"oops\\n\")"'
```

## Configuration

Use `zstyle` in your `.zshrc`:

```sh
zstyle ':stdred:*' skip-first-words cd pushd popd export unset alias unalias source . typeset local readonly setopt unsetopt jobs fg bg wait exec
```

### Escape hatch

To skip wrapping for one command, prefix it with:

```sh
STDRED_PASSTHROUGH=1 your-command
```

## Limitations

- This only affects commands launched from interactive zsh after the plugin is sourced.
- Parent-shell stateful commands such as `cd` and `export` are intentionally skipped.
- Aggressive wrapping runs most commands in a nested `zsh -ic` session, so shell semantics can differ for complex current-shell workflows.
- Output is colorized per PTY read chunk, not per original libc write boundary.

## License

You are free to use this program under the terms of the license found in
LICENSE file.
