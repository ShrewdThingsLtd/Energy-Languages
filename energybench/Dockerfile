FROM ubuntu:18.10
MAINTAINER erez@shrewdthings.com

ARG GIT_REPO='https://github.com/ShrewdThingsLtd/Energy-Languages.git'
ARG GIT_BRANCH='develop'

RUN mkdir -p /usr/local/src
RUN apt-get -y update && apt-get -y install git python3 python3-pip 2to3 libpcre3-dev libpcre3 libgmp-dev libapr1-dev libhts-dev libmpfr-dev libmpfr6 libmpc-dev libmpc3
RUN ln -sf /usr/bin/python3 /usr/bin/python
RUN mkdir -p /usr/local/src/Python-3.6.1/bin && ln -sf /usr/bin/python /usr/local/src/Python-3.6.1/bin/python3.6
RUN pip3 install -U lazyme
RUN pip3 install -U gmpy2

WORKDIR /usr/local/src
RUN apt-get -y update
RUN git clone -b $GIT_BRANCH $GIT_REPO
WORKDIR /usr/local/src/Energy-Languages
RUN ./gen-input.sh
RUN cp knucleotide-input25000000.txt ./C/k-nucleotide/
RUN cp knucleotide-input25000000.txt ./Python/k-nucleotide/
RUN cd ./C/ && 2to3 -w ./compile_all.py && python ./compile_all.py compile
RUN cd ./Python/ && 2to3 -w ./compile_all.py && python ./compile_all.py compile
RUN mkdir -p /etc/eml
COPY ./eml/config /etc/eml/config

