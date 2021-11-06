.text
.align 2
.set	noreorder
.set	nomacro
.globl goto_test4
.type goto_test4, @function
goto_test4:
move	$2,$4
.L0:
j .L1
nop

.L1:
j .L3
nop

.L2:
j .L0
nop

.L3:
bne $4,$0,.L2
addiu $4,$4,-1

lui $2,0
.done:
j $31
ori $2,$2,4
