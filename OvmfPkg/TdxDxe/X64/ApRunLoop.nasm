;------------------------------------------------------------------------------ ;
; Copyright (c) 2021, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   ApRunLoop.nasm
;
; Abstract:
;
;   This is the assembly code for run loop for APs in the guest TD
;
;-------------------------------------------------------------------------------

%include "TdxCommondefs.inc"

DEFAULT REL

SECTION .data

ALIGN 16

global  ASM_PFX(BootGdtTable)

;
; GDT[0]: 0x00: Null entry, never used.
;
NULL_SEL            EQU     $ - GDT_BASE        ; Selector [0]
GDT_BASE:
ASM_PFX(BootGdtTable):    DD      0
                          DD      0
;
;  code segment descriptor
;
CODE_COMPATI_SEL    EQU $ - GDT_BASE  ; Selector [0x8]
    DW  0FFFFh                          ; limit 0xFFFFF
    DW  0                               ; base 0
    DB  0                               ; BaseMid
    DB  09Bh                            ; Type
    DB  0CFh
    DB  0
;
;  code segment descriptor
;
CODE_LONG_SEL       EQU $ - GDT_BASE    ; Selector [0x10]
    DW  0FFFFh                           ; limit 0xFFFFF
    DW  0                                ; base 0
    DB  0
    DB  09Bh
    DB  0AFh
    DB  0
;
;data segment descriptor
;
DATA_SEL            EQU $ - GDT_BASE    ; Selector [0x18]
    DW  0FFFFh                            ; limit 0xFFFFF
    DW  0                                 ; base 0
    DB  0 
    DB  093h                         
    DB  0CFh
    DB  0

TSS_SEL               EQU $ - GDT_BASE    ; Selector [0x20]
    DW  0FFFFh                            ; limit 0xFFFF
    DW  0                                 ; base 0
    DB  0
    DB  089h                              ; TSS-Ava, S=0,                             
    DB  0                             
    DB  0

TSS_BUSY_SEL           EQU $ - GDT_BASE    ; Selector [0x28]
    DW  0FFFFh                            ; limit 0xFFFF
    DW  0                                 ; base 0
    DB  0
    DB  08bh                              ; TSS-busy, S=0,                             
    DB  0                             
    DB  0

; SPARE5_SEL
SPARE5_SEL             EQU $ - GDT_BASE    ; Selector [0x30]
    DW  0                               ; limit 0
    DW  0                               ; base 0
    DB  0
    DB  0                               
    DB  0                             
    DB  0
GDT_SIZE                EQU     $ - GDT_BASE       

GDT_DESC:
    DW GDT_SIZE - 1
    DQ GDT_BASE

; SECTION .bss
; global STACK_BASE
; align   4096
; STACK_BASE:
;  resb 4096*7
; STACK_TOP:
SECTION .text

BITS 32
%define PAGE_PRESENT            0x01
%define PAGE_READ_WRITE         0x02
%define PAGE_USER_SUPERVISOR    0x04
%define PAGE_WRITE_THROUGH      0x08
%define PAGE_CACHE_DISABLE     0x010
%define PAGE_ACCESSED          0x020
%define PAGE_DIRTY             0x040
%define PAGE_PAT               0x080
%define PAGE_GLOBAL           0x0100
%define PAGE_2M_MBO            0x080
%define PAGE_2M_PAT          0x01000

%define PAGE_4K_PDE_ATTR (PAGE_ACCESSED + \
                          PAGE_DIRTY + \
                          PAGE_READ_WRITE + \
                          PAGE_PRESENT)

%define PAGE_2M_PDE_ATTR (PAGE_2M_MBO + \
                          PAGE_ACCESSED + \
                          PAGE_DIRTY + \
                          PAGE_READ_WRITE + \
                          PAGE_PRESENT)


%define PAGE_PDP_ATTR (PAGE_ACCESSED + \
                       PAGE_READ_WRITE + \
                       PAGE_PRESENT)

BITS 64

%define TDVMCALL_EXPOSE_REGS_MASK       0xffec
%define TDVMCALL                        0x0
%define EXIT_REASON_CPUID               0xa

%macro  tdcall  0
  db  0x66, 0x0f, 0x01, 0xcc
%endmacro

