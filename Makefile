DAEMON_SOURCES := src/daemon/main.cpp \
		   src/daemon/frame.cpp \
		   src/daemon/ifaces.cpp \
		   src/daemon/sockets.cpp \
		   src/daemon/cli_commands.cpp \
		   src/daemon/cli_tokens.cpp
CLI_SOURCES := src/cli/main.cpp
SHARED_INCLUDES := -Isrc
DAEMON_INCLUDES := -Isrc/daemon
FLAGS := -std=c++23 -pedantic -Wextra -Wshadow -Wmissing-declarations -Wall

daemon: $(DAEMON_SOURCES) 
	g++ $(FLAGS) -g $(SHARED_INCLUDES) $(DAEMON_INCLUDES) $(DAEMON_SOURCES) -o daemon 

cli: $(CLI_SOURCES)
	g++ $(FLAGS) -g $(SHARED_INCLUDES) $(CLI_SOURCES) -o cli

format:
	test -f .clang-format || { echo "No .clang-format file found"; exit 1; }
	find . -iname '*.h' -o -iname '*.cpp' | xargs clang-format-20 -i
