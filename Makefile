CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall
SRC = main.cpp order_book.cpp mbp_writer.cpp
OBJ = $(SRC:.cpp=.o)
TARGET = reconstruction_mbp

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $<

clean:
	del $(OBJ) $(TARGET).exe 2>nul || true
