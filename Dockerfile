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

ENV AUTHENTICATION_PORT 23000
ENV MONITORING_PORT 8003
ENV SHARDING_PORT 23001

WORKDIR /usr/src/app

RUN apk update && apk upgrade && apk add \
libstdc++ \
sqlite-dev

COPY --from=build /usr/src/app/bin/fusion /bin/fusion
COPY sql ./sql

EXPOSE $AUTHENTICATION_PORT
EXPOSE $MONITORING_PORT
EXPOSE $SHARDING_PORT

CMD ["/bin/fusion"]

LABEL Name=openfusion Version=0.0.2
