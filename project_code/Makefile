CXXFLAGS := -g -Wall -std=c++0x -lm
#CXXFLAGS := -g -Wall -lm
CXX=g++
SRC=procsim.cpp procsim_driver.cpp
PROCSIM=./procsim
R=2
J=3
K=2
L=1
F=4

build:
	$(CXX) $(CXXFLAGS) $(SRC) -o procsim

run:
	$(PROCSIM) -r$R -f$F -j$J -k$K -l$L < traces/gcc.100k.trace 

run1:
	$(PROCSIM) -r$R -f$F -j$J -k$K -l$L < traces/gcc.100k.trace 

run2:
	$(PROCSIM) -r$R -f$F -j$J -k$K -l$L < traces/gobmk.100k.trace

run3:
	$(PROCSIM) -r$R -f$F -j$J -k$K -l$L < traces/hmmer.100k.trace

run4:
	$(PROCSIM) -r$R -f$F -j$J -k$K -l$L < traces/mcf.100k.trace

clean:
	rm -f procsim *.o
