CXX = g++
CXXFLAGS = -Wall -std=c++17
LDFLAGS = -lSDL2 -lSDL2_image -lSDL2_ttf

all: visualizer

visualizer: main.cpp
	$(CXX) $(CXXFLAGS) -o os-scheduler_visualizer main.cpp $(LDFLAGS)

clean:
	rm -f os-scheduler_visualizer