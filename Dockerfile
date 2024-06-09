# build
FROM alpine:3 as build

WORKDIR /usr/src/app

RUN apk update && apk upgrade && apk add \
linux-headers \
git \
clang18 \
make \
sqlite-dev

COPY src ./src
COPY vendor ./vendor
COPY .git ./.git
COPY Makefile CMakeLists.txt version.h.in ./

RUN sed -i 's/^CC=clang$/&-18/' Makefile
RUN sed -i 's/^CXX=clang++$/&-18/' Makefile

RUN make -j8

# prod
FROM alpine:3

WORKDIR /usr/src/app

RUN apk update && apk upgrade && apk add \
libstdc++ \
sqlite-dev

COPY --from=build /usr/src/app/bin/fusion /bin/fusion
COPY sql ./sql

CMD ["/bin/fusion"]

LABEL Name=openfusion Version=0.0.2
