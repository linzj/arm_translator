.text
.globl trampolineForHelper32Ret64_0
.type  trampolineForHelper32Ret64_0, @function
trampolineForHelper32Ret64_0:
    subl $12, %esp
    call *16(%esp)
    movl 20(%esp), %ecx
    movl %eax, (%ecx)
    movl %edx, 4(%ecx)
    add $12, %esp
    ret
.size trampolineForHelper32Ret64_0, .-trampolineForHelper32Ret64_0

.globl trampolineForHelper32Ret64_1
.type  trampolineForHelper32Ret64_1, @function
trampolineForHelper32Ret64_1:
    subl $12, %esp
    movl 16+8(%esp), %eax
    movl %eax, (%esp)
    call *16(%esp)
    movl 20(%esp), %ecx
    movl %eax, (%ecx)
    movl %edx, 4(%ecx)
    add $12, %esp
    ret
.size trampolineForHelper32Ret64_1, .-trampolineForHelper32Ret64_1

.globl trampolineForHelper32Ret64_2
.type  trampolineForHelper32Ret64_2, @function
trampolineForHelper32Ret64_2:
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
.size trampolineForHelper32Ret64_2, .-trampolineForHelper32Ret64_2

.globl trampolineForHelper32Ret64_3
.type  trampolineForHelper32Ret64_3, @function
trampolineForHelper32Ret64_3:
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
.size trampolineForHelper32Ret64_3, .-trampolineForHelper32Ret64_3

.globl trampolineForHelper32Ret64_4
.type  trampolineForHelper32Ret64_4, @function
trampolineForHelper32Ret64_4:
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
.size trampolineForHelper32Ret64_4, .-trampolineForHelper32Ret64_4

.globl trampolineForHelper32Ret64_5
.type  trampolineForHelper32Ret64_5, @function
trampolineForHelper32Ret64_5:
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
.size trampolineForHelper32Ret64_5, .-trampolineForHelper32Ret64_5

.text
.globl trampolineForHelper32Ret32_0
.type  trampolineForHelper32Ret32_0, @function
trampolineForHelper32Ret32_0:
    subl $12, %esp
    call *16(%esp)
    movl 20(%esp), %ecx
    movl %eax, (%ecx)
    add $12, %esp
    ret
.size trampolineForHelper32Ret32_0, .-trampolineForHelper32Ret32_0

.globl trampolineForHelper32Ret32_1
.type  trampolineForHelper32Ret32_1, @function
trampolineForHelper32Ret32_1:
    subl $12, %esp
    movl 16+8(%esp), %eax
    movl %eax, (%esp)
    call *16(%esp)
    movl 20(%esp), %ecx
    movl %eax, (%ecx)
    add $12, %esp
    ret
.size trampolineForHelper32Ret32_1, .-trampolineForHelper32Ret32_1

.globl trampolineForHelper32Ret32_2
.type  trampolineForHelper32Ret32_2, @function
trampolineForHelper32Ret32_2:
    subl $12, %esp
    movl 16+8(%esp), %eax
    movl %eax, (%esp)
    movl 16+12(%esp), %eax
    movl %eax, 4(%esp)
    call *16(%esp)
    movl 20(%esp), %ecx
    movl %eax, (%ecx)
    add $12, %esp
    ret
.size trampolineForHelper32Ret32_2, .-trampolineForHelper32Ret32_2

.globl trampolineForHelper32Ret32_3
.type  trampolineForHelper32Ret32_3, @function
trampolineForHelper32Ret32_3:
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
    add $12, %esp
    ret
.size trampolineForHelper32Ret32_3, .-trampolineForHelper32Ret32_3

.globl trampolineForHelper32Ret32_4
.type  trampolineForHelper32Ret32_4, @function
trampolineForHelper32Ret32_4:
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
    add $28, %esp
    ret
.size trampolineForHelper32Ret32_4, .-trampolineForHelper32Ret32_4

.globl trampolineForHelper32Ret32_5
.type  trampolineForHelper32Ret32_5, @function
trampolineForHelper32Ret32_5:
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
    add $28, %esp
    ret
.size trampolineForHelper32Ret32_5, .-trampolineForHelper32Ret32_5

.globl trampolineForHelper32RetNone
.type  trampolineForHelper32RetNone, @function
trampolineForHelper32RetNone:
    addl $16, %esp
    jmp *-4(%esp)
.size trampolineForHelper32RetNone, .-trampolineForHelper32RetNone
/* Let the linker know we don't need an executable stack */
.section .note.GNU-stack,"",@progbits

/*--------------------------------------------------------------------*/
/*--- end                                                          ---*/
/*--------------------------------------------------------------------*/

