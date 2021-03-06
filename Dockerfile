FROM alpine:3.12

RUN apk --update --no-cache add binutils \
                                gcc \
                                make \
                                binutils \
                                libc-dev \
                                bash \
                                gdb \
                                git \
    && rm -rf /var/cache/apk/*
