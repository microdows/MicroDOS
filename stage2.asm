[BITS 16]
[ORG 0x0000]

start:
    push cs
    pop ds
    push cs
    pop es

	cli
	push es
	xor ax, ax
	mov es, ax
	mov word [es:0x84], int21_handler
	mov word [es:0x86], cs
	pop es
	sti

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
    mov di, cmd_type
    call strcmp
    jc do_type

    mov si, buffer
    mov di, cmd_ver
    call strcmp
    jc do_ver

    mov si, buffer
    mov di, cmd_cls
    call strcmp
    jc do_cls

    mov si, buffer
	mov di, cmd_reset
	call strcmp
	jc do_reset

    mov si, buffer
    mov di, cmd_echo
    call strcmp
    jc do_echo

    mov si, buffer
    mov di, cmd_pause
    call strcmp
    jc do_pause

    mov si, buffer
    mov di, cmd_date
    call strcmp
    jc do_date

    mov si, buffer
    mov di, cmd_time
    call strcmp
    jc do_time

    mov si, buffer
    mov di, cmd_fatinfo
    call strcmp
    jc do_fatinfo

    mov si, buffer
    mov di, cmd_mkfile
    call strcmp
    jc do_mkfile

    mov si, buffer
    mov di, cmd_rebootdos
    call strcmp
    jc do_rebootdos

    mov si, buffer
    mov di, cmd_reboot
    call strcmp
    jc do_reboot

    mov si, buffer
    call do_exec
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

do_reset:
	mov ah, 0x00
	mov dl, 0
	int 0x13
	mov si, msg_reset
	call print
	jmp main_loop

do_echo:

	mov si, buffer
	add si, 4
	cmp byte [si], ' '
	jne .done
	inc si

.print:
	lodsb
	or al, al
	jz .newline
	mov ah, 0x0E
	int 0x10
	jmp .print

.newline:

.done:

	mov ah, 0x0E
	mov al, 13
	int 0x10
	mov al, 10
	int 0x10
	jmp main_loop

do_pause:

	mov si, msg_pause
	call print
	mov ah, 0x00
	int 0x16
	jmp main_loop

do_date:

	mov ah, 0x04
	int 0x1A
	; CH=siécle BCD, CL=année BCD, DH=mois BCD, DL=jour BCD
	mov si, msg_date_prefix
	call print
	mov al, dl
	call print_bcd
	mov al, '/'
	mov ah, 0x0E
	int 0x10
	mov al, ch
	call print_bcd
	mov al, cl
	call print_bcd
	mov ah, 0x0E
	mov al, 13
	int 0x10
	mov al, 10
	int 0x10
	jmp main_loop

do_time:

	mov ah, 0x02
	int 0x1A
	; CH=Heure BCD, CL, minutes BCD, DH=secondes BCD
	mov si, msg_time_prefix
	call print
	mov al, ch
	call print_bcd
	mov al, ':'
	mov ah, 0x0E
	int 0x10
	mov al, cl
	call print_bcd
	mov ah, 0x0E
	mov al, 13
	int 0x10
	mov al, 10
	int 0x10
	jmp main_loop

print_bcd:

	push ax
	shr al, 4
	add al, '0'
	mov ah, 0x0E
	int 0x10
	pop ax
	and al, 0x0F
	add al, '0'
	mov ah, 0x0E
	int 0x10
	ret

do_fatinfo:

	call fat_find_free

	push ax

	mov si, msg_fatinfo
	call print

	pop ax

	mov cx, 0
	mov bx, 10

.divide:

	xor dx, dx
	div bx
	push dx
	inc cx
	test ax, ax
	jnz .divide

.print:

	pop dx
	mov al, dl
	add al, '0'
	mov ah, 0x0E
	int 0x10
	loop .print

	mov ah, 0x0E
	mov al, 13
	int 0x10
	mov al, 10
	int 0x10
	jmp main_loop

do_mkfile:
    mov si, buffer
    add si, 7
    cmp byte [si-1], ' '
    jne .usage
    call create_file
    cmp ax, 0xFFFF
    je .error
    mov si, msg_mkfile_ok
    call print
    jmp main_loop

.error:
    mov si, msg_mkfile_err
    call print
    jmp main_loop

