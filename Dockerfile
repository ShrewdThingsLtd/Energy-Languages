FROM ubuntu:18.10
MAINTAINER erez@shrewdthings.com
ARG GIT_REPO='https://github.com/ShrewdThingsLtd/Energy-Languages.git'
ARG GIT_BRANCH='develop'

RUN mkdir -p /usr/src
WORKDIR /usr/src
RUN apt-get -y update
RUN apt-get -y install git
RUN git clone -b $GIT_BRANCH $GIT_REPO
RUN apt-get -y update && apt-get -y install python3 python3-pip libpcre3-dev libpcre3 libgmp-dev libapr1-dev libhts-dev
RUN ln -sf /usr/bin/python3 /usr/bin/python
WORKDIR /usr/src/Energy-Languages
RUN ./gen-input.sh
RUN pip3 install -U lazyme
RUN apt-get -y install 2to3
RUN cd ./C/ && 2to3 -w ./compile_all.py && python ./compile_all.py compile
RUN cd ./Python/ && 2to3 -w ./compile_all.py && python ./compile_all.py compile

