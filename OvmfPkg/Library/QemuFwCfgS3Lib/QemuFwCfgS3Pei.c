/** @file
  Limited functionality QemuFwCfgS3Lib instance, for PEI phase modules.

  QemuFwCfgS3Enabled() queries S3 enablement via fw_cfg. Other library APIs
  will report lack of support.

  Copyright (C) 2017, Red Hat, Inc.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/QemuFwCfgS3Lib.h>
#include <Library/DebugLib.h>
#include <IndustryStandard/UefiTcgPlatform.h>
#include <Library/TdxLib.h>
#include <Library/BaseCryptLib.h>
#include <Library/HobLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

/**
  Install the client module's FW_CFG_BOOT_SCRIPT_CALLBACK_FUNCTION callback for
  when the production of ACPI S3 Boot Script opcodes becomes possible.

  Take ownership of the client-provided Context, and pass it to the callback
  function, when the latter is invoked.

  Allocate scratch space for those ACPI S3 Boot Script opcodes to work upon
  that the client will produce in the callback function.

  @param[in] Callback           FW_CFG_BOOT_SCRIPT_CALLBACK_FUNCTION to invoke
                                when the production of ACPI S3 Boot Script
                                opcodes becomes possible. Callback() may be
                                called immediately from
                                QemuFwCfgS3CallWhenBootScriptReady().

  @param[in,out] Context        Client-provided data structure for the
                                Callback() callback function to consume.

                                If Context points to dynamically allocated
                                memory, then Callback() must release it.

                                If Context points to dynamically allocated
                                memory, and
                                QemuFwCfgS3CallWhenBootScriptReady() returns
                                successfully, then the caller of
                                QemuFwCfgS3CallWhenBootScriptReady() must
                                neither dereference nor even evaluate Context
                                any longer, as ownership of the referenced area
                                has been transferred to Callback().

  @param[in] ScratchBufferSize  The size of the scratch buffer that will hold,
                                in reserved memory, all client data read,
                                written, and checked by the ACPI S3 Boot Script
                                opcodes produced by Callback().

  @retval RETURN_UNSUPPORTED       The library instance does not support this
                                   function.

  @retval RETURN_NOT_FOUND         The fw_cfg DMA interface to QEMU is
                                   unavailable.

  @retval RETURN_BAD_BUFFER_SIZE   ScratchBufferSize is too large.

  @retval RETURN_OUT_OF_RESOURCES  Memory allocation failed.

  @retval RETURN_SUCCESS           Callback() has been installed, and the
                                   ownership of Context has been transferred.
                                   Reserved memory has been allocated for the
                                   scratch buffer.

                                   A successful invocation of
                                   QemuFwCfgS3CallWhenBootScriptReady() cannot
                                   be rolled back.

  @return                          Error codes from underlying functions.
**/
RETURN_STATUS
EFIAPI
QemuFwCfgS3CallWhenBootScriptReady (
  IN     FW_CFG_BOOT_SCRIPT_CALLBACK_FUNCTION  *Callback,
  IN OUT VOID                                  *Context           OPTIONAL,
  IN     UINTN                                 ScratchBufferSize
  )
{
  return RETURN_UNSUPPORTED;
}


