#pragma once
#include <windows.h>
typedef int NTSTATUS;
typedef struct { NTSTATUS Status; unsigned Information; } IO_STATUS_BLOCK;
#define STATUS_PENDING 0x103
#define NT_SUCCESS(st) ((st) >= 0)
NTSTATUS NtReadFile(HANDLE, void*, void*, void*, IO_STATUS_BLOCK*, void*, unsigned, LARGE_INTEGER*);
NTSTATUS NtWaitForSingleObject(HANDLE, int, void*);
