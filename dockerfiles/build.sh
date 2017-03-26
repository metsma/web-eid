#!/bin/bash
VERSION=17.3.26
for f in *-builder
do
    docker run -v ${PWD}:/artifacts martinpaljak/$f:$VERSION
done
