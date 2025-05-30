.bss
.set	.LANCHOR0,. + 0
.type	saved_sp, @object
.size	saved_sp, 8
saved_sp:
	.zero	8
	
.text
.globl start_gthread_asm
.type start_gthread_asm,@function

	//a0 - first argument (new stack)
	//a1 - second argument (function pointer)
	//a2 - third argument (argument
start_gthread_asm:
	add sp, sp, -112
	sd s0,    0(sp)
	sd s1,    8(sp)
	sd s2,    16(sp)
	sd s3,    24(sp)
	sd s4,    32(sp)
	sd s5,    40(sp)
	sd s6,    48(sp)
	sd s7,    56(sp)
	sd s8,    64(sp)
	sd s9,    72(sp)
	sd s10,   80(sp)
	sd s11,   88(sp)
	sd ra,    96(sp)
	//save stack pointer
	sd sp,.LANCHOR0,a3
	//new stack pointer
	mv sp, a0
	mv a0, a2
	jr a1
	ret
	

.globl stop_gthread_asm
.type stop_gthread_asm,@function
stop_gthread_asm:
	ld sp,.LANCHOR0	
	ld s0,    0(sp)
	ld s1,    8(sp)
	ld s2,    16(sp)
	ld s3,    24(sp)
	ld s4,    32(sp)
	ld s5,    40(sp)
	ld s6,    48(sp)
	ld s7,    56(sp)
	ld s8,    64(sp)
	ld s9,    72(sp)
	ld s10,   80(sp)
	ld s11,   88(sp)
	ld ra,    96(sp)
	add sp, sp, 112	
	ret
	
.globl switch_gthread_asm
.type switch_gthread_asm,@function
//a0 - first argument (current threads state)
//a1 - second argument (next threads state)	
switch_gthread_asm:
	sd sp,	  0(a0)
	sd s0,    8(a0)
	sd s1,    16(a0)
	sd s2,    24(a0)
	sd s3,    32(a0)
	sd s4,    40(a0)
	sd s5,    48(a0)
	sd s6,    56(a0)
	sd s7,    64(a0)
	sd s8,    72(a0)
	sd s9,    80(a0)
	sd s10,   88(a0)
	sd s11,   96(a0)
	sd ra,    104(a0)
	ld sp,	  0(a1)
	ld s0,    8(a1)
	ld s1,    16(a1)
	ld s2,    24(a1)
	ld s3,    32(a1)
	ld s4,    40(a1)
	ld s5,    48(a1)
	ld s6,    56(a1)
	ld s7,    64(a1)
	ld s8,    72(a1)
	ld s9,    80(a1)
	ld s10,   88(a1)
	ld s11,   96(a1)
	ld ra,    104(a1)
	ret

.globl switch_and_start_gthread_asm
.type switch_and_start_gthread_asm,@function
//a0 - first argument (current threads state)
//a1 - second argument (new stack)
//a2 - third argument (new function pointer)
//a3 - fourth argumnet (new thread argument)	
switch_and_start_gthread_asm:
	sd sp,	  0(a0)
	sd s0,    8(a0)
	sd s1,    16(a0)
	sd s2,    24(a0)
	sd s3,    32(a0)
	sd s4,    40(a0)
	sd s5,    48(a0)
	sd s6,    56(a0)
	sd s7,    64(a0)
	sd s8,    72(a0)
	sd s9,    80(a0)
	sd s10,   88(a0)
	sd s11,   96(a0)
	sd ra,    104(a0)
	mv sp, a1
	mv a0, a3
	jalr a2
	ret
.end
