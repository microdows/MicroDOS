; ============================================================================
; disk_edition.inc - Ce que la Disk Install Edition ajoute a la Classic :
;   - FDISK  : menu pour creer / supprimer / activer des partitions
;   - FORMAT C: : cree un systeme de fichiers FAT16 (ou FAT12) sur C:
;   - SETUP  : l'installateur (menu au demarrage de la disquette)
; Inclus par kernel.asm quand on compile avec -DDISK_EDITION.
; ============================================================================

FD_MBR      equ XFER_BUF            ; copie du MBR pendant FDISK
FD_VBR      equ XFER_BUF+0x200      ; secteur de boot construit par FORMAT
FD_ZERO     equ XFER_BUF+0x400      ; secteur rempli de zeros

mbr_code:   incbin "mbr_hdd.bin", 0, 446
vbr_code:   incbin "boot_hdd.bin"

hd_total    dd 0                    ; secteurs du disque
hd_heads    dw 255
hd_spt      dw 63
hd_dap      db 0x10, 0
            dw 1
hd_dap_off  dw 0
hd_dap_seg  dw 0
hd_dap_lba  dd 0, 0
hd_params   times 0x1A db 0

; ---------------------------------------------------------------------------
; hd_read / hd_write : EAX = LBA absolu, ES:BX = tampon (disque 0x80)
; ---------------------------------------------------------------------------
hd_op       db 0x42
hd_read:
    mov byte [hd_op], 0x42
    jmp hd_io
hd_write:
    mov byte [hd_op], 0x43
hd_io:
    pushad
    mov [hd_dap_lba], eax
    mov [hd_dap_off], bx
    mov [hd_dap_seg], es
    mov di, 3
.try:
    mov ah, [hd_op]
    xor al, al
    mov dl, 0x80
    mov si, hd_dap
    push ds
    int 0x13
    pop ds
    jnc .ok
    xor ax, ax
    mov dl, 0x80
    int 0x13
    dec di
    jnz .try
    popad
    stc
    ret
.ok:
    popad
    clc
    ret

; hd_probe : y a-t-il un disque dur ? -> taille et geometrie. CF sinon.
hd_probe:
    pushad
    push es
    ; geometrie (pour les champs CHS de la table)
    mov ah, 0x08
    mov dl, 0x80
    xor di, di
    mov es, di
    push ds
    int 0x13
    pop ds
    jc .none
    movzx ax, dh
    inc ax
    mov [hd_heads], ax
    mov al, cl
    and ax, 63
    mov [hd_spt], ax
    ; taille exacte (fonction etendue 48h)
    mov word [hd_params], 0x1A
    mov ah, 0x48
    mov dl, 0x80
    mov si, hd_params
    push ds
    int 0x13
    pop ds
    jc .none
    mov eax, [hd_params+16]
    mov [hd_total], eax
    pop es
    popad
    clc
    ret
.none:
    pop es
    popad
    stc
    ret

; hd_load_mbr : lit le MBR dans FD_MBR (table vide si le disque est vierge)
hd_load_mbr:
    push es
    push cs
    pop es
    xor eax, eax
    mov bx, FD_MBR
    call hd_read
    jc .done
    cmp word [FD_MBR+510], 0xAA55
    je .done
    ; disque vierge : MBR neuf
    mov di, FD_MBR
    mov cx, 256
    xor ax, ax
    rep stosw
    mov word [FD_MBR+510], 0xAA55
    clc
.done:
    pop es
    ret

; hd_save_mbr : ecrit notre code de demarrage + la table + la signature
hd_save_mbr:
    push es
    push cs
    pop es
    mov si, mbr_code
    mov di, FD_MBR
    mov cx, 446
    rep movsb
    mov word [FD_MBR+510], 0xAA55
    xor eax, eax
    mov bx, FD_MBR
    call hd_write
    pop es
    pushf
    call forget_c
    popf
    ret

; forget_c : le lecteur C: devra etre relu (table ou format changes)
forget_c:
    push ax
    cmp byte [cur_drive], 2
    jne .table
    mov byte [dv_mounted], 0
.table:
    mov byte [drive_table + 2*DV_SIZE], 0
    pop ax
    ret

