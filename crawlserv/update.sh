#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

cd $DIR
git submodule foreach '[ "$path" = "crawlserv/src/_extern/EigenRand" ] || [ "$path" = "crawlserv/src/_extern/tomotopy" ] || git pull origin master'
git submodule update src/_extern/EigenRand
git submodule update src/_extern/tomotopy
git pull --recurse-submodules
