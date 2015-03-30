BitShares OS X Build Instructions
===============================

1. Install XCode and its command line tools by following the instructions here: https://guide.macports.org/#installing.xcode

2. Install Homebrew by following the instructions here: http://brew.sh/

3. Initialize Homebrew:
   ```
   brew doctor
   brew update
   ```

4. Install dependencies:
   ```
   brew install boost cmake git openssl readline
   brew link --force openssl readline
   ```

5. *Optional.* To support importing Bitcoin wallet files:
   ```
   brew install berkeley-db
   ```

6. *Optional.* To use TCMalloc in LevelDB:
   ```
   brew install google-perftools
   ```

7. Clone the BitShares repository:
   ```
   git clone git@github.com:BitShares/bitshares.git
   cd bitshares
   ```

8. Build BitShares:
   ```
   git submodule update --init --recursive
   cmake .
   make
   ```

9. *Optional*. Install dependencies for the GUI:
   ```
   brew install node qt5
   ```

10. *Optional*. Build the GUI:
   ```
   cd programs/web_wallet
   npm install .
   cd ../..
   export CMAKE_PREFIX_PATH=/usr/local/opt/qt5/
   cmake -DINCLUDE_QT_WALLET=1 .
   make buildweb
   make
   ```
   Note: By default, the web wallet will not be rebuilt even after pulling new changes. To force the web wallet to rebuild, use `make forcebuildweb`.

11. *Optional*. Create GUI installation package:
   ```
   make package
   ```
