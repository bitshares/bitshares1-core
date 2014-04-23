Windows - Visual Studio 2013
============================
#### Prerequisites ####
* Microsoft Visual C++ 2013 Update 1 (the free Express edition will work)
* If you have multiple MSVS installation use MSVS Developer console from target version.
* This file builds only x86 version.

#### Set up the directory structure####
* Create a base directory for all projects.  I'm putting everything in 
  `D:\BitShares`, you can use whatever you like.  In several of the batch files 
  and makefiles, this directory will be referred to as `BITSHARES_ROOT`:
  ```
mkdir D:\BitShares
```

* Clone the BitShares repository
  ```
cd D:\BitShares
git clone https://github.com/bitshares/bitshares_toolkit.git
cd bitshares_toolkit
git submodule init
git submodule update
cd vendor
git clone https://github.com/InvictusInnovations/leveldb-win.git
```

* Dowload CMake
  
  Download the latest *Win32 Zip* build CMake from 
  http://cmake.org/cmake/resources/software.html (version 2.8.12.2 as of this 
  writing).  Unzip it to your base directory, which will create a directory that
  looks something like `D:\BitShares\cmake-2.8.12.2-win32-x86`.  Rename this 
  directory to `D:\BitShares\CMake`.

  If you already have CMake installed elsewhere on your system you can use it, 
  but BitShares has a few batch files that expect it to be in the base 
  directory's `CMake` subdirectory, so those scripts would need tweaking.

* Download library dependencies:
   You can get prebuilt package containing all of the libraries described below built
   by using currently used compiler. This package can be downloaded from:
   http://get.syncad.com/invictus/Bitshare_toolkit_prebuilt_libs-vs2013.7z
   and should be unpacked into Bitshare-root directory (ie: D:\Bitshares).
   Now the package contains: BerkeleyDB, boost 1.55, OpenSSL 1.0.1g.

 * BerkeleyDB

   BitShares depends on BerkeleyDB 12c Release 1 (12.1.6.0.20).  You can build 
   this from source or download our pre-built binaries to speed things up.

 * Boost
 
   BitShares depends on the Boost libraries version 1.55 or later (I assume 
   you're using 1.55, the latest as of this writing).  You must build them from
   source.
   * download the latest boost source from http://www.boost.org/users/download/
   * unzip it to the base directory `D:\BitShares`. 
   * This will create a directory like `D:\BitShares\boost_1_55_0`.
   
 * OpenSSL

   BitShares depends on OpenSSL, and you must build this from source.
    * download the latest OpenSSL source from http://www.openssl.org/source/
    * Untar it to the base directory `D:\BitShares`
    * this will create a directory like `D:\BitShares\openssl-1.0.1g`.

At the end of this, your base directory should look like this:
```
D:\BitShares
+- BerkeleyDB
+- bitshares_toolkit
+- boost_1.55
+- CMake
+- OpenSSL
```

#### Build the library dependencies ####

* Set up environment for building:
  ```
cd D:\BitShares\bitshares_toolkit
setenv.bat
```

* Build boost libraries:
  ```
cd D:\BitShares\boost
bootstrap.bat
b2.exe toolset=msvc-11.0 variant=debug,release link=static threading=multi runtime-link=shared address-model=32
```
  The file `D:\BitShares\bitshares_toolkit\libraries\fc\CMakeLists.txt` has the 
  `FIND_PACKAGE(Boost ...)`
  command that makes CMake link in Boost.  That file contains the line:
  ```
set(Boost_USE_DEBUG_PYTHON ON)
```
  Edit this file and comment this line out (with a `#`).
  This line  tells CMake to look for a boost library that was built with 
  `b2.exe link=shared python-debugging=on`.  That would cause debug builds to 
  have `-gyd` mangled into their filename.  We don't need python debugging here,
  so we didn't give the `python-debugging` argument to `b2.exe`, and
  that causes our boost debug libraries to have `-gd` mangled into the filename 
  instead.  If this option in `fc\CMakeLists.txt` doesn't match the way you 
  compiled boost, CMake won't be able to find the debug version of the boost 
  libraries, and you'll get some strange errors when you try to run the
  debug version of BitShares.

* Build OpenSSL DLLs
  ```
cd D:\BitShares\openssl-1.0.1g
perl Configure --openssldir=D:\BitShares\OpenSSL VC-WIN32
ms\do_ms.bat
nmake -f ms\ntdll.mak
nmake -f ms\ntdll.mak install
```
  This will create the directory `D:\BitShares\OpenSSL` with the libraries, DLLs,
  and header files.

#### Build project files for BitShares ####

* Run CMake:
  ```
cd D:\BitShares\bitshares_toolkit
run_cmake.bat
```
 This pops up the cmake gui, but if you've used CMake before it will probably be
 showing the wrong data, so fix that:
 * Where is the source code: `D:\BitShares\bitshares_toolkit`
 * Where to build the binaries: `D:\BitShares\bin`
 
 Then hit **Configure**.  It may ask you to specify a generator for this 
 project; if it does, choose **Visual Studio 12** and select **Use default 
 native compilers**.  Look through the output and fix any errors.  Then 
 hit **Generate**.

#### Build BitShares ####
* Launch *Visual Studio* and load `D:\BitShares\bin\BitShares.sln`
* Set Configuration to Win32 - RelWithDebInfo
* *Build Solution*

 This will build bts_xt_server.exe, bts_xt_client.exe, and various unit tests.

Or you can build the `INSTALL` target in Visual Studio which will
copy all of the necessary files into your `D:\BitShares\install`
directory, then copy all of those files to the `bin` directory.