.text
.align 2
.set	noreorder
.set	nomacro
.globl goto_test8
.type goto_test8, @function
goto_test8:
move	$2,$4
.L0:
j .L2
nop

.L1:
j .L0
nop

.L2:
j .L7
nop

.L3:
j .L4
nop

.L4:
j .L5
nop

.L5:
j .L1
nop

.L6:
j .L3
nop

.L7:
bne $4,$0,.L6
addiu $4,$4,-1

lui $2,0
.done:
j $31
ori $2,$2,8
