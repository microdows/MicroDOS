[BITS 16]
[ORG 0x0000]

start:
    push cs
    pop ds
    push cs
    pop es

    mov word [current_cluster], 0

    ;---initialisation du hdd---
    mov eax, 0
    mov cx, 1
    mov bx, 0x7000
    call lba_read          ; <-- IL MANQUAIT CECI
    jc .no_hdd
    mov bx, 0x7000
    call mbr_find_partition
    jc .no_hdd
    mov [saved_part_lba], eax

    mov cx, 1
    mov bx, 0x7200
    call lba_read
    jc .no_hdd
    mov bx, 0x7200
    call bpb16_parse
    mov eax, [saved_part_lba]
    call fat16_init
.no_hdd:

    mov ax, 0x0003
    int 0x10
    call draw_ui

.wait_key:
    mov ah, 0x00
    int 0x16
    cmp al, '1'
    je .option1
    cmp al, '2'
    je .option2
    cmp al, '3'
    je .option3
    jmp .wait_key

.option1:
    mov ax, 0x0003
    int 0x10

    mov dh, 2
    mov dl, 21
    mov si, title_txt
    call print_at

    mov dh, 4
    mov dl, 18
    mov si, msg_subtitle_install
    call print_at

    mov dh, 12
    mov dl, 26
    mov si, msg_copying
    call print_at

    mov si, kernel_name
    call read_file      ; <-- read_file fait tout le boulot
    cmp ax, 0xFFFF
    je .install_error

    mov si, kernel_name
    call build_fat_name
    mov bx, [src_size]
    call fat16_create_file
    jc .install_error

    mov bx, [src_size]
    call fat16_write_file
    jc .install_error

    call hide_kernel_file
    jc .install_error

    mov word [current_cluster], 0
    mov si, games_dir_name
    call build_fat_name
    call fat16_create_dir
    jc .install_error
    mov ax, [fat16_new_dir_cluster]
    mov [games_cluster], ax

    mov si, tetrix_name
    call .install_game
    jc .install_error
    mov si, pcman_v1_name
    call .install_game
    jc .install_error
    mov si, pcman_v2_name
    call .install_game
    jc .install_error
    mov si, pong_name
    call .install_game
    jc .install_error

        ; 4. Lire BOOT_HDD.BIN depuis la disquette
    mov word [current_cluster], 0
    mov si, boot_hdd_name
    call read_file
    cmp ax, 0xFFFF
    je .install_error

    ; 5. Écrire BOOT_HDD.BIN au LBA 2048
    mov ax, 0x1000
    mov es, ax
    mov bx, 0xA000
    mov eax, 2048
    mov cx, 1
    call lba_write
    jc .install_error

    ; 6. Lire MBR_HDD.BIN depuis la disquette
    mov word [current_cluster], 0
    mov si, mbr_hdd_name
    call read_file
    cmp ax, 0xFFFF
    je .install_error

    ; 7. Écrire MBR_HDD.BIN au LBA 0
    mov ax, 0x1000
    mov es, ax
    mov bx, 0xA000
    xor eax, eax        ; LBA 0
    mov cx, 1
    call lba_write
    jc .install_error

    mov ax, 0x0003
    int 0x10
    mov dh, 2
    mov dl, 21
    mov si, title_txt
    call print_at
    mov dh, 4
    mov dl, 18
    mov si, msg_subtitle_install
    call print_at
    mov dh, 12
    mov dl, 29
    mov si, msg_install_ok
    call print_at
    mov dh, 16
    mov dl, 20
    mov si, msg_reboot_1
    call print_at
    mov dh, 17
    mov dl, 20
    mov si, msg_reboot_2
    call print_at

    mov ah, 0x00
    int 0x16
    mov ah, 0x00
    int 0x19
    jmp $

.install_error:
    mov ax, 0x0003
    int 0x10
    mov dh, 12
    mov dl, 25
    mov si, msg_install_err
    call print_at
    mov ah, 0x00
    int 0x16
    mov ax, 0x0003
    int 0x10
    call draw_ui
    jmp near .wait_key

.install_game:
    push si
    mov word [current_cluster], 0
    call read_file
    cmp ax, 0xFFFF
    je .game_error

    mov ax, [games_cluster]
    mov [current_cluster], ax
    pop si
    call build_fat_name
    mov bx, [src_size]
    call fat16_create_file
    jc .game_error_no_stack

    mov bx, [src_size]
    call fat16_write_file
    jc .game_error_no_stack
    ret
.game_error:
    pop si
.game_error_no_stack:
    stc
    ret

