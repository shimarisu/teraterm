echo %~dp0\install_myinnosetup.bat

pushd %~dp0

if "%CMAKE_COMMAND%" == "" (
   call find_cmake.bat
)

"%CMAKE_COMMAND%" -P ../buildtools/install_innosetup.cmake

popd
