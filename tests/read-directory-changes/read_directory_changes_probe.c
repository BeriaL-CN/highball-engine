/* Generic ReadDirectoryChangesW probe for Wine's Darwin directory backend. */
#include <windows.h>
#include <stdio.h>
#include <wchar.h>

struct watcher
{
    HANDLE dir;
    HANDLE thread;
    HANDLE done;
    DWORD filter;
    BOOL result;
    DWORD error;
    DWORD bytes;
    BYTE buffer[8192];
};

static DWORD WINAPI watch_thread( void *arg )
{
    struct watcher *watcher = arg;
    watcher->result = ReadDirectoryChangesW( watcher->dir, watcher->buffer,
                                             sizeof(watcher->buffer), FALSE,
                                             watcher->filter,
                                             &watcher->bytes, NULL, NULL );
    watcher->error = watcher->result ? ERROR_SUCCESS : GetLastError();
    SetEvent( watcher->done );
    return 0;
}

static int start_watcher( struct watcher *watcher, HANDLE dir, DWORD filter )
{
    ZeroMemory( watcher, sizeof(*watcher) );
    watcher->dir = dir;
    watcher->filter = filter;
    watcher->done = CreateEventW( NULL, TRUE, FALSE, NULL );
    if (!watcher->done) return 0;
    watcher->thread = CreateThread( NULL, 0, watch_thread, watcher, 0, NULL );
    if (!watcher->thread)
    {
        CloseHandle( watcher->done );
        watcher->done = NULL;
        return 0;
    }
    Sleep( 50 );
    return 1;
}

static void close_watcher( struct watcher *watcher )
{
    if (watcher->thread) CloseHandle( watcher->thread );
    if (watcher->done) CloseHandle( watcher->done );
    watcher->thread = NULL;
    watcher->done = NULL;
}

static int wait_for_change( struct watcher *watcher, DWORD timeout )
{
    if (WaitForSingleObject( watcher->done, timeout ) != WAIT_OBJECT_0)
        return 0;
    return watcher->result;
}

static int cancel_no_change( struct watcher *watcher )
{
    if (WaitForSingleObject( watcher->done, 300 ) != WAIT_TIMEOUT) return 0;
    if (!CancelSynchronousIo( watcher->thread )) return 0;
    if (WaitForSingleObject( watcher->done, 2000 ) != WAIT_OBJECT_0) return 0;
    return watcher->error == ERROR_OPERATION_ABORTED;
}

static int event_name_matches( const FILE_NOTIFY_INFORMATION *event, const WCHAR *expected )
{
    size_t len = wcslen( expected );
    return event->FileNameLength / sizeof(WCHAR) == len &&
           !wcsncmp( event->FileName, expected, len );
}

static int parse_events( struct watcher *watcher, const WCHAR *expected,
                         const WCHAR *second_expected, DWORD first_action,
                         DWORD second_action, int *fallback )
{
    DWORD offset = 0, count = 0, first = 0, second = 0;
    const FILE_NOTIFY_INFORMATION *event;

    while (offset < watcher->bytes)
    {
        event = (const FILE_NOTIFY_INFORMATION *)(watcher->buffer + offset);
        if (!event->FileNameLength ||
            !event_name_matches( event, count && second_expected ? second_expected : expected )) return 0;
        if (!count) first = event->Action;
        else if (count == 1) second = event->Action;
        count++;
        if (!event->NextEntryOffset) break;
        offset += event->NextEntryOffset;
    }

    if (count == 1 && first == first_action) return 1;
    if (second_action && count == 2 && first == first_action && second == second_action) return 1;
    if (fallback && count == 2 && first == FILE_ACTION_REMOVED && second == FILE_ACTION_ADDED)
    {
        *fallback = 1;
        return 1;
    }
    return 0;
}

static HANDLE open_directory( const WCHAR *path )
{
    return CreateFileW( path, FILE_LIST_DIRECTORY,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL );
}

static int make_path( WCHAR *path, DWORD size )
{
    DWORD len = GetTempPathW( size, path );
    if (!len || len >= size - 32) return 0;
    swprintf( path + len, size - len, L"rdc-probe-%lu", GetCurrentProcessId() );
    RemoveDirectoryW( path );
    return CreateDirectoryW( path, NULL );
}

