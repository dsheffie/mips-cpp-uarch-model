#!/usr/bin/python3
import sys
import subprocess
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
import re


def run(argv):
    blob = argv[1]
    icnt = 4*1024*1024
    
    if len(argv) > 2:
        icnt = int(argv[2])
        
    blobname = blob.split('.')[0]
    bloblist = blobname.split('/')
    blobname = bloblist[len(bloblist)-1]
    ipcre = re.compile('(\d+|\d+.\d+) instructions/cycle')
    cmd_ = ['sim_ooo', '-p', '1', '--oracle', '1', '--taken_branches_per_cycle', '8', '--mem_model', '0', '-m', str(icnt), '-f', blob]
    ipc = {}
    z = []
    for scale in range(1, 16):
        if scale != 1 and scale % 2 != 0:
            continue
        cmd = cmd_ + ['--scale', str(scale)]
        logname = '/tmp/' + blobname + '_scale_' + str(scale) + '.txt'
        with open(logname, 'w') as out:
            subprocess.call(cmd,stdout=out,stderr=out)
        with open(logname, 'r') as in_:
            for line in in_:
                m = re.search(ipcre, line)
                if m:
                    ipc[scale] = float(m.groups()[0])
                    z.append([scale,ipc[scale]])
                    print('%d,%f' % (scale, ipc[scale]))

    print(ipc)
    l = len(z)
    print('collected %d samples' % l)
    i = [x[0] for x in z[0:l]]
    c = [x[1] for x in z[0:l]]
    with PdfPages(blobname+'.pdf') as pdf:
#        plt.rc('text', usetex=True)
        plt.figure()
        plt.xlabel('scale')
        plt.ylabel('ipc')
        plt.title(blobname)
        plt.plot(i,c,'go-')
        pdf.savefig()  # saves the current figure into a pdf page
        plt.close()

if __name__ == '__main__':
    run(sys.argv)
