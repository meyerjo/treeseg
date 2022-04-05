FROM ubuntu:20.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update &&  apt-get -y install tzdata
RUN apt-get update && apt-get install -y libpcl-dev libarmadillo-dev cmake build-essential

WORKDIR /code
COPY . .
RUN ls -l
RUN mkdir -p build/ && cd build/ && cmake .. && make -j

ENV PATH="$PATH:/code/build"

VOLUME /data
