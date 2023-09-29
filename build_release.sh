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
    core/text_list.c \
    core/socket.c \
    core/particles.c \
    player.c \
    net.c \
    settings.c \
    projectile.c \
    effects.c \
    editor.c \
    powerups.c \
    main.c \
    -Icore \
    -lglfw -lGLU -lGLEW -lGL -lm -O2 \
    -o ../bin/spacemen
    
    #-lglfw -lGLU -lGLEW -lGL -lm -O2 \

cd ..
rm -rf release
mkdir -p release/src/core/
mkdir -p release/bin

cp -r src/core/fonts ./release/src/core/
cp -r src/core/shaders ./release/src/core/

cp -r src/img ./release/src/
cp -r src/effects ./release/src/
cp -r src/themes ./release/src/

cp bin/spacemen ./release/bin/
echo "#!/bin/sh" > ./release/run.sh
echo "./bin/spacemen" >> ./release/run.sh
chmod +x ./release/run.sh

echo "Done. Run 'cd release && ./run.sh'"
