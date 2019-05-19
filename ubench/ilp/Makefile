OBJ = ilp_test.o funcs.o
CXX = mips-sde-elf-g++
CC = mips-sde-elf-gcc
EXE = ilp_test
OPT = 
CXXFLAGS = -std=c++11 -g $(OPT)
DEP = $(OBJ:.o=.d)

.PHONY: all clean

all: $(EXE)

$(EXE) : $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) $(LIBS) -o $(EXE) -Tidt.ld

%.o: %.cc
	$(CXX) -MMD $(CXXFLAGS) -c $<

%.o: %.c
	$(CC) -std=c99 -MMD -O2 -c $<

%.o : %.s
	$(CC) -c $<

-include $(DEP)

clean:
	rm -rf $(EXE) $(OBJ) $(DEP) funcs.s funcs.h
