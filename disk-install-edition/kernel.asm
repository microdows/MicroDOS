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

    mov word [current_cluster], 0
	mov byte [current_drive], 0
	mov byte [fat16_ready], 0

    ; --- INITIALISATION DU DISQUE DUR (FAT16) ---
    mov eax, 0
    mov cx, 1
    mov bx, 0x7000
    push cs
    pop es
    call lba_read
    jc .no_hdd

    mov bx, 0x7000
    push cs
    pop es
    call mbr_find_partition
    jc .no_hdd

    mov [saved_part_lba], eax

    mov cx, 1
    mov bx, 0x7200
    push cs
    pop es
    call lba_read
    jc .no_hdd

    mov bx, 0x7200
    push cs
    pop es
    call bpb16_parse
    
    mov eax, [saved_part_lba]
    call fat16_init
	mov byte [fat16_ready], 1
.no_hdd:

	mov si, init_path
	mov di, current_path
	call copy_str

    mov si, msg_welcome
    call print

main_loop:
	mov si, current_path
	call print
    mov si, msg_prompt
    call print

    mov di, buffer
    call read_line

	mov al, [buffer]
	and al, 0xDF
	cmp al, 'A'
	jne .check_c
	cmp byte [buffer+1], ':'
	jne .check_c
	cmp byte [buffer+2], 0
	jne .check_c
	mov byte [current_drive], 0
	mov byte [current_path], 'A'
	mov byte [current_path+1], ':'
	mov byte [current_path+2], '\'
	mov byte [current_path+3], 0
	jmp main_loop

.check_c:
	cmp al, 'C'
	jne .not_a_drive
	cmp byte [buffer+1], ':'
	jne .not_a_drive
	cmp byte [buffer+2], 0
	jne .not_a_drive
	mov byte [current_drive], 1
	mov byte [current_path], 'C'
	mov byte [current_path+1], ':'
	mov byte [current_path+2], '\'
	mov byte [current_path+3], 0
	jmp main_loop

.not_a_drive:
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
    mov di, cmd_copy_name
    call strcmp
    jc do_copy

    mov si, buffer
    mov di, cmd_del
    call strcmp
    jc do_del

    mov si, buffer
    mov di, cmd_ren
    call strcmp
    jc do_ren

    mov si, buffer
    mov di, cmd_cd
    call strcmp
    jc do_cd

    mov si, buffer
    mov di, cmd_mkdir
    call strcmp
    jc do_mkdir

    mov si, buffer
    mov di, cmd_rmdir
    call strcmp
    jc do_rmdir

    mov si, buffer
    mov di, cmd_rebootdos
    call strcmp
    jc do_rebootdos

    mov si, buffer
    mov di, cmd_reboot
    call strcmp
    jc do_reboot

	mov si, buffer
	mov di, cmd_format
	call strcmp
	jc do_format

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
    call resolve_path
    jc .error
    mov word [src_size], 0
    call create_file
    cmp ax, 0xFFFF
    je .error
    mov si, msg_mkfile_ok
    call print
    jmp .exit
.error:
    mov si, msg_mkfile_err
    call print
    jmp .exit
.usage:
    mov si, msg_mkfile_usage
    call print
    jmp .exit
.exit:
	mov ax, [saved_cluster]
	mov [current_cluster], ax
	jmp main_loop

do_format:

	mov si, msg_format_warn
	call print
	mov ah, 0x00
	int 0x16

	mov ah, 0x0E
	mov al, 13
	int 0x10
	mov al, 10
	int 0x10

	call format_hdd
	jc .error
	
	mov si, msg_format_ok
	call print
	jmp main_loop

	.error:

	mov si, msg_format_err
	call print
	jmp main_loop

do_copy:
    mov word [parse_skip], 5
    call parse_two_args
    jc .error

    mov si, [arg2_ptr]
    mov al, [si]
    and al, 0xDF
    cmp al, 'C'
    jne .copy_fat12
    cmp byte [si+1], ':'
    jne .copy_fat12

    call cmd_copy
    jmp main_loop

.copy_fat12:
    call copy_file
    cmp ax, 0xFFFF
    je .error
    mov si, msg_copy_ok
    call print
    jmp main_loop

.error:
    mov si, msg_copy_err
    call print
    jmp main_loop

do_del:
	mov si, buffer
	add si, 4
	cmp byte [si-1], ' '
	jne .usage
	call resolve_path
	jc .error
	call del_file
	cmp ax, 0xFFFF
	je .error
	cmp ax, 0xFFFE
	je .protected
	cmp ax, 0xFFFD
	je .is_dir
	mov si, msg_del_ok
	call print
	jmp .exit
.error:
	mov si, msg_del_err
	call print
	jmp .exit
