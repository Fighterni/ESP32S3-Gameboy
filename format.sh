#!/usr/bin/bash

clang-format -style=google -dump-config > .clang-format
find . -iname '*.h' -o -iname '*.cpp' -o -iname '*.ino' | xargs clang-format -i

