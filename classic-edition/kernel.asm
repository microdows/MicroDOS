; ============================================================================
; MicroDOS - Classic Edition
; Noyau + shell. Charge par boot.asm en 0x1000:0000.
;
; Fichiers inclus :
;   fs.inc    : disque, FAT12/FAT16, repertoires, chemins
;   file.inc  : fichiers ouverts (lecture/ecriture 32 bits)
;   mem.inc   : gestion memoire DOS (blocs MCB)
;   exec.inc  : chargement des .COM / .EXE, fin de programme
;   dos.inc   : API INT 21h
; ============================================================================
[BITS 16]
[ORG 0x0000]

%define VERSION_STR 'v2.0.0'
%ifdef DISK_EDITION
  %define EDITION_STR 'Disk Install Edition'
%else
  %define EDITION_STR 'Classic Edition'
%endif

start:
    cli
    push cs
    pop ds
    push cs
    pop es
    xor ax, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    cld

    ; lecteur de demarrage (DL donne par le boot)
    mov al, dl
    cmp al, 0x80
    jb .floppy
    mov al, 2
.floppy:
    cmp al, MAX_DRIVES
    jb .drive_ok
    xor al, al
.drive_ok:
    mov [def_drive], al
    mov [boot_drive], al

    call install_vectors
    call sft_init
    call mem_init
    call env_init

    mov al, [def_drive]
    call drive_select

%ifdef DISK_EDITION
    ; demarrage sur la disquette d'installation : menu de l'installateur
    cmp byte [boot_drive], 2
    jae .welcome
    call do_setup
    jmp main_loop
.welcome:
%endif
    mov si, msg_welcome
    call print

; ============================================================================
; Boucle du shell
; ============================================================================
main_loop:
    cld
    push cs
    pop ds
    push cs
    pop es
    call fat_flush
    mov al, [def_drive]
    call drive_select
    call print_prompt

    mov di, buffer
    call read_line

    mov si, buffer
    call skip_spaces
    cmp byte [si], 0
    je main_loop
    mov [cmd_start], si

    ; chercher la commande dans la table
    mov bx, cmd_table
.find:
    mov di, [bx]
    test di, di
    jz .not_builtin
    mov si, [cmd_start]
    call strcmp
    jc .found
    add bx, 4
    jmp .find
.found:
    ; SI = arguments (apres le nom de la commande et les espaces)
    mov si, [cmd_start]
.skip_name:
    lodsb
    call is_end
    jne .skip_name
    dec si
    call skip_spaces
    mov [cmd_args], si
    call word [bx+2]
    jmp main_loop

.not_builtin:
    mov si, [cmd_start]
    call run_program
    jmp main_loop

cmd_start   dw 0
cmd_args    dw 0

; ---------- table des commandes : nom, fonction ----------
cmd_table:
    dw cmd_help,      do_help
    dw cmd_dir,       do_dir
    dw cmd_type,      do_type
    dw cmd_ver,       do_ver
    dw cmd_cls,       do_cls
    dw cmd_reset,     do_reset
    dw cmd_echo,      do_echo
    dw cmd_pause,     do_pause
    dw cmd_date,      do_date
    dw cmd_time,      do_time
    dw cmd_fatinfo,   do_fatinfo
    dw cmd_mkfile,    do_mkfile
    dw cmd_copy,      do_copy
    dw cmd_del,       do_del
    dw cmd_erase,     do_del
    dw cmd_ren,       do_ren
    dw cmd_cd,        do_cd
    dw cmd_chdir,     do_cd
    dw cmd_mkdir,     do_mkdir
    dw cmd_md,        do_mkdir
    dw cmd_rmdir,     do_rmdir
    dw cmd_rd,        do_rmdir
    dw cmd_label,     do_label
    dw cmd_vol,       do_vol
    dw cmd_mem,       do_mem
    dw cmd_diskcopy,  do_diskcopy
    dw cmd_rebootdos, do_rebootdos
    dw cmd_reboot,    do_reboot
    dw cmd_drive_a,   do_drive_a
    dw cmd_drive_b,   do_drive_b
    dw cmd_drive_c,   do_drive_c
%ifdef DISK_EDITION
    dw cmd_fdisk,     do_fdisk
    dw cmd_format,    do_format
    dw cmd_setup,     do_setup
%endif
    dw 0

; ============================================================================
; Commandes simples
; ============================================================================
do_help:
    mov si, msg_help
    jmp print

do_cls:
    mov ax, 0x0003
    int 0x10
    ret

do_ver:
    mov si, msg_ver
    jmp print

do_reboot:
    mov si, msg_reboot
    call print
    mov al, 0xFE
    out 0x64, al
    ret

do_rebootdos:
    mov si, msg_rebootdos
    call print
    int 0x19
    ret

do_echo:
    mov si, [cmd_args]
    call print
    jmp newline

do_pause:
    mov si, msg_pause
    call print
    mov ah, 0x00
    int 0x16
    ret

do_date:
    mov ah, 0x04
    int 0x1A                    ; CH=siecle CL=annee DH=mois DL=jour (BCD)
    mov si, msg_date_prefix
    call print
    mov al, dl
    call print_bcd
    mov al, '/'
    call putc
    mov al, dh
    call print_bcd
    mov al, '/'
    call putc
    mov al, ch
    call print_bcd
    mov al, cl
    call print_bcd
    jmp newline