.protected:
	mov si, msg_del_protected
	call print
	jmp .exit
.usage:
	mov si, msg_del_usage
	call print
	jmp .exit
.is_dir:
	mov si, msg_del_dir
	call print
	jmp .exit
.exit:
	mov ax, [saved_cluster]
	mov [current_cluster], ax
	jmp main_loop

do_ren:
	mov si, buffer
	add si, 4
	cmp byte [si-1], ' '
	jne .usage
	call ren_file
	cmp ax, 0xFFFF
	je .error
	cmp ax, 0xFFFE
	je .protected
	mov si, msg_ren_ok
	call print
	jmp .exit
.error:
	mov si, msg_ren_err
	call print
	jmp .exit
.protected:
	mov si, msg_ren_protected
	call print
	jmp .exit
.usage:
	mov si, msg_ren_usage
	call print
	jmp .exit
.exit:
	mov ax, [saved_cluster]
	mov [current_cluster], ax
	jmp main_loop

do_cd:
	mov si, buffer
	add si, 3
	cmp byte [si-1], ' '
	jne .root
	call cd_dir
	cmp ax, 0xFFFF
	je .error
	jmp main_loop
.root:
	mov word [current_cluster], 0
	jmp main_loop
.error:
	mov si, msg_cd_err
	call print
	jmp main_loop

do_mkdir:
	mov si, buffer
	add si, 6
	cmp byte [si-1], ' '
	jne .usage
	call resolve_path
	jc .error
	call mkdir_dir
	cmp ax, 0xFFFF
	je .error
	mov si, msg_mkdir_ok
	call print
	jmp .exit
.error:
	mov si, msg_mkdir_err
	call print
	jmp .exit
.usage:
	mov si, msg_mkdir_usage
	call print
	jmp .exit
.exit:
	mov ax, [saved_cluster]
	mov [current_cluster], ax
	jmp main_loop

do_rmdir:
	mov si, buffer
	add si, 6
	cmp byte [si-1], ' '
	jne .usage
	call resolve_path
	jc .error
	call rmdir_dir
	cmp ax, 0xFFFF
	je .error
	mov si, msg_rmdir_ok
	call print
	jmp .exit
.error:
	mov si, msg_rmdir_err
	call print
	jmp .exit
.usage:
	mov si, msg_rmdir_usage
	call print
	jmp .exit
.exit:
    mov ax, [saved_cluster]
    mov [current_cluster], ax
    jmp main_loop

; ---------- DIR ----------
do_dir:
	cmp byte [current_drive], 0
	jne .dir_hdd

    ; --- FAT12 ---
    call load_current_dir
    jc .error_fat12
    mov si, 0x8000
    mov bp, 224
.loop_fat12:
    mov al, [si]
    cmp al, 0
    je .done_fat12
    cmp al, 0xE5
    je .next_fat12
    mov al, [si+11]
    cmp al, 0x0F
    je .next_fat12
    test al, 0x08
    jnz .next_fat12
    test al, 0x02
    jnz .next_fat12
    mov di, si
    mov cx, 8
.print_name_fat12:
    mov al, [di]
    cmp al, ' '
    je .name_done_fat12
    mov ah, 0x0E
    int 0x10
    inc di
    loop .print_name_fat12
.name_done_fat12:
    mov al, [si+8]
    cmp al, ' '
    je .no_ext_fat12
    mov ah, 0x0E
    mov al, '.'
    int 0x10
    mov di, si
    add di, 8
    mov cx, 3
.print_ext_fat12:
    mov al, [di]
    cmp al, ' '
    je .ext_done_fat12
    mov ah, 0x0E
    int 0x10
    inc di
    loop .print_ext_fat12
.ext_done_fat12:
.no_ext_fat12:
	push si
	mov al, [si+11]
	test al, 0X10
	jz .not_dir_fat12
	mov si, msg_dir_tag
	call print
.not_dir_fat12:
	pop si
	mov ah, 0x0E
	mov al, 13
	int 0x10
	mov al, 10
	int 0x10
.next_fat12:
    add si, 32
    dec bp
    jnz .loop_fat12
.done_fat12:
    jmp main_loop
.error_fat12:
    mov si, msg_dir_err
    call print
    jmp main_loop

    ; --- FAT16 ---
.dir_hdd:
	mov eax, [lba_rootdir]
	mov cx, 1
	mov bx, 0x8000
	push cs
	pop es
	call lba_read
	jc .error_hdd
	mov si, 0x8000
	mov bp, 16
.hdd_loop:
	mov al, [si]
	cmp al, 0
	je .done_hdd
	cmp al, 0xE5
	je .next_hdd
	mov al, [si+11]
	test al, 0x08
	jnz .next_hdd
	test al, 0x10
	jnz .next_hdd
	mov di, si
	mov cx, 8
