{
  description = "Program to benchmark various hwmon access methods";

  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable"; 

  outputs = { self, nixpkgs }:
    {
      overlays.default = selfpkgs: pkgs: {
        hwmondump = pkgs.callPackage (
          { stdenv
          , cmake
          , pkg-config
          , lm_sensors
          , argparse
          , catch2_3
          , lib
          , tomlplusplus
          , libcpuid
          , libuuid
          , doCheck ? false
          }: stdenv.mkDerivation {
            name = "hwmondump";
            src = ./.;

            nativeBuildInputs = [
              cmake
              pkg-config
            ];

            inherit doCheck;
            cmakeFlags = [
              "-DFETCHCONTENT_FULLY_DISCONNECTED=ON"
              "-DFETCHCONTENT_SOURCE_DIR_TOMLPLUSPLUS=${tomlplusplus.src}"
            ] ++ (lib.optionals doCheck [
              "-DBUILD_TESTING=ON"
            ]);

            buildInputs = [
              lm_sensors
              argparse
              libcpuid
              libuuid
            ] ++ (lib.optionals doCheck [
              catch2_3
            ]);
          }) {
            argparse = pkgs.argparse.overrideAttrs {
              src = pkgs.fetchFromGitHub {
                owner = "teto519f";
                repo = "argparse";
                rev = "helppage_subcommands";
                sha256 = "sha256-Nw0fDaBSXbuNrWoo8D58SPrqFBanLqtcxcU4rPGChzc=";
              };
            };
          };
        hwmondump-with-tests = selfpkgs.hwmondump.override { doCheck = true; };
      };

      packages.x86_64-linux =
        let
          pkgs = import nixpkgs { system = "x86_64-linux"; };
          selfpkgs = self.packages.x86_64-linux;
        in self.overlays.default selfpkgs pkgs;

      defaultPackage.x86_64-linux = self.packages.x86_64-linux.hwmondump;
      devShells.x86_64-linux.default = self.packages.x86_64-linux.hwmondump-with-tests;
    };
}
