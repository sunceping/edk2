/** @file
  Internal interfaces specific to the QemuFwCfgLib instances in OvmfPkg.

  Copyright (C) 2016, Red Hat, Inc.
  Copyright (C) 2017, Advanced Micro Devices. All rights reserved

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __QEMU_FW_CFG_LIB_INTERNAL_H__
#define __QEMU_FW_CFG_LIB_INTERNAL_H__

#include <Base.h>
#include <Uefi/UefiMultiPhase.h>
#include <Uefi/UefiBaseType.h>
#include <Pi/PiBootMode.h>
#include <Pi/PiHob.h>
#include <Library/HobLib.h>
#include <Library/BaseMemoryLib.h>

typedef struct {
  CHAR8                   FileName[QEMU_FW_CFG_FNAME_SIZE];
  FIRMWARE_CONFIG_ITEM    FwCfgItem;
  UINTN                   DataSize;
  // UINT8  *data
} FW_CFG_CACHED_ITEM;

typedef struct {
  BOOLEAN                 CacheReady;
  FIRMWARE_CONFIG_ITEM    FwCfgItem;
  UINTN                   Offset;
  BOOLEAN                 Reading;
} FW_CFG_CACHE_WORK_AREA;

/**
  Returns a boolean indicating if the firmware configuration interface is
  available for library-internal purposes.

  This function never changes fw_cfg state.

  @retval    TRUE   The interface is available internally.
  @retval    FALSE  The interface is not available internally.
**/
BOOLEAN
InternalQemuFwCfgIsAvailable (
  VOID
  );

/**
  Returns a boolean indicating whether QEMU provides the DMA-like access method
  for fw_cfg.

  @retval    TRUE   The DMA-like access method is available.
  @retval    FALSE  The DMA-like access method is unavailable.
**/
BOOLEAN
InternalQemuFwCfgDmaIsAvailable (
  VOID
  );

/**
  Transfer an array of bytes, or skip a number of bytes, using the DMA
  interface.

  @param[in]     Size     Size in bytes to transfer or skip.

  @param[in,out] Buffer   Buffer to read data into or write data from. Ignored,
                          and may be NULL, if Size is zero, or Control is
                          FW_CFG_DMA_CTL_SKIP.

  @param[in]     Control  One of the following:
                          FW_CFG_DMA_CTL_WRITE - write to fw_cfg from Buffer.
                          FW_CFG_DMA_CTL_READ  - read from fw_cfg into Buffer.
                          FW_CFG_DMA_CTL_SKIP  - skip bytes in fw_cfg.
**/
VOID
InternalQemuFwCfgDmaBytes (
  IN     UINT32  Size,
  IN OUT VOID    *Buffer OPTIONAL,
  IN     UINT32  Control
  );

/**
  Check if it is Tdx guest

  @retval    TRUE   It is Tdx guest
  @retval    FALSE  It is not Tdx guest
**/
BOOLEAN
QemuFwCfgIsTdxGuest (
  VOID
  );

/**
  Read the fw_cfg data from Cache.

  @retval  EFI_SUCCESS   - Successfully
  @retval  Others        - As the error code indicates
**/
EFI_STATUS
InternalQemuFwCfgCacheReadBytes (
  IN     UINTN  Size,
  IN OUT VOID   *Buffer
  );

/**
  Select the fw_cfg item for reading from cache. If the fw_cfg item
  is not cached, then it returns FALSE.

  @param[in]    Item    The fw_cfg item to be selected

  @retval       TRUE    The fw_cfg item is selected.
  @retval       FALSE   The fw_cfg item is not selected.
**/
BOOLEAN
InternalQemuFwCfgCacheSelectItem (
  IN  FIRMWARE_CONFIG_ITEM  Item
  );

/**
  Get the pointer to the FW_CFG_CACHE_WORK_AREA. This data is used as the
  workarea to record the onging fw_cfg item and offset.

  @retval   FW_CFG_CACHE_WORK_AREA  Pointer to the FW_CFG_CACHE_WORK_AREA
  @retval   NULL                FW_CFG_CACHE_WORK_AREA doesn't exist
**/
FW_CFG_CACHE_WORK_AREA *
InternalQemuFwCfgCacheGetWorkArea (
  VOID
  );

/**
  Clear the FW_CFG_CACHE_WORK_AREA.
**/
VOID
InternalQemuFwCfgCacheResetWorkArea (
  VOID
  );

/**
  Check if fw_cfg cache is ready.

  @retval    TRUE   Cache is ready
  @retval    FALSE  Cache is not ready
**/
BOOLEAN
InternalQemuFwCfgCacheEnable (
  VOID
  );

