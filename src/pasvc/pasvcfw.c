#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winerror.h>
#include <initguid.h>
#include <objbase.h>
#include <oleauto.h>
#include <oaidl.h>
#include <netfw.h>
#include <shlwapi.h>

#ifndef PAW32_VERSION
#define PAW32_VERSION "unknown"
#endif

#define PAW32_FWRULE L"PulseAudio (TCP-In)"

static struct {
    enum {
        cmd_install,
        cmd_delete,
    } cmd;
    union {
        struct {
            enum {
                mode_none,
                mode_deny,
                mode_allow,
                mode_allowall,
                mode_allowalledge,
            } mode;
            wchar_t *paexe;
        } install;
    };
} opt = {0};

static int run();

int main(int argc, char **argv) {
    #define die(ret, fmt, ...) do { \
        fprintf(stderr, "Error: " fmt "\n", ##__VA_ARGS__); \
        return ret; \
    } while(0)

    if (argc < 2)
        goto help;

    if (!strcasecmp(argv[1], "install")) {
        opt.cmd = cmd_install;
        for (int i = 2; i < argc; i++) {
            if (!strncasecmp(argv[i], "/Mode:", 6)) {
                if (!strcasecmp(argv[i] + 6, "deny"))
                    opt.install.mode = mode_deny;
                else if (!strcasecmp(argv[i] + 6, "allow"))
                    opt.install.mode = mode_allow;
                else if (!strcasecmp(argv[i] + 6, "allowall"))
                    opt.install.mode = mode_allowall;
                else if (!strcasecmp(argv[i] + 6, "allowalledge"))
                    opt.install.mode = mode_allowalledge;
                else
                    goto help;
            } else if (!strncasecmp(argv[i], "/PaExe:", 7)) {
                opt.install.paexe = calloc(strlen(argv[i] + 7) + 1, sizeof(wchar_t));
                mbstowcs(opt.install.paexe, argv[i] + 7, strlen(argv[i] + 7));
            } else {
                goto help;
            }
        }

        if (!opt.install.mode)
            die(ERROR_INVALID_FUNCTION, "/Mode is invalid.");

        if (!opt.install.paexe)
            die(ERROR_INVALID_FUNCTION, "/PaExe is required.");

        if (GetFileAttributesW(opt.install.paexe) == INVALID_FILE_ATTRIBUTES)
            die(ERROR_FILE_NOT_FOUND, "'%ls' does not exist.", opt.install.paexe);

        if (PathIsRelativeW(opt.install.paexe))
            die(ERROR_INVALID_PARAMETER, "'%ls' is not an absolute path.", opt.install.paexe);
    } else if (!strcasecmp(argv[1], "delete")) {
        opt.cmd = cmd_delete;
        if (argc != 2)
            goto help;
    } else {
        goto help;
    }

    return run();

    #undef die

help:
    printf("Usage:\n");
    printf("  %s install /Mode:deny|allow|allowall|allowalledge /PaExe:<PATH>\n", argv[0]);
    printf("  %s delete\n", argv[0]);
    printf("\nRule name:\n");
    printf("  %ls\n", PAW32_FWRULE);
    printf("\nAbout:\n");
    printf("  pulseaudio-win32 %s\n", PAW32_VERSION);
    printf("  github.com/pgaskin/pulseaudio-win32\n");
    return ERROR_INVALID_FUNCTION;
}