.print_name_hdd:
	mov al, [di]
	cmp al, ' '
	je .name_done_hdd
	mov ah, 0x0E
	int 0x10
	inc di
	loop .print_name_hdd
.name_done_hdd:
	mov al, [si+8]
	cmp al, ' '
	je .no_ext_hdd
	mov ah, 0x0E
	mov al, '.'
	int 0x10
	mov di, si
	add di, 8
	mov cx, 3
.print_ext_hdd:
	mov al, [di]
	cmp al, ' '
	je .ext_done_hdd
	mov ah, 0x0E
	int 0x10
	inc di
	loop .print_ext_hdd
.ext_done_hdd:
.no_ext_hdd:
	mov ah, 0x0E
	mov al, 13
	int 0x10
	mov al, 10
	int 0x10
.next_hdd:
	add si, 32
	dec bp
	jnz .hdd_loop
.done_hdd:
	jmp main_loop
.error_hdd:
    mov si, msg_dir_err
    call print
    jmp main_loop

; ---------- TYPE ----------
do_type:
    cmp byte [current_drive], 0
    jne .type_hdd

    ; --- FAT12 ---
    mov si, buffer
    add si, 5
    call resolve_path
    call build_fat_name
    call find_file
    jc .not_found_fat12
    mov al, [si + 11]
    test al, 0x10
    jnz .not_found_fat12
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
    jc .read_err_fat12
    mov si, 0xA000
    mov cx, [file_size]
.print_fat12:
    test cx, cx
    jz .done_fat12
    mov al, [si]
    mov ah, 0x0E
    int 0x10
    inc si
    dec cx
    jmp .print_fat12
.done_fat12:
    mov ax, [saved_cluster]
    mov [current_cluster], ax
    mov ah, 0x0E
    mov al, 13
    int 0x10
    mov al, 10
    int 0x10
    jmp main_loop
.not_found_fat12:
    mov ax, [saved_cluster]
    mov [current_cluster], ax
    mov si, msg_not_found
    call print
    jmp main_loop
.read_err_fat12:
    mov ax, [saved_cluster]
    mov [current_cluster], ax
    mov si, msg_dir_err
    call print
    jmp main_loop

    ; --- FAT16 ---
.type_hdd:
    mov si, buffer
    add si, 5
    call build_fat_name
    call fat16_find_file
    jc .not_found_hdd
    mov ax, [si + 26]
    mov bx, [si + 28]
    mov [file_size], bx
    call fat16_read_file
    jc .read_err_hdd
    mov ax, 0x1000
    mov es, ax
    mov si, 0xA000
    mov cx, [file_size]
.print_hdd:
    test cx, cx
    jz .done_hdd
    mov al, [es:si]
    mov ah, 0x0E
    int 0x10
    inc si
    dec cx
    jmp .print_hdd
.done_hdd:
    mov ah, 0x0E
    mov al, 13
    int 0x10
    mov al, 10
    int 0x10
    jmp main_loop
.not_found_hdd:
    mov si, msg_not_found
    call print
    jmp main_loop
.read_err_hdd:
    mov si, msg_dir_err
    call print
    jmp main_loop

; ---------- Utilitaires FAT12 ----------
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
    cmp al, '\'
    je .pad_name
    cmp al, '/'
    je .pad_name
    cmp al, 'a'
    jb .store_name
    cmp al, 'z'
    ja .store_name
    sub al, 32
.store_name:
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
    cmp al, '\'
    je .pad_ext
    cmp al, '/'
    je .pad_ext
    cmp al, 'a'
    jb .store_ext
    cmp al, 'z'
    ja .store_ext
    sub al, 32
.store_ext:
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

find_file:
    call load_current_dir
    jc .err
    mov di, 0x8000
    mov bp, 224
.loop:
    mov al, [di]
    cmp al, 0
    je .not_found
    cmp al, 0xE5
    je .next
    mov al, [di+11]
    cmp al, 0x0F
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
	mov bx, 0x6000
	mov ah, 0x02
	mov al, 1
	mov dl, 0
	int 0x13
	jc .error
	pop ax
	and ax, 0x01FF
	mov si, 0x6000
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
	mov bx, 0x6000
	mov ah, 0x02
	mov al, 1
	mov dl, 0
	int 0x13
	jc .error
	pop ax
	and ax, 0x01FF
	mov si, 0x6000
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
	mov bx, 0x6000
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

free_cluster_chain:
	push ax
	push bx
	push cx
	mov bx, ax
.loop:
	cmp bx, 0xFF8
	jae .done
	cmp bx, 2
	jb .done
	mov [free_temp], bx
	mov ax, bx
	call fat_get
	mov cx, ax
	mov ax, [free_temp]
	xor dx, dx
	call fat_set
	mov bx, cx
	jmp .loop
