CXX ?= g++

TARGET := dist/resampler
SRCS := resampler/main.cpp
OBJS := $(SRCS:%.cpp=build/%.o)
DEPS := $(OBJS:.o=.d)

CPPFLAGS += -I.
CXXFLAGS ?= -std=c++17 -O3 -Wall -Wextra
DEPFLAGS := -MMD -MP

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS) | dist
	$(CXX) $(CXXFLAGS) -o $@ $^

build/%.o: %.cpp | build/resampler
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

build/resampler dist:
	mkdir -p $@

clean:
	rm -rf build dist

-include $(DEPS)
