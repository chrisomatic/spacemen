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
    core/particles.c \
    player.c \
    net.c \
    settings.c \
    projectile.c \
    effects.c \
    editor.c \
    main.c \
    -Icore \
    -lglfw -lGLU -lGLEW -lGL -lm \
    -o ../bin/spacemen
    
    #-lglfw -lGLU -lGLEW -lGL -lm -O2 \
