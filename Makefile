main: src/main.cpp:
	g++ -pedantic -Wextra -Wshadow -Wmissing-declarations src/main.cpp -o main

clean:
	rm main

format:
	test -f .clang-format || { echo "No .clang-format file found"; exit 1; }
	find . -iname '*.h' -o -iname '*.cpp' | xargs clang-format-20 -i
