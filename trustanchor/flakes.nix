{
  description = "Reproducible Linux kernel build env";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/23.11"; # pin exact version
  };

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
    in {
      devShells.${system}.default = pkgs.mkShell {
        buildInputs = with pkgs; [
          gcc
          bc
          bison
          flex
          openssl
          elfutils
          pkg-config
          perl
          python3
          rsync
          cpio
          zstd
          pahole
          git
        ];

        # Make builds more reproducible
        shellHook = ''
          export KBUILD_BUILD_TIMESTAMP="2000-01-01"
          export KBUILD_BUILD_USER="nix"
          export KBUILD_BUILD_HOST="nix"
        '';
      };
    };
}
