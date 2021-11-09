.text
.align 2
.set	noreorder
.set	nomacro
.globl goto_test32
.type goto_test32, @function
goto_test32:
move	$2,$4
.L0:
j .L18
nop

.L1:
j .L16
nop

.L2:
j .L26
nop

.L3:
j .L6
nop

.L4:
j .L25
nop

.L5:
j .L11
nop

.L6:
j .L9
nop

.L7:
j .L14
nop

.L8:
j .L19
nop

.L9:
j .L28
nop

.L10:
j .L7
nop

.L11:
j .L0
nop

.L12:
j .L13
nop

.L13:
j .L4
nop

.L14:
j .L8
nop

.L15:
j .L22
nop

.L16:
j .L23
nop

.L17:
j .L21
nop

.L18:
j .L3
nop

.L19:
j .L2
nop

.L20:
j .L10
nop

.L21:
j .L12
nop

.L22:
j .L20
nop

.L23:
j .L30
nop

.L24:
j .L31
nop

.L25:
j .L15
nop

.L26:
j .L24
nop

.L27:
j .L29
nop

.L28:
j .L1
nop

.L29:
j .L5
nop

.L30:
j .L17
nop

.L31:
bne $4,$0,.L27
addiu $4,$4,-1

lui $2,0
.done:
j $31
ori $2,$2,32
