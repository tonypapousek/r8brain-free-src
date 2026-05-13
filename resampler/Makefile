CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -I..

SRC = main.cpp
OBJ = $(SRC:.cpp=.o)
TARGET = resampler

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp WavFile.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
