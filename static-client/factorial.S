#
# a0-a2 - parameters to linux function services
# a7 - linux function number
#

.global _start
.type _start, @function
.global fac
.type fac, @function

# Setup the parameters to print hello world
# and then call Linux to do it.

_start:
        li a0, 4
        call fac

        addi    a7, x0, 93  # Service command code 93 terminates
        ecall               # Call linux to terminate the program

fac:
        mv      a5,a0
        li      a0,1
        li      a3,1
        ble     a5,a0,.L5
.L2:
        mv      a4,a5
        addiw   a5,a5,-1
        mulw    a0,a4,a0
        bne     a5,a3,.L2
.L5:
        ret