; fd_entry : AL = numero (0..3) -> SI = entree dans FD_MBR
fd_entry:
    movzx si, al
    shl si, 4
    add si, FD_MBR+446
    ret

; is_fat_type : AL = type de partition -> ZF=1 si FAT12/FAT16
is_fat_type:
    cmp al, 0x01
    je .ret
    cmp al, 0x04
    je .ret
    cmp al, 0x06
    je .ret
    cmp al, 0x0E
.ret:
    ret

; ============================================================================
; FDISK
; ============================================================================
fd_changed  db 0
fd_start    dd 0
fd_size     dd 0
fd_max      dd 0

do_fdisk:
    call hd_probe
    jnc .have_disk
    mov si, msg_no_hdd
    jmp print
.have_disk:
    call hd_load_mbr
    jc disk_error
    mov byte [fd_changed], 0
.menu:
    call fd_show
    mov si, msg_fd_menu
    call print
.key:
    call fd_getkey
    cmp al, '1'
    je .create
    cmp al, '2'
    je .delete
    cmp al, '3'
    je .active
    cmp al, '4'
    je .quit
    cmp al, 27
    je .quit
    jmp .key
.create:
    call fd_create
    jmp .pause
.delete:
    call fd_delete
    jmp .pause
.active:
    call fd_set_active
.pause:
    mov si, msg_press_key
    call print
    call fd_getkey
    jmp .menu
.quit:
    call newline
    cmp byte [fd_changed], 0
    je .ret
    mov si, msg_fd_changed
    call print
.ret:
    ret

fd_getkey:
    mov ah, 0
    int 0x16
    call putc
    push ax
    call newline
    pop ax
    ret

; fd_show : efface l'ecran et affiche le disque et ses partitions
fd_show:
    mov ax, 0x0003
    int 0x10
    mov si, msg_fd_title
    call print
    mov eax, [hd_total]
    shr eax, 11                 ; secteurs -> Mo
    call print_dec32
    mov si, msg_mb
    call print
    mov si, msg_fd_header
    call print
    mov byte [fd_row], 0
.row:
    mov al, ' '
    call putc
    mov al, [fd_row]
    add al, '1'
    call putc
    mov al, [fd_row]
    call fd_entry
    cmp byte [si+4], 0
    jne .used
    mov si, msg_fd_empty
    call print
    jmp .next
.used:
    push si
    cmp byte [si], 0x80
    mov si, msg_fd_active
    je .act
    mov si, msg_fd_inactive
.act:
    call print
    pop si
    push si
    mov al, [si+4]
    call is_fat_type
    mov si, msg_fd_fat
    je .typ
    mov si, msg_fd_other
.typ:
    call print
    pop si
    mov eax, [si+8]
    mov cx, 10
    call print_dec32_pad
    mov eax, [si+12]
    shr eax, 11
    mov cx, 8
    call print_dec32_pad
    mov si, msg_mb
    call print
.next:
    call newline
    inc byte [fd_row]
    cmp byte [fd_row], 4
    jb .row
    ret

fd_row db 0

; ---- creer une partition ----
fd_create:
    ; une case libre ?
    xor al, al
.slot:
    call fd_entry
    cmp byte [si+4], 0
    je .have_slot
    inc al
    cmp al, 4
    jb .slot
    mov si, msg_fd_full
    jmp print
.have_slot:
    mov [fd_slot], si
    ; debut = apres la derniere partition, aligne sur 1 Mo (2048 secteurs)
    mov dword [fd_start], 2048
    xor bl, bl
.after:
    mov al, bl
    call fd_entry
    cmp byte [si+4], 0
    je .next_part
    mov eax, [si+8]
    add eax, [si+12]
    cmp eax, [fd_start]
    jbe .next_part
    mov [fd_start], eax
.next_part:
    inc bl
    cmp bl, 4
    jb .after
    mov eax, [fd_start]
    add eax, 2047
    and eax, ~2047
    mov [fd_start], eax
    ; place libre
    mov eax, [hd_total]
    sub eax, [fd_start]
    jbe .no_space
    cmp eax, 4096
    jb .no_space
    cmp eax, FAT16_MAX_SECTORS
    jbe .max_ok
    mov eax, FAT16_MAX_SECTORS
