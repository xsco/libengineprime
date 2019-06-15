with import <nixpkgs> {};

stdenv.mkDerivation {
  name = "libdjinterop";
  version = "unstable";
  src = builtins.fetchGit ./.;
  nativeBuildInputs = [
    meson
    ninja
    pkg-config
    clang-tools
  ];
  outputs = [ "out" "dev" ];
  buildInputs = [
    zlib
    boost
  ];
}
