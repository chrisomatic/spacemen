#!/bin/sh
rm -rf bin
mkdir bin

cd src

gcc core/gfx.c \
    core/shader.c \
    core/timer.c \
    core/math2d.c \
    core/window.c \
    core/imgui.c \
    core/glist.c \
    core/socket.c \
    player.c \
    net.c \
    main.c \
    -Icore \
    -lglfw -lGLU -lGLEW -lGL -lm -O2 \
    -o ../bin/spacemen