do_time:
    mov ah, 0x02
    int 0x1A                    ; CH=heures CL=minutes DH=secondes (BCD)
    mov si, msg_time_prefix
    call print
    mov al, ch
    call print_bcd
    mov al, ':'
    call putc
    mov al, cl
    call print_bcd
    mov al, ':'
    call putc
    mov al, dh
    call print_bcd
    jmp newline

print_bcd:
    push ax
    shr al, 4
    add al, '0'
    call putc
    pop ax
    and al, 0x0F
    add al, '0'
    jmp putc

; ---------- reset : relit la disquette ----------
do_reset:
    mov byte [dv_mounted], 0
    mov al, [cur_drive]
    call drive_select
    jc .err
    mov si, msg_reset
    jmp print
.err:
    jmp disk_error

; ---------- a: b: c: ----------
do_drive_a:
    xor al, al
    jmp change_drive
do_drive_b:
    mov al, 1
    jmp change_drive
do_drive_c:
    mov al, 2
change_drive:
    mov ah, [def_drive]
    mov [def_drive], al
    call drive_select
    jnc .ok
    mov [def_drive], ah
    mov si, msg_drive_err
    call print
.ok:
    ret

; ---------- fatinfo : espace libre ----------
do_fatinfo:
    mov al, [def_drive]
    call drive_select
    jc disk_error
    call free_clusters
    push eax
    mov si, msg_fatinfo
    call print
    call print_dec32
    mov si, msg_bytes_free
    call print
    pop eax
    movzx ecx, byte [dv_spc]
    shl ecx, 9
    mul ecx
    call print_dec32
    mov si, msg_bytes
    call print
    ret

; ---------- mem : memoire conventionnelle libre ----------
do_mem:
    call mem_largest
    movzx eax, ax
    shl eax, 4
    mov si, msg_mem
    call print
    call print_dec32
    mov si, msg_bytes
    jmp print

; ============================================================================
; DIR [chemin]
; ============================================================================
dir_count   dw 0
dir_dirs    dw 0
dir_bytes   dd 0
dir_index   dw 0
dir_clust   dw 0
dir_pattern times 11 db 0

do_dir:
    mov si, [cmd_args]
    call path_resolve
    jc .bad_path
    mov [dir_clust], ax
    cmp byte [path_empty], 0
    jne .all
    cmp word [fname], '. '
    je .all
    ; un nom avec jokers est un motif
    mov di, fname
    mov cx, 11
    mov al, '?'
    repne scasb
    je .pattern
    ; est-ce un dossier ? alors on liste son contenu
    mov ax, [dir_clust]
    push ax
    call dir_search
    pop ax
    jc .pattern
    test byte [si+11], ATTR_DIR
    jz .pattern
    cmp word [fname], '..'
    jne .sub
    test ax, ax
    jz .all                     ; ".." depuis la racine
.sub:
    mov ax, [si+26]
    mov [dir_clust], ax
.all:
    mov di, fname
    mov cx, 11
    mov al, '?'
    rep stosb
.pattern:
    mov si, fname
    mov di, dir_pattern
    mov cx, 11
    rep movsb

    mov word [dir_count], 0
    mov word [dir_dirs], 0
    mov dword [dir_bytes], 0
    xor dx, dx
.loop:
    mov si, dir_pattern
    mov di, fname
    mov cx, 11
    rep movsb
    mov ax, [dir_clust]
    mov cl, ATTR_DIR
    call find_entry
    jc .end
    mov [dir_index], dx
    call print_dir_entry
    mov dx, [dir_index]
    inc dx
    jmp .loop
.end:
    ; resume
    mov si, msg_dir_sp
    call print
    movzx eax, word [dir_count]
    call print_dec32
    mov si, msg_dir_files
    call print
    mov eax, [dir_bytes]
    call print_dec32
    mov si, msg_bytes
    call print
    mov si, msg_dir_sp
    call print
    movzx eax, word [dir_dirs]
    call print_dec32
    mov si, msg_dir_dirs
    call print
    call free_clusters
    movzx ecx, byte [dv_spc]
    shl ecx, 9
    mul ecx
    call print_dec32
    mov si, msg_bytes_free
    call print
    mov si, msg_crlf
    jmp print
.bad_path:
    mov si, msg_cd_err
    jmp print

; print_dir_entry : SI = entree -> "NOM      EXT     taille" ou "<DIR>"
print_dir_entry:
    push si
    mov cx, 8
.name:
    lodsb
    call putc
    loop .name
    mov al, ' '
    call putc
    mov cx, 3
.ext:
    lodsb
    call putc
    loop .ext
    pop si
    test byte [si+11], ATTR_DIR
    jz .file
    inc word [dir_dirs]
    push si
    mov si, msg_dir_tag
    call print
    pop si
    jmp .date
.file:
    inc word [dir_count]
    mov eax, [si+28]
    add [dir_bytes], eax
    mov cx, 10
    call print_dec32_pad
.date:
    mov al, ' '
    call putc
    call putc
    mov ax, [si+24]             ; date : jour/mois/annee
    push ax
    and ax, 31
    call print_2dig
    mov al, '/'
    call putc
    pop ax
    push ax
    shr ax, 5
    and ax, 15
    call print_2dig
    mov al, '/'
    call putc
    pop ax
    shr ax, 9
    add ax, 1980
    movzx eax, ax
    call print_dec32
    jmp newline

