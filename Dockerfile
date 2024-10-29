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

RUN make nosandbox -j$(nproc)

# prod
FROM alpine:3

WORKDIR /usr/src/app

RUN apk update && apk upgrade && apk add \
libstdc++ \
sqlite-dev

COPY --from=build /usr/src/app/bin/fusion /bin/fusion
COPY sql ./sql

CMD ["/bin/fusion"]

EXPOSE 23000/tcp
EXPOSE 23001/tcp
EXPOSE 8003/tcp

LABEL Name=openfusion Version=1.6.0
