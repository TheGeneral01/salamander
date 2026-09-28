CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O3
DEPFLAGS = -MMD -MP

# Standard system libraries for Vulkan and shaderc
LDLIBS = -lvulkan -lshaderc

TARGET = SalRuntime
OBJS = sal.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LDLIBS) -o $(TARGET)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

# Auto-dependency tracking
-include $(OBJS:.o=.d)

clean:
	rm -f $(OBJS) $(OBJS:.o=.d) $(TARGET)

.PHONY: all clean