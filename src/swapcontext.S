#define CNAME(csym)		csym
#define ALIGN_TEXT	.p2align 4,0x90	/* 16-byte alignment, nop filled */
#define GEN_ENTRY(name)		ALIGN_TEXT; .globl CNAME(name); \
				.type CNAME(name),@function; CNAME(name):

#define ENTRY(name)		GEN_ENTRY(name) 
#define	END(name)		.size name, . - name


/**
 * Save the current register values onto one stack and load fresh register
 * values from another stack.
 * This method does not return to its caller immediately. It returns to the
 * caller when another thread on the same kernel thread invokes this method
 * with the current value of target as the saved parameter.
 *
 * \param %rdi
 *     Address of the stack location to load register values from.
 * \param %rsi
 *     Address of the stack location to save register values to.
 */

	
ENTRY(arachne_swapcontext)
        pushq %r15
        pushq %r14
        pushq %r13
	pushq %r12
        pushq %rbp
        pushq %rbx
        movq %rsp, (%rsi)


	.globl arachne_context_pushed
arachne_context_pushed:	
	movq (%rdi), %rsp
        popq %rbx
        popq %rbp
        popq %r12
        popq %r13
        popq %r14
        popq %r15
	retq
END(arachne_swapcontext)
