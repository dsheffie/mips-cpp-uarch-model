.text
.align 2
.set	noreorder
.set	nomacro
.globl goto_test128
.type goto_test128, @function
goto_test128:
move	$2,$4
.L0:
j .L6
nop

.L1:
j .L111
nop

.L2:
j .L28
nop

.L3:
j .L62
nop

.L4:
j .L46
nop

.L5:
j .L38
nop

.L6:
j .L64
nop

.L7:
j .L45
nop

.L8:
j .L121
nop

.L9:
j .L126
nop

.L10:
j .L7
nop

.L11:
j .L71
nop

.L12:
j .L85
nop

.L13:
j .L114
nop

.L14:
j .L81
nop

.L15:
j .L69
nop

.L16:
j .L10
nop

.L17:
j .L108
nop

.L18:
j .L127
nop

.L19:
j .L5
nop

.L20:
j .L73
nop

.L21:
j .L37
nop

.L22:
j .L59
nop

.L23:
j .L68
nop

.L24:
j .L35
nop

.L25:
j .L12
nop

.L26:
j .L32
nop

.L27:
j .L94
nop

.L28:
j .L53
nop

.L29:
j .L55
nop

.L30:
j .L112
nop

.L31:
j .L61
nop

.L32:
j .L47
nop

.L33:
j .L75
nop

.L34:
j .L78
nop

.L35:
j .L113
nop

.L36:
j .L79
nop

.L37:
j .L18
nop

.L38:
j .L3
nop

.L39:
j .L122
nop

.L40:
j .L63
nop

.L41:
j .L0
nop

.L42:
j .L40
nop

.L43:
j .L14
nop

.L44:
j .L50
nop

.L45:
j .L118
nop

.L46:
j .L22
nop

.L47:
j .L41
nop

.L48:
j .L29
nop

.L49:
j .L123
nop

.L50:
j .L1
nop

.L51:
j .L115
nop

.L52:
j .L2
nop

.L53:
j .L60
nop

.L54:
j .L43
nop

.L55:
j .L13
nop

.L56:
j .L100
nop

.L57:
j .L23
nop

.L58:
j .L120
nop

.L59:
j .L57
nop

.L60:
j .L76
nop

.L61:
j .L52
nop

.L62:
j .L119
nop

.L63:
j .L15
nop

.L64:
j .L90
nop

.L65:
j .L67
nop

.L66:
j .L49
nop

.L67:
j .L89
nop

.L68:
j .L24
nop

.L69:
j .L44
nop

.L70:
j .L80
nop

.L71:
j .L70
nop

.L72:
j .L103
nop

.L73:
j .L4
nop

.L74:
j .L92
nop

.L75:
j .L48
nop

.L76:
j .L97
nop

.L77:
j .L125
nop

.L78:
j .L56
nop

.L79:
j .L11
nop

.L80:
j .L88
nop

.L81:
j .L106
nop

.L82:
j .L99
nop

.L83:
j .L16
nop

.L84:
j .L30
nop

.L85:
j .L42
nop

.L86:
j .L93
nop

.L87:
j .L104
nop

.L88:
j .L19
nop

.L89:
j .L34
nop

.L90:
j .L65
nop

.L91:
j .L9
nop

.L92:
j .L117
nop

.L93:
j .L20
nop

.L94:
j .L105
nop

.L95:
j .L26
nop

.L96:
j .L116
nop

.L97:
j .L74
nop

.L98:
j .L91
nop

.L99:
j .L39
nop

.L100:
j .L86
nop

.L101:
j .L31
nop

.L102:
j .L84
nop

.L103:
j .L51
nop

.L104:
j .L83
nop

.L105:
j .L96
nop

.L106:
j .L98
nop

.L107:
j .L58
nop

.L108:
j .L124
nop

.L109:
j .L72
nop

.L110:
j .L107
nop

.L111:
j .L109
nop

.L112:
j .L101
nop

.L113:
j .L77
nop

.L114:
j .L17
nop

.L115:
j .L33
nop

.L116:
j .L25
nop

.L117:
j .L36
nop

.L118:
j .L54
nop

.L119:
j .L66
nop

.L120:
j .L21
nop

.L121:
j .L102
nop

.L122:
j .L27
nop

.L123:
j .L110
nop

.L124:
j .L95
nop

.L125:
j .L87
nop

.L126:
j .L8
nop

.L127:
bne $4,$0,.L82
addiu $4,$4,-1

lui $2,0
.done:
j $31
ori $2,$2,128
