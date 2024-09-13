UNAME_S = $(shell uname -s)

OBJ = githash.o saveState.o main.o loadelf.o helper.o interpret.o gthread.o sparse_mem.o ooo_core.o mips_op.o sim_cache.o perceptron.o loop_predictor.o branch_predictor.o disassemble.o


ifeq ($(UNAME_S),Linux)
	CXX = g++ -march=native -flto
	OBJ += gthread_asm.o
	EXTRA_LD = -lunwind -lboost_program_options -lboost_serialization -ljemalloc -lcapstone
endif

ifeq ($(UNAME_S),FreeBSD)
	CXX = CC -march=native -flto
	OBJ += gthread_asm.o
	EXTRA_LD = -L/usr/local/lib -lunwind -lboost_program_options -ltcmalloc_minimal
endif

ifeq ($(UNAME_S),Darwin)
	CXX = clang++ -I/opt/local/include -flto
	OBJ += gthread_asm_darwin.o
	EXTRA_LD = -L/opt/local/lib -lboost_program_options-mt -lboost_serialization-mt -lcapstone #-ltcmalloc_minimal
endif

CXXFLAGS = -std=c++11 -g $(OPT) $(LLVM_CXXFLAGS)
LIBS =  $(EXTRA_LD) -lpthread 


DEP = $(OBJ:.o=.d)
OPT = -O3 -g -std=c++11 -flto
EXE = sim_ooo

.PHONY : all clean

all: $(EXE)

$(EXE) : $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) $(LLVM_LDFLAGS) $(LIBS) -o $(EXE)

githash.cc : ../.git/HEAD ../.git/index
	echo "const char *githash = \"$(shell git rev-parse HEAD)\";" > $@

%.o: %.cc
	$(CXX) -MMD $(CXXFLAGS) -c $< 

%.o : %.S
	$(CXX) -c $< -o $@

gen_html : gen_html.cc pipeline_record.hh
	$(CXX) -MMD $(CXXFLAGS) gen_html.cc $(LIBS) -o gen_html

-include $(DEP)

clean:
	rm -rf $(EXE) $(OBJ) $(DEP)
