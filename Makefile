CXX = g++
CXXFLAGS = -std=c++17 -O2

SRC = src/main.cpp src/sha256.cpp src/base64.cpp
OBJ = $(SRC:.cpp=.o)

password_manager: $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f src/*.o password_manager