.usage:
    mov si, msg_mkfile_usage
    call print
    jmp main_loop

; ---------- DIR ----------
do_dir:
    mov ax, 0x1000
    mov es, ax
    mov bx, 0x8000
    mov ah, 0x02
    mov al, 14
    mov ch, 0
    mov cl, 2
    mov dh, 1
    mov dl, 0
    int 0x13
    jc .error

    mov si, 0x8000
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

; ---------- TYPE ----------
do_type:
    mov si, buffer
    add si, 5
    call build_fat_name
    call find_file
    jc .not_found

    mov ax, [si + 26]
    mov bx, [si + 28]
    mov [file_size], bx

    add ax, 31
    call lba_to_chs

    mov ax, 0x1000
    mov es, ax
    mov bx, 0xA000
    mov ah, 0x02
    mov al, 1
    int 0x13
    jc .read_err

    mov si, 0xA000
    mov cx, [file_size]
.print:
    test cx, cx
    jz .done
    mov al, [si]
    mov ah, 0x0E
    int 0x10
    inc si
    dec cx
    jmp .print

.done:
    mov ah, 0x0E
    mov al, 13
    int 0x10
    mov al, 10
    int 0x10
    jmp main_loop

.not_found:
    mov si, msg_not_found
    call print
    jmp main_loop

.read_err:
    mov si, msg_dir_err
    call print
    jmp main_loop

; ---------- Convertit "HELLO.TXT" en "HELLO   TXT" ----------
build_fat_name:
    push si
    push di
    mov di, fatname

    mov cx, 8
.copy_name:
    mov al, [si]
    cmp al, '.'
    je .pad_name
    cmp al, 0
    je .pad_name
    mov [di], al
    inc di
    inc si
    dec cx
    jnz .copy_name
    cmp byte [si], '.'
    jne .copy_ext
    inc si
    jmp .copy_ext

.pad_name:
    test cx, cx
    jz .skip_dot
    mov byte [di], ' '
    inc di
    dec cx
    jmp .pad_name

.skip_dot:
    cmp byte [si], '.'
    jne .copy_ext
    inc si

.copy_ext:
    mov cx, 3
.copy_ext_loop:
    mov al, [si]
    cmp al, 0
    je .pad_ext
    mov [di], al
    inc di
    inc si
    dec cx
    jnz .copy_ext_loop
    jmp .done

.pad_ext:
    test cx, cx
    jz .done
    mov byte [di], ' '
    inc di
    dec cx
    jmp .pad_ext

.done:
    pop di
    pop si
    ret

; ---------- Cherche [fatname] dans le repertoire racine ----------
; Retourne SI = entree trouvee, ou Carry=1 si pas trouve
find_file:
    mov ax, 0x1000
    mov es, ax
    mov bx, 0x8000
    mov ah, 0x02
    mov al, 14
    mov ch, 0
    mov cl, 2
    mov dh, 1
    mov dl, 0
    int 0x13
    jc .err

    mov di, 0x8000
    mov bp, 224

.loop:
    mov al, [di]
    cmp al, 0
    je .not_found
    cmp al, 0xE5
    je .next

    push si
    push di
    mov si, fatname
    mov cx, 11
.cmp:
    mov al, [si]
    mov bl, [di]
    cmp al, bl
    jne .no_match
    inc si
    inc di
    loop .cmp
    pop di
    pop si
    mov si, di
    clc
    ret

.no_match:
    pop di
    pop si

.next:
    add di, 32
    dec bp
    jnz .loop

.not_found:
    stc
    ret

.err:
    stc
    ret

; ---------- Convertit un LBA (AX) en CHS ----------
lba_to_chs:
    push ax
    push bx

    xor dx, dx
    mov bx, 18
    div bx
    mov cl, dl
    inc cl

    xor dx, dx
    mov bx, 2
    div bx
    mov ch, al
    mov dh, dl
    mov dl, 0

    pop bx
    pop ax
    ret

;----------- FAT12 : lire une entrée --------------
;Entree : AX = numero de cluster
;Sortie AX = valeur de l'entrée FAT

