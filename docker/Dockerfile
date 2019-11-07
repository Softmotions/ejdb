FROM        alpine:3.9

MAINTAINER  Anton Adamansky <adamansky@gmail.com>

RUN         apk add --no-cache cmake gcc binutils libc-dev git ninja

RUN         git clone https://github.com/Softmotions/ejdb.git &&  \
            mkdir -p ejdb/build

WORKDIR     ejdb/build

RUN         cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/ejdb2 &&   \
            ninja install &&                                                                    \
            mkdir -p /ejdb2_data

VOLUME      ["/ejdb2_data"]

EXPOSE		  9191

WORKDIR     /ejdb2_data

ENTRYPOINT	["/opt/ejdb2/bin/jbs", "-b", "0.0.0.0"]
