#!/usr/bin/env python3
import os
import shlex
import signal
import subprocess
import sys
import tempfile
import time


RED = "\x1b[31m"
RESET = "\x1b[0m"


def run(helper, line, *, input_text=None):
    cmd = [helper, "--line", line]
    return subprocess.run(
        cmd,
        input=input_text,
        text=True,
        capture_output=True,
        check=False,
    )


def assert_equal(actual, expected, label):
    if actual != expected:
        raise AssertionError(f"{label}: expected {expected!r}, got {actual!r}")


def assert_contains(haystack, needle, label):
    if needle not in haystack:
        raise AssertionError(f"{label}: missing {needle!r} in {haystack!r}")


def test_write(helper, emit):
    result = run(helper, f"{shlex.quote(emit)} write")
    assert_equal(result.returncode, 0, "write exit")
    assert_equal(result.stdout, "stdout-write\n", "write stdout")
    assert_equal(result.stderr, f"{RED}stderr-write\n{RESET}", "write stderr")


def test_fprintf(helper, emit):
    result = run(helper, f"{shlex.quote(emit)} fprintf")
    assert_equal(result.returncode, 0, "fprintf exit")
    assert_equal(result.stdout, "stdout-fprintf\n", "fprintf stdout")
    assert_equal(result.stderr, f"{RED}stderr-fprintf\n{RESET}", "fprintf stderr")


def test_perror(helper, emit):
    result = run(helper, f"{shlex.quote(emit)} perror")
    assert_equal(result.returncode, 0, "perror exit")
    assert_equal(result.stdout, "stdout-perror\n", "perror stdout")
    assert_contains(result.stderr, "stderr-perror", "perror label")
    assert_contains(result.stderr, RED, "perror start color")
    assert_contains(result.stderr, RESET, "perror reset")


def test_isatty(helper, emit):
    result = run(helper, f"{shlex.quote(emit)} isatty")
    assert_equal(result.returncode, 0, "isatty exit")
    assert_equal(result.stdout, "1\n", "isatty stdout")
    assert_equal(result.stderr, "", "isatty stderr")


def test_stdin_passthrough(helper):
    line = "IFS= read -r line; print -r -- \"$line\" >&2"
    result = run(helper, line, input_text="from-stdin\n")
    assert_equal(result.returncode, 0, "stdin exit")
    assert_equal(result.stdout, "", "stdin stdout")
    assert_equal(result.stderr, f"{RED}from-stdin\n{RESET}", "stdin stderr")


def test_exit_status(helper):
    result = run(helper, "exit 17")
    assert_equal(result.returncode, 17, "exit status")


def test_stderr_redirect_to_file(helper):
    with tempfile.TemporaryDirectory() as tmpdir:
        errfile = os.path.join(tmpdir, "stderr.txt")
        result = run(
            helper,
            "python3 -c 'import sys; print(\"redirected\", file=sys.stderr)' "
            f"2>{shlex.quote(errfile)}",
        )
        assert_equal(result.returncode, 0, "redirect file exit")
        assert_equal(result.stdout, "", "redirect file stdout")
        assert_equal(result.stderr, "", "redirect file helper stderr")
        with open(errfile, "r", encoding="utf-8") as handle:
            contents = handle.read()
        assert_equal(contents, "redirected\n", "redirect file contents")


def test_stderr_merged_into_pipe(helper):
    result = run(
        helper,
        "python3 -c 'import sys; print(\"merged\", file=sys.stderr)' 2>&1 | grep merged",
    )
    assert_equal(result.returncode, 0, "merge pipe exit")
    assert_equal(result.stdout, "merged\n", "merge pipe stdout")
    assert_equal(result.stderr, "", "merge pipe stderr")


def test_pipe_ampersand_cat(helper):
    result = run(
        helper,
        "python3 -c 'import sys; print(\"ampersand\", file=sys.stderr)' |& cat",
    )
    assert_equal(result.returncode, 0, "pipe ampersand exit")
    assert_equal(result.stdout, "ampersand\n", "pipe ampersand stdout")
    assert_equal(result.stderr, "", "pipe ampersand stderr")


def test_forwarded_signal(helper, signal_name):
    with tempfile.TemporaryDirectory() as tmpdir:
        marker = os.path.join(tmpdir, signal_name.lower())
        line = (
            f"trap 'print -r -- {signal_name.lower()} > {shlex.quote(marker)}; exit 0' {signal_name}; "
            "while true; do sleep 1; done"
        )
        proc = subprocess.Popen(
            [helper, "--line", line],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        try:
            time.sleep(0.5)
            os.kill(proc.pid, getattr(signal, signal_name))
            stdout, stderr = proc.communicate(timeout=5)
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.communicate()

        assert_equal(stdout, "", f"{signal_name} stdout")
        assert_equal(stderr, "", f"{signal_name} stderr")
        assert_equal(proc.returncode, 0, f"{signal_name} exit")
        with open(marker, "r", encoding="utf-8") as handle:
            assert_equal(handle.read().strip(), signal_name.lower(), f"{signal_name} marker")


def main(argv):
    helper = argv[1]
    emit = argv[2]
    test_write(helper, emit)
    test_fprintf(helper, emit)
    test_perror(helper, emit)
    test_isatty(helper, emit)
    test_stdin_passthrough(helper)
    test_exit_status(helper)
    test_stderr_redirect_to_file(helper)
    test_stderr_merged_into_pipe(helper)
    test_pipe_ampersand_cat(helper)
    test_forwarded_signal(helper, "SIGTERM")
    test_forwarded_signal(helper, "SIGWINCH")


if __name__ == "__main__":
    try:
        main(sys.argv)
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        raise
