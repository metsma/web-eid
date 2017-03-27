FROM debian:stretch
LABEL maintainer "martin@martinpaljak.net"
ENV DEBIAN_FRONTEND noninteractive
RUN apt-get -y update && apt-get install --no-install-recommends -y \
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
  ca-certificates \
  g++ \
  fakeroot \
  lsb-release \
  xauth \
  python \
 && rm -rf /var/lib/apt/lists/*
