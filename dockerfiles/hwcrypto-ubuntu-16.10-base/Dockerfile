FROM ubuntu:16.10
ENV DEBIAN_FRONTEND noninteractive
RUN apt-get -y update && apt-get install -y \
  git \
  make \
  qt5-qmake \
  qtbase5-dev \
  qttools5-dev-tools \
  libpcsclite-dev \
  debhelper \
  devscripts \
  pkg-config \
  xvfb \
 && rm -rf /var/lib/apt/lists/*
