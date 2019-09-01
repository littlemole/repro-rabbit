# This is a comment
FROM littlemole/devenv_gpp_make
MAINTAINER me <little.mole@oha7.org>

# std dependencies
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y amqp-tools

ARG CXX=g++
ENV CXX=${CXX}

ARG BACKEND=libevent
ENV BACKEND=${BACKEND}

ARG BUILDCHAIN=make
ENV BUILDCHAIN=${BUILDCHAIN}

ARG TS=
ENV TS=${TS}


RUN /usr/local/bin/install.sh repro 
RUN /usr/local/bin/install.sh prio 

ADD ./docker/rabbitcpp.sh /usr/local/bin/amqpcpp.sh

RUN CXX=${CXX} /usr/local/bin/amqpcpp.sh

ARG RABBIT_HOST=
ENV RABBIT_HOST=${RABBIT_HOST}

RUN mkdir -p /usr/local/src/repro-rabbit
ADD . /usr/local/src/repro-rabbit


#CMD bash -c 'sleep 6000'
# CMD sleep 1 && amqp-declare-queue -u=amqp://rabbit -q test && /usr/local/bin/build.sh repro-rabbit
CMD sleep 1 && /usr/local/bin/build.sh repro-rabbit
