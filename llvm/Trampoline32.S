.text
.globl trampolineForHelper32_0
.type  trampolineForHelper32_0, @function
trampolineForHelper32_0:
    subl $12, %esp
    call *16(%esp)
    movl 20(%esp), %ecx
    movl %eax, (%ecx)
    movl %edx, 4(%ecx)
    add $12, %esp
    ret
.size trampolineForHelper32_0, .-trampolineForHelper32_0

.globl trampolineForHelper32_1
.type  trampolineForHelper32_1, @function
trampolineForHelper32_1:
    subl $12, %esp
    movl 16+8(%esp), %eax
    movl %eax, (%esp)
    call *16(%esp)
    movl 20(%esp), %ecx
    movl %eax, (%ecx)
    movl %edx, 4(%ecx)
    add $12, %esp
    ret
.size trampolineForHelper32_1, .-trampolineForHelper32_1

.globl trampolineForHelper32_2
.type  trampolineForHelper32_2, @function
trampolineForHelper32_2:
    subl $12, %esp
    movl 16+8(%esp), %eax
    movl %eax, (%esp)
    movl 16+12(%esp), %eax
    movl %eax, 4(%esp)
    call *16(%esp)
    movl 20(%esp), %ecx
    movl %eax, (%ecx)
    movl %edx, 4(%ecx)
    add $12, %esp
    ret
.size trampolineForHelper32_2, .-trampolineForHelper32_2

.globl trampolineForHelper32_3
.type  trampolineForHelper32_3, @function
trampolineForHelper32_3:
    subl $12, %esp
    movl 16+8(%esp), %eax
    movl %eax, (%esp)
    movl 16+12(%esp), %eax
    movl %eax, 4(%esp)
    movl 16+16(%esp), %eax
    movl %eax, 8(%esp)
    call *16(%esp)
    movl 20(%esp), %ecx
    movl %eax, (%ecx)
    movl %edx, 4(%ecx)
    add $12, %esp
    ret
.size trampolineForHelper32_3, .-trampolineForHelper32_3

.globl trampolineForHelper32_4
.type  trampolineForHelper32_4, @function
trampolineForHelper32_4:
    subl $28, %esp
    movl 32+8(%esp), %eax
    movl %eax, (%esp)
    movl 32+12(%esp), %eax
    movl %eax, 4(%esp)
    movl 32+16(%esp), %eax
    movl %eax, 8(%esp)
    movl 32+20(%esp), %eax
    movl %eax, 12(%esp)
    call *32(%esp)
    movl 32 + 4(%esp), %ecx
    movl %eax, (%ecx)
    movl %edx, 4(%ecx)
    add $28, %esp
    ret
.size trampolineForHelper32_4, .-trampolineForHelper32_4

.globl trampolineForHelper32_5
.type  trampolineForHelper32_5, @function
trampolineForHelper32_5:
    subl $28, %esp
    movl 32+8(%esp), %eax
    movl %eax, (%esp)
    movl 32+12(%esp), %eax
    movl %eax, 4(%esp)
    movl 32+16(%esp), %eax
    movl %eax, 8(%esp)
    movl 32+20(%esp), %eax
    movl %eax, 12(%esp)
    movl 32+24(%esp), %eax
    movl %eax, 16(%esp)
    call *32(%esp)
    movl 32 + 4(%esp), %ecx
    movl %eax, (%ecx)
    movl %edx, 4(%ecx)
    add $28, %esp
    ret
.size trampolineForHelper32_5, .-trampolineForHelper32_5
/* Let the linker know we don't need an executable stack */
.section .note.GNU-stack,"",@progbits

/*--------------------------------------------------------------------*/
/*--- end                                                          ---*/
/*--------------------------------------------------------------------*/

