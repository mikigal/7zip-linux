/* NTSecAPI.h -- the LSA and access token types of Windows/SecurityUtils.h,
   which FileManager/FM.cpp includes. 7zFM calls these only in Windows code
   (the lock-memory and symbolic link privileges, see linux/docs/PLAN.md,
   Appendix A), so they are declared and never implemented. */

#ifndef Z7_LINUX_SHIM_NTSECAPI_H
#define Z7_LINUX_SHIM_NTSECAPI_H

#include <windows.h>

#define NTAPI

typedef LONG NTSTATUS;
typedef DWORD ACCESS_MASK;
typedef DWORD *PDWORD;
typedef ULONG *PULONG;
typedef void *PSID;
typedef PVOID LSA_HANDLE, *PLSA_HANDLE;

typedef struct _LSA_UNICODE_STRING
{
  USHORT Length;
  USHORT MaximumLength;
  WCHAR *Buffer;
} LSA_UNICODE_STRING, *PLSA_UNICODE_STRING;

typedef struct _LSA_OBJECT_ATTRIBUTES
{
  ULONG Length;
  HANDLE RootDirectory;
  PLSA_UNICODE_STRING ObjectName;
  ULONG Attributes;
  PVOID SecurityDescriptor;
  PVOID SecurityQualityOfService;
} LSA_OBJECT_ATTRIBUTES, *PLSA_OBJECT_ATTRIBUTES;

typedef struct _LSA_ENUMERATION_INFORMATION { PSID Sid; } LSA_ENUMERATION_INFORMATION, *PLSA_ENUMERATION_INFORMATION;
typedef struct _LSA_REFERENCED_DOMAIN_LIST *PLSA_REFERENCED_DOMAIN_LIST;
typedef struct _LSA_TRANSLATED_NAME *PLSA_TRANSLATED_NAME;

typedef struct _LUID
{
  DWORD LowPart;
  LONG HighPart;
} LUID;

typedef struct _LUID_AND_ATTRIBUTES
{
  LUID Luid;
  DWORD Attributes;
} LUID_AND_ATTRIBUTES;

typedef struct _TOKEN_PRIVILEGES
{
  DWORD PrivilegeCount;
  LUID_AND_ATTRIBUTES Privileges[1];
} TOKEN_PRIVILEGES, *PTOKEN_PRIVILEGES;

BOOL OpenProcessToken(HANDLE process, DWORD desiredAccess, PHANDLE token);
BOOL AdjustTokenPrivileges(HANDLE token, BOOL disableAll, PTOKEN_PRIVILEGES newState,
    DWORD bufferLength, PTOKEN_PRIVILEGES previousState, PDWORD returnLength);
NTSTATUS LsaEnumerateAccountsWithUserRight(LSA_HANDLE policy, PLSA_UNICODE_STRING userRight,
    PVOID *buffer, PULONG countReturned);
NTSTATUS LsaEnumerateAccountRights(LSA_HANDLE policy, PSID sid, PLSA_UNICODE_STRING *userRights,
    PULONG countOfRights);
NTSTATUS LsaLookupSids(LSA_HANDLE policy, ULONG count, PSID *sids,
    PLSA_REFERENCED_DOMAIN_LIST *domains, PLSA_TRANSLATED_NAME *names);
NTSTATUS LsaRemoveAccountRights(LSA_HANDLE policy, PSID sid, BOOLEAN allRights,
    PLSA_UNICODE_STRING userRights, ULONG countOfRights);

#endif
