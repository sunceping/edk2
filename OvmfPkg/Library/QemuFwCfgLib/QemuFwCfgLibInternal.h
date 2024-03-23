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


EFI_STATUS
QemuFwCfgGetBytesFromCache (
  IN     UINT8                 *Cache,
  IN     UINT32                CacheSize,
  IN     FIRMWARE_CONFIG_ITEM  Item,
  IN     UINT32                Size,
  IN     UINT32                OffSet, 
  IN OUT VOID                  *Buffer
  );


EFI_STATUS
InternalQemuFwCfgCacheBytes (
  IN     UINT32  Size,
  IN OUT VOID    *Buffer
  );


VOID
InternalQemuFwCfgSelectItem (
  IN     FIRMWARE_CONFIG_ITEM  Item
  );

BOOLEAN
QemuFwCfgCacheEnable (
  VOID
  );

BOOLEAN
QemuFwCfgSkipCache (
  VOID
  );

RETURN_STATUS
QemuFwCfgItemInCacheList (
  IN  CONST CHAR8 *Name,
  OUT FIRMWARE_CONFIG_ITEM  *Item,
  OUT  UINTN                 *Size
  );


#endif