; print_2dig : AX (0..99) -> deux chiffres
print_2dig:
    push ax
    push bx
    mov bl, 10
    div bl
    add al, '0'
    call putc
    mov al, ah
    add al, '0'
    call putc
    pop bx
    pop ax
    ret

; ============================================================================
; TYPE fichier
; ============================================================================
do_type:
    mov si, [cmd_args]
    cmp byte [si], 0
    je .usage
    mov al, 0
    call file_open
    jc .not_found
.loop:
    push es
    push cs
    pop es
    mov di, XFER_BUF
    mov ecx, 4096
    call file_read
    pop es
    jc .read_err
    test eax, eax
    jz .done
    mov cx, ax
    mov si, XFER_BUF
.print:
    lodsb
    cmp al, 0x1A                ; fin de fichier texte
    je .done
    call putc
    loop .print
    jmp .loop
.done:
    call file_close
    jmp newline
.read_err:
    call file_close
    jmp disk_error
.not_found:
    mov si, msg_not_found
    jmp print
.usage:
    mov si, msg_type_usage
    jmp print

; ============================================================================
; MKFILE nom
; ============================================================================
do_mkfile:
    mov si, [cmd_args]
    cmp byte [si], 0
    je .usage
    xor cl, cl
    call file_create
    jc .error
    call file_close
    mov si, msg_mkfile_ok
    jmp print
.error:
    mov si, msg_mkfile_err
    jmp print
.usage:
    mov si, msg_mkfile_usage
    jmp print

; ============================================================================
; COPY source destination
; ============================================================================
cp_src      dw 0
cp_dst      dw 0
cp_in       dw 0
cp_out      dw 0
cp_path     times 140 db 0
cp_canon1   times 80 db 0
cp_canon2   times 80 db 0

do_copy:
    mov si, [cmd_args]
    cmp byte [si], 0
    je .usage
    mov [cp_src], si
    call next_arg
    jc .no_dest
    mov [cp_dst], si
    call terminate_arg
    mov si, [cp_src]
    call terminate_arg
    jmp .open_src
.no_dest:
    ; "copy A:\X.TXT" : copie dans le dossier courant, meme nom
    mov si, [cp_src]
    call terminate_arg
    call last_component
    mov [cp_dst], si
.open_src:

    ; ouvrir la source
    mov si, [cp_src]
    mov al, 0
    call file_open
    jc .not_found
    mov [cp_in], bx

    ; destination = dossier ? alors on garde le nom de la source
    mov si, [cp_dst]
    call path_resolve
    jc .dst_err
    cmp byte [path_empty], 0
    jne .into_dir
    push ax
    call dir_search
    pop ax
    jc .plain
    test byte [si+11], ATTR_DIR
    jz .plain
.into_dir:
    ; cp_path = destination + "\" + nom de la source
    mov si, [cp_dst]
    mov di, cp_path
    call copy_str
    dec di
    mov al, [di-1]
    cmp al, '\'
    je .add_name
    cmp al, ':'
    je .add_name
    mov byte [di], '\'
    inc di
.add_name:
    mov si, [cp_src]
    call last_component
    call copy_str
    mov word [cp_dst], cp_path
.plain:
    ; interdit : copier un fichier sur lui-meme (il serait vide)
    mov si, [cp_src]
    mov di, cp_canon1
    call path_canon
    mov si, [cp_dst]
    mov di, cp_canon2
    call path_canon
    mov si, cp_canon1
    mov di, cp_canon2
.same:
    lodsb
    cmp al, [di]
    jne .different
    inc di
    test al, al
    jnz .same
    mov bx, [cp_in]
    call file_close
    mov si, msg_copy_self
    jmp print
.different:
    mov si, [cp_dst]
    xor cl, cl
    call file_create
    jc .dst_err_close
    mov [cp_out], bx

.loop:
    mov bx, [cp_in]
    mov di, XFER_BUF
    mov ecx, XFER_SIZE
    call file_read
    jc .io_err
    test eax, eax
    jz .done
    mov ecx, eax
    mov bx, [cp_out]
    mov di, XFER_BUF
    call file_write
    jc .io_err
    cmp eax, ecx
    jne .io_err                 ; disque plein
    jmp .loop
.done:
    mov bx, [cp_out]
    call file_close
    mov bx, [cp_in]
    call file_close
    mov si, msg_copy_ok
    jmp print
.io_err:
    mov bx, [cp_out]
    call file_close
.dst_err_close:
    mov bx, [cp_in]
    call file_close
.dst_err:
    mov si, msg_copy_err
    jmp print
.not_found:
    mov si, msg_not_found
    jmp print
.usage:
    mov si, msg_copy_usage
    jmp print

; ============================================================================
; DEL fichier
; ============================================================================
do_del:
    mov si, [cmd_args]
    cmp byte [si], 0
    je .usage
    ; refuser les dossiers et les fichiers systeme avec un message clair
    call path_lookup
    jc .error
    test byte [si+11], ATTR_DIR
    jnz .is_dir
    test byte [si+11], ATTR_RO | ATTR_SYSTEM
    jnz .protected
    mov si, [cmd_args]
    call file_delete
    jc .error
    mov si, msg_del_ok
    jmp print
.error:
    mov si, msg_del_err
    jmp print
.is_dir:
    mov si, msg_del_dir
    jmp print
.protected:
    mov si, msg_del_protected
    jmp print
.usage:
    mov si, msg_del_usage
    jmp print

