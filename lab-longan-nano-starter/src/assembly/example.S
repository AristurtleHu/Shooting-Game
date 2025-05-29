.section .data
	choice0: .asciz "simple"
	choice1: .asciz "medium"
	choice2: .asciz "hard"
	.globl choice
	choice: .word 0

.section .text
.globl start
.type start, @function

start:
	la s0, choice
	lw s0, 0(s0)
	li s1, 1 // the loop index
	addi sp,sp,-4
	sw ra,0(sp)

loop:
	li a0, 4 // JOY_CTR
	jal Get_Button
	bnez a0, end

	li a0, 1 // JOY_DOWN
	jal Get_Button
	beqz a0, no_add
	addi s0, s0, 1
	li t0, 3
	beq s0, t0, wrap_zero
	j continue
	wrap_zero:
		li s0, 0
	continue:

no_add:

	li a0, 3 // JOY_UP
	jal Get_Button
	beqz a0, no_sub
	addi s0, s0, -1
	li t0, -1
	beq s0, t0, wrap_two
	j continue_1
	wrap_two:
		li s0, 2
	continue_1:

no_sub:
	li a0,50
	li a1,15
	la a2,choice0
	li a3,65535
	li t0, 0
	li t1, 8000
	bne s0, t0, white_a3_0
	mv a3, t1
	white_a3_0:
	jal LCD_ShowString

	li a0,50
	li a1,35
	la a2,choice1
	li a3,65535
	li t0, 1
	li t1, 8000
	bne s0, t0, white_a3_1
	mv a3, t1
	white_a3_1:
	jal LCD_ShowString

	li a0,50
	li a1,55
	la a2,choice2
	li a3,65535
	li t0, 2
	li t1, 8000
	bne s0, t0, white_a3_2
	mv a3, t1
	white_a3_2:
	jal LCD_ShowString

	li t0, 5200000
delay:
	addi t0, t0, -1
	bnez t0, delay

	bnez s1, loop

end:
	lcd_clear:
	mv a0, x0
	jal LCD_Clear

	lw ra,0(sp)
	addi sp,sp,4
	la t0, choice
	sw s0, 0(t0)
	lw a0, 0(t0)
	ret
