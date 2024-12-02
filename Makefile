# IMS Project 2024

# file: Makefile
# brief: Makefile for IMS project 2024
# author: Marko Olesak (xolesa00) && Jan Findra (xfindr01)


CXX = g++
CFLAGS = -std=c++20 -Wall -Wextra -pedantic -g
TARGET = model
SRCS = model.cpp
OBJS = $(SRCS:.cpp=.o)

all : clean $(TARGET)

clean:
	rm -f $(TARGET)
	rm -f 03_xolesa00_xfindr01.zip

$(TARGET): $(OBJS)
	$(CXX) $(CFLAGS) -o $(TARGET) $(OBJS) -l simlib -lm
	rm -f $(OBJS)

%.o: %.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

run: all
	./$(TARGET)

pack: clean
	zip 03_xolesa00_xfindr01.zip Makefile model.cpp doc.pdf