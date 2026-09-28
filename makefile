CXX      = g++
CXXFLAGS = -std=c++20 -O2 -Wall -Wextra -I. -Icompiler

TARGET   = sal
SRC      = sal.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET) $(TEST_TARGETS)

new:
	rm -f $(TARGET)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)