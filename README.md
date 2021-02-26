# eye-native

This is the native library for the eye-candy and eye-projector repositories.

Initialize dependencies by running `yarn install` and build the library by running `yarn build`.

## Compiling OpenCV

The OpenCV static libraries for Mac were built from source:

```sh
cd <working_directory>
git clone https://github.com/opencv/opencv.git
git clone https://github.com/opencv/opencv_contrib.git
mkdir build_opencv
cd build_opencv
cmake -D CMAKE_BUILD_TYPE=Release \
  -D OPENCV_EXTRA_MODULES_PATH=../opencv_contrib/modules \
  -D BUILD_SHARED_LIBS=OFF \
  -D WITH_ITT=OFF \
  ../opencv
```

## Native development

You can shorten your iteration time when developing this library in the context of e.g. eye-candy as follows:

1. Check out eye-candy and eye-native repositories in sibling directories.
2. Modify eye-candy's app `package.json` to change the eye-native dependency from `sclaggett/eye-native.git` to `../../eye-native`.
3. Make your changes in eye-native and run `yarn build` to make sure they compile.
4. Force eye-candy to pick up the changes by running `cd app`, `rm -rf node_modules/eye-native` and `yarn install --check-files`.

Maybe just `yarn upgrade eye-native`?

## Windows development

Development for Windows was done using a VM on AWS. Do the following to set up the build environment:

1. Provision a new *Microsoft Windows Server 2019 Base* VM on AWS. Spring for a medium-sized VM or you may as well compile by hand.
2. Connect to the VM over RDP.
3. Use IE just long enough to download and install a real browser.
4. Download and install [Git for Windows](https://git-scm.com/download/win) and configure it using Git Bash:

```sh
$ git config --global user.name "Shane Claggett"
$ git config --global user.email shaneclaggett@hushmail.com
$ cd ~/.ssh && ssh-keygen
$ cat id_rsa.pub
```

5. Add the identity to your account on GitHub (Settings, SSH and GPG keys, New SSH key).
6. At this point you should be able to check out the repository:

```sh
$ cd ~
$ git clone git@github.com:sclaggett/eye-native.git
```

7. Install [Node.js](https://nodejs.org/en/). Be sure to check the box to install *Tools for Native Modules*.
8. Installation of native tools will take a long time. Reboot the VM when it completes.
9. Open Git Bash and install yarn:

```sh
$ npm install --global yarn
```

10. At this point everything should work.
