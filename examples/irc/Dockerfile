FROM viviend/modulo
MAINTAINER Vivien Didelot <vivien@didelot.org>

RUN apt-get install -y ruby
COPY . /opt/irc
WORKDIR /opt/irc
CMD modulo plugins/ backend -n modbot -c '#modulo'
