# build
FROM debian:trixie-slim as build

WORKDIR /usr/src/app

RUN apt update && apt upgrade -y && apt install -y \
git \
clang \
build-essential \
libsqlite3-dev

COPY src ./src
COPY vendor ./vendor
COPY .git ./.git
COPY Makefile CMakeLists.txt version.h.in ./

RUN make nosandbox -j$(nproc)

# export-only stage: `docker build --target=export --output=./bin .`
FROM scratch AS export
COPY --from=build /usr/src/app/bin/fusion /fusion

# prod
FROM debian:trixie-slim

WORKDIR /usr/src/app

RUN apt update && apt upgrade -y && apt install -y \
libsqlite3-dev

COPY --from=build /usr/src/app/bin/fusion /bin/fusion

CMD ["/bin/fusion"]

EXPOSE 23000/tcp
EXPOSE 23001/tcp
EXPOSE 8003/tcp

LABEL Name=openfusion Version=2.0.0
