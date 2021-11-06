.text
.align 2
.set	noreorder
.set	nomacro
.globl goto_test64
.type goto_test64, @function
goto_test64:
move	$2,$4
.L0:
j .L25
nop

.L1:
j .L34
nop

.L2:
j .L57
nop

.L3:
j .L40
nop

.L4:
j .L46
nop

.L5:
j .L42
nop

.L6:
j .L24
nop

.L7:
j .L6
nop

.L8:
j .L29
nop

.L9:
j .L45
nop

.L10:
j .L27
nop

.L11:
j .L32
nop

.L12:
j .L33
nop

.L13:
j .L5
nop

.L14:
j .L4
nop

.L15:
j .L44
nop

.L16:
j .L48
nop

.L17:
j .L1
nop

.L18:
j .L49
nop

.L19:
j .L11
nop

.L20:
j .L30
nop

.L21:
j .L19
nop

.L22:
j .L62
nop

.L23:
j .L18
nop

.L24:
j .L0
nop

.L25:
j .L28
nop

.L26:
j .L7
nop

.L27:
j .L63
nop

.L28:
j .L43
nop

.L29:
j .L59
nop

.L30:
j .L13
nop

.L31:
j .L8
nop

.L32:
j .L26
nop

.L33:
j .L37
nop

.L34:
j .L10
nop

.L35:
j .L21
nop

.L36:
j .L23
nop

.L37:
j .L20
nop

.L38:
j .L52
nop

.L39:
j .L53
nop

.L40:
j .L38
nop

.L41:
j .L58
nop

.L42:
j .L9
nop

.L43:
j .L22
nop

.L44:
j .L61
nop

.L45:
j .L35
nop

.L46:
j .L60
nop

.L47:
j .L16
nop

.L48:
j .L17
nop

.L49:
j .L2
nop

.L50:
j .L56
nop

.L51:
j .L3
nop

.L52:
j .L39
nop

.L53:
j .L41
nop

.L54:
j .L12
nop

.L55:
j .L36
nop

.L56:
j .L31
nop

.L57:
j .L50
nop

.L58:
j .L55
nop

.L59:
j .L14
nop

.L60:
j .L47
nop

.L61:
j .L54
nop

.L62:
j .L51
nop

.L63:
bne $4,$0,.L15
addiu $4,$4,-1

lui $2,0
.done:
j $31
ori $2,$2,64
