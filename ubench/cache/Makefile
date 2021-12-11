OBJ = mem_micro.o traverse.o
CXX = mips-sde-elf-g++
EXE = mem_micro
OPT = -O2 -mno-branch-likely
CXXFLAGS = -std=c++11 -g $(OPT)
DEP = $(OBJ:.o=.d)

.PHONY: all clean

all: $(EXE)

$(EXE) : $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) $(LIBS) -o $(EXE) -Tidt.ld

%.o: %.cc
	$(CXX) -MMD $(CXXFLAGS) -c $< 

-include $(DEP)

clean:
	rm -f $(EXE) $(OBJ) $(DEP) *.csv *~
