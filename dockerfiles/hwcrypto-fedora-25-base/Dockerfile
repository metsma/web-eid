FROM fedora:25
LABEL maintainer "martin@martinpaljak.net"
RUN dnf -y update && dnf clean all
RUN dnf -y install git qt5-qtbase-devel qt5-linguist pcsc-lite-devel fedpkg gcc-c++ && dnf clean all
