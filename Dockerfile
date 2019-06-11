FROM        ubuntu:18.04

RUN         apt-get update && apt-get install --force-yes -y  cmake       \
                                                              git

RUN         git clone https://github.com/Softmotions/ejdb.git &&          \
            mkdir ejdb/build

WORKDIR     ejdb/build

RUN         cmake .. -DCMAKE_BUILD_TYPE=Release &&                        \
            make install

EXPOSE		  9191

ENTRYPOINT	["/usr/local/bin/jbs", "-b", "0.0.0.0"]
