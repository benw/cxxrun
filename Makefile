
CXXFLAGS = -g -Wall -Werror -O3

all: c++run test

test: c++run
	./hello.cpp -foo --bar wiz ; echo "Result code: $$?"