STATIC
EFI_STATUS
BuildTdxMeasurementGuidHob (
  UINT32  RtmrIndex,
  UINT32  EventType,
  UINT8   *EventData,
  UINT32  EventSize,
  UINT8   *HashValue,
  UINT32  HashSize
  )
{
  VOID                *EventHobData;
  UINT8               *Ptr;
  TPML_DIGEST_VALUES  *TdxDigest;

  if (HashSize != SHA384_DIGEST_SIZE) {
    return EFI_INVALID_PARAMETER;
  }

  #define TDX_DIGEST_VALUE_LEN  (sizeof (UINT32) + sizeof (TPMI_ALG_HASH) + SHA384_DIGEST_SIZE)

  EventHobData = BuildGuidHob (
                   &gCcEventEntryHobGuid,
                   sizeof (TCG_PCRINDEX) + sizeof (TCG_EVENTTYPE) +
                   TDX_DIGEST_VALUE_LEN +
                   sizeof (UINT32) + EventSize
                   );

  if (EventHobData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Ptr = (UINT8 *)EventHobData;

  //
  // There are 2 types of measurement registers in TDX: MRTD and RTMR[0-3].
  // According to UEFI Spec 2.10 Section 38.4.1, RTMR[0-3] is mapped to MrIndex[1-4].
  // So RtmrIndex must be increased by 1 before the event log is created.
  //
  RtmrIndex++;
  CopyMem (Ptr, &RtmrIndex, sizeof (UINT32));
  Ptr += sizeof (UINT32);

  CopyMem (Ptr, &EventType, sizeof (TCG_EVENTTYPE));
  Ptr += sizeof (TCG_EVENTTYPE);

  TdxDigest                     = (TPML_DIGEST_VALUES *)Ptr;
  TdxDigest->count              = 1;
  TdxDigest->digests[0].hashAlg = TPM_ALG_SHA384;
  CopyMem (
    TdxDigest->digests[0].digest.sha384,
    HashValue,
    SHA384_DIGEST_SIZE
    );
  Ptr += TDX_DIGEST_VALUE_LEN;

  CopyMem (Ptr, &EventSize, sizeof (UINT32));
  Ptr += sizeof (UINT32);

  CopyMem (Ptr, (VOID *)EventData, EventSize);
  Ptr += EventSize;

  return EFI_SUCCESS;
}

/**
 * Calculate the sha384 of input Data and extend it to RTMR register.
 *
 * @param RtmrIndex       Index of the RTMR register
 * @param DataToHash      Data to be hashed
 * @param DataToHashLen   Length of the data
 * @param Digest          Hash value of the input data
 * @param DigestLen       Length of the hash value
 *
 * @retval EFI_SUCCESS    Successfully hash and extend to RTMR
 * @retval Others         Other errors as indicated
 */
STATIC
EFI_STATUS
HashAndExtendToRtmr (
  IN UINT32  RtmrIndex,
  IN VOID    *DataToHash,
  IN UINTN   DataToHashLen,
  OUT UINT8  *Digest,
  IN  UINTN  DigestLen
  )
{
  EFI_STATUS  Status;

  if ((DataToHash == NULL) || (DataToHashLen == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Digest == NULL) || (DigestLen != SHA384_DIGEST_SIZE)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Calculate the sha384 of the data
  //
  if (!Sha384HashAll (DataToHash, DataToHashLen, Digest)) {
    return EFI_ABORTED;
  }

  //
  // Extend to RTMR
  //
  Status = TdExtendRtmr (
             (UINT32 *)Digest,
             SHA384_DIGEST_SIZE,
             (UINT8)RtmrIndex
             );

  ASSERT (!EFI_ERROR (Status));
  return Status;
}


VOID
TdxMeasurementQemuFwCfgS3 (
  IN VOID    *EventLog,
  IN UINT32  LogLen,
  IN VOID    *HashData,
  IN UINT64  HashDataLen
)
{
  EFI_STATUS  Status;
  UINT8       Digest[SHA384_DIGEST_SIZE];

  if ((EventLog == NULL) || (HashData == NULL)) {
    return;
  }

  if (!TdIsEnabled ()) {
    return;
  }

  Status = HashAndExtendToRtmr (
             0,
             (UINT8 *)HashData,
             (UINTN)HashDataLen,
             Digest,
             SHA384_DIGEST_SIZE
             );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: HashAndExtendToRtmr failed with %r\n", __func__, Status));
    return;
  }

  Status = BuildTdxMeasurementGuidHob (
             0,
             EV_PLATFORM_CONFIG_FLAGS,
             EventLog,
             LogLen,
             Digest,
             SHA384_DIGEST_SIZE
             );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: BuildTdxMeasurementGuidHob failed with %r\n", __func__, Status));
    return;
  }
}
