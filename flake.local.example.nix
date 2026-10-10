{
  pkgs,
  system ? null,
}:
{
  # Copy to flake.local.nix for local shell customizations.
  #
  # NOTE: git-ignored local files, such as flake.local.nix, are not visible to
  # `nix develop .`, because `.` is evaluated as a Git snapshot. Use
  # `nix develop path:.` when you want your customizations to be picked up.

  packages = [
    pkgs.zsh
  ];

  shellHook = ''
    if [ -t 0 ] && [ -z "''${IN_NIX_LOCAL_ZSH:-}" ]; then
      export IN_NIX_LOCAL_ZSH=1
      export SHELL=${pkgs.zsh}/bin/zsh
      exec ${pkgs.zsh}/bin/zsh
    fi
  '';
}
