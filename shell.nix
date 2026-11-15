{ pkgs ? import <nixpkgs> {
    config = {
      allowUnfree = true;
      android_sdk.accept_license = true;
    };
  }
}:
let
  androidComposition = pkgs.androidenv.composeAndroidPackages {
    platformVersions = [ "35" ];
    buildToolsVersions = [ "35.0.0" ];
    includeNDK = true;
  };
  sdkRoot = "${androidComposition.androidsdk}/libexec/android-sdk";
in
pkgs.mkShell {
  packages = [ pkgs.jdk17 pkgs.cmake pkgs.ninja pkgs.android-tools ];

  JAVA_HOME = "${pkgs.jdk17}/lib/openjdk";
  ANDROID_HOME = sdkRoot;
  ANDROID_SDK_ROOT = sdkRoot;

  # AGP's downloaded aapt2 is a generic Linux binary and won't run on NixOS
  GRADLE_OPTS = "-Dorg.gradle.project.android.aapt2FromMavenOverride=${sdkRoot}/build-tools/35.0.0/aapt2";

  shellHook = ''
    # NDK from the Nix SDK (folder name differs between nixpkgs versions)
    export ANDROID_NDK_ROOT=$(ls -d ${sdkRoot}/ndk/* ${sdkRoot}/ndk-bundle 2>/dev/null | head -n1)
    # use Nix's cmake instead of the one AGP would download
    printf 'sdk.dir=%s\ncmake.dir=%s\n' "${sdkRoot}" "${pkgs.cmake}" > local.properties
    echo "NDK: $ANDROID_NDK_ROOT"
  '';
}
