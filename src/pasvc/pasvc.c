#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>

#ifndef PAW32_VERSION
#define PAW32_VERSION "unknown"
#endif

#define PAW32_SVC L"PulseAudio"

static wchar_t fn_executable[MAX_PATH] = {0};
static wchar_t fn_pulseaudio[MAX_PATH] = {0};
static wchar_t fn_appdata[MAX_PATH]    = {0};

// COMMAND

static int main_help(int argc, char **argv);
static int main_install(int argc, char **argv);
static int main_start(int argc, char **argv);
static int main_stop(int argc, char **argv);
static int main_delete(int argc, char **argv);

static const struct {
    const char *name;
    int (*main)(int argc, char **argv);
    const wchar_t *desc;
} main_cmd[] = {
    {"/?", main_help, L"Show this help message."},
    {"install", main_install, L"Install the service."},
    {"start", main_start, L"Start or restart the service."},
    {"stop", main_stop, L"Stop the service if it is running."},
    {"delete", main_delete, L"Delete the service (it must be stopped first or it will not be deleted until a reboot)."},
    {0},
};

int main(int argc, char **argv) {
    switch (GetModuleFileNameW(NULL, fn_executable, sizeof(fn_executable))) {
    case 0:
    case sizeof(fn_executable):
        fprintf(stderr, "Error: Failed to get own filename.\n");
        return GetLastError();
    }

    if (!PathCombineW(fn_pulseaudio, fn_executable, L"..\\pulseaudio.exe")) {
        fprintf(stderr, "Error: Failed to resolve pulseaudio.exe path.\n");
        return ERROR_GEN_FAILURE;
    }

    if (GetFileAttributesW(fn_pulseaudio) == INVALID_FILE_ATTRIBUTES)
        fprintf(stderr, "Warning: %ls does not exist.\n", fn_pulseaudio);

    if (SHGetFolderPathAndSubDirW(NULL, CSIDL_COMMON_APPDATA|CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, L"PulseAudio", fn_appdata) != S_OK) {
        fprintf(stderr, "Error: Failed to resolve PulseAudio common appdata path.\n");
        return ERROR_GEN_FAILURE;
    }

    if (argc >= 2)
        for (typeof(*main_cmd) *cmd = main_cmd; cmd->name; cmd++)
            if (!strcasecmp(argv[1], cmd->name))
                return cmd->main(argc, argv);

    return main_help(argc, argv);
}

int main_help(int argc, char** argv) {
    (void) argc; (void) argv;

    printf("Usage:\n");
    for (typeof(*main_cmd) *cmd = main_cmd; cmd->name; cmd++) {
        if (cmd->desc) {
            printf("  %s %-16s %ls\n", argv[0], cmd->name, cmd->desc);
        }
    }

    printf("\nPulseAudio path:\n");
    printf("  %ls\n", fn_pulseaudio);

    printf("\nData path:\n");
    printf("  %ls\n", fn_appdata);

    printf("\nLog path:\n");
    printf("  %ls\\pulseaudio.log\n", fn_appdata);

    printf("\nService name:\n");
    printf("  %ls\n", PAW32_SVC);

    printf("\nAbout:\n");
    printf("  pulseaudio-win32 %s\n", PAW32_VERSION);
    printf("  github.com/pgaskin/pulseaudio-win32\n");

    return ERROR_INVALID_FUNCTION;
}

