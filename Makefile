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
	rm -f xolesa00.zip

$(TARGET): $(OBJS)
	$(CXX) $(CFLAGS) -o $(TARGET) $(OBJS) -lsimlib -lm
	rm -f $(OBJS)

%.o: %.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

run: all
	./$(TARGET)

pack: clean
	zip xolesa00.zip *.cpp *.h Makefile README.md