.max_ok:
    mov [fd_max], eax
    ; demander la taille
    mov si, msg_fd_size1
    call print
    mov eax, [fd_max]
    shr eax, 11
    call print_dec32
    mov si, msg_fd_size2
    call print
    mov di, buffer
    call read_line
    mov si, buffer
    call parse_dec
    test eax, eax
    jz .use_max
    shl eax, 11                 ; Mo -> secteurs
    cmp eax, [fd_max]
    jbe .size_ok
.use_max:
    mov eax, [fd_max]
.size_ok:
    mov [fd_size], eax

    ; remplir l'entree
    mov si, [fd_slot]
    mov byte [si], 0
    ; active si c'est la seule partition active
    call fd_any_active
    jnc .not_first
    mov si, [fd_slot]
    mov byte [si], 0x80
.not_first:
    mov si, [fd_slot]
    mov eax, [fd_size]
    mov byte [si+4], 0x06       ; FAT16 (> 32 Mo)
    cmp eax, 65536
    jae .typed
    mov byte [si+4], 0x04       ; FAT16 (< 32 Mo)
    cmp eax, 8400
    jae .typed
    mov byte [si+4], 0x01       ; FAT12 (tres petite)
.typed:
    mov [si+12], eax
    mov eax, [fd_start]
    mov [si+8], eax
    lea di, [si+1]
    call lba_to_chs_field
    mov eax, [fd_start]
    add eax, [fd_size]
    dec eax
    lea di, [si+5]
    call lba_to_chs_field
    call hd_save_mbr
    jc .write_err
    mov byte [fd_changed], 1
    mov si, msg_fd_created
    jmp print
.no_space:
    mov si, msg_fd_nospace
    jmp print
.write_err:
    jmp disk_error

fd_slot dw 0

FAT16_MAX_SECTORS equ 4190208       ; 2046 Mo (limite du FAT16)

; fd_any_active : CF=1 si aucune partition n'est active
fd_any_active:
    push si
    push ax
    xor al, al
.loop:
    call fd_entry
    cmp byte [si+4], 0
    je .next
    cmp byte [si], 0x80
    je .yes
.next:
    inc al
    cmp al, 4
    jb .loop
    pop ax
    pop si
    stc
    ret
.yes:
    pop ax
    pop si
    clc
    ret

; lba_to_chs_field : EAX = LBA, DS:DI = 3 octets CHS de la table
lba_to_chs_field:
    pushad
    xor edx, edx
    movzx ecx, word [hd_spt]
    div ecx
    inc dx
    mov bl, dl                  ; secteur
    xor edx, edx
    movzx ecx, word [hd_heads]
    div ecx                     ; EAX = cylindre, DX = tete
    cmp eax, 1023
    jbe .fits
    ; au-dela du cylindre 1023 : valeurs maximales (on utilise le LBA)
    mov eax, 1023
    mov dx, [hd_heads]
    dec dx
    mov bl, [hd_spt]
.fits:
    mov [di], dl
    mov cl, ah
    shl cl, 6
    or cl, bl
    mov [di+1], cl
    mov [di+2], al
    popad
    ret

; ---- supprimer une partition ----
fd_delete:
    mov si, msg_fd_which_del
    call print
    call fd_ask_number
    jc .ret
    push si
    mov si, msg_fd_confirm
    call print
    call ask_yes
    pop si
    jc .ret
    mov di, si
    mov cx, 16
    xor al, al
    push es
    push cs
    pop es
    rep stosb
    pop es
    call hd_save_mbr
    jc disk_error
    mov byte [fd_changed], 1
    mov si, msg_fd_deleted
    call print
.ret:
    ret

; ---- rendre une partition active ----
fd_set_active:
    mov si, msg_fd_which_act
    call print
    call fd_ask_number
    jc .ret
    push si
    xor al, al
.clear:
    call fd_entry
    mov byte [si], 0
    inc al
    cmp al, 4
    jb .clear
    pop si
    mov byte [si], 0x80
    call hd_save_mbr
    jc disk_error
    mov byte [fd_changed], 1
    mov si, msg_fd_activated
    call print
.ret:
    ret

