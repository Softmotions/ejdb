/**
 * Metro configuration for React Native
 * https://github.com/facebook/react-native
 *
 * @format
 */

const path = require('path');

module.exports = {
  transformer: {
    getTransformOptions: async () => ({
      transform: {
        experimentalImportSupport: false,
        inlineRequires: false,
      },
    }),
  },
  resolver: {
    extraNodeModules: new Proxy(
      {
        'ejdb2_react_native': path.resolve(__dirname, '../binding')
      },
      {
        get: (target, name) => {
          if (target.hasOwnProperty(name)) {
            return target[name];
          } else {
            return path.join(process.cwd(), `node_modules/${name}`);
          }
        }
      }
    )
  },
  projectRoot: path.resolve(__dirname),
  watchFolders: [
    path.resolve(__dirname, '../binding')
  ]
};

