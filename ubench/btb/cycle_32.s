.text
.align 2
.set	noreorder
.set	nomacro
.globl goto_test32
.type goto_test32, @function
goto_test32:
move	$2,$4
.L0:
j .L21
nop

.L1:
j .L5
nop

.L2:
j .L22
nop

.L3:
j .L10
nop

.L4:
j .L2
nop

.L5:
j .L31
nop

.L6:
j .L14
nop

.L7:
j .L13
nop

.L8:
j .L28
nop

.L9:
j .L15
nop

.L10:
j .L24
nop

.L11:
j .L16
nop

.L12:
j .L27
nop

.L13:
j .L19
nop

.L14:
j .L0
nop

.L15:
j .L29
nop

.L16:
j .L25
nop

.L17:
j .L12
nop

.L18:
j .L17
nop

.L19:
j .L26
nop

.L20:
j .L11
nop

.L21:
j .L8
nop

.L22:
j .L23
nop

.L23:
j .L30
nop

.L24:
j .L20
nop

.L25:
j .L9
nop

.L26:
j .L1
nop

.L27:
j .L7
nop

.L28:
j .L4
nop

.L29:
j .L18
nop

.L30:
j .L3
nop

.L31:
bne $4,$0,.L6
addiu $4,$4,-1

lui $2,0
.done:
j $31
ori $2,$2,32
