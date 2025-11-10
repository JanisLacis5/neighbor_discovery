SOURCES := src/main.cpp \
		   src/frame.cpp \
		   src/net.cpp
INCLUDES := -Isrc
FLAGS := -std=c++23 -pedantic -Wextra -Wshadow -Wmissing-declarations

main: $(SOURCES) 
	g++ $(FLAGS) $(INLCUDES) $(SOURCES) -o main

format:
	test -f .clang-format || { echo "No .clang-format file found"; exit 1; }
	find . -iname '*.h' -o -iname '*.cpp' | xargs clang-format-20 -i
