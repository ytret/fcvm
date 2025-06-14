.set PRINT_DEV_CLASS, 0x01
.set BUS_MMIO_START, 0xF0000000

.origin 0
.strd start

.origin 0x400
start:
        mov     r1, BUS_MMIO_START
        ldr     r2, [r1]                ; r1 = slot status register
        tst     r2, 2
        jeqr    halt                    ; halt if slot 1 is unused
        ldr     r2, [r1+16]             ; r2 = slot 1 MMIO start
        mov     r7, 24
        ldr     r3, [r1+r7]             ; r3 = slot 1 IRQ + device class
        shr     r3, 8
        and     r3, 0xFF                ; r3 = slot 1 device class
        mov     r4, PRINT_DEV_CLASS     ; r4 = PRINT_DEV_CLASS
        cmp     r3, r4                  ; slot device class = PRINT_DEV_CLASS?
        jeqr    print_str
        jmpr    halt

print_str:
        mov     r4, d_str
        mov     r5, d_str_end
        sub     r5, r4                  ; r5 - size of the 'd_str' string
        mov     r7, r2
        add     r7, 4                   ; r7 - outbuf start

copy_str:
        ;; Loop until r5 is zero, r5 is assumed to be a multiple of 4.
        ;; - Copy byte [r4] to [r7].
        ;; - Add 1 to r4 and r7.
        ;; - Subtract 1 from r5.
        ldr     rb6, [r4]
        str     [r7], rb6

        add     r4, 1
        add     r7, 1

        sub     r5, 1
        jner    copy_str

flush:
        mov     r5, 1
        str     [r2], r5                ; print_dev.flush = 1

halt:
        halt
        jmpr    halt

d_str: .strs "Hello, World!"
d_str_end:
