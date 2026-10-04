CXX ?= g++
CXXFLAGS ?= -std=c++11 -O2 -Wall -Wextra

all: huffman

huffman: huffman.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f huffman

test: huffman
	./test/roundtrip.sh

.PHONY: all clean test
