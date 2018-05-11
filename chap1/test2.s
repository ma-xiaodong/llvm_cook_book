	.text
	.file	"test2.c"
	.globl	main
	.p2align	4, 0x90
	.type	main,@function
main:                                   # @main
	.cfi_startproc
# BB#0:
	pushq	%rbp
.Lcfi0:
	.cfi_def_cfa_offset 16
.Lcfi1:
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
.Lcfi2:
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movl	$0, -4(%rbp)
	movl	$5, -8(%rbp)
	movl	-8(%rbp), %edi
	movb	$0, %al
	callq	func
	movabsq	$.L.str, %rdi
	movl	%eax, -8(%rbp)
	movl	-8(%rbp), %esi
	movb	$0, %al
	callq	printf
	movl	-8(%rbp), %esi
	movl	%eax, -12(%rbp)         # 4-byte Spill
	movl	%esi, %eax
	addq	$16, %rsp
	popq	%rbp
	retq
.Lfunc_end0:
	.size	main, .Lfunc_end0-main
	.cfi_endproc

	.type	.L.str,@object          # @.str
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str:
	.asciz	"number is %d.\n"
	.size	.L.str, 15


	.ident	"clang version 4.0.1-6 (tags/RELEASE_401/final)"
	.section	".note.GNU-stack","",@progbits
