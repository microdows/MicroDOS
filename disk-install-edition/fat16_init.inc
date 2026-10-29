; ============================================
; fat16_init.inc - Calcul des LBA FAT16
; Pour MicroDOS Disk Install Edition
; ============================================


lba_fat1 dd 0
lba_rootdir dd 0
lba_data dd 0

fat16_init:

	push ax
	push bx
	push cx
	push dx

	movzx ebx, word [bpb_reserved_sectors]
	add eax, ebx
	mov [lba_fat1], eax

	movzx ebx, byte [bpb_num_fats]
	movzx ecx, word [bpb_sectors_per_fat]
	imul ebx, ecx
	add eax, ebx 
	mov [lba_rootdir], eax
	

	movzx ebx, word [bpb_root_entries]
	shl ebx, 5
	movzx ecx, word [bpb_bytes_per_sector]
	mov eax, ebx
	xor edx, edx
	div ecx
	add eax, [lba_rootdir]
	mov [lba_data], eax

	pop dx
	pop cx
	pop bx
	pop ax
	ret

