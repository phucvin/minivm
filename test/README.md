cd ..

git submodule update --init

make

time build/bin/minivm test/fib/fib.lua 40

> 10.3s

time luajit -joff test/fib/fib.lua 40

> 4.5s