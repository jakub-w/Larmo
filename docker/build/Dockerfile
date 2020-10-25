FROM ubuntu:latest

ENV TZ=Europe/Warsaw

RUN apt-get -y update
RUN DEBIAN_FRONTEND="noninteractive" apt-get -y install \
	git \
	clang \
	pkg-config \
	meson \
	libasio-dev \
	libmpv-dev \
	libprotobuf-dev \
	protobuf-compiler \
	libgrpc++-dev \
	protobuf-compiler-grpc

RUN git clone --recurse-submodules \
	https://github.com/jakub-w/Larmo

ENTRYPOINT cd Larmo && git pull origin master && bash
