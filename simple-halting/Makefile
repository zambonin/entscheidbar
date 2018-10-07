CXXFLAGS = -Wall -Wextra -O2 -std=c++14
OUTPUT = $(basename $(wildcard *.cpp))

all: $(OUTPUT)

debug: CXXFLAGS += -g
debug: all

clean:
	rm $(OUTPUT)

test: $(addsuffix .test, $(basename $(wildcard tests/*.in)))
%.test: %.in %.out /usr/bin/cmp all
	./$(OUTPUT) < $< | cmp -s $(word 2, $?) -
