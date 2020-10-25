#!/usr/bin/sh

cppcheck --enable=all --suppress=missingIncludeSystem --suppress=unusedFunction --quiet -ibuilddir -i.ccls-cache -ispdlog --language=c++ --std=c++17 --template=gcc --relative-paths=remote-music-player .
