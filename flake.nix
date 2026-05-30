{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-25.11";
    my-nixos.url = "github:UsatiyNyan/flake.nix";
  };

  outputs = {
    nixpkgs,
    my-nixos,
    ...
  }: let
    system = "x86_64-linux";
    pkgs = nixpkgs.legacyPackages.${system};
  in {
    devShells.${system}.default = my-nixos.lib.mkComposedShell {
      inherit system;
      alias = "http";
      bases = ["nix" "cpp-clang"];
      extraBuildInputs = with pkgs; [
          just
          apacheHttpd
      ];
      extraShellHook = ''
      '';
    };
  };
}
