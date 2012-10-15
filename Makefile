CXX=g++

ifdef DEBUG
	BOOST_ROOT=/opt/boost/1.50.0-debug
	CXXFLAGS+=-O0 -g
else
	BOOST_ROOT=/opt/boost/1.50.0-release
	CXXFLAGS+=-O3
endif

ifdef CHECK_DATA
	CXXFLAGS+=-DCHECK_DATA
endif

CXXFLAGS+=-std=c++0x -L$(BOOST_ROOT)/stage/lib -Wl,-rpath $(BOOST_ROOT)/stage/lib
INCLUDES=-I$(BOOST_ROOT)
LIBS=-lrt -lboost_thread -lboost_system -lboost_program_options -lboost_serialization -lboost_chrono
ADDITIONAL_SOURCES=portable_binary_iarchive.cpp portable_binary_oarchive.cpp
PROGRAMS=zero_copy_test
DIRECTORIES=build

all: directories $(PROGRAMS)

.PHONY: directories
directories: $(DIRECTORIES)/  

$(DIRECTORIES)/:
	mkdir -p $@ 

% : %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBS) $(ADDITIONAL_SOURCES) $< -o build/$@

clean:
	rm -rf $(DIRECTORIES) 

