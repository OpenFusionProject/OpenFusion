# build
FROM debian:stable-slim as build

WORKDIR /usr/src/app

RUN apt-get -y update && apt-get install -y \
git \
clang \
make \
libsqlite3-dev

COPY src ./src
COPY vendor ./vendor
COPY .git ./.git
COPY Makefile CMakeLists.txt version.h.in ./

RUN make -j8

# prod
FROM debian:stable-slim

WORKDIR /usr/src/app

RUN apt-get -y update && apt-get install -y \
libsqlite3-dev

COPY --from=build /usr/src/app/bin/fusion /bin/fusion
COPY sql ./sql

CMD ["/bin/fusion"]

LABEL Name=openfusion Version=0.0.2
