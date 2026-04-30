ducky: 4d vectors -> quad-vect -> qua-vec -> qua-ec-> quack -> duck -> ducky

command used to compile:

x86_64-w64-mingw32-g++ src/glad.c src/main.cpp \
  -Iinclude \
  -Lexternal/glfw/build-win/src \
  -lglfw3 -lopengl32 -lgdi32 -luser32 -lshell32 \
  -o bin/ducky.exe \

ducky.exe is located in bin

.dky format:
// comments like this are allowed
[x1] [y1] [z1] [w1] [u1] [v1]
[x2] [y2] [z2] [w2] [u2] [v2]
...
face
[index1] [index2] [index3]

