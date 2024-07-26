#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

void ProcessNotification(const FILE_NOTIFY_INFORMATION* pInfo) {
    WCHAR fileName[MAX_PATH];
    wcsncpy_s(fileName, MAX_PATH, pInfo->FileName, pInfo->FileNameLength / sizeof(WCHAR));
    fileName[pInfo->FileNameLength / sizeof(WCHAR)] = L'\0';

    switch (pInfo->Action) {
    case FILE_ACTION_ADDED:
        wprintf(L"File added: %ls\n", fileName);
        break;
    case FILE_ACTION_REMOVED:
        wprintf(L"File removed: %ls\n", fileName);
        break;
    case FILE_ACTION_MODIFIED:
        wprintf(L"File modified: %ls\n", fileName);
        break;
    case FILE_ACTION_RENAMED_OLD_NAME:
        wprintf(L"File renamed from: %ls\n", fileName);
        break;
    case FILE_ACTION_RENAMED_NEW_NAME:
        wprintf(L"File renamed to: %ls\n", fileName);
        break;
    default:
        wprintf(L"Unknown action: %ls\n", fileName);
    }
}

int main() {
    HANDLE hDir = CreateFileW(L"\\PATH\\TO\\FILE", FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
    if (hDir == INVALID_HANDLE_VALUE) {
        printf("CreateFile failed (%d)\n", GetLastError());
        return 1;
    }

    HANDLE hCompletionPort = CreateIoCompletionPort(hDir, NULL, 0, 0);
    if (hCompletionPort == NULL) {
        printf("CreateIoCompletionPort failed (%d)\n", GetLastError());
        CloseHandle(hDir);
        return 1;
    }

    char buffer[4096];
    DWORD bytesReturned;
    OVERLAPPED overlapped = { 0 };

    if (!ReadDirectoryChangesW(hDir, buffer, sizeof(buffer), TRUE,
        FILE_NOTIFY_CHANGE_FILE_NAME |
        FILE_NOTIFY_CHANGE_DIR_NAME |
        FILE_NOTIFY_CHANGE_ATTRIBUTES |
        FILE_NOTIFY_CHANGE_SIZE |
        FILE_NOTIFY_CHANGE_LAST_WRITE |
        FILE_NOTIFY_CHANGE_LAST_ACCESS |
        FILE_NOTIFY_CHANGE_CREATION |
        FILE_NOTIFY_CHANGE_SECURITY,
        NULL, &overlapped, NULL)) {
        printf("ReadDirectoryChangesW failed (%d)\n", GetLastError());
        CloseHandle(hDir);
        CloseHandle(hCompletionPort);
        return 1;
    }

    while (TRUE) {
        DWORD numberOfBytes;
        ULONG_PTR completionKey;
        LPOVERLAPPED pOverlapped;

        BOOL result = GetQueuedCompletionStatus(hCompletionPort, &numberOfBytes, &completionKey, &pOverlapped, INFINITE);
        if (result) {
            FILE_NOTIFY_INFORMATION* pNotify = (FILE_NOTIFY_INFORMATION*)buffer;
            do {
                ProcessNotification(pNotify);
                if (pNotify->NextEntryOffset == 0) {
                    break;
                }
                pNotify = (FILE_NOTIFY_INFORMATION*)((char*)pNotify + pNotify->NextEntryOffset);
            } while (TRUE);

            // Reissue the ReadDirectoryChangesW call
            if (!ReadDirectoryChangesW(hDir, buffer, sizeof(buffer), TRUE,
                FILE_NOTIFY_CHANGE_FILE_NAME |
                FILE_NOTIFY_CHANGE_DIR_NAME |
                FILE_NOTIFY_CHANGE_ATTRIBUTES |
                FILE_NOTIFY_CHANGE_SIZE |
                FILE_NOTIFY_CHANGE_LAST_WRITE |
                FILE_NOTIFY_CHANGE_LAST_ACCESS |
                FILE_NOTIFY_CHANGE_CREATION |
                FILE_NOTIFY_CHANGE_SECURITY,
                NULL, &overlapped, NULL)) {
                printf("ReadDirectoryChangesW failed (%d)\n", GetLastError());
                CloseHandle(hDir);
                CloseHandle(hCompletionPort);
                return 1;
            }
        }
        else {
            printf("GetQueuedCompletionStatus failed (%d)\n", GetLastError());
            CloseHandle(hDir);
            CloseHandle(hCompletionPort);
            return 1;
        }
    }

    CloseHandle(hDir);
    CloseHandle(hCompletionPort);
    return 0;
}
