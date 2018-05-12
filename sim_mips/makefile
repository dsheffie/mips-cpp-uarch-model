UNAME_S = $(shell uname -s)

ifeq ($(UNAME_S),Linux)
     CXX = clang++-4.0 -march=native
     EXTRA_LD = -ldl -lffi -lbfd
endif

ifeq ($(UNAME_S),FreeBSD)
     CXX = CC -march=native
     EXTRA_LD = -L/usr/local/lib
endif

CXXFLAGS = -std=c++11 -g $(OPT) $(LLVM_CXXFLAGS)
LIBS =  $(EXTRA_LD) -lpthread -lffi -lcurses -lz -lunwind -lboost_system

OBJ = main.o loadelf.o parseMips.o helper.o profileMips.o gthread.o gthread_asm.o mips_op.o
DEP = $(OBJ:.o=.d)
OPT = -O0 -g -fomit-frame-pointer -std=c++11
EXE = sim_mips

.PHONY : all clean

all: $(EXE)

$(EXE) : $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) $(LLVM_LDFLAGS) $(LIBS) -o $(EXE)

%.o: %.cc
	$(CXX) -MMD $(CXXFLAGS) -c $< 

%.o : %.s
	as $< -o $@

-include $(DEP)

clean:
	rm -rf $(EXE) $(OBJ) $(DEP)
