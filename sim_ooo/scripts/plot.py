#!/usr/bin/python3
import os
import sys
import re
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages

pre = re.compile('(\d+) cycles, (\d+) insns retired, avg ipc (\d+(.\d+)?), window ipc (\d+(.\d+)?)(, dcu hit rate \d+.\d+, window dcu hit rate \d+.\d+)?\n')

z = list()
max_cycle = 0
with open(sys.argv[1]) as fp:
    for line in fp:
        mtx = re.match(pre, line)
        if mtx:
            g = mtx.groups()
            try:
                cycle = int(g[0])
                a_ipc = float(g[2])
                w_ipc = float(g[4])
            except ValueError:
                continue

            if max_cycle < cycle:
                max_cycle = cycle
            else:
                continue
            
            t = [cycle,a_ipc,w_ipc]
            z.append(t)

l = len(z)
t = [x[0] for x in z[0:l]]
a = [x[1] for x in z[0:l]]
w = [x[2] for x in z[0:l]]


with PdfPages('results.pdf') as pdf:
    plt.rc('text', usetex=True)
    plt.figure()
    plt.plot(t,w,t,a)
    pdf.savefig()  # saves the current figure into a pdf page
    plt.close()


