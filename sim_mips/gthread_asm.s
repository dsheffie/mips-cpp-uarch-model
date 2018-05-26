.data
saved_sp:
	.long 0x0
	
.text
.globl start_gthread_asm
.type start_gthread_asm,@function

	//x0 - first argument (new stack)
	//x1 - second argument (function pointer)
	//x2 - third argument (argument
start_gthread_asm:
	//save initial state
	sub sp, sp, #(8*12)
	str x19, [sp, #(8*0)]
	str x20, [sp, #(8*1)]
	str x21, [sp, #(8*2)]
	str x22, [sp, #(8*3)]	
	str x23, [sp, #(8*4)]
	str x24, [sp, #(8*5)]
	str x25, [sp, #(8*6)]
	str x26, [sp, #(8*7)]
	str x27, [sp, #(8*8)]	
	str x28, [sp, #(8*9)]
	str x29, [sp, #(8*10)]
	str x30, [sp, #(8*11)]
	adrp	x9, saved_sp
	mov x10, sp
	str	x10, [x9, #:lo12:saved_sp]
	mov sp, x0
	mov x0, x2
	br x1
	ret


.globl stop_gthread_asm
.type stop_gthread_asm,@function
stop_gthread_asm:
	adrp	x9, saved_sp
	ldr x10, [x9, #:lo12:saved_sp]
	mov sp, x10
	ldr x19, [sp, #(8*0)]
	ldr x20, [sp, #(8*1)]
	ldr x21, [sp, #(8*2)]
	ldr x22, [sp, #(8*3)]	
	ldr x23, [sp, #(8*4)]
	ldr x24, [sp, #(8*5)]
	ldr x25, [sp, #(8*6)]
	ldr x26, [sp, #(8*7)]
	ldr x27, [sp, #(8*8)]	
	ldr x28, [sp, #(8*9)]
	ldr x29, [sp, #(8*10)]
	ldr x30, [sp, #(8*11)]
	add sp, sp, #(8*12)
	ret
	
.globl switch_gthread_asm
.type switch_gthread_asm,@function
//x0 - first argument (current threads state)
//x1 - second argument (next threads state)	
switch_gthread_asm:
	mov x10, sp
	str x10, [x0, #(8*0)]	
	str x19, [x0, #(8*1)]
	str x20, [x0, #(8*2)]
	str x21, [x0, #(8*3)]
	str x22, [x0, #(8*4)]	
	str x23, [x0, #(8*5)]
	str x24, [x0, #(8*6)]
	str x25, [x0, #(8*7)]
	str x26, [x0, #(8*8)]
	str x27, [x0, #(8*9)]	
	str x28, [x0, #(8*10)]
	str x29, [x0, #(8*11)]
	str x30, [x0, #(8*12)]
	
	ldr x10, [x1, #(8*0)]	
	ldr x19, [x1, #(8*1)]
	ldr x20, [x1, #(8*2)]
	ldr x21, [x1, #(8*3)]
	ldr x22, [x1, #(8*4)]	
	ldr x23, [x1, #(8*5)]
	ldr x24, [x1, #(8*6)]
	ldr x25, [x1, #(8*7)]
	ldr x26, [x1, #(8*8)]
	ldr x27, [x1, #(8*9)]	
	ldr x28, [x1, #(8*10)]
	ldr x29, [x1, #(8*11)]
	ldr x30, [x1, #(8*12)]
	mov sp, x10
	ret

.globl switch_and_start_gthread_asm
.type switch_and_start_gthread_asm,@function
//x0 - first argument (current threads state)
//x1 - second argument (new stack)
//x2 - third argument (new function pointer)
//x3 - fourth argumnet (new thread argument)	
switch_and_start_gthread_asm:
	mov x10, sp
	str x10, [x0, #(8*0)]	
	str x19, [x0, #(8*1)]
	str x20, [x0, #(8*2)]
	str x21, [x0, #(8*3)]
	str x22, [x0, #(8*4)]	
	str x23, [x0, #(8*5)]
	str x24, [x0, #(8*6)]
	str x25, [x0, #(8*7)]
	str x26, [x0, #(8*8)]
	str x27, [x0, #(8*9)]	
	str x28, [x0, #(8*10)]
	str x29, [x0, #(8*11)]
	str x30, [x0, #(8*12)]

	mov sp, x1
	mov x0, x3
	br x2
	ret
	
.end