fat_get:

	push bx
	push cx
	push dx
	push si
	push es

	mov [cluster_temp], ax
	mov bx, ax
	mov ax, ax
	shr ax, 1
	add ax, bx
	push ax

	xor dx, dx
	mov cx, 512
	div cx
	add ax, 1
	call lba_to_chs

	mov ax, 0x1000
	mov es, ax
	mov bx, 0x0C00
	mov ah, 0x02
	mov al, 1
	mov dl, 0
	int 0x13
	jc .error

	pop ax
	and ax, 0x01FF
	mov si, 0x0C00
	add si, ax
	mov ax, [si]

	mov bx, [cluster_temp]
	test bx, 1
	je .even
	shr ax, 4

.even:
	and ax, 0x0FFF

	pop es
	pop si
	pop dx
	pop cx
	pop bx
	ret

.error:

	xor ax, ax
	pop ax
	pop es
	pop si
	pop dx
	pop cx
	pop bx
	ret

;--------------- FAT12 : ecrire une entrée ----------------
;Entrée : AX = cluster, DX= Valeur

fat_set:

	push ax
	push bx
	push cx
	push dx
	push si
	push es

	mov [cluster_temp], ax
	mov bx, ax
	push dx

	mov ax, ax
	shr ax, 1
	add ax, bx
	push ax

	xor dx, dx
	mov cx, 512
	div cx
	add ax, 1
	call lba_to_chs

	mov ax, 0x1000
	mov es, ax
	mov bx, 0x0C00
	mov ah, 0x02
	mov al, 1
	mov dl, 0
	int 0x13
	jc .error

	pop ax
	and ax, 0x01FF
	mov si, 0x0C00
	add si, ax

	mov ax, [si]
	pop dx

	mov bx, [cluster_temp]
	test bx, 1
	jnz .odd

	and ax, 0xF000
	and dx, 0x0FFF
	or ax, dx
	jmp .write

.odd:

	and ax, 0x000F
	shl dx, 4
	and dx, 0xFFF0
	or ax, dx

.write:
	mov [si], ax
	mov ax, 0x1000
	mov es, ax
	mov bx, 0x0C00
	mov ch, 0
	mov cl, 2
	mov dh, 0
	mov dl, 0
	mov ah, 0x03
	mov al, 1
	int 0x13

.error:

	pop es
	pop si
	pop dx
	pop cx
	pop bx
	pop ax
	ret

fat_find_free:

	push bx
	push cx

	mov ax, 2

.loop:

	push ax
	call fat_get
	pop bx
	test ax, ax
	jz .found

	mov ax, bx
	inc ax
	cmp ax, 2848
	jb .loop

	mov ax, 0xFFFF
	jmp .done

.found:

	mov ax, bx

.done:

	pop cx
	pop bx
	ret

;------------------- Trouver une entrée libre ---------------------

find_free_entry:

	push ax
	push bx
	push cx
	push dx
	push es

	mov ax, 0x1000
	mov es, ax
	mov bx, 0x8000
	mov ah, 0x02
	mov al, 14
	mov ch, 0
	mov cl, 2
	mov dh, 1
	mov dl, 0
	int 0x13
	jc .error

	mov si, 0x8000
	mov bp, 224

.loop:

	mov al, [si]
	cmp al, 0
	je .found
	cmp al, 0xE5
	je .found

	add si, 32
	dec bp
	jnz .loop

	stc
	jmp .done

.found:

	clc
	jmp .done

.error:

	stc

.done:

	pop es
	pop dx
	pop cx
	pop bx
	pop ax
	ret

;-------------------------- Ecrire une entrée ---------------------

write_dir_entry:

	push ax
	push bx
	push cx
	push dx
	push si
	push di
	push es

	mov cx, 11

.copy_name:

	lodsb
	stosb
	loop .copy_name

	xor al, al
	stosb

	mov cx, 10

.reserved:

	xor al, al
	stosb
	loop .reserved

	xor ax, ax
	stosw
	stosw

	mov ax, dx
	stosw

	mov ax, bx
	stosw
	xor ax, ax
	stosw

	mov ax, 0x1000
	mov es, ax
	mov bx, 0x8000
	mov ah, 0x03
	mov al, 14
	mov ch, 0
	mov cl, 2
	mov dh, 1
	mov dl, 0
	int 0x13

	pop es
	pop di
	pop si
	pop dx
	pop cx
	pop bx
	pop ax
	ret

