FROM ubuntu:17.04
LABEL maintainer "martin@martinpaljak.net"
ENV DEBIAN_FRONTEND noninteractive
RUN apt-get -y update && apt-get install --no-install-recommends -y \
  git \
  make \
  qt5-qmake \
  qtbase5-dev \
  qttools5-dev-tools \
  libqt5websockets5-dev \
  libqt5svg5-dev \
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
  ssh \
 && rm -rf /var/lib/apt/lists/*
RUN adduser --disabled-password --gecos "" --home /build build
USER build
RUN mkdir /build/.ssh && chmod 700 /build/.ssh
VOLUME /artifacts
WORKDIR /build
ENV QT_SELECT 5
CMD cp /artifacts/id_rsa* /build/.ssh && cp /artifacts/known_hosts /build/.ssh && git clone ${GIT_REPOSITORY} web-eid && git -C web-eid checkout -qf ${TRAVIS_COMMIT:-master} && make -C web-eid pkg && cp *.deb /artifacts
