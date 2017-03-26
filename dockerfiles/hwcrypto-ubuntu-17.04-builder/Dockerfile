FROM hwcrypto-ubuntu-17.04-base
MAINTAINER Martin Paljak
ENV QT_SELECT 5
VOLUME /artifacts
RUN adduser --disabled-password --gecos "" --home /build build
USER build
WORKDIR /build
CMD git clone https://github.com/hwcrypto/hwcrypto-native && make -C hwcrypto-native && cp hwcrypto-native_*.deb /artifacts
