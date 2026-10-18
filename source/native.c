#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "debris_targets.h"
#include "compatibility.h"

enum { PATCH_SIZE = 7 };
static const unsigned char signature[] = {0x48,0x8B,0xC4,0x48,0x89,0x58,0x10,0x48,0x89,0x70,0x18,0x48,0x89,0x78,0x20,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0xA8,0x78,0xFD,0xFF,0xFF,0x48,0x81,0xEC,0x60,0x03,0x00,0x00,0x0F,0x29};
static const unsigned char power_access[] = {0x8B,0x87,0xB8,0,0,0,0x89,0x87,0x48,1,0,0};
static const unsigned char destroy_signature[] = {0x48,0x8B,0xC4,0x48,0x89,0x58,0x18,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0xA8,0x68,0xFE,0xFF,0xFF,0x48,0x81,0xEC,0x60,0x02,0x00,0x00,0x0F,0x29,0x70,0xB8,0x0F,0x29,0x78,0xA8,0x44,0x0F,0x29,0x40,0x98,0x44,0x0F,0x29,0x48,0x88,0x44,0x0F,0x29,0x90,0x78,0xFF,0xFF,0xFF,0x44,0x0F,0x29,0x98,0x68,0xFF,0xFF,0xFF,0x44,0x0F,0x29,0xA0,0x58,0xFF,0xFF,0xFF,0x44,0x0F,0x29,0xA8,0x48,0xFF,0xFF,0xFF,0x44,0x0F,0x29,0xB0,0x38,0xFF,0xFF,0xFF,0x44,0x0F,0x29,0xB8,0x28,0xFF,0xFF,0xFF,0x48,0x8B,0x05,0xA7,0xF8,0xA6,0x04,0x48,0x33,0xC4,0x48,0x89,0x85,0xB0,0x00,0x00,0x00,0x4C,0x8B,0xFA,0x4C,0x8B,0xE9,0x48,0x89,0x4C,0x24,0x48,0xC6,0x44,0x24,0x41};
static HMODULE self_module;
static volatile LONG initialized;
static const char *last_status = "Not initialized.";
static char compatibility_status[256];
static volatile LONG safe_explosions = 1;
static int allow_attached_vehicle_destruction;

static const char *active_status(void) {
    if (!safe_explosions) return allow_attached_vehicle_destruction
        ? "ACTIVE: Safe explosions OFF. Attached vehicle destruction ON. Vanilla explosions; vehicle setting saved for when Babyproof is ON."
        : "ACTIVE: Safe explosions OFF. Attached vehicle destruction OFF. Vanilla explosions; vehicle setting saved for when Babyproof is ON.";
    return allow_attached_vehicle_destruction
        ? "ACTIVE: Safe explosions ON. Attached vehicle destruction ON. Tiny blasts; base equipment protected; directly attached vehicles allowed; caches/debris breakable."
        : "ACTIVE: Safe explosions ON. Attached vehicle destruction OFF. Tiny blasts; vehicles and base equipment protected; caches/debris breakable.";
}

static void load_options(const wchar_t *path) {
    wchar_t value[32];
    GetPrivateProfileStringW(L"Explosions",L"Enabled",L"true",value,32,path);
    InterlockedExchange(&safe_explosions,_wcsicmp(value,L"false")!=0);
    GetPrivateProfileStringW(L"Explosions",L"AllowAttachedVehicleDestruction",
                            L"false",value,32,path);
    allow_attached_vehicle_destruction = !_wcsicmp(value,L"true");
}

static void load_config(void) {
    wchar_t path[32768];
    DWORD n=GetModuleFileNameW(self_module,path,32768);
    if (!n || n>=32600) return;
    while(n && path[n-1]!=L'\\' && path[n-1]!=L'/') --n;
    if(!n) return;
    --n; // Skip the Scripts directory as well.
    while(n && path[n-1]!=L'\\' && path[n-1]!=L'/') --n;
    if(!n) return;
    const wchar_t filename[]=L"config.ini";
    memcpy(path+n,filename,sizeof(filename));
    load_options(path);
}

