/*
 * mov_avg.s
 *
 * Created on: 2/2/2026
 * Author: Hitesh B, Hou Linxin
 */
.syntax unified
 .cpu cortex-m4
 .thumb
 .global mov_avg
 .equ N_MAX, 8
 .bss
 .align 4

 .text
 .align 2
@ CG2028 Assignment, Sem 2, AY 2025/26
@ (c) ECE NUS, 2025
@ Write Student 1’s Name here: ABCD (A1234567R)
@ Write Student 2’s Name here: WXYZ (A0000007X)
@ You could create a look-up table of registers here:
@ R0 ...
@ R1 ...
@ write your program from here:
mov_avg:
 PUSH {r4-r7, lr}

 	mov  r4, r0          // r4 = N
    mov  r5, r1          // r5 = accel_buff pointer
    mov r6, #0          // r6 = sum
    mov r7, #0          // r7 = i

    // if (N <= 0) return 0;
    cmp  r4, #0
    ble  ret_zero

 loop:
    cmp  r7, r4
    bge  done_sum

    ldr  r3, [r5], #4  // post index addressing so r5 points to the next number
    add  r6, r6, r3        // sum += value
    add r7, r7, #1    // i++
    b    loop

done_sum:
    sdiv r0, r6, r4             // r0 = sum / N
    POP  {r4-r7, pc}

ret_zero:
    movs r0, #0
    POP  {r4-r7, pc}
