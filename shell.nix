with import <nixpkgs> {};
let

in
#mkShell { # for GCC
llvmPackages.stdenv.mkDerivation { # for clang
    name = "wync-env";
    buildInputs = [
        clang-tools # to use clandg this must come first
        meson
        ninja
        gcovr # coverage tool
        libllvm # clang coverage
    ];
    shellHook = ''
        # git prompt
        source ${git}/share/git/contrib/completion/git-prompt.sh
        PS1='\[\033[0;33m\]nix:\w\[\033[0m\] $(__git_ps1 %s)\n$ '
    '';
}
