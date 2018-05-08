	.text
	.file	"multiply.bc"
	.globl	mult
	.p2align	4, 0x90
	.type	mult,@function
mult:                                   # @mult
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
	movl	$5, -4(%rbp)
	movl	$3, -8(%rbp)
	movl	-4(%rbp), %eax
	imull	-8(%rbp), %eax
	movl	%eax, -12(%rbp)
	movl	-12(%rbp), %eax
	popq	%rbp
	retq
.Lfunc_end0:
	.size	mult, .Lfunc_end0-mult
	.cfi_endproc


	.ident	"clang version 4.0.1-6 (tags/RELEASE_401/final)"
	.section	".note.GNU-stack","",@progbits