.done:
	pop cx
	pop bx
	pop ax
	ret

load_current_dir:
	push ax
	push bx
	push cx
	push dx
	push es
	mov ax, [current_cluster]
	test ax, ax
	jz .root
	add ax, 31
	call lba_to_chs
	jmp .load
.root:
	mov ch, 0
	mov cl, 2
	mov dh, 1
.load:
	mov ax, 0x1000
	mov es, ax
	mov bx, 0x8000
	mov dl, 0
	mov ah, 0x02
	cmp word [current_cluster], 0
	jne .one_sector
	mov al, 14
	jmp .do_read
.one_sector:
	mov al, 1
.do_read:
	int 0x13
	pop es
	pop dx
	pop cx
	pop bx
	pop ax
	ret

find_free_handle:
	push bx
	push di
	mov di, open_files
	mov cx, 5
	mov bx, 5
.loop:
	cmp byte [di+6], 0
	je .found
	add di, 8
	inc bx
	loop .loop
	mov ax, 0xFFFF
	jmp .done
.found:
	mov ax, bx
.done:
	pop di
	pop bx
	ret

find_handle:
	cmp ax, 5
	jb .error
	cmp ax, 10
	jae .error
	sub ax, 5
	shl ax, 3
	mov di, open_files
	add di, ax
	clc
	ret
.error:
	stc
	ret

copy_str:
	lodsb
	stosb
	test al, al
	jnz copy_str
	ret

append_path:
	push ax
	push di
	push si
	mov di, current_path
.find_end:
	cmp byte [di], 0
	je .add
	inc di
	jmp .find_end
.add:
	cmp di, current_path
	je .no_backslash
	cmp byte [di-1], '\'
	je .copy
	mov byte [di], '\'
	inc di
	jmp .copy
.no_backslash:
	mov byte [di], '\'
	inc di
.copy:
	lodsb
	test al, al
	jz .copy_done
	cmp al, 'a'
	jb .store
	cmp al, 'z'
	ja .store
	sub al, 32
.store:
	stosb
	jmp .copy
.copy_done:
	pop si
	pop di
	pop ax
	ret

chop_path:
	push ax
	push di
	mov di, current_path
	cmp byte [di+3], 0
	je .done
.find_end:
	cmp byte [di], 0
	je .go_back
	inc di
	jmp .find_end
.go_back:
.back:
	cmp di, current_path
	jbe .done
	dec di
	cmp byte [di], '\'
	je .truncate
	jmp .back
.truncate:
	mov ax, di
	sub ax, current_path
	cmp ax, 2
	je .keep_root
	mov byte [di], 0
	jmp .done
.keep_root:
	mov byte [di+1], 0
.done:
	pop di
	pop ax
	ret

resolve_path:
	push ax
	push bx
	push di
	push es
	mov ax, [current_cluster]
	mov [saved_cluster], ax
	mov [path_filename], si
.loop:
	mov di, si
.scan:
	mov al, [di]
	test al, al
	jz .done
	cmp al, '\'
	je .found_backslash
	inc di
	jmp .scan
.found_backslash:
	mov byte [di], 0
	inc di
	push di
	call cd_dir
	pop si
	jc .error
	mov [path_filename], si
	jmp .loop
.done:
	clc
	jmp .exit
.error:
	mov ax, [saved_cluster]
	mov [current_cluster], ax
	stc
.exit:
	pop es
	pop di
	pop bx
	pop ax
	ret

save_current_dir:
	push ax
	push bx
	push cx
	push bx
	push es
	mov ax, 0x1000
	mov es, ax
	mov bx, 0x8000
	mov ax, [current_cluster]
	test ax, ax
	jz .root
	add ax, 31
	call lba_to_chs
	mov al, 1
	jmp .do_write
.root:
	mov ch, 0
	mov cl, 2
	mov dh, 1
	mov al, 14
.do_write:
	mov dl, 0
	mov ah, 0x03
	int 0x13
	push ax
	xor ax, ax
	mov dl, 0
	int 0x13
	pop ax
	pop es
	pop dx
	pop cx
	pop dx
	pop ax
	ret

mkdir_dir:
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
	mov [dir_entry_addr], si
	mov di, si
	mov si, fatname
	mov ax, [cluster_temp]
	mov dx, ax
	xor bx, bx
	call write_dir_entry
	mov di, [dir_entry_addr]
	mov byte [di+11], 0x10
	call init_new_dir
	call save_current_dir
	call save_current_dir
	xor ax, ax
	jmp .done
.error:
	mov ax, 0xFFFF
.done:
	pop di
	pop si
	pop dx
	pop cx
	pop bx
	ret

