/** @file

  QemuFwCfg cached feature related functions.

  Copyright (c) 224, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/QemuFwCfgLib.h>
#include "QemuFwCfgLibInternal.h"

/**
  Get the pointer to the cached fw_cfg item.

  @param[in] Item   The fw_cfg item to be retrieved.

  @retval    FW_CFG_CACHED_ITEM   Pointer to the cached fw_cfg item.
  @retval    NULL                The fw_cfg item is not cached.
**/
FW_CFG_CACHED_ITEM *
InternalQemuFwCfgItemCached (
  IN FIRMWARE_CONFIG_ITEM  Item
  )
{
  UINT16              HobSize;
  UINT16              Offset;
  BOOLEAN             Cached;
  FW_CFG_CACHED_ITEM  *CachedItem;
  EFI_HOB_GUID_TYPE   *GuidHob;
  UINT32              CachedItemSize;

  if (!InternalQemuFwCfgCacheEnable ()) {
    return NULL;
  }

  GuidHob = GetFirstGuidHob (&gOvmfFwCfgInfoHobGuid);
  ASSERT (GuidHob != NULL);

  HobSize = GET_GUID_HOB_DATA_SIZE (GuidHob);
  Offset  = sizeof (EFI_HOB_GUID_TYPE) + sizeof (FW_CFG_CACHE_WORK_AREA);
  ASSERT (Offset < HobSize);

  CachedItem = InternalQemuFwCfgCacheFirstItem ();

  Cached = FALSE;
  while (Offset < HobSize) {
    if (CachedItem->FwCfgItem == Item) {
      Cached = TRUE;
      break;
    }

    CachedItemSize = CachedItem->DataSize + sizeof (FW_CFG_CACHED_ITEM);
    Offset        += CachedItemSize;
    CachedItem     = (FW_CFG_CACHED_ITEM *)((UINT8 *)CachedItem + CachedItemSize);
  }

  return Cached ? CachedItem : NULL;
}

/**
  Clear the FW_CFG_CACHE_WORK_AREA.
**/
VOID
InternalQemuFwCfgCacheResetWorkArea (
  VOID
  )
{
  FW_CFG_CACHE_WORK_AREA  *FwCfgCacheWrokArea;

  FwCfgCacheWrokArea = InternalQemuFwCfgCacheGetWorkArea ();
  if (FwCfgCacheWrokArea != NULL) {
    FwCfgCacheWrokArea->FwCfgItem = 0;
    FwCfgCacheWrokArea->Offset    = 0;
    FwCfgCacheWrokArea->Reading   = FALSE;
  }
}

/**
  Check if reading from FwCfgCache is ongoing.

  @retval   TRUE  Reading from FwCfgCache is ongoing.
  @retval   FALSE Reading from FwCfgCache is not ongoing.
**/
BOOLEAN
InternalQemuFwCfgCacheReading (
  VOID
  )
{
  BOOLEAN                 Reading;
  FW_CFG_CACHE_WORK_AREA  *FwCfgCacheWrokArea;

  Reading            = FALSE;
  FwCfgCacheWrokArea = InternalQemuFwCfgCacheGetWorkArea ();
  if (FwCfgCacheWrokArea != NULL) {
    Reading = FwCfgCacheWrokArea->Reading;
  }

  return Reading;
}

BOOLEAN
InternalQemuFwCfgCacheSelectItem (
  IN  FIRMWARE_CONFIG_ITEM  Item
  )
{
  FW_CFG_CACHE_WORK_AREA  *FwCfgCacheWrokArea;

  // Walk thru cached fw_items to see if Item is cached.
  if (InternalQemuFwCfgItemCached (Item) == NULL) {
    return FALSE;
  }

  FwCfgCacheWrokArea = InternalQemuFwCfgCacheGetWorkArea ();
  ASSERT (FwCfgCacheWrokArea);

  FwCfgCacheWrokArea->FwCfgItem = Item;
  FwCfgCacheWrokArea->Offset    = 0;
  FwCfgCacheWrokArea->Reading   = TRUE;

  return TRUE;
}

/**
  Get the first cached item.

  @retval    FW_CFG_CACHED_ITEM  The first cached item.
  @retval    NULL               There is no cached item.
**/
FW_CFG_CACHED_ITEM *
InternalQemuFwCfgCacheFirstItem (
  VOID
  )
{
  EFI_HOB_GUID_TYPE  *GuidHob;
  UINT16             HobSize;
  UINT16             Offset;

  if (!InternalQemuFwCfgCacheEnable ()) {
    return NULL;
  }

  GuidHob = GetFirstGuidHob (&gOvmfFwCfgInfoHobGuid);
  ASSERT (GuidHob != NULL);

  HobSize = GET_GUID_HOB_DATA_SIZE (GuidHob);
  Offset  = sizeof (EFI_HOB_GUID_TYPE) + sizeof (FW_CFG_CACHE_WORK_AREA);
  ASSERT (Offset < HobSize);

  return (FW_CFG_CACHED_ITEM *)((UINT8 *)GuidHob + Offset);
}