; ============================================================================
; REN ancien nouveau  (le nouveau nom reste dans le meme dossier)
; ============================================================================
do_ren:
    mov si, [cmd_args]
    mov [cp_src], si
    call next_arg
    jc .usage
    mov [cp_dst], si
    call terminate_arg
    mov si, [cp_src]
    call terminate_arg
    ; protection des fichiers systeme
    mov si, [cp_src]
    call path_lookup
    jc .error
    test byte [si+11], ATTR_RO | ATTR_SYSTEM
    jnz .protected
    ; cp_path = dossier de l'ancien nom + nouveau nom
    mov si, [cp_src]
    mov di, cp_path
    call copy_str
    mov si, [cp_src]
    call last_component
    sub si, [cp_src]
    mov di, cp_path
    add di, si
    mov si, [cp_dst]
    call last_component
    call copy_str
    mov si, [cp_src]
    mov di, cp_path
    call file_rename
    jc .error
    mov si, msg_ren_ok
    jmp print
.error:
    mov si, msg_ren_err
    jmp print
.protected:
    mov si, msg_ren_protected
    jmp print
.usage:
    mov si, msg_ren_usage
    jmp print

; ============================================================================
; CD [chemin]
; ============================================================================
do_cd:
    mov si, [cmd_args]
    cmp byte [si], 0
    je .show
    call dir_change
    jc .error
    ret
.show:
    mov si, cd_tmp
    mov byte [si], 0
    mov di, cp_path
    call path_canon
    mov si, cp_path
    call print
    jmp newline
.error:
    mov si, msg_cd_err
    jmp print

cd_tmp db 0

; ============================================================================
; MKDIR / RMDIR
; ============================================================================
do_mkdir:
    mov si, [cmd_args]
    cmp byte [si], 0
    je .usage
    call dir_make
    jc .error
    mov si, msg_mkdir_ok
    jmp print
.error:
    mov si, msg_mkdir_err
    jmp print
.usage:
    mov si, msg_mkdir_usage
    jmp print

do_rmdir:
    mov si, [cmd_args]
    cmp byte [si], 0
    je .usage
    call dir_remove
    jc .error
    mov si, msg_rmdir_ok
    jmp print
.error:
    mov si, msg_rmdir_err
    jmp print
.usage:
    mov si, msg_rmdir_usage
    jmp print

; ============================================================================
; LABEL nom / VOL
; Le nom est ecrit dans le secteur de boot ET dans l'entree "volume" de la
; racine (c'est celle que lisent DOS et Windows).
; ============================================================================
lb_name times 11 db ' '

do_label:
    mov si, [cmd_args]
    cmp byte [si], 0
    je .usage
    ; nom en majuscules, complete par des espaces
    mov di, lb_name
    mov cx, 11
.fmt:
    lodsb
    call is_end
    je .pad
    call to_upper
    stosb
    loop .fmt
    jmp .write
.pad:
    dec si
    mov al, ' '
    rep stosb
.write:
    mov al, [def_drive]
    call drive_select
    jc .err
    ; 1) secteur de boot
    mov bx, BOOT_BUF
    xor eax, eax
    call disk_read
    jc .err
    mov si, lb_name
    mov di, BOOT_BUF+43
    mov cx, 11
    rep movsb
    xor eax, eax
    call disk_write
    jc .err
    ; 2) entree volume dans la racine (existante ou nouvelle)
    mov di, fname
    mov cx, 11
    mov al, '?'
    rep stosb
    xor ax, ax
    mov cl, ATTR_VOLUME
    xor dx, dx
    call find_entry
    jnc .have_entry
    xor ax, ax
    call dir_alloc
    jc .err
    mov di, si
    mov cx, 32
    xor al, al
    rep stosb
    mov byte [si+11], ATTR_VOLUME
.have_entry:
    mov di, si
    push si
    mov si, lb_name
    mov cx, 11
    rep movsb
    pop si
    call dos_datetime
    mov [si+22], ax
    mov [si+24], dx
    call dir_store
    jc .err
    mov si, msg_label_ok
    jmp print
.err:
    mov si, msg_label_err
    jmp print
.usage:
    mov si, msg_label_usage
    jmp print

do_vol:
    mov al, [def_drive]
    call drive_select
    jc .err
    mov si, msg_vol_name
    call print
    mov di, fname
    mov cx, 11
    mov al, '?'
    rep stosb
    xor ax, ax
    mov cl, ATTR_VOLUME
    xor dx, dx
    call find_entry
    jnc .print
    ; pas d'entree volume : on lit le secteur de boot
    mov bx, BOOT_BUF
    xor eax, eax
    call disk_read
    jc .err
    mov si, BOOT_BUF+43
.print:
    mov cx, 11
.loop:
    lodsb
    call putc
    loop .loop
    jmp newline
.err:
    mov si, msg_vol_err
    jmp print

; ============================================================================
; DISKCOPY source: cible:
;   Copie une disquette secteur par secteur. Avec "diskcopy a: a:", le
;   programme demande d'echanger les disquettes entre chaque passage.
; ============================================================================
dk_src      db 0
dk_dst      db 0
dk_next     dd 0
dk_total    dd 0
dk_count    dw 0
dk_bufseg   dw 0
dk_bufsecs  dw 0
dk_saved_bios db 0

