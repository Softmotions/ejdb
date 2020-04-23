#!/bin/bash
set -e

SCRIPTPATH="$(
  cd "$(dirname "$0")"
  pwd -P
)"
cd $SCRIPTPATH

if [ ! -d ./build ]; then
  mkdir ./build
fi

cd ./build
cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Release \
         -DBUILD_JNI_BINDING=ON \
         -DBUILD_DART_BINDING=ON \
         -DBUILD_NODEJS_BINDING=ON \
         && make
pvs-studio-analyzer analyze -l ${HOME}/.config/PVS-Studio/PVS-Studio.lic -o ./pvs.log
rm -rf ./pvs_report
plog-converter -a 'GA:1,2;64:1,2;OP:1,2;MISRA:1,2' -t fullhtml -o ./pvs_report ./pvs.log