; fd_ask_number : touche 1-4 -> SI = entree (non vide). CF si annule.
fd_ask_number:
    call fd_getkey
    sub al, '1'
    cmp al, 4
    jae .bad
    call fd_entry
    cmp byte [si+4], 0
    je .bad
    clc
    ret
.bad:
    mov si, msg_fd_bad
    call print
    stc
    ret

; ask_yes : attend O/N. CF=0 si oui.
ask_yes:
    call fd_getkey
    call to_upper
    cmp al, 'O'
    je .yes
    cmp al, 'Y'
    je .yes
    stc
    ret
.yes:
    clc
    ret

; parse_dec : SI = texte -> EAX = nombre decimal (0 si vide)
parse_dec:
    push ebx
    push ecx
    xor eax, eax
    mov ebx, 10
.loop:
    movzx ecx, byte [si]
    sub cl, '0'
    cmp cl, 9
    ja .done
    mul ebx
    add eax, ecx
    inc si
    jmp .loop
.done:
    pop ecx
    pop ebx
    ret

; ============================================================================
; FORMAT C:
; ============================================================================
fm_part     dd 0                ; debut de la partition
fm_total    dd 0                ; taille
fm_type     db 0
fm_spc      db 0
fm_fatsz    dd 0
fm_clusters dd 0
fm_entry    dw 0

do_format:
    mov si, [cmd_args]
    call parse_drive
    jc .usage
    cmp al, 2
    jne .floppy
    mov si, msg_fm_warning
    call print
    call ask_yes
    jc .cancel
    mov si, msg_fm_working
    call print
    call format_c
    jc .err
    mov si, msg_fm_done
    jmp print
.err:
    mov si, msg_fm_err
    jmp print
.cancel:
    ret
.floppy:
    mov si, msg_fm_floppy
    jmp print
.usage:
    mov si, msg_fm_usage
    jmp print

; format_c : formate la premiere partition FAT du disque dur. CF si erreur.
format_c:
    pushad
    push es
    push cs
    pop es
    call hd_probe
    jc .err
    call hd_load_mbr
    jc .err
    ; premiere partition FAT (de preference la partition active)
    mov word [fm_entry], 0
    xor al, al
.find:
    call fd_entry
    push ax
    mov al, [si+4]
    call is_fat_type
    pop ax
    jne .next
    cmp word [fm_entry], 0
    je .take
    cmp byte [si], 0x80
    jne .next
.take:
    mov [fm_entry], si
.next:
    inc al
    cmp al, 4
    jb .find
    mov si, [fm_entry]
    test si, si
    jz .err
    mov eax, [si+8]
    mov [fm_part], eax
    mov eax, [si+12]
    mov [fm_total], eax
    mov al, [si+4]
    mov [fm_type], al

    ; ---- calcul de la geometrie FAT ----
    mov byte [fm_spc], 1
.try_spc:
    ; donnees = total - 1 (boot) - 32 (racine)
    mov eax, [fm_total]
    sub eax, 1 + 32
    movzx ecx, byte [fm_spc]
    xor edx, edx
    div ecx
    ; taille d'une FAT (16 bits par cluster) = (clusters + 2) / 256 arrondi
    add eax, 2 + 255
    shr eax, 8
    mov [fm_fatsz], eax
    ; vrai nombre de clusters
    mov eax, [fm_total]
    sub eax, 1 + 32
    sub eax, [fm_fatsz]
    sub eax, [fm_fatsz]
    movzx ecx, byte [fm_spc]
    xor edx, edx
    div ecx
    mov [fm_clusters], eax
    cmp byte [fm_type], 0x01
    jne .fat16_limit
    cmp eax, 4085               ; partition FAT12 : rester sous 4085
    jb .geometry_ok
    jmp .bigger
.fat16_limit:
    cmp eax, 65525
    jb .geometry_ok
.bigger:
    shl byte [fm_spc], 1
    cmp byte [fm_spc], 128
    jae .err
    jmp .try_spc
