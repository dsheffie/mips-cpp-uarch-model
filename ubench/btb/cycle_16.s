.text
.align 2
.set	noreorder
.set	nomacro
.globl goto_test16
.type goto_test16, @function
goto_test16:
move	$2,$4
.L0:
j .L6
nop

.L1:
j .L9
nop

.L2:
j .L1
nop

.L3:
j .L2
nop

.L4:
j .L7
nop

.L5:
j .L11
nop

.L6:
j .L15
nop

.L7:
j .L14
nop

.L8:
j .L0
nop

.L9:
j .L10
nop

.L10:
j .L13
nop

.L11:
j .L4
nop

.L12:
j .L8
nop

.L13:
j .L12
nop

.L14:
j .L3
nop

.L15:
bne $4,$0,.L5
addiu $4,$4,-1

lui $2,0
.done:
j $31
ori $2,$2,16
