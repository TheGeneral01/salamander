CXX      = g++
CXXFLAGS = -std=c++20 -O2 -Wall -Wextra -I. -Icompiler

TARGET   = sal
SRC      = sal.cpp
TEST_TARGETS = preprocessor_test type_checker_test

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET) $(TEST_TARGETS)

new:
	rm -f $(TARGET)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

test: tests/preprocessor_test.cpp tests/type_checker_test.cpp
	$(CXX) $(CXXFLAGS) tests/preprocessor_test.cpp -o preprocessor_test
	./preprocessor_test
	$(CXX) $(CXXFLAGS) tests/type_checker_test.cpp -o type_checker_test
	./type_checker_test
	rm -f $(TEST_TARGETS)