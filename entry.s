; entry.s - switch to protected mode and jump to kernel

[BITS 16]
global start

start:
    cli                 ; disable interrupts

    ; =====================
    ; Setup GDT
    ; =====================
    lgdt [gdt_descriptor]


    ; =====================
    ; Enter protected mode
    ; =====================
    mov eax, cr0
    or eax, 1           ; set PE bit
    mov cr0, eax

    ; Far jump to flush prefetch + load CS
    jmp 0x08:pm_start

gdt_start:
    ; Null descriptor
    dq 0

    ; Code segment: base=0, limit=4GB, exec/read, 32-bit
    dw 0xFFFF           ; limit low
    dw 0                 ; base low
    db 0                 ; base middle
    db 10011010b         ; access byte: present, ring0, code segment, executable, readable
    db 11001111b         ; flags: granularity, 32-bit
    db 0                 ; base high

    ; Data segment: base=0, limit=4GB, read/write
    dd 0x0000FFFF
    db 0
    db 10010010b         ; access: present, ring0, data, writable
    db 11001111b         ; flags: granularity, 32-bit
    db 0

gdt_descriptor:
    dw gdt_descriptor - gdt_start - 1   ; limit
    dd gdt_start                 ; base

[BITS 32]
extern kentry
pm_start:
    ; Setup segment registers
    ; Enable A20 line (simplest version, can skip for emulators)
    in al, 0x92
    or al, 2
    out 0x92, al
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x7C00    ; setup stack somewhere safe

    ; Jump to kernel entry
    call kentry

.hang:
    hlt
    jmp .hang

; =====================
; GDT (flat segments)
; =====================