/**
  Get the pointer to the cached fw_cfg item.

  @param[in] Item   The fw_cfg item to be retrieved.

  @retval    FW_CFG_CACHED_ITEM   Pointer to the cached fw_cfg item.
  @retval    NULL                The fw_cfg item is not cached.
**/
FW_CFG_CACHED_ITEM *
InternalQemuFwCfgItemCached (
  IN FIRMWARE_CONFIG_ITEM  Item
  );

/**
  Check if reading from FwCfgCache is ongoing.

  @retval   TRUE  Reading from FwCfgCache is ongoing.
  @retval   FALSE Reading from FwCfgCache is not ongoing.
**/
BOOLEAN
InternalQemuFwCfgCacheReading (
  VOID
  );

/**
  Get the first cached item.

  @retval    FW_CFG_CACHED_ITEM  The first cached item.
  @retval    NULL               There is no cached item.
**/
FW_CFG_CACHED_ITEM *
InternalQemuFwCfgCacheFirstItem (
  VOID
  );

/**
  Check if the fw_cfg file name is in cache.

  @retval    TRUE   The item in cache.
  @retval    FALSE  The item not in cache.
**/
RETURN_STATUS
InternalQemuFwCfgItemInCacheList (
  IN  CONST  CHAR8                 *Name,
  OUT        FIRMWARE_CONFIG_ITEM  *Item,
  OUT        UINTN                 *Size
  );

#include <Library/TdxHelperLib.h>

#define EV_POSTCODE_INFO_QEMU_FW_CFG_DATA  "QEMU FW CFG"
#define QEMU_FW_CFG_SIZE                   sizeof (EV_POSTCODE_INFO_QEMU_FW_CFG_DATA)

#define E820_FWCFG_FILE \
        "etc/e820"

#define SYSTEM_STATES_FWCFG_FILE \
        "etc/system-states"

#define EXTRA_PCI_ROOTS_FWCFG_FILE \
        "etc/extra-pci-roots"

#define EXTRA_PCI_ROOTS_FWCFG_SIZE  sizeof (UINT64)

#define BOOT_MENU_WAIT_TIME_FWCFG_FILE \
        "etc/boot-menu-wait"

#define BOOT_MENU_WAIT_TIME_FWCFG_SIZE  sizeof (UINT16)

#define RESERVED_MEMORY_END_FWCFG_FILE \
        "etc/reserved-memory-end"

#define RESERVED_MEMORY_END_FWCFG_SIZE  sizeof (UINT64)

#define PCI_MMIO_FWCFG_FILE \
        "opt/ovmf/X-PciMmio64Mb"

#define BOOTORDER_FWCFG_FILE \
        "bootorder"

#define INVALID_FW_CFG_ITEM  0xFFFF

#pragma pack(1)

typedef struct {
  CHAR8                   FileName[QEMU_FW_CFG_FNAME_SIZE];
  BOOLEAN                 NeedMeasure;
  FIRMWARE_CONFIG_ITEM    FwCfgItem;
  UINTN                   FwCfgSize;
} CACHE_FW_CFG_STRCUT;

typedef struct {
  UINT8    FwCfg[QEMU_FW_CFG_SIZE];
  UINT8    FwCfgFileName[QEMU_FW_CFG_FNAME_SIZE];
} FW_CFG_EVENT;

#pragma pack()

STATIC CONST CACHE_FW_CFG_STRCUT  mCacheFwCfgList[] = {
  { E820_FWCFG_FILE,                FALSE, INVALID_FW_CFG_ITEM, 0                              },
  { SYSTEM_STATES_FWCFG_FILE,       FALSE, INVALID_FW_CFG_ITEM, 0                              },
  { EXTRA_PCI_ROOTS_FWCFG_FILE,     TRUE,  INVALID_FW_CFG_ITEM, EXTRA_PCI_ROOTS_FWCFG_SIZE     },
  { BOOT_MENU_WAIT_TIME_FWCFG_FILE, TRUE,  INVALID_FW_CFG_ITEM, BOOT_MENU_WAIT_TIME_FWCFG_SIZE },
  { RESERVED_MEMORY_END_FWCFG_FILE, TRUE,  INVALID_FW_CFG_ITEM, RESERVED_MEMORY_END_FWCFG_SIZE },
  { PCI_MMIO_FWCFG_FILE,            TRUE,  INVALID_FW_CFG_ITEM, 0                              },
  { BOOTORDER_FWCFG_FILE,           TRUE,  INVALID_FW_CFG_ITEM, 0                              },
};

#define CACHE_FW_CFG_COUNT  sizeof (mCacheFwCfgList)/sizeof (mCacheFwCfgList[0])

/**
  init the fw_cfg info hob with optional measurement

**/
EFI_STATUS
IntelnalQemuFwCfgInitCache (
  VOID
  );

#endif
