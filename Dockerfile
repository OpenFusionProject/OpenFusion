FROM debian:latest

WORKDIR /usr/src/app

RUN apt-get -y update && apt-get install -y \
git \
clang \
make \
libsqlite3-dev

COPY . ./

RUN make -j8

# tabledata should be copied from the host;
# clone it there before building the container
#RUN git submodule update --init --recursive

CMD ["./bin/fusion"]

LABEL Name=openfusion Version=0.0.1
