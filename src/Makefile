main main.cpp:
	g++ -pedantic -Wextra -Wshadow -Wmissing-declarations main.cpp -o main

make clean:
	find . -iname '*.h' -o -iname '*.cpp' | xargs clang-format-20 -i
