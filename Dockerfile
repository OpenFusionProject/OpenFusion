# build
FROM debian:stable-slim AS build

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

RUN make nosandbox -j$(nproc)

# prod
FROM debian:stable-slim

WORKDIR /usr/src/app

RUN apt-get -y update && apt-get install -y \
libsqlite3-dev

COPY --from=build /usr/src/app/bin/fusion /bin/fusion
COPY sql ./sql

CMD ["/bin/fusion"]

EXPOSE 23000/tcp
EXPOSE 23001/tcp
EXPOSE 8001/tcp

LABEL Name=openfusion Version=1.6.0
