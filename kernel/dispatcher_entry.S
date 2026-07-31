/* SPDX-License-Identifier: GPL-2.0 */

#include <linux/linkage.h>

#include <asm/frame.h>
#include <asm/ibt.h>
#include <asm/nospec-branch.h>

/*
 * Entry raggiunta al posto di x64_sys_call().
 *
 * All'ingresso:
 *
 *   RDI = puntatore ai pt_regs della syscall
 *   ESI = numero della syscall x86-64
 *
 * Sullo stack è ancora presente il return address originale
 * della chiamata a x64_sys_call().
 */
	.text

SYM_FUNC_START(syscall_throttle_dispatch_entry)
	ENDBR

	/*
	 * La funzione chiama un helper C, quindi objtool
	 * richiede un frame pointer valido.
	 */
	FRAME_BEGIN

	/*
	 * Conserva syscall_regs in un registro callee-saved.
	 */
	pushq	%r12
	movq	%rdi, %r12

	/*
	 * Dopo FRAME_BEGIN e pushq %r12 lo stack non è
	 * correttamente allineato per una call C.
	 *
	 * Gli ulteriori 8 byte ripristinano l'allineamento
	 * a 16 byte prima della call.
	 */
	subq	$8, %rsp

	call	syscall_throttle_dispatch_prepare

	addq	$8, %rsp

	/*
	 * Ripristina il primo argomento della syscall originale.
	 * RAX contiene l'indirizzo della syscall oppure un errore.
	 */
	movq	%r12, %rdi
	popq	%r12

	/*
	 * Prima del RET o del sibling jump lo stack e il frame
	 * pointer devono essere identici a quelli d'ingresso.
	 */
	FRAME_END

	/*
	 * Gli errori kernel sono nell'intervallo [-4095, -1].
	 */
	cmpq	$-4095, %rax
	jae	.Lreturn_error

	/*
	 * Salto definitivo alla syscall originale.
	 * Non viene inserito un return address nel modulo.
	 */
	JMP_NOSPEC rax

.Lreturn_error:
	/*
	 * RAX contiene già -ERESTARTSYS o -ENOSYS.
	 * Il return usa il return address originale del kernel.
	 */
	RET

SYM_FUNC_END(syscall_throttle_dispatch_entry)

	.section .note.GNU-stack, "", @progbits