static void write_status(const char *text) {
    wchar_t path[32768];
    DWORD n = GetModuleFileNameW(self_module, path, 32768);
    if (!n || n >= 32700) return;
    while (n && path[n-1] != L'\\' && path[n-1] != L'/') --n;
    const wchar_t name[] = L"BabyProofDynamites-status.txt";
    memcpy(path+n, name, sizeof(name));
    HANDLE f = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                          CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(f, text, (DWORD)strlen(text), &written, NULL);
        CloseHandle(f);
    }
}

// Find the signature in the game executable.
static unsigned char *find_target_masked(unsigned char *base, const unsigned char *pattern, size_t pattern_size, size_t skip, size_t skip_size, unsigned *matches) {
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return NULL;
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) return NULL;
    IMAGE_SECTION_HEADER *section = IMAGE_FIRST_SECTION(nt);
    unsigned char *found = NULL;
    for (unsigned s=0; s<nt->FileHeader.NumberOfSections; ++s) {
        if (!(section[s].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        size_t size = section[s].Misc.VirtualSize;
        unsigned char *start = base + section[s].VirtualAddress;
        if (!image_contains(base,start,size)) return NULL;
        if (size < pattern_size) continue;
        for (size_t i=0; i<=size-pattern_size; ++i) {
            if (!memcmp(start+i,pattern,skip) &&
                !memcmp(start+i+skip+skip_size,pattern+skip+skip_size,pattern_size-skip-skip_size)) {
                ++*matches; found = start+i;
            }
        }
    }
    return found;
}

static unsigned char *find_target(unsigned char *base, const unsigned char *pattern, size_t pattern_size, unsigned *matches) {
    return find_target_masked(base,pattern,pattern_size,pattern_size,0,matches);
}

static unsigned char *allocate_near(unsigned char *target) {
    SYSTEM_INFO info; GetSystemInfo(&info);
    uintptr_t granularity = info.dwAllocationGranularity;
    uintptr_t origin = (uintptr_t)target & ~(granularity-1);
    for (uintptr_t delta=granularity; delta<0x70000000; delta+=granularity) {
        for (int direction=0; direction<2; ++direction) {
            uintptr_t candidate = direction ? origin+delta : (origin>delta ? origin-delta : 0);
            if (candidate < (uintptr_t)info.lpMinimumApplicationAddress ||
                candidate > (uintptr_t)info.lpMaximumApplicationAddress) continue;
            unsigned char *p = VirtualAlloc((void *)candidate, 4096,
                MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            if (p) return p;
        }
    }
    return NULL;
}

static void put32(unsigned char *at, int32_t value) { memcpy(at, &value, 4); }
// Set blast values, replay the overwritten instructions, then return to Detonate.
static size_t build_stub(unsigned char *p, unsigned char *target) {
    const uint32_t offsets[] = {0xB8,0xC0,0xC4,0xC8};
    const float values[] = {0.000001f,0.0f,0.001f,0.001f};
    size_t n=0;
    // Preserve flags and stack. RAX is overwritten by the original prologue.
    p[n++]=0x9C; // pushfq
    p[n++]=0x48;p[n++]=0xB8;
    uintptr_t enabled=(uintptr_t)&safe_explosions;memcpy(p+n,&enabled,8);n+=8;
    p[n++]=0x83;p[n++]=0x38;p[n++]=0; // cmp dword ptr [rax], 0
    p[n++]=0x74;p[n++]=40; // skip the four field writes when OFF
    for (unsigned i=0;i<4;++i) {
        p[n++]=0xC7; p[n++]=0x81; // mov dword ptr [rcx+offset], immediate
        memcpy(p+n,&offsets[i],4); n+=4;
        memcpy(p+n,&values[i],4); n+=4;
    }
    p[n++]=0x9D; // popfq
    memcpy(p+n,signature,PATCH_SIZE); n+=PATCH_SIZE;
    p[n++]=0xE9;
    put32(p+n,(int32_t)((intptr_t)(target+PATCH_SIZE)-(intptr_t)(p+n+4))); n+=4;
    return n;
}

// Use class names to decide which objects can break. Protect unknown classes.
static unsigned char *name_pool;
typedef unsigned char (*DestroyFunction)(void *, void *);
static DestroyFunction original_destroy;

static int name_is_breakable(const char *n) {
    for (size_t i=0;i<sizeof(debris_classes)/sizeof(*debris_classes);++i)
        if (!strcmp(n,debris_classes[i])) return 1;
    return !strncmp(n,"Destructible_PlacementActor_",27) ||
           !strncmp(n,"Debris",6) || !strncmp(n,"Wreck",5) ||
           !strncmp(n,"Item_ModernPOI_Wrecked_",22) ||
           !strncmp(n,"item_modernPOI_wrecked_",22) ||
           !strncmp(n,"Item_Wreck",10) ||
           !strncmp(n,"HolidayGift_Debris",18) ||
           !strncmp(n,"PackagedItem_T1_Debris",22) ||
           !strncmp(n,"PackagedItem_T2_Debris",22) ||
           !strncmp(n,"PackagedItem_T3_Debris",22) ||
           !strcmp(n,"Dynamite_C") || !strcmp(n,"ConsumableGasCanister_C") ||
           !strcmp(n,"ConsumableLiquidCanister_C");
}

static int class_matches(unsigned char *cls, int vehicle) {
    if (!name_pool) return 0;
    for (unsigned depth=0; cls && depth<64; ++depth) {
        uint32_t index; memcpy(&index,cls+0x18,4);
        if ((index>>16)>=8192) return 0;
        unsigned char *block=*(unsigned char **)(name_pool+0x10+(index>>16)*8);
        if (!block) return 0;
        unsigned char *entry=block+(index&65535)*2;
        uint16_t header; memcpy(&header,entry,2);
        unsigned len=header>>6;
        if (len && len<256 && !(header&1)) {
            char name[256]; memcpy(name,entry+2,len); name[len]=0;
            if (vehicle == 5 ? !strcmp(name,"PackagedItem_T1_C") :
                vehicle == 4 ? !strcmp(name,"RailCarBase") :
                vehicle == 3 ? !strcmp(name,"ModernPOI_WindFarm_B-01_Small_C") :
                vehicle == 2 ? !strcmp(name,"Item_PuzzleBox_Top_C") :
                (vehicle == 1 ? !strcmp(name,"VehicleBase") : name_is_breakable(name))) return 1;
        }
        // UStruct::SuperStruct
        cls=*(unsigned char **)(cls+0x40);
    }
    return 0;
}

static int actor_matches(void *actor, int vehicle) {
    return actor && class_matches(*(unsigned char **)((unsigned char *)actor+0x10),vehicle);
}

static int actor_is_breakable(void *actor) { return actor_matches(actor,0); }

static int is_attached_vehicle(void *component, void *actor) {
    if (!allow_attached_vehicle_destruction || !component || !actor_matches(actor,1)) return 0;
    // Only the immediate parent counts; cargo and connected vehicles are excluded.
    unsigned char *owner=*(unsigned char **)((unsigned char *)component+0xA8);
    if (!owner) return 0;
    unsigned char *root=*(unsigned char **)(owner+0x148);
    if (!root) return 0;
    unsigned char *parent=*(unsigned char **)(root+0xC8);
    return parent && *(void **)(parent+0xA8)==actor;
}

// Read the immediate attachment parent.
static void *immediate_parent(void *actor) {
    if (!actor) return NULL;
    unsigned char *root=*(unsigned char **)((unsigned char *)actor+0x148);
    if (!root) return NULL;
    unsigned char *parent=*(unsigned char **)(root+0xC8);
    return parent ? *(void **)(parent+0xA8) : NULL;
}

static int is_packaged_rail_car(void *actor) {
    if (!actor_matches(actor,5)) return 0;
    unsigned char *item=*(unsigned char **)((unsigned char *)actor+0x3D8);
    if (!item) return 0;
    // Generic boxes use storedSubItemType to identify their contents.
    unsigned char *type_class=*(unsigned char **)(item+0x160);
    if (!type_class) return 0;
    // storedSubItemType is a UClass; PickupActor belongs to its default object.
    unsigned char *type=*(unsigned char **)(type_class+0x118);
    if (!type) return 0;
    unsigned char *pickup_class=*(unsigned char **)(type+0xC8);
    return class_matches(pickup_class,4);
}

static unsigned char guarded_destroy(void *component, void *actor) {
    int enabled=safe_explosions!=0;
    int allowed=1,packaged_rail=0;
    if (enabled) {
        int attached_vehicle=is_attached_vehicle(component,actor);
        allowed=actor_is_breakable(actor) || attached_vehicle;
        packaged_rail=is_packaged_rail_car(actor);
        if (actor_matches(actor,4)) allowed=0;
        if (packaged_rail) {
            void *owner=component ? *(void **)((unsigned char *)component+0xA8) : NULL;
            allowed=allow_attached_vehicle_destruction && owner && immediate_parent(owner)==actor;
        }
        // Wrecked Medium Platform B.
        if (actor_matches(actor,3)) {
            void *owner=component ? *(void **)((unsigned char *)component+0xA8) : NULL;
            allowed=owner && immediate_parent(owner)==actor;
        }
        // Protect lids that are still attached to a research aid.
        if (actor_matches(actor,2) && immediate_parent(actor)) allowed=0;
    }
    // Record the filter decision and the game's return value.
    char name[256]="unknown",report[384];
    if (actor && name_pool) {
        unsigned char *cls=*(unsigned char **)((unsigned char *)actor+0x10);
        if (cls) {
            uint32_t index;memcpy(&index,cls+0x18,4);
            if ((index>>16)<8192) {
                unsigned char *block=*(unsigned char **)(name_pool+0x10+(index>>16)*8);
                if(block) {
                    unsigned char *entry=block+(index&65535)*2;
                    uint16_t header;memcpy(&header,entry,2);unsigned len=header>>6;
                    if(len && len<256 && !(header&1)) {memcpy(name,entry+2,len);name[len]=0;}
                }
            }
        }
    }
    unsigned char result=allowed ? original_destroy(component,actor) : 0;
    snprintf(report,sizeof(report),"classic-v1.3.1 target=%s allowed=%d native_result=%u packaged_rail=%d safe_bombs=%d\r\n",name,allowed,(unsigned)result,packaged_rail,enabled);
    // Keep event records separate from startup status.
    wchar_t path[32768];DWORD n=GetModuleFileNameW(self_module,path,32768);
    if(n && n<32600) {
        while(n && path[n-1]!=L'\\') --n;
        wcscpy(path+n,L"BabyProofDynamites-events.txt");
        HANDLE f=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ,NULL,OPEN_ALWAYS,0,NULL);
        if(f!=INVALID_HANDLE_VALUE) {DWORD written;WriteFile(f,report,(DWORD)strlen(report),&written,NULL);CloseHandle(f);}
    }
    return result;
}

typedef struct Patch {
    unsigned char *target;
    const unsigned char *expected;
    size_t expected_size;
    unsigned char jump[PATCH_SIZE];
} Patch;

static void prepare_jump(Patch *p, unsigned char *destination) {
    p->jump[0]=0xE9;
    put32(p->jump+1,(int32_t)((intptr_t)destination-(intptr_t)(p->target+5)));
    p->jump[5]=p->jump[6]=0x90;
}

// Install both hooks together. Stop if a thread is inside either patch.
// Avoid file and heap operations while threads are paused.
static int install_patches(Patch patches[2]) {
    HANDLE handles[512]; unsigned count=0,paused=0,protected_count=0;
    DWORD old_protection[2]; int success=0;
    DWORD pid=GetCurrentProcessId(),tid=GetCurrentThreadId();
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);
    if (snapshot==INVALID_HANDLE_VALUE) return 0;
    THREADENTRY32 entry; entry.dwSize=sizeof(entry);
    if (!Thread32First(snapshot,&entry)) { CloseHandle(snapshot); return 0; }
    do {
        if (entry.th32OwnerProcessID!=pid || entry.th32ThreadID==tid) continue;
        if (count==512) goto cleanup;
        HANDLE h=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,entry.th32ThreadID);
        if (!h) { if(GetLastError()==ERROR_INVALID_PARAMETER) continue; goto cleanup; }
        handles[count++]=h;
    } while(Thread32Next(snapshot,&entry));
    CloseHandle(snapshot); snapshot=INVALID_HANDLE_VALUE;
    for (unsigned j=0;j<2;++j) {
        if(!VirtualProtect(patches[j].target,PATCH_SIZE,PAGE_EXECUTE_READWRITE,&old_protection[j])) goto resume;
        ++protected_count;
    }
    for (unsigned i=0;i<count;++i) {
        if(SuspendThread(handles[i])==(DWORD)-1) goto resume;
        ++paused;
        CONTEXT ctx; memset(&ctx,0,sizeof(ctx)); ctx.ContextFlags=CONTEXT_CONTROL;
        if(!GetThreadContext(handles[i],&ctx)) goto resume;
        for(unsigned j=0;j<2;++j)
            if(ctx.Rip>=(DWORD64)patches[j].target && ctx.Rip<(DWORD64)(patches[j].target+PATCH_SIZE)) goto resume;
    }
    for(unsigned j=0;j<2;++j)
        if(memcmp(patches[j].target,patches[j].expected,patches[j].expected_size)) goto resume;
    for(unsigned j=0;j<2;++j) memcpy(patches[j].target,patches[j].jump,PATCH_SIZE);
    if(!FlushInstructionCache(GetCurrentProcess(),NULL,0)) {
        for(unsigned j=0;j<2;++j) memcpy(patches[j].target,patches[j].expected,PATCH_SIZE);
        FlushInstructionCache(GetCurrentProcess(),NULL,0);
        goto resume;
    }
    success=1;
