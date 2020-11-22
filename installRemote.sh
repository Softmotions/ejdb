version=$(cat ./package.json | grep version | head -1 | awk -F: '{ print $2 }' | sed 's/[",]//g' | tr -d '[[:space:]]')

. /etc/os-release
if [ "$(uname)" == "Darwin" ]; then
  os=darwin   
elif [ "${ID}" == "alpine" ]; then
  os=alpine
elif [ "$(expr substr $(uname -s) 1 5)" == "Linux" ]; then
  os=linux
else
  echo "Could not detect operating system"
  echo "Will attempt to build from source"
  npm install
  npm run build:source
  exit 0
fi

echo "Your OS is $os"

downloadUrl="https://github.com/markwylde/node-ejdb-lite/releases/download/v$version-lite/ejdb2_node_x64_${os}_14.x.node"
echo "Downloading $downloadUrl"
mkdir -p build/src/bindings/ejdb2_node/ejdb2_node/linux-x64
wget -O build/src/bindings/ejdb2_node/ejdb2_node/linux-x64/ejdb2_node.node $downloadUrl
