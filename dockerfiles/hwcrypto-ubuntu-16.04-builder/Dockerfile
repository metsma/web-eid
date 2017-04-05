FROM hwcrypto-ubuntu-16.04-base:latest
LABEL maintainer "martin@martinpaljak.net"
ENV QT_SELECT 5
VOLUME /artifacts
RUN adduser --disabled-password --gecos "" --home /build build
USER build
WORKDIR /build
CMD git clone https://github.com/hwcrypto/hwcrypto-native && git -C hwcrypto-native checkout -qf ${TRAVIS_COMMIT:-master} && make -C hwcrypto-native && cp hwcrypto-native_*.deb /artifacts