resume:
    while(paused) ResumeThread(handles[--paused]);
    while(protected_count) { --protected_count; DWORD ignored;
        VirtualProtect(patches[protected_count].target,PATCH_SIZE,old_protection[protected_count],&ignored);
    }
cleanup:
    if(snapshot!=INVALID_HANDLE_VALUE) CloseHandle(snapshot);
    for(unsigned i=0;i<count;++i) CloseHandle(handles[i]);
    return success;
}

static const char *compatibility_failure(unsigned char *game, const char *reason) {
    IMAGE_DOS_HEADER *dos=(IMAGE_DOS_HEADER *)game;
    IMAGE_NT_HEADERS64 *nt=(IMAGE_NT_HEADERS64 *)(game+dos->e_lfanew);
    snprintf(compatibility_status,sizeof(compatibility_status),
        "INACTIVE: %s Game build %08lX/%08lX; mod 1.3.1. No memory was patched.",
        reason,(unsigned long)nt->FileHeader.TimeDateStamp,(unsigned long)nt->OptionalHeader.SizeOfImage);
    return compatibility_status;
}

static const char *activate(void) {
    load_config();
    unsigned char *game=(unsigned char *)GetModuleHandleW(L"Astro-Win64-Shipping.exe");
    if(!game) return "INACTIVE: Steam Win64 Astroneer is required.";
    unsigned matches=0,destroy_matches=0;
    unsigned char *target=find_target(game,signature,sizeof(signature),&matches);
    // Only the security-cookie address is allowed to vary in this signature.
    unsigned char *destroy=find_target_masked(game,destroy_signature,sizeof(destroy_signature),0x65,4,&destroy_matches);
    const unsigned char owner_access[]={0x48,0x8B,0x81,0xA8,0,0,0,0x48,0x8B,0x88,0x48,1,0,0};
    if(matches!=1 || destroy_matches!=1 || !image_contains(game,target,0xD2) ||
       memcmp(target+0xC6,power_access,sizeof(power_access)) || memcmp(target+0x76,owner_access,sizeof(owner_access)))
        return compatibility_failure(game,"Explosion code could not be verified.");
    name_pool=find_name_pool(game);
    if(!name_pool) return compatibility_failure(game,"Name pool could not be verified.");
    unsigned char *objects=find_object_array(game,destroy);
    if(!objects || !validate_layout(objects,name_pool))
        return compatibility_failure(game,"Required object fields could not be verified.");
    unsigned char expected_destroy[sizeof(destroy_signature)];
    memcpy(expected_destroy,destroy,sizeof(expected_destroy));
    unsigned char *stub=allocate_near(target),*guard=allocate_near(destroy);
    if(!stub || !guard) {
        if(stub) VirtualFree(stub,0,MEM_RELEASE);
        if(guard) VirtualFree(guard,0,MEM_RELEASE);
        return "INACTIVE: Cannot allocate nearby memory. No memory was patched.";
    }
    build_stub(stub,target);
    // Relay to our callback, then keep a trampoline to the original function.
    guard[0]=0x48; guard[1]=0xB8;
    uintptr_t callback=(uintptr_t)&guarded_destroy; memcpy(guard+2,&callback,8);
    guard[10]=0xFF; guard[11]=0xE0;
    unsigned char *trampoline=guard+32;
    memcpy(trampoline,destroy_signature,PATCH_SIZE);
    trampoline[PATCH_SIZE]=0xE9;
    put32(trampoline+PATCH_SIZE+1,(int32_t)((intptr_t)(destroy+PATCH_SIZE)-(intptr_t)(trampoline+PATCH_SIZE+5)));
    original_destroy=(DestroyFunction)trampoline;
    DWORD old; HMODULE pinned;
    if(!VirtualProtect(stub,4096,PAGE_EXECUTE_READ,&old) ||
       !VirtualProtect(guard,4096,PAGE_EXECUTE_READ,&old) ||
       !FlushInstructionCache(GetCurrentProcess(),NULL,0) ||
       !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCWSTR)&guarded_destroy,&pinned)) {
        VirtualFree(stub,0,MEM_RELEASE);VirtualFree(guard,0,MEM_RELEASE);
        return "INACTIVE: Cannot prepare native filters. No memory was patched.";
    }
    Patch patches[2]={{target,signature,sizeof(signature),{0}},{destroy,expected_destroy,sizeof(expected_destroy),{0}}};
    prepare_jump(&patches[0],stub);prepare_jump(&patches[1],guard);
    if(!install_patches(patches)) {
        VirtualFree(stub,0,MEM_RELEASE);VirtualFree(guard,0,MEM_RELEASE);
        return "INACTIVE: Could not safely install both filters. Restart to retry.";
    }
    return active_status();
}


__declspec(dllexport) int luaopen_babyproofdynamites(void *lua_state) {
    (void)lua_state;
    if (InterlockedCompareExchange(&initialized, 1, 0)==0) last_status=activate();
    write_status(last_status);
    return 0;
}

__declspec(dllexport) int luaopen_babyproofdynamites_refresh(void *lua_state) {
    (void)lua_state;
    if (!strncmp(last_status,"ACTIVE:",7)) {
        load_config();
        last_status=active_status();
    }
    write_status(last_status);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason==DLL_PROCESS_ATTACH) { self_module=instance; DisableThreadLibraryCalls(instance); }
    return TRUE;
}
