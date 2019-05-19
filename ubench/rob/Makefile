OBJ = rob.o
CXX = mips-sde-elf-g++
CC = mips-sde-elf-gcc
EXE = rob.mips
OPT = -O2
CXXFLAGS = -std=c++11 -g $(OPT)
DEP = $(OBJ:.o=.d)

.PHONY: all clean

all: $(EXE)

$(EXE) : $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) $(LIBS) -o $(EXE) -Tidt.ld

%.o: %.cc
	$(CXX) -MMD $(CXXFLAGS) -c $<

%.o: %.c
	$(CC) -MMD -O2 -c $<

-include $(DEP)

clean:
	rm -rf $(EXE) $(OBJ) $(DEP)