/**
  Read the fw_cfg data from Cache.

  @param[in]  Size    Data size to be read
  @param[in]  Buffer  Pointer to the buffer to which data is written

  @retval  EFI_SUCCESS   - Successfully
  @retval  Others        - As the error code indicates
**/
EFI_STATUS
InternalQemuFwCfgCacheReadBytes (
  IN     UINTN  Size,
  IN OUT VOID   *Buffer
  )
{
  FW_CFG_CACHE_WORK_AREA  *FwCfgCacheWrokArea;
  FW_CFG_CACHED_ITEM      *CachedItem;
  UINT32                  ReadSize;

  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  FwCfgCacheWrokArea = InternalQemuFwCfgCacheGetWorkArea ();
  if (FwCfgCacheWrokArea == NULL) {
    return RETURN_NOT_FOUND;
  }

  if (!FwCfgCacheWrokArea->Reading) {
    return RETURN_NOT_READY;
  }

  CachedItem = InternalQemuFwCfgItemCached (FwCfgCacheWrokArea->FwCfgItem);
  if (CachedItem == NULL) {
    return RETURN_NOT_FOUND;
  }

  if (CachedItem->DataSize - FwCfgCacheWrokArea->Offset > Size) {
    ReadSize = Size;
  } else {
    ReadSize = CachedItem->DataSize - FwCfgCacheWrokArea->Offset;
  }

  CopyMem (Buffer, (UINT8 *)CachedItem + sizeof (FW_CFG_CACHED_ITEM) + FwCfgCacheWrokArea->Offset, ReadSize);
  FwCfgCacheWrokArea->Offset += ReadSize;

  DEBUG ((DEBUG_INFO, "%a: found Item 0x%x in FwCfg Cache\n", __func__, FwCfgCacheWrokArea->FwCfgItem));
  return RETURN_SUCCESS;
}

RETURN_STATUS
InternalQemuFwCfgItemInCacheList (
  IN   CONST CHAR8           *Name,
  OUT  FIRMWARE_CONFIG_ITEM  *Item,
  OUT  UINTN                 *Size
  )
{
  UINT16              HobSize;
  UINT16              Offset;
  FW_CFG_CACHED_ITEM  *CachedItem;
  EFI_HOB_GUID_TYPE   *GuidHob;
  UINT32              CachedItemSize;

  CachedItem = InternalQemuFwCfgCacheFirstItem ();
  if (CachedItem == NULL) {
    return RETURN_NOT_FOUND;
  }

  GuidHob = GetFirstGuidHob (&gOvmfFwCfgInfoHobGuid);
  ASSERT (GuidHob != NULL);

  HobSize = GET_GUID_HOB_DATA_SIZE (GuidHob);
  Offset  = sizeof (EFI_HOB_GUID_TYPE) + sizeof (FW_CFG_CACHE_WORK_AREA);
  ASSERT (Offset < HobSize);

  while (Offset < HobSize) {
    if (AsciiStrCmp (Name, CachedItem->FileName) == 0) {
      *Item = CachedItem->FwCfgItem;
      *Size = CachedItem->DataSize;
      return RETURN_SUCCESS;
    }

    CachedItemSize = CachedItem->DataSize + sizeof (FW_CFG_CACHED_ITEM);
    Offset        += CachedItemSize;
    CachedItem     = (FW_CFG_CACHED_ITEM *)((UINT8 *)CachedItem + CachedItemSize);
  }

  return RETURN_NOT_FOUND;
}

