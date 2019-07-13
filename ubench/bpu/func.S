.text
.align	2
.globl	func
.set nomips16
.ent func
.type	func, @function
.set	noreorder
.set	nomacro
.set    noat
func:
	//save starting value
	ori $8, $4, 0
	//loop counter
	ori $9, $6, 0
	xor $10,$10,$10
	xor $12,$12,$12
	//the loop
	
.L1:	
	sll	$2,$4,13
	xor	$4,$2,$4
 	srl	$2,$4,17
	xor	$4,$2,$4
	sll	$2,$4,5
	xor	$4,$2,$4

	andi $11,$4,1
	beq $11,$0,.L4
	div     $0, $10, $5
	addiu $12,$12,1	
.L4:	
	mfhi    $11
	//reset when we hit limit
	movz    $4,$8,$11
	
	addiu $10,$10,1	
	bnezl $9, .L1
	addiu $9,$9,-1
	//all done
	j	$31
	//ori     $2,$4,0
	ori $2,$12,0

.set	reorder
.set	macro
.end    func
