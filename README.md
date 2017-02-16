# hwcrypto installer

 * License: LGPL 2.1
 * &copy; Estonian Information System Authority

## Please refer to the [wiki](https://github.com/hwcrypto/hwcrypto-native/wiki) for some time!

## Building
[![Build Status](https://travis-ci.org/open-eid/chrome-token-signing.svg?branch=master)](https://travis-ci.org/open-eid/chrome-token-signing)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/2449/badge.svg)](https://scan.coverity.com/projects/2449)

1. Install dependencies

   1.1 Ubuntu

        sudo apt-get install qtbase5-dev libssl-dev

   1.2 Windows

     * [Visual Studio 2015 Community Edition](https://www.visualstudio.com/downloads/)
     * [WiX toolset](http://wixtoolset.org/releases/)

   1.3 OSX

     * [XCode](https://itunes.apple.com/en/app/xcode/id497799835?mt=12)

2. Fetch the source

        git clone --recursive https://github.com/hwcrypto/hwcrypto-native
        cd hwcrypto-native

3. Build

        make
