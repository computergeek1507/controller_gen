### FPP View

FPP Status Monitor 

### Building
Uses C++23, QT 5.15, spdlog, and cMake 3.20.

```git clone https://github.com/computergeek1507/fpp_view.git```

To build on Windows, use Visual Studio 2022

```VS2022.bat```

If you get a qt cmake error, update the QT location in batch file.

To build on Linux with g++.

```
mkdir build
cd build
cmake ..
cmake --build .
./fpp_view
```
