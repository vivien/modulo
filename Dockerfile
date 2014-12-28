FROM ubuntu:14.10
MAINTAINER Vivien Didelot <vivien@didelot.org>

RUN apt-get update
RUN apt-get dist-upgrade -y
RUN apt-get install -y build-essential

COPY *.c *.h Makefile /tmp/modulo/
RUN make -C /tmp/modulo clean install
