### Controller FSEQ Export

Export Controller Specific FSEQ Files

### Building
Uses C++23, QT 5.15, spdlog, zstd, pugixml, and cMake 3.20.

```git clone https://github.com/computergeek1507/controller_gen.git```

To build on Windows, use Visual Studio 2022

```VS2022.bat```

If you get a qt cmake error, update the QT location in batch file.

To build on Linux with g++.

```
mkdir build
cd build
cmake ..
cmake --build .
./controller_gen
```