.geometry_ok:

    ; ---- secteur de boot ----
    mov si, vbr_code
    mov di, FD_VBR
    mov cx, 512
    rep movsb
    mov dword [FD_VBR+3], 'MICR'
    mov dword [FD_VBR+7], 'ODOS'
    mov al, [fm_spc]
    mov [FD_VBR+13], al
    mov word [FD_VBR+14], 1
    mov byte [FD_VBR+16], 2
    mov word [FD_VBR+17], 512
    mov word [FD_VBR+19], 0
    mov dword [FD_VBR+32], 0
    mov eax, [fm_total]
    cmp eax, 65536
    jae .total32
    mov [FD_VBR+19], ax
    jmp .total_set
.total32:
    mov [FD_VBR+32], eax
.total_set:
    mov byte [FD_VBR+21], 0xF8
    mov eax, [fm_fatsz]
    mov [FD_VBR+22], ax
    mov ax, [hd_spt]
    mov [FD_VBR+24], ax
    mov ax, [hd_heads]
    mov [FD_VBR+26], ax
    mov eax, [fm_part]
    mov [FD_VBR+28], eax
    mov byte [FD_VBR+36], 0x80
    mov byte [FD_VBR+38], 0x29
    push ecx
    push edx
    mov ah, 0
    int 0x1A                    ; numero de serie : l'horloge
    mov [FD_VBR+39], dx
    mov [FD_VBR+41], cx
    pop edx
    pop ecx
    mov si, fm_label
    mov di, FD_VBR+43
    mov cx, 11
    rep movsb
    mov dword [FD_VBR+54], 'FAT1'
    mov dword [FD_VBR+58], '6   '
    cmp dword [fm_clusters], 4085
    jae .fs16
    mov byte [FD_VBR+58], '2'
.fs16:
    mov eax, [fm_part]
    mov bx, FD_VBR
    call hd_write
    jc .err

    ; ---- les deux FAT ----
    mov di, FD_ZERO
    mov cx, 256
    xor ax, ax
    rep stosw
    mov eax, [fm_part]
    inc eax                     ; premiere FAT
    mov cx, 2
.fat_copy:
    push cx
    ; 1er secteur : octet media + fin de chaine pour les clusters 0 et 1
    mov dword [FD_ZERO], 0xFFFFFFF8
    cmp dword [fm_clusters], 4085
    jae .media16
    mov dword [FD_ZERO], 0x00FFFFF8
.media16:
    mov bx, FD_ZERO
    call hd_write
    jc .err_pop
    mov dword [FD_ZERO], 0
    mov ecx, [fm_fatsz]
    dec ecx
.fat_sector:
    jecxz .fat_done
    inc eax
    call hd_write
    jc .err_pop
    dec ecx
    jmp .fat_sector
.fat_done:
    inc eax
    pop cx
    loop .fat_copy

    ; ---- racine (32 secteurs) avec le nom du volume ----
    mov si, fm_label
    mov di, FD_ZERO
    mov cx, 11
    rep movsb
    mov byte [FD_ZERO+11], ATTR_VOLUME
    push eax
    call dos_datetime
    mov [FD_ZERO+22], ax
    mov [FD_ZERO+24], dx
    pop eax
    mov cx, 32
.root:
    mov bx, FD_ZERO
    call hd_write
    jc .err
    mov di, FD_ZERO
    push cx
    push eax
    mov cx, 128
    xor eax, eax
    rep stosd
    pop eax
    pop cx
    inc eax
    loop .root

    call forget_c
    pop es
    popad
    clc
    ret
.err_pop:
    pop cx
.err:
    call forget_c
    pop es
    popad
    stc
    ret

fm_label db 'MICRODOS   '

; ============================================================================
; SETUP : installation de MicroDOS sur le disque dur
; ============================================================================
do_setup:
.menu:
    mov ax, 0x0003
    int 0x10
    mov dh, 2
    mov dl, 18
    mov si, msg_setup_title
    call print_at
    mov dh, 4
    mov dl, 30
    mov si, msg_setup_sub
    call print_at
    mov dh, 8
    mov dl, 18
    mov si, msg_setup_1
    call print_at
    mov dh, 10
    mov dl, 18
    mov si, msg_setup_2
    call print_at
    mov dh, 12
    mov dl, 18
    mov si, msg_setup_3
    call print_at
    mov dh, 14
    mov dl, 18
    mov si, msg_setup_4
    call print_at
    mov dh, 16
    mov dl, 18
    mov si, msg_setup_5
    call print_at
    mov dh, 20
    mov dl, 0
    mov si, msg_empty
    call print_at
