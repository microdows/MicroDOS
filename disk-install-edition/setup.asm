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
msg_subtitle_install db 'Installation de MicroDOS sur le Disque Dur', 0
msg_copying db 'Copie des Fichiers en cours...', 0
msg_install_ok db 'Installation Terminee !', 0
msg_reboot_1 db 'Enlever toute les disquettes du lecteur et', 0
msg_reboot_2 db 'appuyer sur une touche pour redemarrer', 0
msg_install_err db 'Erreur lors de l installation.', 0

saved_part_lba dd 0
fatname      times 11 db 0
src_size     dw 0
cluster_temp dw 0
free_temp    dw 0
current_cluster dw 0
path_filename dw 0
saved_cluster dw 0

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
%include "fat12.inc"         ; <-- AJOUTÉ