do_diskcopy:
    mov si, [cmd_args]
    call parse_drive
    jc .usage
    mov [dk_src], al
    call next_arg
    jc .usage
    call parse_drive
    jc .usage
    mov [dk_dst], al
    cmp byte [dk_src], 2
    jae .usage
    cmp byte [dk_dst], 2
    jae .usage

    ; memoire pour le tampon : le plus grand bloc libre
    call mem_largest
    cmp ax, 0x20
    jb .no_mem
    mov bx, ax
    and bx, 0xFFE0              ; multiple de 512 octets
    shr ax, 5
    mov [dk_bufsecs], ax
    call mem_alloc
    jc .no_mem
    mov [dk_bufseg], ax

    mov dword [dk_next], 0
    mov al, [dk_src]
    cmp al, [dk_dst]
    jne .first
    mov si, msg_dc_insert_src
    call wait_key_msg
.first:
    call dc_select_src
    jc .src_err
    mov eax, [dv_total]
    mov [dk_total], eax
    mov si, msg_dc_info
    call print
    call print_dec32
    mov si, msg_dc_sectors
    call print

.pass:
    mov eax, [dk_total]
    sub eax, [dk_next]
    jbe .finished
    movzx ecx, word [dk_bufsecs]
    cmp eax, ecx
    jbe .count_ok
    mov eax, ecx
.count_ok:
    mov [dk_count], ax

    ; ---- lecture depuis la source ----
    cmp dword [dk_next], 0
    je .read                    ; 1er passage : la source est deja la
    mov al, [dk_src]
    cmp al, [dk_dst]
    jne .read
    mov si, msg_dc_insert_src
    call wait_key_msg
.read:
    call dc_select_src
    jc .src_err
    mov eax, [dk_next]
    mov cx, [dk_count]
    mov dx, [dk_bufseg]
.read_loop:
    mov es, dx
    xor bx, bx
    call disk_read
    jc .src_err
    inc eax
    add dx, 0x20
    loop .read_loop
    push cs
    pop es

    ; ---- ecriture sur la cible (meme geometrie) ----
    mov al, [dk_src]
    cmp al, [dk_dst]
    jne .write
    mov si, msg_dc_insert_dst
    call wait_key_msg
.write:
    mov al, [dv_bios]
    mov [dk_saved_bios], al
    mov al, [dk_dst]
    mov [dv_bios], al
    xor ax, ax
    mov dl, [dv_bios]
    int 0x13
    mov eax, [dk_next]
    mov cx, [dk_count]
    mov dx, [dk_bufseg]
.write_loop:
    mov es, dx
    xor bx, bx
    call disk_write
    jc .dst_err
    inc eax
    add dx, 0x20
    loop .write_loop
    push cs
    pop es
    mov al, [dk_saved_bios]
    mov [dv_bios], al
    mov al, '.'
    call putc
    movzx eax, word [dk_count]
    add [dk_next], eax
    jmp .pass

.finished:
    call newline
    call dc_forget_dst
    mov si, msg_dc_ok
    call print
    jmp .free
.dst_err:
    push cs
    pop es
    mov al, [dk_saved_bios]
    mov [dv_bios], al
    call dc_forget_dst
    call newline
    mov si, msg_dc_dst_err
    call print
    jmp .free
.src_err:
    push cs
    pop es
    call newline
    mov si, msg_dc_src_err
    call print
.free:
    mov es, [dk_bufseg]
    call mem_free
    push cs
    pop es
    ret
.no_mem:
    mov si, msg_no_mem
    jmp print
.usage:
    mov si, msg_dc_usage
    jmp print

; dc_select_src : remonte la disquette source (elle a pu etre echangee)
dc_select_src:
    mov al, [dk_src]
    call drive_select
    jc .ret
    mov byte [dv_mounted], 0
    mov al, [dk_src]
    call drive_select
.ret:
    ret

; dc_forget_dst : la cible a change, il faudra relire son BPB
dc_forget_dst:
    push ax
    push bx
    mov al, [dk_dst]
    cmp al, [cur_drive]
    jne .table
    mov byte [dv_mounted], 0
    jmp .done
.table:
    xor ah, ah
    mov bx, DV_SIZE
    mul bx
    mov bx, drive_table
    add bx, ax
    mov byte [bx], 0            ; dv_mounted est le 1er octet
.done:
    pop bx
    pop ax
    ret

wait_key_msg:
    call newline
    call print
    mov ah, 0
    int 0x16
    jmp newline

; parse_drive : SI = "A:" -> AL = 0/1/2, SI apres. CF si invalide.
parse_drive:
    mov al, [si]
    call to_upper
    cmp al, 'A'
    jb .bad
    cmp al, 'C'
    ja .bad
    cmp byte [si+1], ':'
    jne .bad
    sub al, 'A'
    add si, 2
    clc
    ret
.bad:
    stc
    ret

; ============================================================================
; Lancer un programme : "NOM [arguments]"
; Cherche NOM, NOM.COM puis NOM.EXE.
; ============================================================================
rp_name     times 80 db 0
rp_tail     times 130 db 0

run_program:
    ; copier le nom
    mov di, rp_name
    mov cx, 70
.copy:
    lodsb
    call is_end
    je .name_end
    stosb
    loop .copy
.name_end:
    mov byte [di], 0
    dec si
    ; queue de commande : " args" + CR, longueur en tete
    mov di, rp_tail+1
    xor cx, cx
.tail:
    lodsb
    test al, al
    jz .tail_end
    cmp cx, 126
    jae .tail_end
    stosb
    inc cx
    jmp .tail
