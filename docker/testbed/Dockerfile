# EJDB2 testbed docker image

FROM ubuntu:xenial

LABEL maintainer="adamansky@gmail.com"

RUN mkdir /setup

WORKDIR   /setup
COPY      ./*.sh ./
RUN       ./setup.sh

VOLUME    /home/worker/.jenkins
VOLUME    /home/worker/agent

USER root
COPY jenkins-agent /usr/local/bin/jenkins-agent
RUN chmod +x /usr/local/bin/jenkins-agent &&\
    ln -s /usr/local/bin/jenkins-agent /usr/local/bin/jenkins-slave

USER worker
WORKDIR   /home/worker

ENTRYPOINT ["/usr/local/bin/jenkins-agent"]




