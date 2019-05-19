#!/bin/bash

cfg_mips -f rob.mips

sim_ooo -f blob.bin -p 1 -i 0 --mem_model 1 --heartbeat $((1024*1024*1024)) --use_l2 0 --mem_latency 200 --rob_size 32 --retire_bw 1
mv sim.txt sim32.txt

#sim_ooo -f blob.bin -p 1 -i 0 --mem_model 1 --heartbeat $((1024*1024*1024)) --use_l2 0 --mem_latency 200 --rob_size 64
#mv sim.txt sim64.txt

#sim_ooo -f blob.bin -p 1 -i 0 --mem_model 1 --heartbeat $((1024*1024*1024)) --use_l2 0 --mem_latency 200 --rob_size 128
#mv sim.txt sim128.txt

#sim_ooo -f blob.bin -p 1 -i 0 --mem_model 1 --heartbeat $((1024*1024*1024)) --use_l2 0 --mem_latency 200 --rob_size 256
#mv sim.txt sim256.txt

#python3 plot.py sim32.txt sim64.txt sim128.txt sim256.txt

#mailx -A sim.pdf -s "different rob sizes (delay slot)" sheffield.david@gmail.com < /dev/null 