init_new_dir:
	push ax
	push bx
	push cx
	push dx
	push di
	push es
	mov ax, 0x1000
	mov es, ax
	mov di, 0xA000
	mov al, '.'
	stosb
	mov cx, 10
	mov al, '.'
	stosb
	mov cx, 10
.dot_pad:
	mov al, ' '
	stosb
	loop .dot_pad
	mov al, 0x10
	stosb
	mov cx, 10
.dot_reserved:
	xor al, al
	stosb
	loop .dot_reserved
	xor ax, ax
	stosw
	stosw
	mov ax, [cluster_temp]
	stosw
	xor ax, ax
	stosw
	stosw
	mov al, '.'
	stosb
	mov al, '.'
	stosb
	mov cx, 9
.dotdot_pad:
	mov al, ' '
	stosb
	loop .dotdot_pad
	mov al, 0x10
	stosb
	mov cx, 10
.dotdot_reserved:
	xor al, al
	stosb
	loop .dotdot_reserved
	xor ax, ax
	stosw
	stosw
	mov ax, [current_cluster]
	stosw
	xor ax, ax
	stosw
	stosw
	mov ax, [cluster_temp]
	add ax, 31
	call lba_to_chs
	mov ax, 0x1000
	mov es, ax
	mov bx, 0xA000
	mov dl, 0
	mov ah, 0x03
	mov al, 1
	int 0x13
	push ax
	xor ax, ax
	mov dl, 0
	int 0x13
	pop ax
	pop es
	pop di
	pop dx
	pop cx
	pop bx
	pop ax
	ret

rmdir_dir:
	push bx
	push cx
	push dx
	push si
	push di
	push es
	call build_fat_name
	call find_file
	jc .error
	mov al, [si+11]
	test al, 0x10
	jz .error
	mov ax, [si + 26]
	mov [free_temp], ax
	add ax, 31
	call lba_to_chs
	mov ax, 0x1000
	mov es, ax
	mov bx, 0xA000
	mov dl, 0
	mov ah, 0x02
	mov al, 1
	int 0x13
	jc .error
	mov di, 0xA000
	mov cx, 16
.check_loop:
	mov al, [di]
	cmp al, 0
	je .empty
	cmp al, 0xE5
	je .skip
	cmp al, '.'
	je .skip
	jmp .error
.skip:
	add di, 32
	dec cx
	jnz .check_loop
	jmp .error
.empty:
	mov byte [si], 0xE5
	call save_current_dir
	mov ax, [free_temp]
	call free_cluster_chain
	xor ax, ax
	jmp .done
.error:
	mov ax, 0xFFFF
.done:
	pop es
	pop di
	pop si
	pop dx
	pop cx
	pop bx
	ret

cd_dir:
	push bx
	push cx
	push dx
	push si
	push di
	cmp byte [si], '.'
	jne .normal
	cmp byte [si+1], '.'
	jne .normal
	mov word [current_cluster], 0
	call chop_path
	xor ax, ax
	jmp .done
.normal:
	mov [path_filename], si
	call build_fat_name
	call find_file
	jc .error
	mov al, [si + 11]
	test al, 0x10
	jz .error
	mov ax, [si + 26]
	mov [current_cluster], ax
	mov si, [path_filename]
	call append_path
	xor ax, ax
	jmp .done
.error:
	mov ax, 0xFFFF
.done:
	pop di
	pop si
	pop dx
	pop cx
	pop bx
	ret

find_free_entry:
	push ax
	push bx
	push cx
	push dx
	push es
	call load_current_dir
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

write_cluster:
	push ax
	push bx
	push cx
	push dx
	push es
	add ax, 31
	call lba_to_chs
	push ds
	pop es
	mov bx, 0xA000
	mov ah, 0x03
	mov al, 1
	mov dl, 0
	int 0x13
	push ax
	xor ax, ax
	mov dl, 0
	int 0x13
	pop ax
	pop es
	pop dx
	pop cx
	pop bx
	pop ax
	ret

write_file:
	push ax
	push bx
	push cx
	push dx
	push si
	push di
	push es
	mov [free_temp], ax
	mov [src_size], bx
	mov si, 0xA000
.write_loop:
	mov ax, [free_temp]
	add ax, 31
	call lba_to_chs
	push si
	mov ax, 0x1000
	mov es, ax
	mov bx, si
	mov dl, 0
	mov ah, 0x03
	mov al, 1
	int 0x13
	pop si
	jc .error
	add si, 512
	mov ax, [src_size]
	sub ax, 512
	jbe .last_cluster
	mov [src_size], ax
	call fat_find_free
	cmp ax, 0xFFFF
	je .error
	mov bx, ax
	mov dx, bx
	mov ax, [free_temp]
	call fat_set
	mov ax, dx
	mov dx, 0xFFF
	call fat_set
	mov [free_temp], bx
	jmp .write_loop