.tail_end:
    mov byte [di], 13
    mov [rp_tail], cl

    ; le nom a-t-il une extension ?
    mov si, rp_name
    call last_component
.ext_scan:
    lodsb
    test al, al
    jz .no_ext
    cmp al, '.'
    jne .ext_scan
    jmp .try                    ; extension donnee
.no_ext:
    ; essayer .COM puis .EXE
    mov di, rp_name
    call str_end
    mov [rp_ext_pos], di
    mov si, ext_com
    call copy_str
    call .exists
    jnc .run
    mov di, [rp_ext_pos]
    mov si, ext_exe
    call copy_str
    call .exists
    jnc .run
    mov di, [rp_ext_pos]
    mov byte [di], 0
    jmp .unknown
.try:
    call .exists
    jc .unknown
.run:
    mov si, rp_name
    mov bx, rp_tail
    call exec_program
    jnc .ret
    cmp ax, ERR_NOMEM
    je .no_mem
    mov si, msg_mz_err
    jmp print
.no_mem:
    mov si, msg_no_mem
    jmp print
.unknown:
    mov si, msg_unknow
    jmp print
.ret:
    ret

; .exists : le fichier rp_name existe-t-il ? (CF=1 sinon)
.exists:
    mov si, rp_name
    call path_lookup
    jc .ex_no
    test byte [si+11], ATTR_DIR | ATTR_VOLUME
    jnz .ex_no
    clc
    ret
.ex_no:
    stc
    ret

rp_ext_pos  dw 0
ext_com     db '.COM', 0
ext_exe     db '.EXE', 0

; ============================================================================
; Petites fonctions utiles
; ============================================================================

; print : DS:SI = chaine terminee par 0
print:
    push ax
    push si
.loop:
    lodsb
    test al, al
    jz .done
    call putc
    jmp .loop
.done:
    pop si
    pop ax
    ret

; putc : AL = caractere (gere 10 = saut de ligne via BIOS)
putc:
    push ax
    push bx
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
    pop bx
    pop ax
    ret

newline:
    push si
    mov si, msg_crlf
    call print
    pop si
    ret

disk_error:
    mov si, msg_dir_err
    jmp print

; print_prompt : "A:\JEUX>"
print_prompt:
    mov al, [cur_drive]
    add al, 'A'
    call putc
    mov al, ':'
    call putc
    mov si, dv_cwd_path
    cmp byte [si], 0
    jne .path
    mov al, '\'
    call putc
    jmp .end
.path:
    call print
.end:
    mov al, '>'
    jmp putc

; print_dec32 : EAX en decimal
print_dec32:
    push eax
    push ebx
    push cx
    push edx
    mov ebx, 10
    xor cx, cx
.div:
    xor edx, edx
    div ebx
    push dx
    inc cx
    test eax, eax
    jnz .div
.out:
    pop ax
    add al, '0'
    call putc
    loop .out
    pop edx
    pop cx
    pop ebx
    pop eax
    ret

; print_dec32_pad : EAX en decimal, aligne a droite sur CX caracteres
print_dec32_pad:
    pushad
    mov esi, eax
    mov ebx, 10
    xor di, di                  ; nombre de chiffres
.count:
    inc di
    xor edx, edx
    div ebx
    test eax, eax
    jnz .count
.pad:
    cmp cx, di
    jbe .digits
    mov al, ' '
    call putc
    dec cx
    jmp .pad
.digits:
    mov eax, esi
    call print_dec32
    popad
    ret

; skip_spaces : avance SI tant qu'il y a des espaces
skip_spaces:
    cmp byte [si], ' '
    jne .ret
    inc si
    jmp skip_spaces
.ret:
    ret

; next_arg : SI = argument courant -> SI = argument suivant. CF s'il n'y en a pas.
next_arg:
.skip_word:
    mov al, [si]
    call is_end
    je .spaces
    inc si
    jmp .skip_word
.spaces:
    call skip_spaces
    cmp byte [si], 0
    je .none
    clc
    ret
.none:
    stc
    ret

; terminate_arg : met un 0 a la fin de l'argument SI (sans bouger SI)
terminate_arg:
    push si
.loop:
    mov al, [si]
    call is_end
    je .end
    inc si
    jmp .loop
.end:
    mov byte [si], 0
    pop si
    ret

; last_component : SI = chemin -> SI = debut du dernier nom
last_component:
    push ax
    push di
    mov di, si
.loop:
    lodsb
    test al, al
    jz .done
    cmp al, '\'
    je .mark
    cmp al, '/'
    je .mark
    cmp al, ':'
    je .mark
    jmp .loop
.mark:
    mov di, si
    jmp .loop
.done:
    mov si, di
    pop di
    pop ax
    ret

; str_end : DI = chaine -> DI = son 0 final
str_end:
    cmp byte [di], 0
    je .ret
    inc di
    jmp str_end
.ret:
    ret

; copy_str : DS:SI -> ES:DI (0 final compris)
copy_str:
    lodsb
    stosb
    test al, al
    jnz copy_str
    ret

; read_line : lit une ligne au clavier dans ES:DI (255 car. max)
read_line:
    xor cx, cx
.next:
    mov ah, 0x00
    int 0x16
    cmp al, 13
    je .done
    cmp al, 8
    je .backspace
    cmp al, 32
    jb .next
    cmp cx, 254
    jae .next
    stosb
    inc cx
    call putc
    jmp .next
