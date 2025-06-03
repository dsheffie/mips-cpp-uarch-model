UNAME_S = $(shell uname -s)

OBJ = githash.o saveState.o main.o loadelf.o helper.o interpret.o gthread.o sparse_mem.o ooo_core.o mips_op.o sim_cache.o perceptron.o loop_predictor.o branch_predictor.o disassemble.o


CXX = /home/dsheffie/buildroot/output/host/bin/riscv64-buildroot-linux-gnu-g++ 


OBJ += gthread_asm.o
EXTRA_LD = /home/dsheffie/rv64-linux-apps/capstone/libcapstone.a


CXXFLAGS = -std=c++11 -g $(OPT)
LIBS =  $(EXTRA_LD) -lpthread 


DEP = $(OBJ:.o=.d)
OPT = -I/home/dsheffie/rv64-linux-apps/capstone/include -march=rv64ima_zbb_zba_zicond -O3 -g -flto -std=c++11
EXE = sim_ooo

.PHONY : all clean

all: $(EXE)

$(EXE) : $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) $(LIBS) -o $(EXE) -z max-page-size=65536 -static

githash.cc : ../.git/HEAD ../.git/index
	echo "const char *githash = \"$(shell git rev-parse HEAD)\";" > $@

%.o: %.cc
	$(CXX) -MMD $(CXXFLAGS) -c $< 

%.o : %.S
	$(CXX) -c $< -o $@

-include $(DEP)

clean:
	rm -rf $(EXE) $(OBJ) $(DEP)
