{
  description = "fcvm (a virtual machine) development shell";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
  };

  outputs =
    { nixpkgs, ... }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];

      forAllSystems =
        f:
        nixpkgs.lib.genAttrs systems (
          system: f system (import nixpkgs { inherit system; })
        );
    in
    {
      devShells = forAllSystems (
        system:
        pkgs:
        let
          # Optional per-developer additions; see flake.local.example.nix.
          localDevShell =
            if builtins.pathExists ./flake.local.nix then
              import ./flake.local.nix {
                inherit pkgs system;
              }
            else
              { };
          localPackages = localDevShell.packages or [ ];
          localShellHook = localDevShell.shellHook or "";
        in
        {
          default = pkgs.mkShell {
            packages = [
              # Build system
              pkgs.cmake
              pkgs.ninja
              pkgs.pkg-config

              # Libs
              pkgs.boost
              pkgs.abseil-cpp

              # Assembler (tools/asm-rust)
              pkgs.cargo
              pkgs.rustc

              # Docs
              pkgs.doxygen
              pkgs.asciidoctor

              pkgs.git
              pkgs.clang-tools
            ] ++ localPackages;

            shellHook = ''
              export FCVM_ROOT="$PWD"

              # CMakeLists.txt expects the assembler at
              # tools/installed/bin/asm-rust, so build it on first entry.
              # Set FCVM_BUILD_ASSEMBLER=0 to skip (e.g. when offline).
              if [ "''${FCVM_BUILD_ASSEMBLER:-1}" = 1 ] \
                 && [ -f "$FCVM_ROOT/tools/asm-rust/Cargo.toml" ] \
                 && [ ! -x "$FCVM_ROOT/tools/installed/bin/asm-rust" ]; then
                echo "fcvm: building the assembler (tools/asm-rust)..." >&2
                if ! cargo install \
                       --path "$FCVM_ROOT/tools/asm-rust" \
                       --root "$FCVM_ROOT/tools/installed"; then
                  echo "fcvm: WARNING: could not build tools/asm-rust;" >&2
                  echo "      the hello_world_bin target will fail." >&2
                  echo "      Retry: cargo install --path tools/asm-rust --root tools/installed" >&2
                fi
              fi

              echo "fcvm dev shell"
              echo "  configure: cmake -S . -B build -G Ninja"
              echo "  build:     cmake --build build"
              echo "  test:      ctest --test-dir build --output-on-failure"
              ${localShellHook}
            '';
          };
        }
      );
    };
}
