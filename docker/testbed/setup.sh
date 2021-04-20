#!/bin/bash

set -e
set -x

echo "Setup docker host"

SCRIPTPATH="$(
  cd "$(dirname "$0")"
  pwd -P
)"
cd $SCRIPTPATH

JENKINS_AGENT_VERSION=4.6

dpkg --add-architecture i386
apt-get update
apt-get install -y apt-utils software-properties-common apt-transport-https sudo curl wget zip unzip
apt-get update
apt-get dist-upgrade -y

wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
wget -qO- https://deb.nodesource.com/setup_12.x | bash -
wget -qO- https://dl.yarnpkg.com/debian/pubkey.gpg | apt-key add -
wget -qO- https://dl-ssl.google.com/linux/linux_signing_key.pub | apt-key add -

apt-add-repository -y 'deb https://apt.kitware.com/ubuntu/ xenial main'
echo "deb https://dl.yarnpkg.com/debian/ stable main" | tee /etc/apt/sources.list.d/yarn.list
wget -qO- https://storage.googleapis.com/download.dartlang.org/linux/debian/dart_unstable.list >/etc/apt/sources.list.d/dart_unstable.list

./llvm.sh 10
./llvm-update-alternatives.sh 10 1000

apt-get update

echo ttf-mscorefonts-installer msttcorefonts/accepted-mscorefonts-eula select true | debconf-set-selections

apt-get install -y \
  autopoint \
  binutils \
  bison \
  build-essential \
  ca-certificates \
  cmake \
  dart \
  debhelper \
  debianutils \
  devscripts \
  flex \
  g++ \
  gcc \
  git \
  gnupg \
  gperf \
  intltool \
  lib32z1 \
  libc6-dbg \
  libcunit1-dev \
  libcurl4-openssl-dev \
  libgdk-pixbuf2.0-0 \
  libgdk-pixbuf2.0-dev \
  libtool \
  libtool-bin \
  lzip \
  make \
  mc \
  nano \
  ninja \
  nodejs \
  openjdk-8-jdk-headless \
  p7zip-full \
  ruby \
  scons \
  wine \
  yarn

echo 'JAVA_HOME=/usr/lib/jvm/java-8-openjdk-amd64' >> /etc/environment

mkdir -p /opt
(
  cd /opt
  wget -qO swift.tar.gz https://swift.org/builds/swift-5.2.3-release/ubuntu1604/swift-5.2.3-RELEASE/swift-5.2.3-RELEASE-ubuntu16.04.tar.gz
  tar -xf ./swift.tar.gz
  rm -f ./swift.tar.gz
)

useradd -d /home/worker -m -s /bin/bash worker

sudo -iu worker /bin/bash -i <<"EOF"
set -e
set -x

echo 'export JAVA_HOME=/usr/lib/jvm/java-8-openjdk-amd64' >> ~/.profile
echo 'export PATH=$JAVA_HOME/bin:$PATH' >> ~/.profile
curl -s "https://get.sdkman.io" | /bin/bash
EOF

sudo -iu worker /bin/bash -i <<"EOF"
set -e
set -x

sdk install gradle 5.4.1
sdk install maven

echo 'PATH=/usr/lib/dart/bin:$PATH' >> ~/.profile
echo 'PATH=~/.sdkman/candidates/maven/current/bin:$PATH' >> ~/.profile
echo 'PATH=~/.sdkman/candidates/gradle/current/bin:$PATH' >> ~/.profile
echo 'PATH=$PATH:/opt/swift-5.2.3-RELEASE-ubuntu16.04/usr/bin' >> ~/.profile
echo 'export PATH=$PATH:~/.yarn/bin' >> ~/.profile
EOF

sudo -iu worker /bin/bash -i <<"EOF"
set -e
set -x

wget -O commandlinetools.zip https://dl.google.com/android/repository/commandlinetools-linux-6200805_latest.zip
unzip commandlinetools.zip -d ./Android
rm commandlinetools.zip

echo 'export ANDROID_NDK_VERSION=21.1.6352462' >> ~/.profile
echo 'export ANDROID_HOME=~/Android' >> ~/.profile
echo 'export ANDROID_SDK_ROOT=${ANDROID_HOME}' >> ~/.profile
echo 'PATH=$PATH:$ANDROID_HOME/tools' >> ~/.profile
echo 'PATH=$PATH:$ANDROID_HOME/tools/bin' >> ~/.profile
echo 'PATH=$PATH:$ANDROID_HOME/platform-tools' >> ~/.profile
echo 'PATH=~/flutter/bin:$PATH' >> ~/.profile
echo 'export PATH' >> ~/.profile
echo 'export ANDROID_NDK_HOME=$ANDROID_HOME/ndk/$ANDROID_NDK_VERSION' >> ~/.profile
EOF

sudo -iu worker /bin/bash -i <<"EOF"
set -e
set -x

yes | sdkmanager --sdk_root=${ANDROID_HOME} --licenses
sdkmanager --sdk_root=${ANDROID_HOME} \
           --install \
           "platform-tools" \
           "platforms;android-28" \
           "platforms;android-29" \
           "ndk;${ANDROID_NDK_VERSION}" \
           "build-tools;28.0.3" \
           "build-tools;29.0.3"

sdkmanager --sdk_root=${ANDROID_HOME} tools
yes | sdkmanager --sdk_root=${ANDROID_HOME} --licenses
EOF

sudo -iu worker /bin/bash -i <<"EOF"
set -e
set -x

git clone https://github.com/flutter/flutter.git -b stable
flutter config --android-sdk $ANDROID_HOME
flutter doctor --android-licenses
flutter precache
EOF

sudo -iu worker /bin/bash -i <<"EOF"
set -e
set -x

mkdir -p ~/tmp && cd ~/tmp
wget https://sourceware.org/pub/valgrind/valgrind-3.15.0.tar.bz2
tar -xf ./valgrind-3.15.0.tar.bz2
cd ./valgrind-3.15.0
./configure --prefix=/home/worker/.local
make && make install
cd ~/ && rm -rf ./tmp/*
EOF

curl --create-dirs -fsSLo /usr/share/jenkins/agent.jar \
  https://repo.jenkins-ci.org/public/org/jenkins-ci/main/remoting/${JENKINS_AGENT_VERSION}/remoting-${JENKINS_AGENT_VERSION}.jar
chmod 755 /usr/share/jenkins
chmod 644 /usr/share/jenkins/agent.jar
ln -sf /usr/share/jenkins/agent.jar /usr/share/jenkins/slave.jar

sudo -iu worker /bin/bash -i <<"EOF"
set -e
set -x

git clone https://github.com/mxe/mxe.git
cd ./mxe
echo 'JOBS := 1'                                >  ./settings.mk
echo 'MXE_TARGETS := x86_64-w64-mingw32.static' >> ./settings.mk
echo 'LOCAL_PKG_LIST := cunit libiberty'        >> ./settings.mk
echo '.DEFAULT local-pkg-list:'                 >> ./settings.mk
echo 'local-pkg-list: $(LOCAL_PKG_LIST)'        >> ./settings.mk

make
echo 'export MXE_HOME=~/mxe' >> ~/.profile
EOF

sudo -iu worker /bin/bash -i <<"EOF"
set -e
set -x
mkdir -p ~/.jenkins
mkdir -p ~/agent
cat ~/.profile
echo $PATH
EOF

