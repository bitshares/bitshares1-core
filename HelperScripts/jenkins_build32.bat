echo Available processor count: %NUMBER_OF_PROCESSORS%
echo Starting build # %BUILD_NUMBER%
pushd bitshares_toolkit
rem git reset --hard
rem git pull
rem reset and pull not needed because we assume
rem that bitshares_toolkit was just cloned fresh
git submodule init || exit /b 21
git submodule update || exit /b 22
popd

if exist bitshares_toolkit/vendor/leveldb-win (
    pushd bitshares_toolkit/vendor/leveldb-win
    git reset --hard || exit /b 23
    git pull || exit /b 24
    popd
) else (
    git clone https://www.github.com/InvictusInnovations/leveldb-win.git bitshares_toolkit/vendor/leveldb-win || exit /b 25
)

call bitshares_toolkit/setenv.bat || exit /b 26
mkdir build
cd build
cmake -G "Visual Studio 12" ../bitshares_toolkit || exit /b 27
msbuild.exe /M:%NUMBER_OF_PROCESSORS% /p:Configuration=RelWithDebinfo /p:Platform=Win32 /target:rebuild /clp:ErrorsOnly BitShares.sln || exit /b 28