.last_cluster:
	mov ax, [free_temp]
	mov dx, 0xFFF
	call fat_set
	xor ax, ax
	jmp .done
.error:
	mov ax, 0xFFFF
.done:
	pop es
	pop di
	pop si
	pop dx
	pop cx
	pop bx
	pop ax
	ret

parse_two_args:
	push ax
	push si
	mov si, buffer
	add si, [parse_skip]
	mov [arg1_ptr], si
.skip1:
	lodsb
	or al, al
	jz .error
	cmp al, ' '
	jne .skip1
	dec si
	mov byte [si], 0
	inc si
	mov [arg2_ptr], si
	clc
	jmp .done
.error:
	stc
.done:
	pop si
	pop ax
	ret

read_file:
	push bx
	push cx
	push dx
	push si
	push di
	push es
	call build_fat_name
	call find_file
	jc .error
	mov ax, [si + 28]
	mov [src_size], ax
	mov ax, [si + 26]
	mov [free_temp], ax
	mov di, 0xA000
.read_loop:
	mov ax, [free_temp]
	cmp ax, 0xFF8
	jae .done
	cmp ax, 2
	jb .done
	add ax, 31
	call lba_to_chs
	push di
	mov ax, 0x1000
	mov es, ax
	mov bx, di
	mov dl, 0
	mov ah, 0x02
	mov al, 1
	int 0x13
	pop di
	jc .error
	add di, 512
	mov ax, [free_temp]
	call fat_get
	mov [free_temp], ax
	jmp .read_loop
.done:
	xor ax, ax
	jmp .exit
.error:
	mov ax, 0xFFFF
.exit:
	pop es
	pop di
	pop si
	pop dx
	pop cx
	pop bx
	ret

copy_file:
	push bx
	push cx
	push dx
	push si
	push di
	mov word [parse_skip], 5
	call parse_two_args
	jc .error
	mov si, [arg1_ptr]
	call resolve_path
	jc .error
	mov si, [path_filename]
	mov [arg1_ptr], si
	call read_file
	cmp ax, 0xFFFF
	je .error
	mov ax, [saved_cluster]
	mov [current_cluster], ax
	mov si, [arg2_ptr]
	call resolve_path
	jc .error
	mov si, [path_filename]
	mov [arg2_ptr], si
	call create_file
	cmp ax, 0xFFFF
	je .error
	mov bx, [src_size]
	call write_file
	cmp ax, 0xFFFF
	je .error
	call write_cluster
	xor ax, ax
	jmp .done
.error:
	mov ax, 0xFFFF
.done:
	mov ax, [saved_cluster]
	mov [current_cluster], ax
	pop di
	pop si
	pop dx
	pop cx
	pop bx
	ret

del_file:
	push bx
	push cx
	push dx
	push si
	push di
	push es
	call build_fat_name
	call find_file
	jc .error
	mov al, [si+11]
	test al, 0x10
	jnz .is_dir
	mov al, [si + 11]
	test al, 0x01
	jnz .protected
	test al, 0x04
	jnz .protected
	mov ax, [si + 26]
	call free_cluster_chain
	mov byte [si], 0xE5
	mov ax, 0x1000
	mov es, ax
	mov bx, 0x8000
	mov ch, 0
	mov cl, 2
	mov dh, 1
	mov dl, 0
	mov ah, 0x03
	mov al, 14
	int 0x13
	jc .error
	push ax
	xor ax, ax
	mov dl, 0
	int 0x13
	pop ax
	xor ax, ax
	jmp .done
.is_dir:
	mov ax, 0xFFFD
	jmp .done
.protected:
	mov ax, 0xFFFE
	jmp .done
.error:
	mov ax, 0xFFFF
.done:
	pop es
	pop di
	pop si
	pop dx
	pop cx
	pop bx
	ret

ren_file:
	push bx
	push cx
	push dx
	push si
	push di
	push es
	mov word [parse_skip], 4
	call parse_two_args
	jc .error
	mov si, [arg1_ptr]
	call build_fat_name
	call find_file
	jc .error
	mov al, [si + 11]
	test al, 0x01
	jnz .protected
	test al, 0x04
	jnz .protected
	mov di, si
	mov si, [arg2_ptr]
	call build_fat_name
	mov si, fatname
	mov cx, 11
.copy_name:
	lodsb
	stosb
	loop .copy_name
	mov ax, 0x1000
	mov es, ax
	mov bx, 0x8000
	mov ch, 0
	mov cl, 2
	mov dh, 1
	mov dl, 0
	mov ah, 0x03
	mov al, 14
	int 0x13
	jc .error
	push ax
	xor ax, ax
	mov dl, 0
	int 0x13
	pop ax
	xor ax, ax
	jmp .done