int run() {
    HRESULT hr;
    int out_ret = 0;

    BSTR           sr = NULL;
    BSTR           sd = NULL;
    BSTR           se = NULL;
    BSTR           sp = NULL;
    BSTR           sg = NULL;
    INetFwPolicy2 *fw = NULL;
    INetFwRules   *rs = NULL;
    INetFwRule    *rl = NULL;

    #define die(ret, fmt, ...) do { \
        fprintf(stderr, "Error: " fmt "\n", ##__VA_ARGS__); \
        out_ret = ret; \
        goto out; \
    } while(0)

    #define check(what, x) do { \
        if ((hr = (x)) != S_OK) \
            die(ERROR_GEN_FAILURE, "Failed to %s (%ld).", what, hr); \
    } while(0)

    #define checked_set(obj, prop, value) \
        check("set property "#prop" to "#value, \
            obj->lpVtbl->put_##prop(obj, value))

    if (opt.cmd == cmd_install || opt.cmd == cmd_delete) {
        check("initialize COM",
            CoInitializeEx(NULL, COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE));

        check("create NetFwPolicy2 instance",
            CoCreateInstance(&CLSID_NetFwPolicy2, NULL, CLSCTX_INPROC_SERVER, &IID_INetFwPolicy2, (void**) &fw));

        check("get firewall rules",
            fw->lpVtbl->get_Rules(fw, &rs));

        if (!(sr = SysAllocString(PAW32_FWRULE)))
            die(ERROR_OUTOFMEMORY, "Failed to allocate string.");

        switch ((hr = rs->lpVtbl->Item(rs, sr, &rl))) {
        case S_OK:
            printf("Deleting existing firewall rule.\n");
            check("delete firewall rule",
                rs->lpVtbl->Remove(rs, sr));
            rl->lpVtbl->Release(rl);
            break;
        case HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND):
            break;
        default:
            check("get firewall rule", hr);
        }

        if (opt.cmd == cmd_install) {
            printf("Installing new firewall rule.\n");

            check("create NetFwRule instance",
                CoCreateInstance(&CLSID_NetFwRule, NULL, CLSCTX_INPROC_SERVER, &IID_INetFwRule, (void**) &rl));

            checked_set(rl, Enabled,         VARIANT_TRUE);
            checked_set(rl, Name,            sr);

            if (!(sd = SysAllocString(L"Inbound rule for PulseAudio to allow native connections. [TCP 4713]")))
                die(ERROR_OUTOFMEMORY, "Failed to allocate string.");
            checked_set(rl, Description,     sd);

            if (!(se = SysAllocString(opt.install.paexe)))
                die(ERROR_OUTOFMEMORY, "Failed to allocate string.");
            checked_set(rl, ApplicationName, se);

            checked_set(rl, Direction,       NET_FW_RULE_DIR_IN);
            checked_set(rl, Protocol,        NET_FW_IP_PROTOCOL_TCP);

            if (!(sp = SysAllocString(L"4713")))
                die(ERROR_OUTOFMEMORY, "Failed to allocate string.");
            checked_set(rl, LocalPorts,      sp);

            if (!(sg = SysAllocString(L"@firewallapi.dll,-23255")))
                die(ERROR_OUTOFMEMORY, "Failed to allocate string.");
            checked_set(rl, Grouping,        sg);

            switch (opt.install.mode) {
            case mode_none:
                break;
            case mode_deny:
                checked_set(rl, Profiles,      NET_FW_PROFILE2_DOMAIN|NET_FW_PROFILE2_PRIVATE|NET_FW_PROFILE2_PUBLIC);
                checked_set(rl, EdgeTraversal, false);
                checked_set(rl, Action,        NET_FW_ACTION_BLOCK);
                break;
            case mode_allow:
                checked_set(rl, Profiles,      NET_FW_PROFILE2_DOMAIN|NET_FW_PROFILE2_PRIVATE);
                checked_set(rl, EdgeTraversal, false);
                checked_set(rl, Action,        NET_FW_ACTION_ALLOW);
                break;
            case mode_allowall:
                checked_set(rl, Profiles,      NET_FW_PROFILE2_DOMAIN|NET_FW_PROFILE2_PRIVATE|NET_FW_PROFILE2_PUBLIC);
                checked_set(rl, EdgeTraversal, false);
                checked_set(rl, Action,        NET_FW_ACTION_ALLOW);
                break;
            case mode_allowalledge:
                checked_set(rl, Profiles,      NET_FW_PROFILE2_DOMAIN|NET_FW_PROFILE2_PRIVATE|NET_FW_PROFILE2_PUBLIC);
                checked_set(rl, EdgeTraversal, true);
                checked_set(rl, Action,        NET_FW_ACTION_ALLOW);
                break;
            }

            check("add firewall rule",
                rs->lpVtbl->Add(rs, rl));
        }
    }

    printf("Done.\n");

    #undef check
    #undef die

out:
    if (rl)
        rl->lpVtbl->Release(rl);
    if (rs)
        rs->lpVtbl->Release(rs);
    if (fw)
        fw->lpVtbl->Release(fw);
    if (sg)
        SysFreeString(sg);
    if (sp)
        SysFreeString(sp);
    if (se)
        SysFreeString(se);
    if (sd)
        SysFreeString(sd);
    if (sr)
        SysFreeString(sr);
    return out_ret;
}
