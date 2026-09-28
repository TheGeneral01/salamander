CXX      = g++
CXXFLAGS = -std=c++20 -O2 -Wall -Wextra -I. -Icompiler
GPU_LIBS = $(shell pkg-config --libs shaderc vulkan)

TARGET   = sal
SRC      = sal.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(GPU_LIBS)

clean:
	rm -f $(TARGET) $(TEST_TARGETS)

new:
	rm -f $(TARGET)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(GPU_LIBS)

gpu:
	$(MAKE) new