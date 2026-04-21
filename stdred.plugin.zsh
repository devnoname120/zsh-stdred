# Source this file from interactive zsh to wrap commands with stdred.

typeset -ga STDRED_DEFAULT_SKIP_WORDS=(
  cd pushd popd export unset alias unalias source .
  typeset local readonly setopt unsetopt jobs fg bg wait exec
)

stdred::skip_words() {
  emulate -L zsh
  local -a words
  if zstyle -a ':stdred:*' skip-first-words words; then
    print -rl -- "${words[@]}"
  else
    print -rl -- "${STDRED_DEFAULT_SKIP_WORDS[@]}"
  fi
}

stdred::is_assignment_word() {
  emulate -L zsh
  [[ "$1" =~ '^[A-Za-z_][A-Za-z0-9_]*=.*$' ]]
}

stdred::command_word() {
  emulate -L zsh
  local -a words
  words=(${(z)1})

  local word
  for word in "${words[@]}"; do
    if stdred::is_assignment_word "$word"; then
      continue
    fi

    print -r -- "$word"
    return 0
  done

  return 1
}

stdred::passthrough_requested() {
  emulate -L zsh
  local -a words
  words=(${(z)1})

  local word
  for word in "${words[@]}"; do
    if [[ "$word" == STDRED_PASSTHROUGH=1 ]]; then
      return 0
    fi

    stdred::is_assignment_word "$word" || break
  done

  return 1
}

stdred::should_wrap_buffer() {
  emulate -L zsh

  [[ -n "${BUFFER//[[:space:]]/}" ]] || return 1
  [[ "${STDRED_ACTIVE:-0}" != 1 ]] || return 1
  stdred::passthrough_requested "$BUFFER" && return 1

  local command_word
  command_word=$(stdred::command_word "$BUFFER") || return 1

  local skip_word
  for skip_word in ${(f)"$(stdred::skip_words)"}; do
    [[ "$skip_word" == "$command_word" ]] && return 1
  done

  return 0
}

stdred::maybe-wrap-buffer() {
  emulate -L zsh

  stdred::should_wrap_buffer || return 0
  BUFFER="stdred --line ${(qqq)BUFFER}"
}

stdred::accept-line() {
  emulate -L zsh
  stdred::maybe-wrap-buffer
  zle .accept-line
}

if [[ -o interactive ]]; then
  zle -N accept-line stdred::accept-line
fi
