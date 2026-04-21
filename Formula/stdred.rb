class Stdred < Formula
  desc "Wrap interactive zsh commands and colorize stderr via a PTY helper"
  homepage "https://github.com/devnoname120/zsh-stdred"
  license "MIT"
  head "https://github.com/devnoname120/zsh-stdred.git", branch: "main"

  depends_on "cmake" => :build
  depends_on "zsh"

  def caveats
    <<~EOS
      To enable stdred for interactive zsh commands, add this to your `.zshrc`:
        source "$(brew --prefix stdred)/share/stdred/stdred.plugin.zsh"
    EOS
  end

  def install
    system "cmake", "-S", "src", "-B", "build", *std_cmake_args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end

  test do
    assert_path_exists share/"stdred/stdred.plugin.zsh"

    output = shell_output(%Q(#{bin}/stdred --line 'print -n -- test >&2' 2>&1))
    assert_match "test", output
  end
end
