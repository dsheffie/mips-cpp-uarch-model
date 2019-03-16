#!/usr/bin/python3
import os
import sys
import re
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages

pre = re.compile('c (\d+), i (\d+), *')

z = list()
max_cycle = 0
with open(sys.argv[1]) as fp:
    for line in fp:
        mtx = re.match(pre, line)
        if mtx:
            g = mtx.groups()
            try:
                cycle = int(g[0])
                insns = int(g[1])
            except ValueError:
                continue

            if max_cycle < cycle:
                max_cycle = cycle
            else:
                continue
            
            t = [insns,cycle]
            z.append(t)

l = len(z)
print('collected %d samples' % l)
i = [x[0] for x in z[0:l]]
c = [x[1] for x in z[0:l]]


with PdfPages('results.pdf') as pdf:
    plt.rc('text', usetex=True)
    plt.figure()
    plt.plot(i,c)
    pdf.savefig()  # saves the current figure into a pdf page
    plt.close()