static int create_file( const WCHAR *dir, const WCHAR *name )
{
    WCHAR path[MAX_PATH];
    HANDLE file;
    swprintf( path, ARRAYSIZE(path), L"%s\\%s", dir, name );
    file = CreateFileW( path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
    if (file == INVALID_HANDLE_VALUE) return 0;
    CloseHandle( file );
    return 1;
}

static int test_create( const WCHAR *dir_path )
{
    WCHAR path[MAX_PATH];
    HANDLE dir, file;
    struct watcher watcher;
    int ok;

    dir = open_directory( dir_path );
    if (dir == INVALID_HANDLE_VALUE || !start_watcher( &watcher, dir,
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_CREATION )) return 0;
    swprintf( path, ARRAYSIZE(path), L"%s\\baseurl.json", dir_path );
    file = CreateFileW( path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL );
    if (file != INVALID_HANDLE_VALUE) CloseHandle( file );
    ok = file != INVALID_HANDLE_VALUE && wait_for_change( &watcher, 3000 ) &&
         parse_events( &watcher, L"baseurl.json", NULL, FILE_ACTION_ADDED, 0, NULL );
    printf( "create: %s\n", ok ? "PASS" : "FAIL" );
    close_watcher( &watcher );
    CloseHandle( dir );
    DeleteFileW( path );
    return ok;
}

static int test_modify( const WCHAR *dir_path )
{
    WCHAR path[MAX_PATH];
    HANDLE dir, file;
    struct watcher watcher;
    DWORD written;
    int ok;

    swprintf( path, ARRAYSIZE(path), L"%s\\modify.txt", dir_path );
    if (!create_file( dir_path, L"modify.txt" )) return 0;
    dir = open_directory( dir_path );
    if (dir == INVALID_HANDLE_VALUE || !start_watcher( &watcher, dir,
        FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE )) return 0;
    file = CreateFileW( path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
    if (file != INVALID_HANDLE_VALUE) WriteFile( file, "changed", 7, &written, NULL );
    if (file != INVALID_HANDLE_VALUE) CloseHandle( file );
    ok = file != INVALID_HANDLE_VALUE && wait_for_change( &watcher, 3000 ) &&
         parse_events( &watcher, L"modify.txt", NULL, FILE_ACTION_MODIFIED, 0, NULL );
    printf( "modify: %s\n", ok ? "PASS" : "FAIL" );
    close_watcher( &watcher );
    CloseHandle( dir );
    DeleteFileW( path );
    return ok;
}

static int test_delete( const WCHAR *dir_path )
{
    WCHAR path[MAX_PATH];
    HANDLE dir;
    struct watcher watcher;
    int ok;

    swprintf( path, ARRAYSIZE(path), L"%s\\delete.txt", dir_path );
    if (!create_file( dir_path, L"delete.txt" )) return 0;
    dir = open_directory( dir_path );
    if (dir == INVALID_HANDLE_VALUE || !start_watcher( &watcher, dir,
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME )) return 0;
    DeleteFileW( path );
    ok = wait_for_change( &watcher, 3000 ) &&
         parse_events( &watcher, L"delete.txt", NULL, FILE_ACTION_REMOVED, 0, NULL );
    printf( "delete: %s\n", ok ? "PASS" : "FAIL" );
    close_watcher( &watcher );
    CloseHandle( dir );
    return ok;
}

static int test_rename( const WCHAR *dir_path )
{
    WCHAR old_path[MAX_PATH], new_path[MAX_PATH];
    HANDLE dir;
    struct watcher watcher;
    int fallback = 0, ok;

    swprintf( old_path, ARRAYSIZE(old_path), L"%s\\old.txt", dir_path );
    swprintf( new_path, ARRAYSIZE(new_path), L"%s\\new.txt", dir_path );
    if (!create_file( dir_path, L"old.txt" )) return 0;
    dir = open_directory( dir_path );
    if (dir == INVALID_HANDLE_VALUE || !start_watcher( &watcher, dir,
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME )) return 0;
    MoveFileW( old_path, new_path );
    ok = wait_for_change( &watcher, 3000 ) &&
         parse_events( &watcher, L"old.txt", L"new.txt", FILE_ACTION_RENAMED_OLD_NAME,
                       FILE_ACTION_RENAMED_NEW_NAME, &fallback );
    if (!ok)
    {
        /* A second read accepts the documented REMOVED + ADDED fallback. */
        ok = wait_for_change( &watcher, 3000 ) &&
             parse_events( &watcher, L"new.txt", NULL, FILE_ACTION_ADDED, 0, &fallback );
    }
    printf( "rename: %s%s\n", ok ? "PASS" : "FAIL", fallback ? " (removed+added)" : "" );
    close_watcher( &watcher );
    CloseHandle( dir );
    DeleteFileW( new_path );
    return ok;
}

static int test_no_change( const WCHAR *dir_path )
{
    HANDLE dir;
    struct watcher watcher;
    int ok;

    dir = open_directory( dir_path );
    if (dir == INVALID_HANDLE_VALUE || !start_watcher( &watcher, dir, FILE_NOTIFY_CHANGE_FILE_NAME )) return 0;
    ok = cancel_no_change( &watcher );
    printf( "no-change: %s\n", ok ? "PASS" : "FAIL" );
    close_watcher( &watcher );
    CloseHandle( dir );
    return ok;
}

int main(void)
{
    WCHAR dir_path[MAX_PATH];
    int ok;

    setvbuf( stdout, NULL, _IONBF, 0 );
    if (!make_path( dir_path, ARRAYSIZE(dir_path) ))
    {
        printf( "setup: FAIL err=%lu\n", GetLastError() );
        return 1;
    }
    ok = test_create( dir_path );
    ok &= test_modify( dir_path );
    ok &= test_delete( dir_path );
    ok &= test_rename( dir_path );
    ok &= test_no_change( dir_path );
    RemoveDirectoryW( dir_path );
    return ok ? 0 : 1;
}
