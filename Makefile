CXX=g++
FLAGS=-std=c++17 -O2 -Wall -Wextra -pthread
all: test bench
test: test.cpp kv.hpp
	$(CXX) $(FLAGS) test.cpp -o test && ./test
bench: bench.cpp kv.hpp
	$(CXX) $(FLAGS) bench.cpp -o bench && ./bench 4