create_file:

	push bx
	push cx
	push dx
	push si
	push di

	call build_fat_name

	call fat_find_free
	cmp ax, 0xFFFF
	je .error
	mov [cluster_temp], ax

	mov dx, 0xFFF
	call fat_set

	call find_free_entry
	jc .error
	mov di, si
	mov si, fatname
	mov ax, [cluster_temp]
	mov dx, ax
	xor bx, bx
	call write_dir_entry

	mov ax, [cluster_temp]

	pop di
	pop si
	pop dx
	pop cx
	pop bx
	ret

.error:

	mov ax, 0xFFFF
	pop di
	pop si
	pop dx
	pop cx
	pop bx
	ret

; ---------- Affichage ----------
print:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp print
.done:
    ret

; ---------- Exécution d'un .COM ---------
do_exec:

	push ds
	push es

	mov si, buffer
	call build_com_name
	call find_file
	jc .not_found

	mov ax, [si + 26]
	mov bx, [si + 28]
	mov [file_size], bx

	add ax, 31
	call lba_to_chs

	mov ax, 0x2000
	mov es, ax
	mov bx, 0x100
	mov ah, 0x02
	mov al, 1
	int 0x13
	jc .read_err

	push cs
	push .ret_from_com

	mov ax, 0x2000
	mov ds, ax
	mov es, ax
	jmp 0x2000:0x100

.ret_from_com:

	pop es
	pop ds
	ret

.not_found:

	mov si, msg_unknow
	call print
	pop es
	pop ds
	ret

.read_err:

	mov si, msg_dir_err
	call print
	pop es
	pop ds
	ret

; --------Convertit "Hello" en "HELLO COM"------------

build_com_name:

	push si
	push di
	mov di, fatname

	mov cx, 8

.copy_name:

	mov al, [si]
	cmp al, 0
	je .pad_name
	cmp al, ' '
	je .pad_name
	cmp al, 'a'
	jb .not_lower
	cmp al, 'z'
	ja .not_lower
	sub al, 32

.not_lower:

	mov [di], al
	inc di
	inc si
	dec cx
	jnz .copy_name
	jmp .add_com

.pad_name:

	test cx, cx
	jz .add_com
	mov byte [di], ' '
	inc di
	dec cx
	jmp .pad_name

.add_com:

	mov byte [di], 'C'
	mov byte [di+1], 'O'
	mov byte [di+2], 'M'

	pop di
	pop si
	ret

; ---------- Lecture clavier ----------
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

; ---------- Comparaison de chaines ----------
strcmp:

push si
push di

.loop:

	mov al, [di]
	test al, al
	jz .cmd_end
	mov bl, [si]
	cmp al, bl
	jne .no_match
	inc si
	inc di
	jmp .loop

.cmd_end:

	mov al, [si]
	cmp al, 0
	je .match
	cmp al, ' '
	je .match
	jmp .no_match

.match:

	pop di
	pop si
	stc
	ret

.no_match:

	pop di
	pop si
	clc
	ret

; ----------------Handler INT 21 (API DOS)------------------------------
int21_handler:

	sti
	push ax
	push bx
	push cx
	push si
	push dx
	push di
	push ds
	push es

	cmp ah, 0x09
	je .ah09
	cmp ah, 0x02
	je .ah02
	cmp ah, 0x4C
	je .ah4c
	cmp ah, 0x30
	je .ah30
	cmp ah, 0x0B
	je .ah0b
	cmp ah, 0x01
	je .ah01
	cmp ah, 0x08
	je .ah08
	cmp ah, 0x2C
	je .ah2c
	cmp ah, 0x00
	je .ah00
	cmp ah, 0x0F
	je .ah0f
	cmp ah, 0x25
	je .ah25
	cmp ah, 0x35
	je .ah35
	cmp ah, 0x3C
	je .ah3c

	jmp .done

.ah09:

	mov si, dx

.loop09:

	lodsb
	cmp al, '$'
	je .done
	mov ah, 0x0E
	int 0x10
	jmp .loop09

.ah02:

	mov al,dl
	mov ah, 0x0E
	int 0x10
	jmp .done

.ah30:

	mov al, 3
	mov ah, 30
	jmp .done

.ah0b:

	mov ah, 0x01
	int 0x16
	jz .nokey
	mov al, 0xFF
	jmp .done