.key:
    mov ah, 0
    int 0x16
    cmp al, '1'
    je .install
    cmp al, '2'
    je .fdisk
    cmp al, '3'
    je .format
    cmp al, '4'
    je .shell
    cmp al, '5'
    je .reboot
    jmp .key
.install:
    call install_hdd
    jmp .wait
.fdisk:
    call do_fdisk
    jmp .wait
.format:
    mov ax, 0x0003
    int 0x10
    mov word [cmd_args], arg_c
    call do_format
.wait:
    mov si, msg_press_key
    call print
    mov ah, 0
    int 0x16
    jmp .menu
.shell:
    mov ax, 0x0003
    int 0x10
    mov si, msg_welcome
    jmp print
.reboot:
    int 0x19
    jmp .reboot

arg_c db 'c:', 0

; print_at : DH = ligne, DL = colonne, SI = texte
print_at:
    push ax
    push bx
    mov ah, 0x02
    xor bh, bh
    int 0x10
    pop bx
    pop ax
    jmp print

; ---------------------------------------------------------------------------
; install_hdd : formate C:, copie MicroDOS et les jeux, rend C: demarrable
; ---------------------------------------------------------------------------
install_hdd:
    mov ax, 0x0003
    int 0x10
    mov si, msg_setup_title
    call print
    call newline
    call newline
    call hd_probe
    jc .no_disk
    call hd_load_mbr
    jc .disk_err
    ; une partition FAT existe-t-elle ?
    xor al, al
.find:
    call fd_entry
    push ax
    mov al, [si+4]
    call is_fat_type
    pop ax
    je .found
    inc al
    cmp al, 4
    jb .find
    mov si, msg_inst_nopart
    jmp print
.found:
    mov si, msg_fm_warning
    call print
    call ask_yes
    jc .cancel

    ; 1. formater
    mov si, msg_inst_format
    call print
    call format_c
    jc .disk_err

    ; 2. le noyau en premier (le VBR le cherche au debut des donnees)
    mov si, msg_inst_copy
    call print
    mov al, [boot_drive]
    add al, 'A'
    mov [inst_kernel_src], al
    mov si, inst_kernel_src
    mov di, inst_kernel_dst
    mov cl, ATTR_HIDDEN | ATTR_SYSTEM | ATTR_RO
    call copy_one
    jc .disk_err

    ; 3. tous les fichiers et dossiers de la disquette
    mov al, [boot_drive]
    add al, 'A'
    mov [ct_src], al
    mov word [ct_src+1], ':\'
    mov byte [ct_src+3], 0
    mov dword [ct_dst], 'C:\'
    mov byte [ct_level], 0
    call copy_tree
    jc .disk_err

    ; 4. MBR de MicroDOS et partition active
    call hd_load_mbr
    jc .disk_err
    call fd_any_active
    jnc .has_active
    xor al, al
.first_fat:
    call fd_entry
    push ax
    mov al, [si+4]
    call is_fat_type
    pop ax
    je .activate
    inc al
    jmp .first_fat
.activate:
    mov byte [si], 0x80
.has_active:
    call hd_save_mbr
    jc .disk_err
    call fat_flush
    mov si, msg_inst_done
    jmp print
.cancel:
    ret
.no_disk:
    mov si, msg_no_hdd
    jmp print
.disk_err:
    mov si, msg_inst_err
    jmp print

inst_kernel_src db 'A:\KERNEL.BIN', 0
inst_kernel_dst db 'C:\KERNEL.BIN', 0

; ---------------------------------------------------------------------------
; copy_one : SI = source, DI = destination, CL = attributs -> CF si erreur
; ---------------------------------------------------------------------------
co_in   dw 0
co_out  dw 0

copy_one:
    pushad
    push es
    push cs
    pop es
    push di
    push cx
    mov al, 0
    call file_open
    pop cx
    pop si
    jc .err
    mov [co_in], bx
    call file_create
    jc .err_in
    mov [co_out], bx
