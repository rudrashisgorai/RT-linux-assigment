# Compilers
CXX     := g++
CC      := gcc
PKG_CFG := pkg-config

# pkg-config for OpenCV
OPENCV_CFLAGS := $(shell $(PKG_CFG) --cflags opencv4)
OPENCV_LIBS   := $(shell $(PKG_CFG) --libs   opencv4)

# Flags
CXXFLAGS := -Wall -Wextra -I. -pthread $(OPENCV_CFLAGS)
CFLAGS   := -Wall -Wextra -I.        $(OPENCV_CFLAGS)
LDFLAGS  := -pthread $(OPENCV_LIBS)

# Sources → Objects
SRCS     := p3_util.cpp p3.cpp canny_util.c
OBJS     := $(SRCS:.cpp=.o)
OBJS     := $(OBJS:.c=.o)

# Final executable
TARGET   := p3

.PHONY: all clean

all: $(TARGET)

# Link step
$(TARGET): $(OBJS)
	$(CXX) $^ -o $@ $(LDFLAGS)

# C++ compilation
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# C compilation
%.o: %.c
	$(CC) $(CFLAGS)   -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
