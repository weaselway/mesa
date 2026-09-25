{
  description = "mesa (weaselway d3d12/dzn) dev environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
  };

  outputs =
    { self, nixpkgs }:
    let
      forAllSystems =
        f:
        nixpkgs.lib.genAttrs [ "x86_64-linux" "aarch64-linux" ] (
          system: f nixpkgs.legacyPackages.${system}
        );
    in
    {
      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          # Full build closure of nixpkgs' own mesa: llvm, spirv, wayland, x11,
          # DirectX-Headers, python mako/yaml, ...
          inputsFrom = [ pkgs.mesa ];

          packages = with pkgs; [
            libglvnd

            # meson looks for the newest pythonX.Y on PATH before plain python3,
            # so a python3.14 from the user profile would otherwise beat the
            # shell's python3.13 (and its mako/yaml). Provide our own newest.
            (python314.withPackages (ps: with ps; [ mako packaging ply pycparser pyyaml ]))

            meson
            ninja
            pkg-config
            gdb
            git
          ];

          # The meson flags live in ./weaselway-build.sh, see WEASELWAY.md.
          shellHook = ''
            echo "build with: ./weaselway-build.sh"
          '';
        };
      });
    };
}
