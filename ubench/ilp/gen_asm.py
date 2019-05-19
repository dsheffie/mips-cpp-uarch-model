#!/usr/bin/python3
import os
import sys

def cg(num_ops,unroll):
    asm = []
    fname = 'func' + str(num_ops)
    label = '.L' + str(num_ops)
    
    asm.append('.align	2')
    asm.append('.globl	' + fname)
    asm.append('.set nomips16')
    asm.append('.ent ' + fname)
    asm.append('.type	' + fname + ', @function')
    asm.append('.set	noreorder')
    asm.append('.set	nomacro')
    asm.append('.set    noat')
    regs = []
    for r in range(3, 26):
        regs.append('$'+str(r))
        
    asm.append(fname + ':')
    #preserve registers
    asm.append('addiu	$29,$29,-32')
    asm.append('sw $16, 0($29)')
    asm.append('sw $17, 4($29)')
    asm.append('sw $18, 8($29)')
    asm.append('sw $19, 12($29)')
    asm.append('sw $20, 16($29)')
    asm.append('sw $21, 20($29)')
    asm.append('sw $22, 24($29)')
    asm.append('sw $24, 28($29)')
    #move iterations into register 1 (loop counter)
    asm.append('addiu $1, $4, 0')
    #move iterations into register 2 (return value)
    asm.append('addiu $2, $4, 0')
    #initialize values
    for i in range(0, num_ops):
        asm.append('addiu ' + regs[i] + ',$0,' + str(i+1))
    
    asm.append(label+':')

    c = 0
    for u in range(0, unroll):
        for r in range(0, num_ops):
            d = c % num_ops
            #asm.append('addu ' + regs[d] + ',' + regs[d] + ',$1')
            asm.append('mul ' + regs[d] + ',' + regs[d] + ',$1')
            c = c + 1
    
    asm.append('bnezl $1,' + label)
    asm.append('addiu $1, $1, -' + str(unroll))


    #restore registers
    asm.append('lw $16, 0($29)')
    asm.append('lw $17, 4($29)')
    asm.append('lw $18, 8($29)')
    asm.append('lw $19, 12($29)')
    asm.append('lw $20, 16($29)')
    asm.append('lw $21, 20($29)')
    asm.append('lw $22, 24($29)')
    asm.append('lw $24, 28($29)')
    
    asm.append('j $31')
    asm.append('addiu	$29,$29,32')
    
    asm.append('.set	reorder')
    asm.append('.set	macro')
    asm.append('.end    ' + fname)
    return asm


def main():
    nf = 16
    with open('funcs.s', 'w') as out:
        out.write('.text\n')
        for num_ops in range(1,nf+1):
            code = cg(num_ops, 4)
            out.write('\n'.join(code))
            out.write('\n\n')
        out.write('\n\n')
    with open('funcs.h', 'w') as out:
        out.write('#ifndef __funcsh__\n')
        out.write('#define __funcsh__\n')
        out.write('#define NUM_FUNCS ' + str(nf) + '\n')
        out.write('typedef int (*fn_t)(int x);\n')
        for num_ops in range(1,nf):
            out.write('int func' + str(num_ops) + '(int x);\n')
            
        out.write('fn_t funcs[] = {\n')
        out.write('NULL,\n')
        for num_ops in range(1,nf):
            out.write('func' + str(num_ops))
            if(num_ops != (nf-1)):
                out.write(',')
            out.write('\n')
        out.write('};\n')
        out.write('#endif\n')

if __name__ == '__main__':
    main()
    
