/** @file
  Shared code for the PEI fw_cfg and DXE fw_cfg instances of the QemuFwCfgS3Lib
  class.

  Copyright (C) 2017, Red Hat, Inc.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/QemuFwCfgLib.h>
#include <Library/QemuFwCfgS3Lib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

/**
  Determine if S3 support is explicitly enabled.

  @retval  TRUE   If S3 support is explicitly enabled. Other functions in this
                  library may be called (subject to their individual
                  restrictions).

           FALSE  Otherwise. This includes unavailability of the firmware
                  configuration interface. No other function in this library
                  must be called.
**/
BOOLEAN
EFIAPI
QemuFwCfgS3Enabled (
  VOID
  )
{
  RETURN_STATUS         Status;
  FIRMWARE_CONFIG_ITEM  FwCfgItem;
  UINTN                 FwCfgSize;
  UINT8                 SystemStates[6];

  DEBUG((DEBUG_INFO, "[Sunce] QemuFwCfgS3Enabled\n"));

  if (TdIsEnabled ()) {
  UINT8  *Buffer;
  Buffer = NULL;

  Status = QemuFwCfgGetDataFromCache("etc/system-states", &FwCfgSize , Buffer);
  if (EFI_ERROR (Status) || Buffer == NULL) {
    return FALSE;
  }

  return (BOOLEAN)(Buffer[3] & BIT7);
  }

  Status = QemuFwCfgFindFile ("etc/system-states", &FwCfgItem, &FwCfgSize);
  if ((Status != RETURN_SUCCESS) || (FwCfgSize != sizeof SystemStates)) {
    return FALSE;
  }

  QemuFwCfgSelectItem (FwCfgItem);
  QemuFwCfgReadBytes (sizeof SystemStates, SystemStates);
  return (BOOLEAN)(SystemStates[3] & BIT7);
}
