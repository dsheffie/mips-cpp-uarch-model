#!/usr/bin/env python3
import glob
import os
import re
import subprocess
import time
import sys
import matplotlib
import numpy as np
matplotlib.use('PDF')
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages


def parse(src):
    perf_re = re.compile('(\d+) insns, (\d+(\.\d+)?) cycles')
    x = []
    y = []
    with open(src, 'r') as fp:
        for line in fp:
            mtx = re.match(perf_re, line)
            g = mtx.groups()
            insns = int(g[0])
            cycles = float(g[1])
            x.append(insns)
            y.append(cycles)
    return (x,y)

h = 'sim'
pp = PdfPages(h + '.pdf')
plt.figure()
plt.xlabel('insns')
plt.ylabel('cycles')
plt.title(h + ' rob size')

for i in range(1, len(sys.argv)):
    (ilist, clist) = parse(sys.argv[i])
    plt.plot(ilist,clist,'--b')
    
plt.savefig(pp,format='pdf')

pp.close()