int main_install(int argc, char **argv) {
    bool ml = false;
    if (argc < 2)
        return main_help(argc, argv);
    for (int i = 2; i < argc; i++) {
        if (!strcasecmp(argv[i], "/AllowModuleLoad")) {
            ml = true;
        } else {
            return main_help(argc, argv);
        }
    }

    wchar_t qp[MAX_PATH*3] = {0};
    wcscat(qp, L"\"");
    wcscat(qp, fn_pulseaudio);
    wcscat(qp, L"\" --system --disallow-exit --log-target=file:\"");
    wcscat(qp, fn_appdata);
    wcscat(qp, L"\\pulseaudio.log\" --log-time --log-level=3");
    if (!ml)
        wcscat(qp, L" --disallow-module-loading");

    SC_HANDLE scm;
    if (!(scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS))) {
        int err = GetLastError();
        fprintf(stderr, "Error: Failed to open service manager (%d).\n", err);
        return err;
    }

    SC_HANDLE scs;
    if (!(scs = CreateServiceW( 
        scm,
        PAW32_SVC, L"PulseAudio Sound Server",
        SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
        qp,
        NULL, NULL,
        L"AudioSrv\0\0", // note that the each \0 character correctly represents 2 bytes each for the wide string
        L"NT AUTHORITY\\LocalService", NULL
    ))) {
        int err = GetLastError();
        if (err == ERROR_SERVICE_EXISTS)
            fprintf(stderr, "Note: The service already exists. Please stop and remove it and try again.\n");
        fprintf(stderr, "Error: Failed to create service (%d).\n", err); 
        CloseServiceHandle(scm);
        return err;
    }

    if (!ChangeServiceConfig2W(scs, SERVICE_CONFIG_FAILURE_ACTIONS, &(SERVICE_FAILURE_ACTIONSW){
        .dwResetPeriod = 30000,
        .cActions = 5,
        .lpsaActions = (SC_ACTION[]){
            {.Type = SC_ACTION_RESTART, .Delay = 3000},
            {.Type = SC_ACTION_RESTART, .Delay = 3000},
            {.Type = SC_ACTION_RESTART, .Delay = 60000},
            {.Type = SC_ACTION_RESTART, .Delay = 12000},
            {.Type = SC_ACTION_NONE,    .Delay = 3600000},
        },
    })) {
        int err = GetLastError();
        fprintf(stderr, "Error: Failed to set service failure actions (%d).\n", err); 
        CloseServiceHandle(scm);
        CloseServiceHandle(scs);
        return err;
    };

    if (!ChangeServiceConfig2W(scs, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, &(SERVICE_FAILURE_ACTIONS_FLAG){
        .fFailureActionsOnNonCrashFailures = true,
    })) {
        int err = GetLastError();
        fprintf(stderr, "Error: Failed to set service failure action flag (%d).\n", err); 
        CloseServiceHandle(scm);
        CloseServiceHandle(scs);
        return err;
    };

    CloseServiceHandle(scs); 
    CloseServiceHandle(scm);

    printf("Successfully created service %ls.\n", PAW32_SVC);
    return NO_ERROR;
}

int main_start(int argc, char **argv) {
    if (argc != 2)
        return main_help(argc, argv);

    SC_HANDLE scm;
    if (!(scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS))) {
        int err = GetLastError();
        fprintf(stderr, "Error: Failed to open service manager (%d).\n", err);
        return err;
    }

    SC_HANDLE scs;
    if (!(scs = OpenServiceW(scm, PAW32_SVC, SERVICE_START|SERVICE_STOP|SERVICE_QUERY_STATUS))) { 
        int err = GetLastError();
        if (err == ERROR_SERVICE_DOES_NOT_EXIST) {
            fprintf(stderr, "Warning: The service does not exist, ignoring.\n");
            return NO_ERROR;
        }
        fprintf(stderr, "Error: Failed to open service (%d).\n", err);
        CloseServiceHandle(scm);
        return err;
    }

    bool restarted = false;

    if (!StartServiceW(scs, 0, NULL)) {
        int err = GetLastError();
        if (err != ERROR_SERVICE_ALREADY_RUNNING) {
            if (err == ERROR_SERVICE_DISABLED)
                fprintf(stderr, "Note: The service is disabled.\n");
            fprintf(stderr, "Error: Failed to start service (%d).\n", err);
            CloseServiceHandle(scs);
            CloseServiceHandle(scm);
            return err;
        }

        printf("The service is already running, restarting.\n");

        SERVICE_STATUS st;
        if (!ControlService(scs, SERVICE_CONTROL_STOP, &st)) {
            err = GetLastError();
            if (err != ERROR_SERVICE_NOT_ACTIVE) {
                fprintf(stderr, "Error: Failed to stop service (%d).\n", err);
                CloseServiceHandle(scs);
                CloseServiceHandle(scm);
                return err;
            }
        }

        SERVICE_STATUS_PROCESS ssp = {0};
        DWORD t = GetTickCount();
        for (;;) {
            Sleep(ssp.dwWaitHint);

            DWORD bn;
            if (!QueryServiceStatusEx(scs, SC_STATUS_PROCESS_INFO, (LPBYTE) &ssp, sizeof(SERVICE_STATUS_PROCESS), &bn)) {
                err = GetLastError();
                fprintf(stderr, "Error: Failed to get service status (%d).\n", err);
                CloseServiceHandle(scs);
                CloseServiceHandle(scm);
                return err;
            }

            if (ssp.dwCurrentState == SERVICE_STOPPED)
                break;

            if (GetTickCount()-t > 10000) {
                fprintf(stderr, "Error: Timed out while waiting for service to stop.\n");
                CloseServiceHandle(scs);
                CloseServiceHandle(scm);
                return ERROR_TIMEOUT;
            }
        }

        if (!StartServiceW(scs, 0, NULL)) {
            err = GetLastError();
            if (err != ERROR_SERVICE_ALREADY_RUNNING) {
                if (err == ERROR_SERVICE_DISABLED)
                    fprintf(stderr, "Note: The service is disabled.\n");
                fprintf(stderr, "Error: Failed to start service (%d).\n", err);
                CloseServiceHandle(scs);
                CloseServiceHandle(scm);
                return err;
            }
        }

        restarted = true;
    }

    SERVICE_STATUS_PROCESS ssp = {0};
    DWORD t = GetTickCount();
    for (;;) {
        Sleep(ssp.dwWaitHint);

        DWORD bn;
        if (!QueryServiceStatusEx(scs, SC_STATUS_PROCESS_INFO, (LPBYTE) &ssp, sizeof(SERVICE_STATUS_PROCESS), &bn)) {
            int err = GetLastError();
            fprintf(stderr, "Error: Failed to get service status (%d).\n", err);
            CloseServiceHandle(scs);
            CloseServiceHandle(scm);
            return err;
        }

        if (ssp.dwCurrentState == SERVICE_RUNNING)
            break;

        if (GetTickCount()-t > 10000) {
            fprintf(stderr, "Error: Timed out while waiting for service to start.\n");
            CloseServiceHandle(scs);
            CloseServiceHandle(scm);
            return ERROR_TIMEOUT;
        }
    }
 
    CloseServiceHandle(scs); 
    CloseServiceHandle(scm);

    printf("Successfully %s service %ls.\n", restarted ? "restarted" : "started", PAW32_SVC);
    return NO_ERROR;
}

