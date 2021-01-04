CC := g++
CFLAGS := -Wall -g -lboost_system -lpthread -std=c++11
TARGET := test



# $(wildcard *.cpp /xxx/xxx/*.cpp): get all .cpp files from the current directory and dir "/xxx/xxx/"
SRCS := $(wildcard SCDTR-PC/*.cpp)
# $(patsubst %.cpp,%.o,$(SRCS)): substitute all ".cpp" file name strings to ".o" file name strings
OBJS := $(patsubst %.cpp,%.o,$(SRCS))
OBJS := $(patsubst SCDTR-PC/%.cpp,%.o,$(SRCS))

all: $(TARGET)
$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS)
%.o: SCDTR-PC/%.cpp
	$(CC) -c $< $(CFLAGS)
clean:
	rm -rf $(TARGET) *.o
	
.PHONY: all clean