UNAME_S = $(shell uname -s)

OBJ = save_state.o main.o loadelf.o parseMips.o helper.o profileMips.o gthread.o sparse_mem.o ooo_core.o mips_op.o sim_cache.o perceptron.o loop_predictor.o

ifeq ($(UNAME_S),Linux)
	CXX = g++
	OBJ += gthread_asm.o
	EXTRA_LD = -lunwind -lboost_program_options
endif

ifeq ($(UNAME_S),FreeBSD)
	CXX = CC -march=native
	OBJ += gthread_asm.o
	EXTRA_LD = -L/usr/local/lib -lunwind -lboost_program_options
endif

ifeq ($(UNAME_S),Darwin)
	CXX = clang++ -march=native
	OBJ += gthread_asm_osx.o
	EXTRA_LD = -L/usr/local/lib -lboost_program_options-mt
endif

CXXFLAGS = -std=c++11 -g $(OPT) $(LLVM_CXXFLAGS)
LIBS =  $(EXTRA_LD) -lpthread 


DEP = $(OBJ:.o=.d)
OPT = -O3 -g -std=c++11
EXE = sim_ooo

.PHONY : all clean

all: $(EXE)

$(EXE) : $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) $(LLVM_LDFLAGS) $(LIBS) -o $(EXE)

%.o: %.cc
	$(CXX) -MMD $(CXXFLAGS) -c $< 

%.o : %.S
	$(CXX) -c $< -o $@

-include $(DEP)

clean:
	rm -rf $(EXE) $(OBJ) $(DEP)