.loop:
    mov bx, [co_in]
    mov di, XFER_BUF
    mov ecx, XFER_SIZE
    call file_read
    jc .err_both
    test eax, eax
    jz .done
    mov ecx, eax
    mov bx, [co_out]
    mov di, XFER_BUF
    call file_write
    jc .err_both
    cmp eax, ecx
    jne .err_both
    jmp .loop
.done:
    mov bx, [co_out]
    call file_close
    mov bx, [co_in]
    call file_close
    pop es
    popad
    clc
    ret
.err_both:
    mov bx, [co_out]
    call file_close
.err_in:
    mov bx, [co_in]
    call file_close
.err:
    pop es
    popad
    stc
    ret

; ---------------------------------------------------------------------------
; copy_tree : copie le dossier ct_src dans ct_dst (et ses sous-dossiers,
; sur un niveau). Ignore KERNEL.BIN et les fichiers caches.
; ---------------------------------------------------------------------------
CT_LEN      equ 80
ct_level    db 0
ct_src      times 2*CT_LEN db 0     ; un chemin par niveau
ct_dst      times 2*CT_LEN db 0
ct_index    dw 0, 0
ct_name     times 16 db 0
ct_attr     db 0

copy_tree:
    movzx bx, byte [ct_level]
    shl bx, 1
    mov word [ct_index+bx], 0
.next:
    ; dossier source
    call ct_paths               ; SI = source du niveau, DI = destination
    call path_resolve
    jc .err
    push ax
    mov di, fname
    mov cx, 11
    mov al, '?'
    rep stosb
    pop ax
    movzx bx, byte [ct_level]
    shl bx, 1
    mov dx, [ct_index+bx]
    mov cl, ATTR_DIR
    call find_entry
    jc .done
    inc dx
    mov [ct_index+bx], dx
    mov al, [si+11]
    mov [ct_attr], al
    mov di, ct_name
    call entry_to_text
    cmp byte [ct_name], '.'
    je .next
    test byte [ct_attr], ATTR_DIR
    jnz .dir
    ; fichier (sauf le noyau, deja copie)
    cmp byte [ct_level], 0
    jne .copy_file
    mov si, ct_name
    mov di, name_kernel
    call str_equal
    je .next
.copy_file:
    call ct_append_name
    mov si, msg_inst_file
    call print
    mov si, ct_tmp_dst
    call print
    call newline
    mov si, ct_tmp_src
    mov di, ct_tmp_dst
    mov cl, [ct_attr]
    call copy_one
    jc .err
    jmp .next
.dir:
    cmp byte [ct_level], 0
    jne .next                   ; un seul niveau de sous-dossiers
    call ct_append_name
    mov si, ct_tmp_dst
    call dir_make
    ; niveau 1 : chemins "X:\NOM\"
    mov si, ct_tmp_src
    mov di, ct_src+CT_LEN
    call copy_str
    mov byte [di-1], '\'
    mov byte [di], 0
    mov si, ct_tmp_dst
    mov di, ct_dst+CT_LEN
    call copy_str
    mov byte [di-1], '\'
    mov byte [di], 0
    mov byte [ct_level], 1
    call copy_tree
    mov byte [ct_level], 0
    jc .err
    jmp .next
.done:
    clc
    ret
.err:
    stc
    ret

; ct_paths : SI = ct_src du niveau, DI = ct_dst du niveau
ct_paths:
    movzx ax, byte [ct_level]
    mov dx, CT_LEN
    mul dx
    mov si, ct_src
    add si, ax
    mov di, ct_dst
    add di, ax
    ret

; ct_append_name : ct_tmp_src = source + nom, ct_tmp_dst = destination + nom
ct_tmp_src  times CT_LEN db 0
ct_tmp_dst  times CT_LEN db 0
ct_append_name:
    call ct_paths
    push di
    mov di, ct_tmp_src
    call copy_str
    dec di
    mov si, ct_name
    call copy_str
    pop si
    mov di, ct_tmp_dst
    call copy_str
    dec di
    mov si, ct_name
    call copy_str
    ret

; str_equal : compare SI et DI (chaines terminees par 0). ZF=1 si egales.
str_equal:
    push si
    push di
