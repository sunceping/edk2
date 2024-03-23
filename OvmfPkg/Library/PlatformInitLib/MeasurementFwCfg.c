/** @file
 * Measure FW_CFG data in TDX.
 *
 * Copyright (c) 2021, Intel Corporation. All rights reserved.<BR>
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/
#include <IndustryStandard/E820.h>
#include <Base.h>
#include <PiPei.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <IndustryStandard/Tdx.h>
#include <IndustryStandard/IntelTdx.h>
#include <Library/PeiServicesLib.h>
#include <Pi/PrePiHob.h>
#include <WorkArea.h>
#include <ConfidentialComputingGuestAttr.h>
#include <Library/QemuFwCfgLib.h>
#include <Library/QemuFwCfgSimpleParserLib.h>
#include <Library/TdxHelperLib.h>


#define EV_POSTCODE_INFO_QEMU_FW_CFG_DATA "QEMU FW CFG"
#define QEMU_FW_CFG_SIZE                  sizeof(EV_POSTCODE_INFO_QEMU_FW_CFG_DATA)

#define E820_FWCFG_FILE \
        "etc/e820"

#define SYSTEM_STATES_FWCFG_FILE \
        "etc/system-states"

#define EXTRA_PCI_ROOTS_FWCFG_FILE \
        "etc/extra_pci_roots"

#define BOOT_MENU_WAIT_TIME_FWCFG_FILE \
        "etc/boot-menu-wait"

#define SMBIOS_VERSION_FWCFG_FILE \
        "etc/smbios/smbios-anchor"

#define RESERVED_MEMORY_END_FWCFG_FILE \
        "etc/reserved-memory-end"

#define PCI_MMIO_FWCFG_FILE \
        "opt/ovmf/X-PciMmio64Mb"

#define BOOTORDER_FWCFG_FILE \
        "bootorder"

#pragma pack(1)

typedef struct {
    UINT8 FwCfg[QEMU_FW_CFG_SIZE];
    UINT8 FwCfgFileName[QEMU_FW_CFG_FNAME_SIZE];
} FW_CFG_EVENT;
typedef struct {
    CHAR8 FileName[QEMU_FW_CFG_FNAME_SIZE];
    BOOLEAN Measured;
} CACHE_FW_CFG_STRCUT;


#pragma pack()

static const CACHE_FW_CFG_STRCUT mCacheFwCfgList[] = {
    { E820_FWCFG_FILE, TRUE},
    { SYSTEM_STATES_FWCFG_FILE, TRUE},
    { EXTRA_PCI_ROOTS_FWCFG_FILE, FALSE},
    { BOOT_MENU_WAIT_TIME_FWCFG_FILE, FALSE},
    { SMBIOS_VERSION_FWCFG_FILE, FALSE},
    { RESERVED_MEMORY_END_FWCFG_FILE, FALSE},
    { PCI_MMIO_FWCFG_FILE, FALSE},
    { BOOTORDER_FWCFG_FILE, FALSE}

};

UINTN
GetSumFwCfgItemsDataSize(
    UINT32 Count
    )
{
    UINTN DataSize;
    FIRMWARE_CONFIG_ITEM FwCfgItem;
    UINTN FwCfgSize;

    DataSize = Count * sizeof(FW_CFG_INFO);

    for (UINT32 i = 0; i < Count ; i++)
    {
        if (EFI_ERROR(QemuFwCfgFindFile(mCacheFwCfgList[i].FileName, &FwCfgItem, &FwCfgSize))){
            continue;
        }
        DataSize += FwCfgSize;
    }

    return DataSize;
}

VOID
BuildEventLog(
    CONST CHAR8  *FileName,
    FW_CFG_EVENT *FwCfgEvent
    )
{
    ZeroMem(FwCfgEvent, sizeof(FW_CFG_EVENT));
    CopyMem(FwCfgEvent->FwCfg, EV_POSTCODE_INFO_QEMU_FW_CFG_DATA,
            sizeof (EV_POSTCODE_INFO_QEMU_FW_CFG_DATA));
    CopyMem(FwCfgEvent->FwCfgFileName, FileName, QEMU_FW_CFG_FNAME_SIZE);
}

EFI_STATUS
BuildFwCfgInfo(
    IN CONST CHAR8  *FileName,
    IN CONST BOOLEAN Measurement,
    OUT UINTN    *Size,
    OUT UINT8    *Buffer
    )
{

    EFI_STATUS Status;
    FIRMWARE_CONFIG_ITEM Item;

    if (FileName == NULL || Buffer == NULL){
        return EFI_INVALID_PARAMETER;
    }

    if (RETURN_ERROR(QemuFwCfgFindFile(FileName, &Item, Size))){
        return EFI_NOT_FOUND;
    }

    FW_CFG_INFO FwCfgInfo;
    FwCfgInfo.FwCfgItem = Item;
    FwCfgInfo.DataSize = *Size;

    CopyMem(Buffer, &FwCfgInfo, sizeof(FW_CFG_INFO));

    Buffer += sizeof(FW_CFG_INFO);

    QemuFwCfgSelectItem(Item);
    QemuFwCfgReadBytes(*Size, Buffer);

    if (Measurement == FALSE){
        return EFI_SUCCESS;
    }

/*  #if (defined (TDX_GUEST_SUPPORTED) || defined (TDX_PEI_LESS_BOOT)) */

    FW_CFG_EVENT FwCfgEvent;
    BuildEventLog(FileName, &FwCfgEvent);

    Status = TdxHelperMeasureFwCfgData (
        (VOID *)&FwCfgEvent,
        sizeof(FwCfgEvent),
        (VOID *)Buffer,
        *Size
        );
    if (EFI_ERROR(Status)){
        return Status;
    }

/*  #endif */
    return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
PlatformInitFwCfgInfo (
    VOID
    )
{
    VOID   *FwCfhHobData;
    UINT32 SumDataSize;
    UINT8   *Ptr;
    EFI_STATUS Status;
    UINTN Size;

    DEBUG ((DEBUG_INFO, "[Sunce] PlatformInitFwCfgInfo Loaded\n"));

    UINT32 Index;
    UINT32 Count;

    Count = sizeof(mCacheFwCfgList)/sizeof(mCacheFwCfgList[0]);

    SumDataSize = GetSumFwCfgItemsDataSize(Count);
    if (SumDataSize < (sizeof (EFI_E820_ENTRY64) + Count * sizeof(FW_CFG_INFO))){
        return EFI_ABORTED;
    }

    FwCfhHobData = BuildGuidHob (&gOvmfFwCfgInfoHobGuid, SumDataSize);
    if (FwCfhHobData == NULL){
        return EFI_OUT_OF_RESOURCES;
    }

    ZeroMem(FwCfhHobData, SumDataSize);

    Ptr = FwCfhHobData;

    for (Index = 0; Index < Count; Index++)
    {

        Status = BuildFwCfgInfo(mCacheFwCfgList[Index].FileName,
                                mCacheFwCfgList[Index].Measured,
                                &Size, Ptr);
        if (EFI_ERROR(Status)) {
            if (Status == EFI_NOT_FOUND) {
                continue;
            }

            return Status;
        }


        if (Status == EFI_SUCCESS){
            Ptr += sizeof(FW_CFG_INFO) + Size;
        }

    }

    DEBUG ((DEBUG_INFO, "[Sunce] PlatformInitFwCfgInfo Pass!!!\n"));
    return EFI_SUCCESS;

}