int main_stop(int argc, char **argv) {
    if (argc != 2)
        return main_help(argc, argv);

    SC_HANDLE scm;
    if (!(scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS))) {
        int err = GetLastError();
        fprintf(stderr, "Error: Failed to open service manager (%d).\n", err);
        return err;
    }

    SC_HANDLE scs;
    if (!(scs = OpenServiceW(scm, PAW32_SVC, SERVICE_START|SERVICE_STOP|SERVICE_QUERY_STATUS))) { 
        int err = GetLastError();
        if (err == ERROR_SERVICE_DOES_NOT_EXIST) {
            fprintf(stderr, "Warning: The service does not exist, ignoring.\n");
            return NO_ERROR;
        }
        fprintf(stderr, "Error: Failed to open service (%d).\n", err);
        CloseServiceHandle(scm);
        return err;
    }

    SERVICE_STATUS st;
    if (!ControlService(scs, SERVICE_CONTROL_STOP, &st)) {
        int err = GetLastError();
        if (err == ERROR_SERVICE_NOT_ACTIVE) {
            fprintf(stderr, "Warning: The service is already stopped, ignoring.\n");
            return NO_ERROR;
        }
        fprintf(stderr, "Error: Failed to stop service (%d).\n", err);
        CloseServiceHandle(scs);
        CloseServiceHandle(scm);
        return err;
    }

    SERVICE_STATUS_PROCESS ssp = {0};
    DWORD t = GetTickCount();
    for (;;) {
        Sleep(ssp.dwWaitHint);

        DWORD bn;
        if (!QueryServiceStatusEx(scs, SC_STATUS_PROCESS_INFO, (LPBYTE) &ssp, sizeof(SERVICE_STATUS_PROCESS), &bn)) {
            int err = GetLastError();
            fprintf(stderr, "Error: Failed to get service status (%d).\n", err);
            CloseServiceHandle(scs);
            CloseServiceHandle(scm);
            return err;
        }

        if (ssp.dwCurrentState == SERVICE_STOPPED)
            break;

        if (GetTickCount()-t > 10000) {
            fprintf(stderr, "Error: Timed out while waiting for service to stop.\n");
            CloseServiceHandle(scs);
            CloseServiceHandle(scm);
            return ERROR_TIMEOUT;
        }
    }
 
    CloseServiceHandle(scs); 
    CloseServiceHandle(scm);

    printf("Successfully stopped service %ls.\n", PAW32_SVC);
    return NO_ERROR;
}

int main_delete(int argc, char **argv) {
    if (argc != 2)
        return main_help(argc, argv);

    SC_HANDLE scm;
    if (!(scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS))) {
        int err = GetLastError();
        fprintf(stderr, "Error: Failed to open service manager (%d).\n", err);
        return err;
    }

    SC_HANDLE scs;
    if (!(scs = OpenServiceW(scm, PAW32_SVC, DELETE))) { 
        int err = GetLastError();
        if (err == ERROR_SERVICE_DOES_NOT_EXIST) {
            fprintf(stderr, "Warning: The service does not exist, ignoring.\n");
            return NO_ERROR;
        }
        fprintf(stderr, "Error: Failed to open service (%d).\n", err);
        CloseServiceHandle(scm);
        return err;
    }

    if (!DeleteService(scs)) {
        int err = GetLastError();
        if (err == ERROR_SERVICE_MARKED_FOR_DELETE)
            fprintf(stderr, "Note: The service in use and has already been marked for deletion. A system restart is required.\n");
        fprintf(stderr, "Error: Failed to delete service (%d).\n", err);
        CloseServiceHandle(scs);
        CloseServiceHandle(scm);
        return err;
    }
 
    CloseServiceHandle(scs); 
    CloseServiceHandle(scm);

    printf("Successfully deleted service %ls.\n", PAW32_SVC);
    return NO_ERROR;
}