;
; Relocated Ap Mailbox loop
;
; @param[in]  RBX:  Relocated mailbox address
; @param[in]  RBP:  vCpuId
;
; @return     None  This routine does not return
;
global ASM_PFX(AsmRelocateApMailBoxLoop)
ASM_PFX(AsmRelocateApMailBoxLoop):
AsmRelocateApMailBoxLoopStart:

    mov         rax, TDVMCALL
    mov         rcx, TDVMCALL_EXPOSE_REGS_MASK
    xor         r10, r10
    mov         r11, EXIT_REASON_CPUID
    mov         r12, 0xb
    tdcall
    test        r10, r10
    jnz         Panic
    mov         r8, r15
    mov         qword[rel mailbox_address], rbx

MailBoxLoop:
    ; Spin until command set
    cmp        dword [rbx + CommandOffset], MpProtectedModeWakeupCommandNoop
    je         MailBoxLoop
    ; Determine if this is a broadcast or directly for my apic-id, if not, ignore
    cmp        dword [rbx + ApicidOffset], MailboxApicidBroadcast
    je         MailBoxProcessCommand
    cmp        dword [rbx + ApicidOffset], r8d
    jne        MailBoxLoop
MailBoxProcessCommand:
    cmp        dword [rbx + CommandOffset], MpProtectedModeWakeupCommandWakeup
    je         MailBoxWakeUp
    cmp        dword [rbx + CommandOffset], MpProtectedModeWakeupCommandTest
    je         MailBoxTest
    cmp        dword [rbx + CommandOffset], MpProtectedModeWakeupCommandSleep
    je         MailBoxSleep
    ; Don't support this command, so ignore
    jmp        MailBoxLoop
MailBoxWakeUp:
    mov        rax, [rbx + WakeupVectorOffset]
    ; OS sends a wakeup command for a given APIC ID, firmware is supposed to reset
    ; the command field back to zero as acknowledgement.
    mov        qword [rbx + CommandOffset], 0
    jmp        rax
MailBoxTest:
    mov        qword [rbx + CommandOffset], 0
    jmp        MailBoxLoop
MailBoxSleep:
    jmp       $
Panic:
    ud2
AsmRelocateApResetVector:

global STACK_BASE
align   4096
STACK_BASE:
 resb 4096*7 ; 4096*6 as Page Table Entry, 4096 as Stack
STACK_TOP:

PrepareStack:
    ; prepare the stack 
    ; The stack can then be used to switch from long mode to compatibility mode
    mov rsp, STACK_TOP
    mov rbp, STACK_BASE

InitialStack:
    ; since the stack is not in .bss, so not zeroed
    mov rdi, rbp
    mov ecx, 4096*7
    mov al, 0
    cld
    rep stosb

DisablePaging:
    cli
.loadGDT:
    mov     rsi, GDT_DESC
    lgdt    [rsi]
.loadSwicthModeCode:
    mov     rcx, dword 0x10    ; load compatible mode selector
    shl     rcx, 32
    lea     rdx, [LongMode]    ; assume address < 4G
    or      rcx, rdx
    push    rcx 

    mov     rcx, dword 0x8    ; load compatible mode selector
    shl     rcx, 32
    lea     rdx, [Compatible] ; assume address < 4G
    or      rcx, rdx
    push    rcx
    retf
Compatible:
    mov     rax, dword 0x18
;     ; reload DS/ES/SS to make sure they are correct referred to current GDT
    mov     ds, ax
    mov     es, ax
    mov     ss, ax
    ; reload the fs and gs
    mov     fs, ax
    mov     gs, ax

.ClearTSS:
    mov     rax, dword 0x20
    ltr     ax

    ; Must clear the CR4.PCIDE before clearing paging
    mov     rcx, cr4
    btc     ecx, 17
    mov     cr4, rcx
    ;
    ; Disable paging
    ;
    mov     rcx, cr0
    btc     ecx, 31
    mov     cr0, rcx
    ;
RestoreCr0:
    ; Only enable  PE(bit 0), NE(bit 5), ET(bit 4) 0x31
    mov    rcx, cr0
    btc    ecx, 18  ;CR0.AM
    btc    ecx, 16  ;CR0.WP
    btc    ecx, 1   ;CR0.MP
    mov    cr0, rcx

