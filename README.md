# eye-native

This is the native library for the eye-candy and eye-projector repositories.

This project requires OpenCV. You will need to update the paths in *binding.gyp* to the location where you install the OpenCV headers and library.

Initialize dependencies by running `yarn install` and build the library by running `yarn build`.

You can shorten your iteration time when developing this library in the context of e.g. eye-candy as follows:

1. Check out eye-candy and eye-native repositories in sibling directories.
2. Modify eye-candy's `package.json` to change the eye-native dependency from `sclaggett/eye-native.git` to `../eye-native`.
3. Make your changes in eye-native and run `yarn build` to make sure they compile.
4. Force eye-candy to pick up the changes by running `rm -rf node_modules/eye-native` and `yarn install --check-files`.

