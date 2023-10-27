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

SECTION .text

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
    mov         rax, cr0
    mov         qword[rel saved_cr0], rax
    mov         rax, cr3
    mov         qword[rel saved_cr3], rax
    mov         rax, cr4
    mov         qword[rel saved_cr4], rax
    lea         rax, qword[rel saved_gdt]
    sgdt        [rax]

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
    ; FIXME: handle switch to paging mode matching CR3
    mov      rbx, qword[rel mailbox_address]
    mov      rax, qword[rel saved_cr0]
    mov      cr0, rax
    mov      rax, qword[rel saved_cr4]
    mov      cr4, rax
    mov      rax, qword[rel saved_cr3]
    mov      cr3, rax
    lea      rax, qword[rel saved_gdt]
    lgdt     [rax]
    mov      rax, 0x18 ; FIXME: Magic number
    mov      ds, rax
    mov      es, rax
    mov      fs, rax
    mov      gs, rax
    mov      ss, rax
    ; FIXME: swtich to the right CS descriptor
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