EFI_STATUS
IntelnalQemuFwCfgInitCache (
  VOID
  )
{
  UINTN                 TotalSize;
  UINT32                Index;
  UINT32                Count;
  UINTN                 FwCfgSize;
  FIRMWARE_CONFIG_ITEM  FwCfgItem;
  CACHE_FW_CFG_STRCUT   CacheFwCfgList[CACHE_FW_CFG_COUNT];
  FW_CFG_CACHED_ITEM    *CachedItem;
  UINT8                 *ItemData;
  UINT8                 *FwCfhHobData;

  CopyMem (CacheFwCfgList, mCacheFwCfgList, sizeof (mCacheFwCfgList));
  Count     = CACHE_FW_CFG_COUNT;
  TotalSize = 0;

  for (Index = 0; Index < Count; Index++) {
    if (EFI_ERROR (QemuFwCfgFindFile (CacheFwCfgList[Index].FileName, &FwCfgItem, &FwCfgSize))) {
      continue;
    }

    if (FwCfgSize == 0) {
      continue;
    }

    if ((CacheFwCfgList[Index].FwCfgSize != 0) && (FwCfgSize != CacheFwCfgList[Index].FwCfgSize)) {
      continue;
    }

    CacheFwCfgList[Index].FwCfgItem = FwCfgItem;
    CacheFwCfgList[Index].FwCfgSize = FwCfgSize;
    TotalSize                      += (FwCfgSize + sizeof (FW_CFG_CACHED_ITEM));
  }

  DEBUG ((DEBUG_INFO, "%a: FW_CFG TotalSize is %d (Bytes)\n", __func__, TotalSize));

  if (TotalSize < sizeof (FW_CFG_CACHED_ITEM)) {
    return EFI_INVALID_PARAMETER;
  }

  FwCfhHobData = BuildGuidHob (&gOvmfFwCfgInfoHobGuid, TotalSize + sizeof (FW_CFG_CACHE_WORK_AREA));
  if (FwCfhHobData == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: BuildGuidHob Failed with TotalSize(0x%x)\n", __func__, TotalSize));
    return EFI_OUT_OF_RESOURCES;
  }

  ZeroMem (FwCfhHobData, TotalSize);

  FW_CFG_CACHE_WORK_AREA  *FwCfgCacheWrokArea;

  FwCfgCacheWrokArea             = (FW_CFG_CACHE_WORK_AREA *)FwCfhHobData;
  FwCfgCacheWrokArea->CacheReady = FALSE;

  CachedItem = (FW_CFG_CACHED_ITEM *)(FwCfhHobData + sizeof (FW_CFG_CACHE_WORK_AREA));

  for (Index = 0; Index < Count; Index++) {
    if (CacheFwCfgList[Index].FwCfgItem == INVALID_FW_CFG_ITEM) {
      continue;
    }

    DEBUG (
           (
            DEBUG_INFO,
            "%a: Name: %a Item:0x%x Size: 0x%x \n",
            __func__,
            CacheFwCfgList[Index].FileName,
            CacheFwCfgList[Index].FwCfgItem,
            CacheFwCfgList[Index].FwCfgSize
           )
           );
    CachedItem->FwCfgItem = CacheFwCfgList[Index].FwCfgItem;
    CachedItem->DataSize  = CacheFwCfgList[Index].FwCfgSize;
    CopyMem (CachedItem->FileName, CacheFwCfgList[Index].FileName, QEMU_FW_CFG_FNAME_SIZE);

    ItemData = (UINT8 *)CachedItem + sizeof (FW_CFG_CACHED_ITEM);

    QemuFwCfgSelectItem (CacheFwCfgList[Index].FwCfgItem);
    QemuFwCfgReadBytes (CacheFwCfgList[Index].FwCfgSize, ItemData);

    if (CacheFwCfgList[Index].NeedMeasure == FALSE) {
      CachedItem = (FW_CFG_CACHED_ITEM *)((UINT8 *)CachedItem +  sizeof (FW_CFG_CACHED_ITEM) + CachedItem->DataSize);
      continue;
    }

    if (TdIsEnabled ()) {
      FW_CFG_EVENT  FwCfgEvent;
      EFI_STATUS    Status;

      ZeroMem (&FwCfgEvent, sizeof (FW_CFG_EVENT));
      CopyMem (&FwCfgEvent.FwCfg, EV_POSTCODE_INFO_QEMU_FW_CFG_DATA, sizeof (EV_POSTCODE_INFO_QEMU_FW_CFG_DATA));
      CopyMem (&FwCfgEvent.FwCfgFileName, CacheFwCfgList[Index].FileName, QEMU_FW_CFG_FNAME_SIZE);

      Status = TdxHelperMeasureFwCfgData (
                                          (VOID *)&FwCfgEvent,
                                          sizeof (FwCfgEvent),
                                          (VOID *)ItemData,
                                          CacheFwCfgList[Index].FwCfgSize
                                          );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "TdxHelperMeasureFwCfgData failed with %r\n", Status));
      }
    }

    CachedItem = (FW_CFG_CACHED_ITEM *)((UINT8 *)CachedItem +  sizeof (FW_CFG_CACHED_ITEM) + CachedItem->DataSize);
  }

  FwCfgCacheWrokArea->CacheReady = TRUE;
  return EFI_SUCCESS;
}
