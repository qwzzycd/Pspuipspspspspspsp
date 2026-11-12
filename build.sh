#!/bin/sh
# Build inside the official pspdev Docker image. Extra args go to make, e.g.:  ./build.sh NO_CFW=1
docker run --rm -v "$PWD":/src -w /src pspdev/pspdev:latest make "$@"
