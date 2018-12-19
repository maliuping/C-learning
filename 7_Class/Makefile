SOURCE = $(shell find ./ -name "*.cpp")
DEPS = $(shell find ./ -name "*.h")
#OBJS = $(SOURCE:%.cpp=$(OBJDIR)/%.o)
OBJS = $(patsubst %.cpp,$(OBJDIR)/%.o,$(SOURCE))
OBJDIR = ./obj
CXX = g++
CXXFLAGS = -Wall -O -g -std=c++11 -fdiagnostics-color=auto

build : $(OBJS)
	$(CXX) $(OBJS) -o build

#main.o : main.cpp $(DEFS)
#	$(CXX) $(CXXFLAGS) -c $< -o $@

#sys.o : sys.cpp $(DEFS)
#	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.cpp $(DEPS)
		$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJS) build
