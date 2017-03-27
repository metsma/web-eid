FROM hwcrypto-fedora-25-base:latest
LABEL maintainer "martin@martinpaljak.net"
ENV QMAKE qmake-qt5
VOLUME /artifacts
RUN adduser --home /build build
USER build
WORKDIR /build
CMD git clone https://github.com/hwcrypto/hwcrypto-native && make -C hwcrypto-native && cp hwcrypto-native/x86_64/*.rpm /artifacts