.loop:
    mov al, [si]
    cmp al, [di]
    jne .done
    inc si
    inc di
    test al, al
    jnz .loop
.done:
    pop di
    pop si
    ret

name_kernel db 'KERNEL.BIN', 0

; ---------------------------------------------------------------------------
; Messages de la Disk Edition
; ---------------------------------------------------------------------------
msg_no_hdd      db 'Aucun disque dur detecte.', 13, 10, 0
msg_mb          db ' Mo', 0
msg_press_key   db 13, 10, 'Appuyez sur une touche pour continuer...', 0
msg_fd_title    db 'MicroDOS FDISK - Disque dur 1 : ', 0
msg_fd_header   db 13, 10, 13, 10, ' N  Etat    Type        Debut  Taille', 13, 10
                db ' -------------------------------------------', 13, 10, 0
msg_fd_empty    db '  -       (vide)', 0
msg_fd_active   db '  ACTIVE  ', 0
msg_fd_inactive db '          ', 0
msg_fd_fat      db 'DOS FAT', 0
msg_fd_other    db 'AUTRE  ', 0
msg_fd_menu     db 13, 10, ' 1. Creer une partition DOS', 13, 10
                db ' 2. Supprimer une partition', 13, 10
                db ' 3. Rendre une partition active', 13, 10
                db ' 4. Quitter', 13, 10, 13, 10
                db ' Votre choix : ', 0
msg_fd_full     db 'La table des partitions est pleine.', 13, 10, 0
msg_fd_nospace  db 'Plus de place libre sur le disque.', 13, 10, 0
msg_fd_size1    db 'Taille de la partition en Mo (Entree = ', 0
msg_fd_size2    db ' Mo, le maximum) : ', 0
msg_fd_created  db 'Partition creee. Formatez-la avec FORMAT C:', 13, 10, 0
msg_fd_which_del db 'Numero de la partition a supprimer (1-4) : ', 0
msg_fd_which_act db 'Numero de la partition a rendre active (1-4) : ', 0
msg_fd_confirm  db 'Toutes les donnees de cette partition seront perdues. Continuer (O/N) ? ', 0
msg_fd_deleted  db 'Partition supprimee.', 13, 10, 0
msg_fd_activated db 'Partition active modifiee.', 13, 10, 0
msg_fd_bad      db 'Numero invalide.', 13, 10, 0
msg_fd_changed  db 'Table des partitions modifiee.', 13, 10, 0
msg_fm_usage    db 'Usage : format c:', 13, 10, 0
msg_fm_floppy   db 'FORMAT ne gere que le disque dur (C:).', 13, 10, 0
msg_fm_warning  db 'ATTENTION : toutes les donnees du disque C: seront effacees !', 13, 10
                db 'Continuer (O/N) ? ', 0
msg_fm_working  db 'Formatage en cours...', 13, 10, 0
msg_fm_done     db 'Disque C: formate avec succes.', 13, 10, 0
msg_fm_err      db 'Erreur de formatage (pas de partition DOS ? lancez FDISK).', 13, 10, 0
msg_setup_title db 'MicroDOS ', VERSION_STR, ' - Disk Install Edition', 0
msg_setup_sub   db 'Selection des choix', 0
msg_setup_1     db '1.   Installer MicroDOS sur le disque dur', 0
msg_setup_2     db '2.   Partitionner le disque dur (FDISK)', 0
msg_setup_3     db '3.   Formater le disque dur (C:)', 0
msg_setup_4     db '4.   Utiliser MicroDOS depuis la disquette', 0
msg_setup_5     db "5.   Redemarrer l'ordinateur", 0
msg_empty       db 0
msg_inst_nopart db 'Aucune partition DOS sur le disque : choisissez d abord FDISK (2).', 13, 10, 0
msg_inst_format db 'Formatage de C:...', 13, 10, 0
msg_inst_copy   db 'Copie de MicroDOS...', 13, 10, 0
msg_inst_file   db '  ', 0
msg_inst_done   db 13, 10, 'Installation terminee !', 13, 10
                db 'Enlevez la disquette et redemarrez (choix 5).', 13, 10, 0
msg_inst_err    db 'Erreur lors de l installation.', 13, 10, 0
