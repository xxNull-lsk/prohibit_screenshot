#!/bin/bash

DIR=`pwd`

mkdir bin 2>/dev/null

g++ -o ./bin/screen_capture screen_capture.cpp -lX11 -lXext -lpng -lXfixes -lXdamage -lXtst

g++ -o ./bin/lock_window lock_window.cpp -lX11 -lrt  -lpthread
g++ -o ./bin/lock_server lock_server.cpp -lrt  -lpthread

g++ -o ./bin/screenshot screenshot.cpp capture.cpp save_png.cpp -lX11 -lXext -lpng
gcc -o ./bin/png2image png2image.c -lpng
gcc -o ./bin/image2png image2png.c -lpng

cd ./bin/
./png2image ../lock.png lock.image
./png2image ../lock2.png lock2.image
./image2png lock.image lock.png
./image2png lock2.image lock2.png

cd $DIR
cp $DIR/prohibit_screenshot.h $DIR/bin/
function build_module() {
    submodule=$1
    cd $DIR
    if [ ! -d $DIR/bin/$submodule ]; then
        cp -rf $DIR/$submodule $DIR/bin/
        cd $DIR/bin/$submodule
        ./autogen.sh
        ./configure --prefix=`pwd`/dist
    else
        cd $DIR
        for file in `git status --short|grep $submodule|awk '{print $2}'`; do
            echo update $file
            cp $DIR/$file $DIR/bin/$file
            touch $DIR/bin/$file
        done

        cd $DIR/bin/$submodule
        for file in `find -name prohibit_screenshot.h`; do
            echo touch $file
            touch $file
        done
    fi
    make -j8
    if [ $? -ne 0 ]; then
        echo "make $submodule error"
        exit 1
    fi
    make install > /dev/null
    if [ $? -ne 0 ]; then
        echo "make $submodule install error"
        exit 1
    fi
}
build_module "libx11-1.6.12.1"
build_module "libxext-1.3.3.1"