.option2:
    mov ax, 0x0003
    int 0x10
    mov dh, 10
    mov dl, 28
    mov si, msg_formatting
    call print_at
    call format_hdd
    jc .format_error

    mov ax, 0x0003
    int 0x10
    mov dh, 12
    mov dl, 28
    mov si, msg_format_ok
    call print_at
    mov dh, 14
    mov dl, 22
    mov si, msg_press_key
    call print_at
    mov ah, 0x00
    int 0x16                  ; <-- J'ai supprimé le double int 0x16
    mov ax, 0x0003
    int 0x10
    call draw_ui
    jmp near .wait_key

.format_error:
    mov ax, 0x0003
    int 0x10
    mov dh, 12
    mov dl, 28
    mov si, msg_format_err
    call print_at
    mov ah, 0x00
    int 0x16
    mov ax, 0x0003
    int 0x10
    call draw_ui
    jmp near .wait_key

.option3:
    mov ah, 0x00
    int 0x19
    jmp $

;------------------------affichage-------------------
draw_ui:
    mov dh, 2
    mov dl, 21
    mov si, title_txt
    call print_at
    mov dh, 4
    mov dl, 30
    mov si, subtitle_txt
    call print_at
    mov dh, 8
    mov dl, 28
    mov si, opt1_txt
    call print_at
    mov dh, 10
    mov dl, 26
    mov si, opt2_txt
    call print_at
    mov dh, 12
    mov dl, 26
    mov si, opt3_txt
    call print_at
    ret

print_at:
    mov ah, 0x02
    mov bh, 0
    int 0x10
.loop:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp .loop
.done:
    ret

;------------------------donnees-------------------
title_txt    db 'MicroDOS v1.0.0 - Disk Install Edition', 0
subtitle_txt db 'Selection des choix', 0
opt1_txt     db '1.   Installer MicroDOS', 0
opt2_txt     db '2.   Formater le Disque Dur', 0
opt3_txt     db "3.   Redemarrer l'ordinateur", 0
msg_formatting db 'Formatage en cours...', 0
msg_format_ok db 'Disque Formate avec succes', 0
msg_format_err db 'Erreur de Formatage.', 0
msg_press_key db 'Appuyer sur une touche pour continuer', 0

kernel_name db 'KERNEL.BIN', 0
boot_hdd_name db 'BOOT_HDD.BIN', 0
mbr_hdd_name  db 'MBR_HDD.BIN', 0
msg_subtitle_install db 'Installation de MicroDOS sur le Disque Dur', 0
msg_copying db 'Copie des Fichiers en cours...', 0
msg_install_ok db 'Installation Terminee !', 0
msg_reboot_1 db 'Enlever toute les disquettes du lecteur et', 0
msg_reboot_2 db 'appuyer sur une touche pour redemarrer', 0
msg_install_err db 'Erreur lors de l installation.', 0

games_dir_name db 'JEUX', 0
tetrix_name db 'TETRIX.COM', 0
pcman_v1_name db 'PCMANV1.COM', 0
pcman_v2_name db 'PCMANV2.COM', 0
pong_name db 'PONG.COM', 0
games_cluster dw 0

saved_part_lba dd 0
fatname      times 11 db 0
src_size     dw 0
cluster_temp dw 0
free_temp    dw 0
current_cluster dw 0
path_filename dw 0
saved_cluster dw 0

kernel_hide_lba dd 0
kernel_hide_sectors dw 0
kernel_short_name db 'KERNEL  BIN'

hide_kernel_file:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push es

    mov eax, [lba_rootdir]
    mov [kernel_hide_lba], eax
    mov cx, [bpb_root_entries]
    shr cx, 4
    mov [kernel_hide_sectors], cx

.read_sector:
    mov eax, [kernel_hide_lba]
    mov cx, 1
    mov bx, 0x6000
    push cs
    pop es
    call lba_read
    jc .error

    mov si, 0x6000
    mov bp, 16
.entry_loop:
    cmp byte [si], 0
    je .not_found
    cmp byte [si], 0xE5
    je .next_entry

    push si
    mov di, kernel_short_name
    mov cx, 11
    repe cmpsb
    pop si
    je .found

.next_entry:
    add si, 32
    dec bp
    jnz .entry_loop

    inc dword [kernel_hide_lba]
    dec word [kernel_hide_sectors]
    jnz .read_sector
    jmp .not_found

.found:
    or byte [si+11], 0x07
    mov eax, [kernel_hide_lba]
    mov cx, 1
    mov bx, 0x6000
    push cs
    pop es
    call lba_write
    jmp .done

.not_found:
    stc
    jmp .done
.error:
    stc
.done:
    pop es
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

%include "lba.inc"
%include "lba_write.inc"
%include "mbr.inc"
%include "bpb16.inc"
%include "fat16_init.inc"
%include "fat16_get.inc"     ; <-- AJOUTÉ
%include "format.inc"
%include "fat16_set.inc"
%include "fat16_find_free.inc"
%include "fat16_write_file.inc"
%include "fat16_create_file.inc"
%include "fat16_create_dir.inc"
%include "fat12.inc"         ; <-- AJOUTÉ