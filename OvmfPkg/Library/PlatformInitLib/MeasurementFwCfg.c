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
        "etc/extra-pci-roots"

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

#define NULL_FW_CFG_FILE \
        "NULL"

#define INVALID_FW_CFG_ITEM 0xFFFF



#define QEMU_FW_CFG_ITEM_BOOT_MENU_SIZE sizeof(UINT16)

STATIC CONST CACHE_FW_CFG_STRCUT mCacheFwCfgList[] = {
    { E820_FWCFG_FILE, TRUE, INVALID_FW_CFG_ITEM, 0},
    { SYSTEM_STATES_FWCFG_FILE, TRUE, INVALID_FW_CFG_ITEM, 0},
    { EXTRA_PCI_ROOTS_FWCFG_FILE, TRUE, INVALID_FW_CFG_ITEM, 0},
    { BOOT_MENU_WAIT_TIME_FWCFG_FILE, TRUE, INVALID_FW_CFG_ITEM, 0},
    { SMBIOS_VERSION_FWCFG_FILE, TRUE, INVALID_FW_CFG_ITEM, 0},
    { RESERVED_MEMORY_END_FWCFG_FILE, TRUE, INVALID_FW_CFG_ITEM, 0},
    { PCI_MMIO_FWCFG_FILE, TRUE, INVALID_FW_CFG_ITEM, 0},
    { BOOTORDER_FWCFG_FILE, TRUE, INVALID_FW_CFG_ITEM, 0}
};

#define CACHE_FW_CFG_COUNT sizeof(mCacheFwCfgList)/sizeof(mCacheFwCfgList[0])

#pragma pack(1)

typedef struct {
    UINT8 FwCfg[QEMU_FW_CFG_SIZE];
    UINT8 FwCfgFileName[QEMU_FW_CFG_FNAME_SIZE];
} FW_CFG_EVENT;
#pragma pack()

// BOOLEAN
// VerifyFwCfgData(
//   IN CONST CHAR8  *Name,
//   IN UINT32  Size,
//   IN UINT8    *Buffer
// )
// {
//     DEBUG((DEBUG_INFO, "%a: %a\n", __func__, Name));
//     if (AsciiStrCmp (Name, BOOTORDER_FWCFG_FILE) == 0){
//         if (Buffer[Size - 1]  != '\0' ){
//            DEBUG((DEBUG_INFO, "%a: Skip measure qemu bootorder \n", __func__));
//            return FALSE;
//         }
 
//     }

//     return TRUE;
// }   

VOID
TestInternalDumpData (
  IN UINT8  *Data,
  IN UINTN  Size
  )
{
  UINTN  Index;

  for (Index = 0; Index < Size; Index++) {
    DEBUG ((DEBUG_INFO, "%02x", (UINTN)Data[Index]));
  }
}

/**

  This function dump raw data with colume format.

  @param  Data  raw data
  @param  Size  raw data size

**/
VOID
TestInternalDumpHex (
  IN UINT8  *Data,
  IN UINTN  Size
  )
{
  UINTN  Index;
  UINTN  Count;
  UINTN  Left;

  #define COLUME_SIZE  (16 * 2)

  Count = Size / COLUME_SIZE;
  Left  = Size % COLUME_SIZE;
  for (Index = 0; Index < Count; Index++) {
    DEBUG ((DEBUG_INFO, "%04x: ", Index * COLUME_SIZE));
    TestInternalDumpData (Data + Index * COLUME_SIZE, COLUME_SIZE);
    DEBUG ((DEBUG_INFO, "\n"));
  }

  if (Left != 0) {
    DEBUG ((DEBUG_INFO, "%04x: ", Index * COLUME_SIZE));
    TestInternalDumpData (Data + Index * COLUME_SIZE, Left);
    DEBUG ((DEBUG_INFO, "\n"));
  }
}


