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
RUN cp knucleotide-input25000000.txt ./C/k-nucleotide/
RUN cp knucleotide-input25000000.txt ./Python/k-nucleotide/
RUN pip3 install -U lazyme
RUN apt-get -y install 2to3 sudo
RUN cd ./C/ && 2to3 -w ./compile_all.py && python ./compile_all.py compile
RUN cd ./Python/ && 2to3 -w ./compile_all.py && python ./compile_all.py compile

WORKDIR /usr/src
RUN git clone https://github.com/HPC-ULL/eml.git
WORKDIR /usr/src/eml
RUN apt-get -y update && apt-get -y install msrtool automake libtool libconfuse2 libconfuse-common libconfuse-dev pkg-config
RUN autoreconf -i
RUN ./configure && make && make install
WORKDIR /usr/src/Energy-Languages
RUN mkdir -p /usr/local/src/Python-3.6.1/bin && ln -sf /usr/bin/python /usr/local/src/Python-3.6.1/bin/python3.6
RUN pip3 install -U pypl2

#RUN cd ./C/ && rm -f ./C.csv && python ./compile_all.py measure
#RUN cat ./C/C.csv
#RUN cd ./Python/ && rm -f ./Python.csv && python ./compile_all.py measure
#RUN cat ./Python/Python.csv