.backspace:
    test cx, cx
    jz .next
    dec di
    dec cx
    mov al, 8
    call putc
    mov al, ' '
    call putc
    mov al, 8
    call putc
    jmp .next
.done:
    xor al, al
    stosb
    jmp newline

; strcmp : compare la commande en SI (arret sur espace ou 0) avec DI
; Insensible aux majuscules. CF=1 si identique.
strcmp:
    push bx
    push si
    push di
.loop:
    mov al, [di]
    test al, al
    jz .cmd_end
    mov bl, [si]
    cmp bl, 'A'
    jb .compare
    cmp bl, 'Z'
    ja .compare
    add bl, 0x20
.compare:
    cmp al, bl
    jne .no_match
    inc si
    inc di
    jmp .loop
.cmd_end:
    mov al, [si]
    call is_end
    je .match
.no_match:
    pop di
    pop si
    pop bx
    clc
    ret
.match:
    pop di
    pop si
    pop bx
    stc
    ret

; ============================================================================
; Vecteurs d'interruption
; ============================================================================
install_vectors:
    push es
    xor ax, ax
    mov es, ax
    cli
    mov word [es:0x20*4], int20_handler
    mov [es:0x20*4+2], cs
    mov word [es:0x21*4], int21_handler
    mov [es:0x21*4+2], cs
    mov word [es:0x22*4], int22_default
    mov [es:0x22*4+2], cs
    mov word [es:0x23*4], int23_default
    mov [es:0x23*4+2], cs
    mov word [es:0x24*4], int24_default
    mov [es:0x24*4+2], cs
    mov word [es:0x27*4], int27_handler
    mov [es:0x27*4+2], cs
    mov word [es:0x28*4], iret_stub
    mov [es:0x28*4+2], cs
    mov word [es:0x29*4], int29_handler
    mov [es:0x29*4+2], cs
    ; multiplex (XMS, DPMI, Windows...) et EMS : "rien d'installe".
    ; Un simple IRET laisse AX inchange, ce qui veut dire "absent".
    mov word [es:0x2F*4], iret_stub
    mov [es:0x2F*4+2], cs
    mov word [es:0x67*4], iret_stub
    mov [es:0x67*4+2], cs
    sti
    pop es
    ret

iret_stub:
int22_default:
    iret

; Ctrl-C : on ignore
int23_default:
    clc
    iret

; erreur critique : "echouer" (AL=3)
int24_default:
    mov al, 3
    iret

; INT 29h : sortie rapide d'un caractere
int29_handler:
    push ax
    push bx
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
    pop bx
    pop ax
    iret

%include "fs.inc"
%include "file.inc"
%include "mem.inc"
%include "exec.inc"
%include "dos.inc"
%ifdef DISK_EDITION
%include "disk_edition.inc"
%endif

; ============================================================================
; Donnees
; ============================================================================
boot_drive   db 0

msg_welcome  db 'MicroDOS ', VERSION_STR, ' ', EDITION_STR, ' - Shell', 13, 10
             db 'tapez "help" pour la liste de commandes.', 13, 10, 10, 0
msg_help     db 'Liste des Commandes', 13, 10
             db ' help             - cette aide', 13, 10
             db ' dir [chemin]     - liste les fichiers', 13, 10
             db ' type <fichier>   - affiche un fichier', 13, 10
             db ' <programme>      - execute un .COM ou un .EXE', 13, 10
             db ' a: b: c:         - change de lecteur', 13, 10
             db ' cd [dossier]     - change de repertoire', 13, 10
             db ' mkdir / rmdir    - cree / supprime un dossier', 13, 10
             db ' copy <src> <dst> - copie un fichier', 13, 10
             db ' del <fichier>    - supprime un fichier', 13, 10
             db ' ren <anc> <nouv> - renomme un fichier', 13, 10
             db ' mkfile <nom>     - cree un fichier vide', 13, 10
             db ' diskcopy a: b:   - copie une disquette entiere', 13, 10
             db ' label <nom>      - renomme la disquette', 13, 10
             db ' vol              - affiche le nom du volume', 13, 10
             db ' fatinfo / mem    - espace disque / memoire libre', 13, 10
             db ' reset            - relit la disquette', 13, 10
%ifdef DISK_EDITION
             db ' fdisk            - partitionne le disque dur', 13, 10
             db ' format c:        - formate le disque dur', 13, 10
             db ' setup            - installe MicroDOS sur le disque dur', 13, 10
%endif
             db ' echo pause cls ver date time reboot rebootdos', 13, 10, 0
