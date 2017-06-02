FROM fedora:25
LABEL maintainer "martin@martinpaljak.net"
RUN dnf -y update && dnf clean all
RUN dnf -y install git qt5-qtbase-devel qt5-linguist qt5-qtwebsockets-devel qt5-qtsvg-devel pcsc-lite-devel fedpkg rpm-build gcc-c++ && dnf clean all
ENV QMAKE qmake-qt5
ENV LRELEASE lrelease-qt5
VOLUME /artifacts
RUN adduser --home /build build
USER build
WORKDIR /build
RUN mkdir /build/.ssh && chmod 700 /build/.ssh
CMD cp /artifacts/id_rsa* /build/.ssh && cp /artifacts/known_hosts /build/.ssh && git clone ${GIT_REPOSITORY} web-eid && git -C web-eid checkout -qf ${TRAVIS_COMMIT:-master} && make -C web-eid pkg && cp web-eid/x86_64/*.rpm /artifacts
