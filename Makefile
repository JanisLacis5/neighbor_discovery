DAEMON_SOURCES := src/main.cpp \
		   src/frame.cpp \
		   src/ifaces.cpp \
		   src/sockets.cpp
DAEMON_INCLUDES := -Isrc
CLI_SOURCES := src/cli.cpp
FLAGS := -std=c++23 -pedantic -Wextra -Wshadow -Wmissing-declarations

daemon: $(DAEMON_SOURCES) 
	g++ $(FLAGS) $(DAEMON_INLCUDES) $(DAEMON_SOURCES) -o daemon 

cli: $(DAEMON_SOURCES)
	g++ $(FLAGS) $(CLI_SOURCES) -o cli

format:
	test -f .clang-format || { echo "No .clang-format file found"; exit 1; }
	find . -iname '*.h' -o -iname '*.cpp' | xargs clang-format-20 -i
