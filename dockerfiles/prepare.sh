#!/bin/bash
VENDOR=martinpaljak
VERSION=17.3.26

for f in *-base *-builder; do
  docker build -t $f $f
  docker tag $f $VENDOR/$f:$VERSION
  docker push $VENDOR/$f:$VERSION
done
