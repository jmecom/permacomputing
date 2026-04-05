{
  description = "Ember bootstrap development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs =
    { nixpkgs, ... }:
    let
      lib = nixpkgs.lib;
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];
      forAllSystems =
        f:
        lib.genAttrs systems (
          system:
          let
            pkgs = import nixpkgs { inherit system; };
          in
          f pkgs
        );
    in
    {
      formatter = forAllSystems (pkgs: pkgs.nixfmt);

      packages = forAllSystems (
        pkgs:
        let
          armToolchain = pkgs.pkgsCross.arm-embedded.buildPackages;
          pythonEnv = pkgs.python3.withPackages (ps: [ ps.reportlab ]);
        in
        rec {
          cortex-m-paper-seed = pkgs.stdenvNoCC.mkDerivation {
            pname = "cortex-m-paper-seed";
            version = "0.1.0";
            src = ./.;

            nativeBuildInputs = [
              armToolchain.binutils
              armToolchain.gcc
              pkgs.gnumake
              pythonEnv
            ];

            buildPhase = ''
              runHook preBuild
              make OUT_DIR=$PWD/dist/cortex-m paper
              runHook postBuild
            '';

            installPhase = ''
              runHook preInstall
              mkdir -p $out
              cp -R dist/cortex-m/. $out/
              runHook postInstall
            '';
          };

          default = cortex-m-paper-seed;
        }
      );

      devShells = forAllSystems (
        pkgs:
        let
          armToolchain = pkgs.pkgsCross.arm-embedded.buildPackages;
          aarch64Toolchain = pkgs.pkgsCross.aarch64-embedded.buildPackages;
          avrToolchain = pkgs.pkgsCross.avr.buildPackages;
          mipsToolchain = pkgs.pkgsCross.mips-embedded.buildPackages;
          pythonEnv = pkgs.python3.withPackages (ps: [ ps.reportlab ]);
          riscv32Toolchain = pkgs.pkgsCross.riscv32-embedded.buildPackages;
          riscv64Toolchain = pkgs.pkgsCross.riscv64-embedded.buildPackages;
          makeQemuLauncher =
            name: target:
            pkgs.writeShellScriptBin name ''
              exec qemu-system-${target} "$@"
            '';
          qemuHelper = pkgs.writeShellScriptBin "qemu" ''
                        cat <<'EOF' >&2
            No plain 'qemu' launcher is installed here.

                        Use an architecture-specific system emulator instead:
                          qemu-system-i386
                          qemu-system-x86_64
                          qemu-system-arm
                          qemu-system-aarch64
                          qemu-system-avr
                          qemu-system-riscv32
                          qemu-system-riscv64
                          qemu-system-mips
                          qemu-system-mipsel

                        Environment shortcuts exported by the shell:
                          $QEMU_SYSTEM_I386
                          $QEMU_SYSTEM_X86_64
                          $QEMU_SYSTEM_ARM
                          $QEMU_SYSTEM_AARCH64
                          $QEMU_SYSTEM_AVR
                          $QEMU_SYSTEM_RISCV32
                          $QEMU_SYSTEM_RISCV64
                          $QEMU_SYSTEM_MIPS
                          $QEMU_SYSTEM_MIPSEL
            EOF
                        exit 64
          '';
          emberEnv = pkgs.writeShellScriptBin "ember-env" ''
                                    cat <<'EOF'
                        Ember development environment

                        Toolchains:
                          Z80     : sjasmplus
                          6502    : ca65, ld65, cc65, dasm, acme
                          8086    : nasm, clang, ld.lld
                          x86_64  : clang, ld.lld, nasm, gdb
                          ARMv7-M : arm-none-eabi-{gcc,as,ld,objcopy,objdump}
                          AArch64 : aarch64-none-elf-{gcc,as,ld,objcopy,objdump}
                          AVR     : avr-{gcc,as,ld,objcopy,objdump}, avrdude
                          RISC-V  : riscv32-none-elf-{gcc,as,ld,objcopy,objdump}
                                    riscv64-none-elf-{gcc,as,ld,objcopy,objdump}
                          MIPS    : use 'nix develop .#full' for mips-none-elf-{gcc,as,ld,objcopy,objdump}

                        QEMU launchers:
                          qemu-i386
                          qemu-x86_64
                          qemu-arm
                          qemu-aarch64
                          qemu-avr
                          qemu-riscv32
                          qemu-riscv64
                          qemu-mips
                          qemu-mipsel

            Environment variables:
              HOST_CC
              HOST_CXX
              HOST_LD
              Z80_AS
              MOS6502_AS
              MOS6502_LD
              I8086_AS
              X86_64_AS
              CROSS_COMPILE
              ARM_CROSS_COMPILE
              AARCH64_CROSS_COMPILE
              AVR_CROSS_COMPILE
              RISCV32_CROSS_COMPILE
                          RISCV64_CROSS_COMPILE
                        EOF
          '';
          qemuLaunchers = [
            (makeQemuLauncher "qemu-i386" "i386")
            (makeQemuLauncher "qemu-x86_64" "x86_64")
            (makeQemuLauncher "qemu-arm" "arm")
            (makeQemuLauncher "qemu-aarch64" "aarch64")
            (makeQemuLauncher "qemu-avr" "avr")
            (makeQemuLauncher "qemu-riscv32" "riscv32")
            (makeQemuLauncher "qemu-riscv64" "riscv64")
            (makeQemuLauncher "qemu-mips" "mips")
            (makeQemuLauncher "qemu-mipsel" "mipsel")
          ];
          commonPackages =
            (with pkgs; [
              acme
              avrdude
              cc65
              clang
              dasm
              emberEnv
              gdb
              git
              gnumake
              lld
              nasm
              openocd
              picocom
              pythonEnv
              qemu
              qemuHelper
              sjasmplus
              srecord
            ])
            ++ qemuLaunchers
            ++ [
              aarch64Toolchain.binutils
              aarch64Toolchain.gcc
              armToolchain.binutils
              armToolchain.gcc
              avrToolchain.binutils
              avrToolchain.gcc
              riscv32Toolchain.binutils
              riscv32Toolchain.gcc
              riscv64Toolchain.binutils
              riscv64Toolchain.gcc
            ];
          commonShellHook = ''
            export CROSS_COMPILE=arm-none-eabi-
            export AARCH64_CROSS_COMPILE=aarch64-none-elf-
            export ARM_CROSS_COMPILE=arm-none-eabi-
            export AVR_CROSS_COMPILE=avr-
            export HOST_CC=${pkgs.clang}/bin/clang
            export HOST_CXX=${pkgs.clang}/bin/clang++
            export HOST_LD=${pkgs.lld}/bin/ld.lld
            export I8086_AS=${pkgs.nasm}/bin/nasm
            export MOS6502_AS=${pkgs.cc65}/bin/ca65
            export MOS6502_LD=${pkgs.cc65}/bin/ld65
            export RISCV32_CROSS_COMPILE=riscv32-none-elf-
            export RISCV64_CROSS_COMPILE=riscv64-none-elf-
            export X86_64_AS=${pkgs.nasm}/bin/nasm
            export Z80_AS=${pkgs.sjasmplus}/bin/sjasmplus

            export CC=''${CROSS_COMPILE}gcc
            export AS=''${CROSS_COMPILE}as
            export LD=''${CROSS_COMPILE}ld
            export OBJCOPY=''${CROSS_COMPILE}objcopy
            export OBJDUMP=''${CROSS_COMPILE}objdump
            export GDB=gdb
            export QEMU_SYSTEM_AARCH64=qemu-system-aarch64
            export QEMU_SYSTEM_ARM=qemu-system-arm
            export QEMU_SYSTEM_AVR=qemu-system-avr
            export QEMU_SYSTEM_I386=qemu-system-i386
            export QEMU_SYSTEM_MIPS=qemu-system-mips
            export QEMU_SYSTEM_MIPSEL=qemu-system-mipsel
            export QEMU_SYSTEM_RISCV32=qemu-system-riscv32
            export QEMU_SYSTEM_RISCV64=qemu-system-riscv64
            export QEMU_SYSTEM_X86_64=qemu-system-x86_64

            cat <<'EOF'
            ember dev shell
              targets: Z80, 6502, 8086, x86_64, ARMv7-M, AArch64, AVR, RISC-V, optional MIPS
              emu    : qemu-{i386,x86_64,arm,aarch64,avr,riscv32,riscv64,mips,mipsel}
              asm    : sjasmplus, ca65, dasm, acme, nasm
              cc/ld  : clang/lld, {arm,aarch64,avr,riscv32,riscv64}-gcc/binutils
              misc   : gdb, openocd, avrdude, picocom, srec_cat, ember-env
            EOF
          '';
        in
        {
          default = pkgs.mkShell {
            packages = commonPackages;
            shellHook = commonShellHook;
          };

          full = pkgs.mkShell {
            packages = commonPackages ++ [
              mipsToolchain.binutils
              mipsToolchain.gcc
            ];

            shellHook = ''
              ${commonShellHook}
              export MIPS_CROSS_COMPILE=mips-none-elf-
            '';
          };
        }
      );
    };
}
