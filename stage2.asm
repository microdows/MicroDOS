[BITS 16]
[ORG 0x0000]

start:
    push cs
    pop ds
    push cs
    pop es

    mov si, msg_welcome
    call print

main_loop:
    mov si, msg_prompt
    call print

    mov di, buffer
    call read_line

    mov si, buffer
    mov di, cmd_help
    call strcmp
    jc do_help

    mov si, buffer
    mov di, cmd_dir
    call strcmp
    jc do_dir

    mov si, buffer
    mov di, cmd_ver
    call strcmp
    jc do_ver

    mov si, buffer
    mov di, cmd_cls
    call strcmp
    jc do_cls

    mov si, buffer
    mov di, cmd_rebootdos
    call strcmp
    jc do_rebootdos

    mov si, buffer
    mov di, cmd_reboot
    call strcmp
    jc do_reboot

    mov si, msg_unknow
    call print
    jmp main_loop

do_help:
    mov si, msg_help
    call print
    jmp main_loop

do_cls:
    mov ax, 0x0003
    int 0x10
    jmp main_loop

do_ver:
    mov si, msg_ver
    call print
    jmp main_loop

do_reboot:
    mov si, msg_reboot
    call print
    mov al, 0xFE
    out 0x64, al
    jmp main_loop

do_rebootdos:
    mov si, msg_rebootdos
    call print
    int 0x19
    jmp main_loop

do_dir:
    mov ax, 0x1000
    mov es, ax
    mov bx, 0x0800
    mov ah, 0x02
    mov al, 14
    mov ch, 0
    mov cl, 6
    mov dh, 1
    mov dl, 0
    int 0x13
    jc .error

    mov si, 0x0800
    mov bp, 224

.loop:
    mov al, [si]
    cmp al, 0
    je .done
    cmp al, 0xE5
    je .next

    mov al, [si+11]
    test al, 0x08
    jnz .next
    test al, 0x10
    jnz .next

    mov di, si
    mov cx, 8
.print_name:
    mov al, [di]
    cmp al, ' '
    je .name_done
    mov ah, 0x0E
    int 0x10
    inc di
    loop .print_name
.name_done:

    mov al, [si+8]
    cmp al, ' '
    je .no_ext
    mov ah, 0x0E
    mov al, '.'
    int 0x10
    mov di, si
    add di, 8
    mov cx, 3
.print_ext:
    mov al, [di]
    cmp al, ' '
    je .ext_done
    mov ah, 0x0E
    int 0x10
    inc di
    loop .print_ext
.ext_done:
.no_ext:

    mov ah, 0x0E
    mov al, 13
    int 0x10
    mov al, 10
    int 0x10

.next:
    add si, 32
    dec bp
    jnz .loop

.done:
    jmp main_loop

.error:
    mov si, msg_dir_err
    call print
    jmp main_loop

print:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp print
.done:
    ret

read_line:
    mov cx, 0
.next:
    mov ah, 0x00
    int 0x16
    cmp al, 13
    je .done
    cmp al, 8
    je .backspace
    cmp cx, 254
    jae .next
    stosb
    inc cx
    mov ah, 0x0E
    int 0x10
    jmp .next
.backspace:
    test cx, cx
    jz .next
    dec di
    dec cx
    mov ah, 0x0E
    mov al, 8
    int 0x10
    mov al, ' '
    int 0x10
    mov al, 8
    int 0x10
    jmp .next
.done:
    xor al, al
    stosb
    mov ah, 0x0E
    mov al, 13
    int 0x10
    mov al, 10
    int 0x10
    ret

strcmp:
    push si
    push di
.loop:
    mov al, [si]
    mov bl, [di]
    cmp al, bl
    jne .ne
    test al, al
    jz .eq
    inc si
    inc di
    jmp .loop
.ne:
    pop di
    pop si
    clc
    ret
.eq:
    pop di
    pop si
    stc
    ret

msg_welcome db 'MicroDOS v0.3 - Shell', 13, 10
            db 'tapez "help" pour la liste de commandes.', 13, 10, 10, 0
msg_prompt  db '> ', 0
msg_help    db 'Liste des Commandes', 13, 10
            db ' help - cette aide', 13, 10
            db ' dir - liste les fichiers', 13, 10
            db ' cls - efface l ecran', 13, 10
            db ' ver - version', 13, 10
            db ' reboot - redemarre le PC', 13, 10
            db ' rebootdos - redemarre MicroDOS', 13, 10, 13, 10, 0
msg_unknow  db 'Commande inconnue. Tapez "help".', 13, 10, 0
msg_ver     db 'MicroDOS v0.3', 13, 10, 0
msg_reboot  db 'Redemarrage en cours...', 13, 10, 0
msg_rebootdos db 'Redemarrage de MicroDOS...', 13, 10, 0
msg_dir_err db 'Erreur lecture repertoire.', 13, 10, 0

cmd_help     db 'help', 0
cmd_dir      db 'dir', 0
cmd_cls      db 'cls', 0
cmd_ver      db 'ver', 0
cmd_reboot   db 'reboot', 0
cmd_rebootdos db 'rebootdos', 0

buffer times 256 db 0