.protected:
	mov ax, 0xFFFE
	jmp .done
.error:
	mov ax, 0xFFFF
.done:
	pop es
	pop di
	pop si
	pop dx
	pop cx
	pop bx
	ret

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
	call save_current_dir
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
	mov bx, [src_size]
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

print:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp print
.done:
    ret

do_exec:
	push ds
	push es
	mov si, buffer
	call build_com_name
	call find_file
	jc .not_found
	mov bx, [si + 28]
	mov [file_size], bx
	mov ax, [si + 26]
	mov [free_temp], ax
	mov di, 0x0100
.read_loop:
	mov ax, [free_temp]
	cmp ax, 0xFF8
	jae .read_done
	cmp ax, 2
	jb .read_done
	add ax, 31
	call lba_to_chs
	push di
	mov ax, 0x2000
	mov es, ax
	mov bx, di
	mov dl, 0
	mov ah, 0x02
	mov al, 1
	int 0x13
	pop di
	jc .read_err
	push ax
	push dx
	xor ax, ax
	mov dl, 0
	int 0x13
	pop dx
	pop ax
	add di, 512
	mov ax, [free_temp]
	call fat_get
	mov [free_temp], ax
	jmp .read_loop
.read_done:
	cli
	mov ax, 0x2000
	mov ss, ax
	mov sp, 0xFFFC
	push cs
	push .ret_from_com
	sti
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
	mov [cs:saved_user_ds], ds
	push cs
	pop ds
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
	cmp ah, 0x3D
	je .ah3d
	cmp ah, 0x3E
	je .ah3e
	cmp ah, 0x3F
	je .ah3f
	cmp ah, 0x40
	je .ah40
	jmp .done
.ah09:
	mov ax, [saved_user_ds]
	mov es, ax
	mov si, dx
.loop09:
	mov al, [es:si]
	inc si
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
.ah3d:
	mov si, dx
	call build_fat_name
	call find_file
	jc .error3d
	mov al, [si + 11]
	test al, 0x10
	jnz .error3d
	call find_free_handle
	cmp ax, 0xFFFF
	je .error3d
	push ax
	call find_handle
	pop ax
	jc .error3d
	mov bx, [si + 26]
	mov [di], bx
	mov word [di+2], si
	mov bx, [si + 28]
	mov [di+4], bx
	mov byte [di+6], 1
	clc
	jmp .done
.error3d:
	mov ax, 0x0004
	stc
	jmp .done
.ah3e:
	mov ax, bx
	call find_handle
	jc .error3e
	mov byte [di+6], 0
	xor ax, ax
	clc
	jmp .done
.error3e:
	mov ax, 0x0006
	stc
	jmp .done
.ah3f:
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push es
    mov [ah3f_count], cx
    mov [ah3f_buffer], dx
    mov ax, bx
    call find_handle
    jc .error3f
    mov bx, [di]
    mov ax, bx
    add ax, 31
    call lba_to_chs
    mov ax, 0x1000
    mov es, ax
    mov bx, 0xA000
    mov dl, 0
    mov ah, 0x02
    mov al, 1
    int 0x13
    jc .error3f
    push ds
    pop es
    mov si, 0xA000
    mov di, [ah3f_buffer]
    mov bp, [ah3f_count]
.copy_loop:
    test bp, bp
    jz .done3f
    mov al, [si]
    mov [di], al
    inc si
    inc di
    dec bp
    jmp .copy_loop
.done3f:
    mov ax, [ah3f_count]
    clc
    jmp .done
.error3f:
    mov ax, 0x0006
    stc
    jmp .done
.ah40:
	mov [ah40_user_ds], ds
	mov [ah40_buffer], dx
	mov [ah40_count], cx
	push cs
	pop ds
	mov ax, bx
	call find_handle
	jc .error40
	mov ax, [di]
	mov [ah40_cluster], ax
	mov ax, [di+2]
	mov [ah40_dir_entry], ax
	mov ax, [ah40_user_ds]
	mov es, ax
	mov si, [ah40_buffer]
	mov di, 0xA000
	mov cx, [ah40_count]
	cmp cx, 512
	jbe .copy_loop
	mov cx, 512
.copy_loop40:
	jcxz .write_cluster
	mov al, [es:si]
	mov [di], al
	inc si
	inc di
	dec cx
	jmp .copy_loop40