msg_unknow   db 'Commande inconnue. Tapez "help".', 13, 10, 0
msg_ver      db 'MicroDOS ', VERSION_STR, ' ', EDITION_STR, 13, 10, 0
msg_reboot   db 'Redemarrage en cours...', 13, 10, 0
msg_rebootdos db 'Redemarrage de MicroDOS...', 13, 10, 0
msg_dir_err  db 'Erreur lecture disque.', 13, 10, 0
msg_not_found db 'Fichier non trouve.', 13, 10, 0
msg_reset    db 'Disque reinitialise', 13, 10, 0
msg_pause    db 'Appuyez sur une touche...', 13, 10, 0
msg_date_prefix db 'Date : ', 0
msg_time_prefix db 'Heure : ', 0
msg_fatinfo  db 'Clusters libres : ', 0
msg_bytes_free db ' octets libres', 0
msg_bytes    db ' octets', 13, 10, 0
msg_mem      db 'Memoire conventionnelle libre : ', 0
msg_dir_sp   db '   ', 0
msg_dir_files db ' fichier(s) ', 0
msg_dir_dirs db ' dossier(s) ', 0
msg_dir_tag  db '     <DIR>', 0
msg_crlf     db 13, 10, 0
msg_type_usage db 'Usage : type FICHIER.TXT', 13, 10, 0
msg_mkfile_ok db 'Fichier cree', 13, 10, 0
msg_mkfile_err db 'Erreur creation', 13, 10, 0
msg_mkfile_usage db 'Usage : mkfile NOM.TXT', 13, 10, 0
msg_copy_ok  db 'Fichier copie.', 13, 10, 0
msg_copy_err db 'Erreur de copie', 13, 10, 0
msg_copy_self db 'Impossible de copier un fichier sur lui-meme.', 13, 10, 0
msg_copy_usage db 'Usage : copy SOURCE DESTINATION', 13, 10, 0
msg_del_ok   db 'Fichier supprime.', 13, 10, 0
msg_del_err  db 'Erreur de Suppression', 13, 10, 0
msg_del_protected db 'Suppression refusee : fichier systeme.',  13, 10, 0
msg_del_dir  db 'Impossible de suprimer un repertoire.', 13, 10, 0
msg_del_usage db 'Usage : del NOM.TXT', 13, 10, 0
msg_ren_ok   db 'Fichier renomme.', 13, 10, 0
msg_ren_err  db 'Erreur renomage.', 13, 10, 0
msg_ren_usage db 'Usage : ren <ANCIEN NOM> <NOUVEAU NOM>', 13, 10, 0
msg_ren_protected db 'Impossible de Renommer : fichier systeme.', 13, 10, 0
msg_cd_err   db 'Repertoire Introuvable', 13, 10, 0
msg_mkdir_ok db 'Repertoire cree.', 13, 10, 0
msg_mkdir_err db 'Erreur lors de la creation du dossier', 13, 10, 0
msg_mkdir_usage db 'Usage : mkdir <NOM DU DOSSIER>', 13, 10, 0
msg_rmdir_ok db 'Dossier suprime.', 13, 10, 0
msg_rmdir_err db 'Erreur de suppression (le dossier doit etre vide)', 13, 10, 0
msg_rmdir_usage db 'Usage : rmdir <NOM DU DOSSIER>', 13, 10, 0
msg_label_ok db 'Disquette renommee avec succes', 13, 10, 0
msg_label_usage db 'Usage : label <NOM>', 13, 10, 0
msg_label_err db 'Erreur de renommage', 13, 10, 0
msg_vol_name db 'Le nom du volume est : ', 0
msg_vol_err  db 'Impossible de lire le nom du volume', 13, 10, 0
msg_drive_err db 'Lecteur pas pret.', 13, 10, 0
msg_mz_err   db 'Erreur chargement du programme.', 13, 10, 0
msg_no_mem   db 'Pas assez de memoire.', 13, 10, 0
msg_dc_usage db 'Usage : diskcopy A: B:  (ou A: A: avec un seul lecteur)', 13, 10, 0
msg_dc_info  db 'Copie de ', 0
msg_dc_sectors db ' secteurs', 13, 10, 0
msg_dc_insert_src db 'Inserez la disquette SOURCE puis appuyez sur une touche', 0
msg_dc_insert_dst db 'Inserez la disquette CIBLE puis appuyez sur une touche', 0
msg_dc_ok    db 'Copie de la disquette terminee.', 13, 10, 0
msg_dc_src_err db 'Erreur de lecture de la disquette source.', 13, 10, 0
msg_dc_dst_err db 'Erreur d ecriture sur la disquette cible.', 13, 10, 0

cmd_help     db 'help', 0
cmd_dir      db 'dir', 0
cmd_type     db 'type', 0
cmd_cls      db 'cls', 0
cmd_ver      db 'ver', 0
cmd_reboot   db 'reboot', 0
cmd_rebootdos db 'rebootdos', 0
cmd_reset    db 'reset', 0
cmd_echo     db 'echo', 0
cmd_pause    db 'pause', 0
cmd_date     db 'date', 0
cmd_time     db 'time', 0
cmd_fatinfo  db 'fatinfo', 0
cmd_mkfile   db 'mkfile', 0
cmd_copy     db 'copy', 0
cmd_del      db 'del', 0
cmd_erase    db 'erase', 0
cmd_ren      db 'ren', 0
cmd_cd       db 'cd', 0
cmd_chdir    db 'chdir', 0
cmd_mkdir    db 'mkdir', 0
cmd_md       db 'md', 0
cmd_rmdir    db 'rmdir', 0
cmd_rd       db 'rd', 0
cmd_label    db 'label', 0
cmd_vol      db 'vol', 0
cmd_mem      db 'mem', 0
cmd_diskcopy db 'diskcopy', 0
cmd_drive_a  db 'a:', 0
cmd_drive_b  db 'b:', 0
cmd_drive_c  db 'c:', 0
cmd_fdisk    db 'fdisk', 0
cmd_format   db 'format', 0
cmd_setup    db 'setup', 0

buffer       times 256 db 0

kernel_end:
; Le noyau doit tenir sous les tampons (KERNEL_MAX) : sinon erreur ici
times -(($ - $$) > KERNEL_MAX) db 0
