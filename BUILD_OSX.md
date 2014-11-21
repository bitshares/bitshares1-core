BitShares Mac OS X Build Instructions
===============================

1. Install XCode and its command line tools by following the instructions here: https://guide.macports.org/#installing.xcode

2. Install Homebrew by following the instructions here: http://brew.sh/

3. Configure and update Homebrew:
   ```
   brew doctor
   brew update
   ```

4. Install dependencies:
   ```
   brew install boost cmake git openssl readline
   ```

5. Link upgraded OpenSSL and Readline:
   ```
   brew link --force openssl readline
   ```

6. *Optional.* To support importing Bitcoin wallet files:
   ```
   brew install berkeley-db
   ```

7. *Optional.* To use TCMalloc in LevelDB:
   ```
   brew install google-perftools
   ```

8. Clone the BitShares repository:
   ```
   git clone git@github.com:BitShares/bitshares.git
   cd bitshares
   ```

9. *Optional.* To help contribute changes:
   ```
   git checkout develop
   ```

10. Build BitShares:
   ```
   git submodule init
   git submodule update
   cmake .
   make
   ```

11. *Optional*. To build the desktop GUI:
   ```
    cd programs/web_wallet/
    npm install lineman-angular
    npm install lineman-less
    cd ../..
    export CMAKE_PREFIX_PATH=~/Qt/5.3/clang_64/
    make buildweb
    cmake -DINCLUDE_QT_WALLET=TRUE
    make
   ```
   By default, the web wallet will not be rebuilt even after pulling new changes. To force the web wallet to rebuild, use `make forcebuildweb`.
