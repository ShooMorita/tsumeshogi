FROM alpine:3.20

RUN apk add --no-cache clang musl-dev

WORKDIR /work
