#!/usr/bin/env python3
import random
import sys

def gen(n):
    v = [x for x in range(0, n)]
    random.shuffle(v)
    arr = []
    for i in range(0,n):
        cc = v[i]
        nn = v[(i+1)%n]
        arr.append((cc,nn))
    arr.sort()
        
    with open('cycle_' + str(n) + '.s', 'w') as out:
        func = 'goto_test' + str(n)
        out.write('.text\n.align 2\n')
        out.write('.set	noreorder\n')
        out.write('.set	nomacro\n')
        out.write('.globl '+func+'\n')
        out.write('.type '+func+', @function\n')
        out.write(func + ':\n')
        out.write('move	$2,$4\n')
        for i in range(0,n):
            x = arr[i]
            label ='.L' + str(x[0])
            nlabel = '.L' + str(x[1])
            out.write('%s:\n' % label)
            if(i != (n-1)):
                out.write('j %s\nnop\n\n' % nlabel)
            else:
                out.write('bne $4,$0,%s\n' % nlabel)
                out.write('addiu $4,$4,-1\n\n')


        out.write('lui $2,' +str(n>>16)+'\n')        
        out.write('.done:\nj $31\n')
        out.write('ori $2,$2,' +str(n & ((2**16)-1))+'\n')                


if __name__ == "__main__":
    n = 2**int(sys.argv[1])
    i = 4
    while i <= n:
        gen(i)
        i = i * 2