.nokey:

	xor al, al
	jmp .done

.ah01:

	mov ah, 0x00
	int 0x16
	mov ah, 0x0E
	int 0x10
	jmp .done

.ah08:

	mov ah, 0x00
	int 0x16
	jmp .done

.ah2c:

	mov ah, 0x02
	int 0x1A
	jmp .done

.ah00:

	jmp .ah4c

.ah0f:

	xor al, al
	jmp .done

.ah25:

	push ds
	push ax
	xor ax, ax
	pop ax
	mov bl, al
	xor bh, bh
	shl bx, 1
	shl bx, 1
	mov [bx], dx
	pop ax
	mov [bx+2], ax
	jmp .done

.ah35:

	push ds
	mov bl, al
	xor bh, bh
	shl bx, 1
	shl bx, 1
	xor ax, ax
	mov ds, ax
	mov ax, [bx]
	mov bx, [bx+2]
	mov es, bx
	mov bx, ax
	pop ds
	jmp .done

.ah3c:

	mov si, dx
	call create_file
	cmp ax, 0xFFFF
	je .error3c
	clc
	jmp .done

.error3c:

	stc
	jmp .done

.ah4c:

	pop es
	pop ds
	pop di
	pop dx
	pop si
	pop cx
	pop bx
	pop ax
	add sp, 6
	retf

.done:

	pop es
	pop ds
	pop di
	pop dx
	pop si
	pop cx
	pop bx
	pop ax
	iret


; ---------- Donnees ----------
msg_welcome  db 'MicroDOS v0.8 - Shell', 13, 10
             db 'tapez "help" pour la liste de commandes.', 13, 10, 10, 0
msg_prompt   db '> ', 0
msg_help     db 'Liste des Commandes', 13, 10
             db ' help - cette aide', 13, 10
             db ' dir - liste les fichiers', 13, 10
             db ' type - affiche un fichier', 13, 10
             db ' <programme> - execute un fichier .COM', 13, 10
             db ' reset - reinitialise la disquette', 13, 10
             db ' cls - efface l ecran', 13, 10
             db ' ver - version', 13, 10
             db ' reboot - redemarre le PC', 13, 10
             db ' rebootdos - redemarre MicroDOS', 13, 10
             db ' echo <txt> - affiche un texte', 13, 10
             db ' pause - attend une touche', 13, 10
             db ' date - affiche la date', 13, 10
             db ' time - affiche l heure', 13, 10
	     db ' mkfile <nom> - cree un fichier', 13, 10, 0
msg_unknow   db 'Commande inconnue. Tapez "help".', 13, 10, 0
msg_ver      db 'MicroDOS v0.8', 13, 10, 0
msg_reboot   db 'Redemarrage en cours...', 13, 10, 0
msg_rebootdos db 'Redemarrage de MicroDOS...', 13, 10, 0
msg_dir_err  db 'Erreur lecture disque.', 13, 10, 0
msg_not_found db 'Fichier non trouve.', 13, 10, 0
msg_reset db 'Disque reinitialise', 13, 10, 0
msg_pause db 'Appuyez sur une touche...', 13, 10, 0
msg_date_prefix db 'Date :', 0
msg_time_prefix db 'Heure :', 0
msg_fatinfo db 'Cluster libre : ', 0
msg_mkfile_ok db 'Fichier cree', 13, 10, 0
msg_mkfile_err db 'Erreur creation', 13, 10, 0
msg_mkfile_usage db 'Usage : mkfile NOM.TXT', 13, 10, 0

cmd_help     db 'help', 0
cmd_dir      db 'dir', 0
cmd_type     db 'type', 0
cmd_cls      db 'cls', 0
cmd_ver      db 'ver', 0
cmd_reboot   db 'reboot', 0
cmd_rebootdos db 'rebootdos', 0
cmd_reset db 'reset', 0
cmd_echo db 'echo', 0
cmd_pause db 'pause', 0
cmd_date db  'date', 0
cmd_time db 'time', 0
cmd_fatinfo db 'fatinfo', 0
cmd_mkfile db 'mkfile', 0

fatname      times 11 db 0
file_size    dw 0
cluster_temp dw 0
buffer       times 256 db 0
