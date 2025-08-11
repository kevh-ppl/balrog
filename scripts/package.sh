#!/bin/bash
set -e

BUILD_DIR="../balrog_build"
mkdir -p "$BUILD_DIR"

dpkg-buildpackage -us -uc

mv ../*.deb ../*.tar.* ../*.dsc ../*.buildinfo ../*.changes "$BUILD_DIR/"

