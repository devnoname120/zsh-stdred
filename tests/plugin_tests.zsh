#!/usr/bin/env zsh
set -euo pipefail

plugin_path=$1

source "$plugin_path"

assert_equal() {
  local actual=$1
  local expected=$2
  local label=$3

  if [[ "$actual" != "$expected" ]]; then
    print -u2 -- "$label: expected ${(qqq)expected}, got ${(qqq)actual}"
    return 1
  fi
}

assert_wrapped() {
  local input=$1
  local expected="stdred --line ${(qqq)input}"

  BUFFER=$input
  stdred::maybe-wrap-buffer
  assert_equal "$BUFFER" "$expected" "wrapped buffer"
}

zstyle ':stdred:*' skip-first-words cd pushd popd export unset alias unalias source . typeset local readonly setopt unsetopt jobs fg bg wait exec

unset STDRED_ACTIVE

assert_wrapped 'echo hello'

BUFFER='cd /tmp'
stdred::maybe-wrap-buffer
assert_equal "$BUFFER" 'cd /tmp' 'skip current-shell builtin'

BUFFER='FOO=1 cd /tmp'
stdred::maybe-wrap-buffer
assert_equal "$BUFFER" 'FOO=1 cd /tmp' 'skip builtin after assignment'

BUFFER='STDRED_PASSTHROUGH=1 echo hello'
stdred::maybe-wrap-buffer
assert_equal "$BUFFER" 'STDRED_PASSTHROUGH=1 echo hello' 'passthrough'

export STDRED_ACTIVE=1
BUFFER='echo recursive'
stdred::maybe-wrap-buffer
assert_equal "$BUFFER" 'echo recursive' 'recursion guard'
unset STDRED_ACTIVE

assert_wrapped 'echo green'

zstyle ':stdred:*' skip-first-words foo exec
BUFFER='foo bar'
stdred::maybe-wrap-buffer
assert_equal "$BUFFER" 'foo bar' 'custom skip list'