EFI_STATUS
CacheFwCfgInfoHobWithOptionalMeasurement(
    IN CHAR8  *FileName,
    IN BOOLEAN Measurement,
    IN FIRMWARE_CONFIG_ITEM Item,
    IN UINT64 Size,
    OUT UINT8    *Buffer
    )
{
    EFI_STATUS Status;

    // DEBUG((DEBUG_INFO, "[Sunce] Build the %a \n", FileName));

    if ( FileName == NULL || Buffer == NULL){
        return EFI_INVALID_PARAMETER;
    }

    FW_CFG_CACHE_INFO FwCfgInfo;
    FwCfgInfo.FwCfgItem = Item;
    FwCfgInfo.DataSize = Size;
    CopyMem(FwCfgInfo.FileName, FileName, QEMU_FW_CFG_FNAME_SIZE );

    CopyMem(Buffer, &FwCfgInfo, sizeof(FW_CFG_CACHE_INFO));

    Buffer += sizeof(FW_CFG_CACHE_INFO);

    QemuFwCfgSelectItem(Item);
    QemuFwCfgReadBytes(Size, Buffer);
    DEBUG((DEBUG_INFO, "[Sunce] Dump the %a info, size is %d\n", FileName, Size));

    TestInternalDumpHex(Buffer, Size);

    // Measurement = VerifyFwCfgData(FileName, Size, Buffer);

    if (Measurement == FALSE){
        return EFI_SUCCESS;
    }

/*  #if (defined (TDX_GUEST_SUPPORTED) || defined (TDX_PEI_LESS_BOOT)) */
    if (TdIsEnabled()) {
        FW_CFG_EVENT FwCfgEvent;
        ZeroMem(&FwCfgEvent, sizeof(FW_CFG_EVENT));
        CopyMem(&FwCfgEvent.FwCfg, EV_POSTCODE_INFO_QEMU_FW_CFG_DATA, sizeof (EV_POSTCODE_INFO_QEMU_FW_CFG_DATA));
        CopyMem(&FwCfgEvent.FwCfgFileName, FileName, QEMU_FW_CFG_FNAME_SIZE);

        Status = TdxHelperMeasureFwCfgData (
            (VOID *)&FwCfgEvent,
            sizeof(FwCfgEvent),
            (VOID *)Buffer,
            Size
            );
        if (EFI_ERROR(Status)) {
            DEBUG((DEBUG_ERROR, "TdxHelperMeasureFwCfgData failed with %r\n", Status));
            return Status;
        }
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
    VOID        *FwCfhHobData;
    UINT32      SumDataSize;
    UINT8       *Ptr;
    EFI_STATUS  Status;
    UINT32      Index;
    UINT32      Count;

    FIRMWARE_CONFIG_ITEM FwCfgItem;
    UINTN                FwCfgSize;

    DEBUG((DEBUG_INFO, "[Sunce] PlatformInitFwCfgInfo\n"));

    CACHE_FW_CFG_STRCUT CacheFwCfgList[CACHE_FW_CFG_COUNT];
    CopyMem(CacheFwCfgList, mCacheFwCfgList, sizeof(mCacheFwCfgList));

    Count = CACHE_FW_CFG_COUNT;

    SumDataSize = 0;
    for (Index = 0; Index < Count; Index++) {

        if (EFI_ERROR(QemuFwCfgFindFile(CacheFwCfgList[Index].FileName, &FwCfgItem, &FwCfgSize))){
            continue;
        }

        if (FwCfgSize == 0) {
            continue;
        }


        CacheFwCfgList[Index].FwCfgItem = FwCfgItem;
        CacheFwCfgList[Index].FwCfgSize = FwCfgSize;
        SumDataSize += FwCfgSize + sizeof(FW_CFG_CACHE_INFO);
    }

    if (SumDataSize < (sizeof (EFI_E820_ENTRY64) + sizeof(FW_CFG_CACHE_INFO))) {
        DEBUG((DEBUG_INFO, "[Sunce] SumDataSize is %d\n", SumDataSize));
        Status = EFI_ABORTED;
    }

    FwCfhHobData = BuildGuidHob (&gOvmfFwCfgInfoHobGuid, SumDataSize + sizeof(FW_CFG_SELECT_INFO));
    if (FwCfhHobData == NULL) {
        DEBUG((DEBUG_INFO, "[Sunce] BuildGuidHob Failed SumDataSize is %d\n", SumDataSize));
        return EFI_OUT_OF_RESOURCES;
    }

    ZeroMem(FwCfhHobData, SumDataSize);
    DEBUG((DEBUG_INFO, "[Sunce] BuildGuidHob Pass SumDataSize is %d\n", SumDataSize));


    FW_CFG_SELECT_INFO *FwCfgSelectInfo;
    FwCfgSelectInfo = (FW_CFG_SELECT_INFO *)FwCfhHobData;

    FwCfgSelectInfo->CacheReady = FALSE;
    FwCfgSelectInfo->SkipCache  = TRUE;

    Ptr = FwCfhHobData + sizeof(FW_CFG_SELECT_INFO);

    for (Index = 0; Index < Count; Index++) {
        if (CacheFwCfgList[Index].FwCfgItem == INVALID_FW_CFG_ITEM &&
            CacheFwCfgList[Index].FwCfgSize == 0){
            continue;
        }

        if (TdIsEnabled()) {
            CacheFwCfgList[Index].Measured = TRUE;
        }

        Status = CacheFwCfgInfoHobWithOptionalMeasurement(CacheFwCfgList[Index].FileName,
                                                          CacheFwCfgList[Index].Measured,
                                                          CacheFwCfgList[Index].FwCfgItem,
                                                          CacheFwCfgList[Index].FwCfgSize,
                                                          Ptr);
        if (EFI_ERROR(Status)) {
            return Status;
        }

        if (Status == EFI_SUCCESS){
            Ptr += sizeof(FW_CFG_CACHE_INFO) + CacheFwCfgList[Index].FwCfgSize;
        }

    }

    FwCfgSelectInfo->CacheReady = TRUE;
    FwCfgSelectInfo->SkipCache  = FALSE;
    DEBUG ((DEBUG_INFO, "[Sunce] PlatformInitFwCfgInfo Pass!!!\n"));
    return EFI_SUCCESS;
}