.write_cluster:
	push ds
	pop es
	mov ax, [ah40_cluster]
	add ax, 31
	call lba_to_chs
	mov bx, 0xA000
	mov ah, 0x03
	mov al, 1
	mov dl, 0
	int 0x13
	jc .error40
	push ax
	xor ax, ax
	mov dl, 0
	int 0x13
	pop ax
	mov si, [ah40_dir_entry]
	mov ax, [ah40_count]
	mov [si+28], ax
	mov word [si+30], 0
	call save_current_dir
	mov ax, [ah40_count]
	clc
	jmp .done
.error40:
	mov ax, 0x0005
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
msg_welcome  db 'MicroDOS v1.0 Classic Edition - Shell', 13, 10
             db 'tapez "help" pour la liste de commandes.', 13, 10, 10, 0
msg_prompt   db '>', 0
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
	     db ' mkfile <nom> - cree un fichier', 13, 10
	     db ' copy <src> <dst> - copie un fichier', 13, 10
	     db ' del - Supprime un fichier', 13, 10
	     db ' ren <ANCIEN NOM> <NOUVEAU NOM> - renomme un fichier', 13, 10
             db ' cd - Change de Repectoire', 13, 10
             db ' mkdir <nom> - cree un dossier', 13, 10
             db ' rmdir <nom> - Suprimme un dossier', 13, 10, 0
msg_unknow   db 'Commande inconnue. Tapez "help".', 13, 10, 0
msg_ver      db 'MicroDOS v1.0 Classic Edition', 13, 10, 0
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
msg_copy_ok db 'Fichier copie.', 13, 10, 0
msg_copy_err db 'Erreur de copie', 13, 10, 0
msg_del_ok db 'Fichier supprime.', 13, 10, 0
msg_del_err db 'Erreur de Suppression', 13, 10, 0
msg_del_protected db 'Suppression refusee : fichier systeme.',  13, 10, 0
msg_del_dir db 'Impossible de suprimer un repertoire.', 13, 10, 0
msg_del_usage db 'Usage : del NOM.TXT', 13, 10, 0
msg_ren_ok db 'Fichier renomme.', 13, 10, 0
msg_ren_err db 'Erreur renomage.', 13, 10, 0
msg_ren_usage db 'Usage : <ANCIEN NOM> <NOUVEAU NOM>', 13, 10, 0
msg_ren_protected db 'Impossible de Renommer : fichier systeme.', 13, 10, 0
msg_cd_err db 'Repertoire Introuvable', 13, 10, 0
msg_mkdir_ok db 'Repertoire', 13, 10, 0
msg_mkdir_err db 'Erreur lors de la creation du dossier', 13, 10, 0
msg_mkdir_usage db 'Usage : mkdir <NOM DU DOSSIER>', 13, 10, 0
msg_rmdir_ok db 'Dossier suprime.', 13, 10, 0
msg_rmdir_err db 'Erreur de suppression', 13, 10, 0
msg_rmdir_usage db 'rmdir <NOM DU DOSSIER>', 13, 10, 0
msg_dir_tag db ' <DIR>', 0
msg_format_warn db 'ATTENTION: Tout le disque dur va etre efface. Appuyez sur une touche pour continuer...', 13, 10, 0
msg_format_ok db 'Disque dur formate avec succes !', 13, 10, 0
msg_format_err db 'Erreur lors du formatage du disque dur.', 13, 10, 0 

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
cmd_copy_name db 'copy', 0
cmd_del db 'del', 0
cmd_ren db 'ren', 0
cmd_cd db 'cd', 0
cmd_mkdir db 'mkdir', 0
cmd_rmdir db 'rmdir', 0
cmd_format db 'format', 0

fatname      times 11 db 0
file_size    dw 0
cluster_temp dw 0
arg1_ptr dw 0
arg2_ptr dw 0
src_size dw 0
parse_skip dw 5
free_temp dw 0
current_cluster dw 0
ah3f_count dw 0
ah40_user_ds dw 0
ah40_buffer dw 0
ah40_count dw 0
ah40_cluster dw 0
ah40_dir_entry dw 0
ah3f_buffer dw 0
saved_user_ds dw 0

temp_name times 64 db 0
open_files times 5*8 db 0
dir_entry_addr dw 0
saved_cluster dw 0
path_filename dw 0
current_path times 64 db 0
init_path db 'A:\', 0
buffer       times 256 db 0
current_drive db 0
saved_part_lba dd 0
fat16_ready db 0

%include "lba.inc"
%include "lba_write.inc"
%include "mbr.inc"
%include "bpb16.inc"
%include "fat16_init.inc"
%include "fat16_get.inc"
%include "fat16_set.inc"
%include "fat16_find_free.inc"
%include "fat16_find_file.inc"
%include "fat16_read_file.inc"
%include "fat16_write_file.inc"
%include "fat16_create_file.inc"
%include "cmd_copy.inc"
%include "format.inc"