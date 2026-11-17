#!/bin/sh
# Packs build/botbase into dist/botbase-<version>-<target>.tar.gz.
# The version is the tag being built, or the short commit otherwise.
set -eu

target=$1
version=${GITHUB_REF_NAME:-}
case $version in
    v*) ;;
    *) version=$(git rev-parse --short HEAD) ;;
esac

name=botbase-$version-$target
mkdir -p "dist/$name"
cp build/botbase README.md "dist/$name/"
tar -czf "dist/$name.tar.gz" -C dist "$name"
rm -rf "dist/$name"
echo "dist/$name.tar.gz"