RestoreCr4:
    ; Only Enable MCE(bit 6), VMXE(bit 13)
    mov      rax, cr4
    btc      eax, 22 ; CR4.PKE
    btc      eax, 21 ; CR4.SMAP
    btc      eax, 20 ; CR4.SMEP
    btc      eax, 18 ; CR4.OSXSAVE
    btc      eax, 16 ; CR4.FSGSBASE
    btc      eax, 12 ; CR4.LA57
    btc      eax, 11 ; CR4.UMIP
    btc      eax, 10 ; CR4.OSXMMEXCPT
    btc      eax, 9  ; CR4.OSFXSR
    btc      eax, 5  ; CR4.PAE
    btc      eax, 4  ; CR4.PSE  
    mov      cr4, rax
; ClearLEM:
    ; Clear EFER.LME, TDX enforec the LME = 1, can not clear it
    ; ;
    ; mov     ecx, 0xC0000080
    ; rdmsr
    ; btc     eax, 8
    ; wrmsr

BITS 32

ConstructPageTables:
    mov     ecx, 6 * 0x1000 / 4
    xor     eax, eax
    xor     edx, edx

clearPageTablesMemoryLoop:
    mov     dword[ecx * 4 + ebp- 4], eax
    loop    clearPageTablesMemoryLoop

    ;
    ; Top level Page Directory Pointers (1 * 512GB entry)
    ;
    lea     ebx, [0x1000 + PAGE_PDP_ATTR + ebp]
    mov     dword[ebp], ebx
    mov     dword[ebp + 4], edx

    ;
    ; Next level Page Directory Pointers (4 * 1GB entries => 4GB)
    ;
    lea     ebx, [0x2000 + PAGE_PDP_ATTR + ebp]
    mov     dword[ebp + (0x1000)], ebx
    mov     dword[ebp +  (0x1004)], edx
    lea     ebx, [0x3000 + PAGE_PDP_ATTR + ebp]
    mov     dword[ebp +  (0x1008)], ebx
    mov     dword[ebp +  (0x100C)], edx
    lea     ebx, [0x4000 + PAGE_PDP_ATTR + ebp]
    mov     dword[ebp +  (0x1010)], ebx
    mov     dword[ebp +  (0x1014)], edx
    lea     ebx, [0x5000 + PAGE_PDP_ATTR + ebp]
    mov     dword[ebp +  (0x1018)], ebx
    mov     dword[ebp +  (0x101C)], edx

    ;
    ; Page Table Entries (2048 * 2MB entries => 4GB)
    ;
    mov     ecx, 0x800
pageTableEntriesLoop:
    mov     eax, ecx
    dec     eax
    shl     eax, 21
    add     eax, PAGE_2M_PDE_ATTR
    mov     [ecx * 8 + ebp + (0x2000 - 8)], eax
    mov     [(ecx * 8 + ebp + (0x2000 - 8)) + 4], edx
    loop    pageTableEntriesLoop

SetCr3:
    ;
    ; Set CR3 now that the paging structures are available
    ;
    mov     cr3, ebp

EnablePAE:
    mov     eax, cr4
    bts     eax, 5
    mov     cr4, eax
    mov     ecx, cr4

EnablePaging:
    mov     eax, cr0
    bts     eax, 31                     ; set PG
    mov     cr0, eax                    ; enable paging    
    ; return to LongMode
    retf

BITS  64
LongMode:
.set_mailbox_addr:
    mov      rbx, qword[rel mailbox_address]
.jump_mailbox:
    jmp      AsmRelocateApMailBoxLoopStart
align 16
mailbox_address:
    dq 0
saved_cr0:
    dq 0
saved_cr3:
    dq 0
saved_cr4:
    dq 0
saved_gdt:
    db 10 dup 0
BITS 64
AsmRelocateApMailBoxLoopEnd:

;-------------------------------------------------------------------------------------
;  AsmGetRelocationMap (&RelocationMap);
;-------------------------------------------------------------------------------------
global ASM_PFX(AsmGetRelocationMap)
ASM_PFX(AsmGetRelocationMap):
    lea        rax, [AsmRelocateApMailBoxLoopStart]
    mov        qword [rcx], rax
    mov        qword [rcx +  8h], AsmRelocateApMailBoxLoopEnd - AsmRelocateApMailBoxLoopStart
    lea        rax, [AsmRelocateApResetVector]
    mov        qword [rcx + 10h], rax
    ret

