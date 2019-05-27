FROM ubuntu:18.10
MAINTAINER erez@shrewdthings.com
ARG GIT_REPO='https://github.com/ShrewdThingsLtd/Energy-Languages.git'
ARG GIT_BRANCH='develop'

RUN mkdir -p /usr/src
WORKDIR /usr/src
RUN apt-get -y update
RUN apt-get -y install git
RUN git clone -b $GIT_BRANCH $GIT_REPO
WORKDIR /usr/src/Energy-Languages

