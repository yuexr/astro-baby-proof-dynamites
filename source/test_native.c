#include "native.c"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while(0)

static unsigned char fake_pool[32],fake_block[65536];
static unsigned cursor;
static uint32_t add_name(const char *name) {
    size_t len=strlen(name);uint32_t index=cursor/2;
    uint16_t header=(uint16_t)(len<<6);memcpy(fake_block+cursor,&header,2);
    memcpy(fake_block+cursor+2,name,len);cursor+=(unsigned)((len+3)&~1);
    return index;
}
static void set_class(unsigned char *object,unsigned char *cls,const char *name) {
    memset(cls,0,96);uint32_t index=add_name(name);memcpy(cls+0x18,&index,4);
    memcpy(object+0x10,&cls,sizeof(cls));
}
static void set_derived(unsigned char *actor,unsigned char *cls,unsigned char *base_class,const char *name,const char *base_name) {
    set_class(actor,base_class,base_name);
    set_class(actor,cls,name);
    memcpy(cls+0x40,&base_class,sizeof(base_class));
}
typedef int (*DetonateTest)(void *);
static unsigned char *make_function(const unsigned char *prefix) {
    unsigned char *p=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    CHECK(p);memcpy(p,prefix,7);
    const unsigned char tail[]={0xB8,1,0,0,0,0xC3};memcpy(p+7,tail,sizeof(tail));
    return p;
}
int main(void) {
    unsigned char *bp=fake_block;memcpy(fake_pool+16,&bp,sizeof(bp));name_pool=fake_pool;
    unsigned char actor[384]={0},cls[96]={0},parent[96]={0};
    const char *allowed[]={"Destructible_PlacementActor_DataCache_EXOChip_C",
        "Destructible_PlacementActor_Base_BoulderIce_C","Destructible_PlacementActor_DataCache_MissionA_C",
        "Debris_Base_01_C","DebrisBundle_C","Wreck","WreckageStation01_C",
        "Item_ModernPOI_Wrecked_Smelter_C","item_modernPOI_wrecked_Solar_T3_C",
        "Item_Wreckage_HazardousContainer_A_C","HolidayGift_Debris1_C",
        "PackagedItem_T2_Debris_Special_C","Dynamite_C","ConsumableGasCanister_C","ConsumableLiquidCanister_C"};
    const char *protected[]={"Smelter_C","LargePlatformA_C","MediumGasCanister_BP_C",
        "ResourceCanister_Reusable_C","MediumResourceCanister_BP_C","Oxygen_Tank_Small_C",
        "HydrazineJetpack_BP_C","ResearchModule","PhysicalItem","MyModMachine_C","MyWreckMachine_C"};
    for(unsigned i=0;i<sizeof(allowed)/sizeof(*allowed);++i) {
        set_class(actor,cls,allowed[i]);CHECK(actor_is_breakable(actor));
    }
    for(unsigned i=0;i<sizeof(protected)/sizeof(*protected);++i) {
        set_class(actor,cls,protected[i]);CHECK(!actor_is_breakable(actor));
    }
    for(size_t i=0;i<sizeof(debris_classes)/sizeof(*debris_classes);++i) {
        set_class(actor,cls,debris_classes[i]);CHECK(actor_is_breakable(actor));
    }
    const char *inherited_wrecks[][2] = {
        {"ModernPOI_Base_HabitatSM_01_C","POI_Item_BASE_C"},
        {"ModernPOI_Base_HabitatXL_01_Medium_C","Slottable_Item_C"},
        {"ModernPOI_Base_HabitatXL_02_Medium_C","Slottable_Item_C"},
        {"ModernPOI_WindFarm_HabitatSM_01_C","POI_Item_BASE_C"},
        {"ModernPOI_Base_T2_Platform_A_C","Slottable_Item_C"},
        {"ModernPOI_Base_T2_Platform_B_C","Slottable_Item_C"},
        {"ModernPOI_Base_T3_Platform_A_C","Slottable_Item_C"},
        {"ModernPOI_Breadboard_T4_BASE_C","POI_Item_BASE_C"},
        {"ModernPOI_Rover_B-01_Small_C","Slottable_Item_C"},
        {"ModernPOI_SolarFarm_A-01_Small_C","Slottable_Item_C"},
        {"ModernPOI_SolarFarm_A-01_XL_C","POI_Item_BASE_C"},
        {"ModernPOI_SolarFarm_B-01_Small_C","Slottable_Item_C"},
        {"ModernPOI_SolarFarm_C-01_Small_C","ModernPOI_Breadboard_T2_BASE_C"},
        {"ModernPOI_SolarFarm_D-01_Small_C","ModernPOI_Breadboard_T2_BASE_C"},
        {"ModernPOI_WindFarm_A-01_Small_C","Slottable_Item_C"},
        {"ModernPOI_WindFarm_A-01_XL_C","POI_Item_BASE_C"},
        {"ModernPOI_WindFarm_C-01_Small_C","ModernPOI_Breadboard_T2_BASE_C"},
        {"ModernPOI_WindFarm_D-01_Small_C","ModernPOI_Breadboard_T2_BASE_C"},
    };
    for(size_t i=0;i<sizeof(inherited_wrecks)/sizeof(*inherited_wrecks);++i) {
        set_class(actor,parent,inherited_wrecks[i][1]);
        set_class(actor,cls,inherited_wrecks[i][0]);
        unsigned char *super=parent; memcpy(cls+0x40,&super,8);
        CHECK(actor_is_breakable(actor));
    }
    const char *excluded[]={"BrokenCatalog_C","HolidayGift_ShippingContainer1_C"};
    for(size_t i=0;i<2;++i) {
        set_class(actor,parent,"PhysicalItem"); set_class(actor,cls,excluded[i]);
        unsigned char *super=parent; memcpy(cls+0x40,&super,8);
        CHECK(!actor_is_breakable(actor));
    }
    puts("PASS: all 18 verified wreck descendants allowed; excluded catalog/container protected.");
    CHECK(!actor_is_breakable(NULL));
    set_class(actor,parent,"Debris_Base_01_C");set_class(actor,cls,"CustomDebrisSubclass_C");
    unsigned char *pp=parent;memcpy(cls+0x40,&pp,sizeof(pp));CHECK(actor_is_breakable(actor));

    unsigned char *target=make_function(signature),*destroy=make_function(destroy_signature);
    unsigned char *stub=allocate_near(target),*guard=allocate_near(destroy);CHECK(stub && guard);
    build_stub(stub,target);
    guard[0]=0x48;guard[1]=0xB8;uintptr_t cb=(uintptr_t)&guarded_destroy;memcpy(guard+2,&cb,8);
    guard[10]=0xFF;guard[11]=0xE0;
    unsigned char *trampoline=guard+32;memcpy(trampoline,destroy_signature,7);trampoline[7]=0xE9;
    put32(trampoline+8,(int32_t)((intptr_t)(destroy+7)-(intptr_t)(trampoline+12)));
    original_destroy=(DestroyFunction)trampoline;
    DWORD old;CHECK(VirtualProtect(stub,4096,PAGE_EXECUTE_READ,&old));
    CHECK(VirtualProtect(guard,4096,PAGE_EXECUTE_READ,&old));
    unsigned char wrong[7]={0};
    Patch patches[2]={{target,signature,7,{0}},{destroy,wrong,7,{0}}};
    prepare_jump(&patches[0],stub);prepare_jump(&patches[1],guard);
    CHECK(!install_patches(patches));CHECK(!memcmp(target,signature,7));CHECK(!memcmp(destroy,destroy_signature,7));
    patches[1].expected=destroy_signature;CHECK(install_patches(patches));
    unsigned char component[512],expected[512];memset(component,0x5A,sizeof(component));memcpy(expected,component,sizeof(component));
    const uint32_t offsets[]={0xB8,0xC0,0xC4,0xC8};const float values[]={0.000001f,0.0f,0.001f,0.001f};
    for(unsigned i=0;i<4;++i)memcpy(expected+offsets[i],values+i,4);
    CHECK(((DetonateTest)target)(component)==1);CHECK(!memcmp(component,expected,sizeof(component)));
    set_class(actor,cls,"Smelter_C");CHECK(((DestroyFunction)destroy)(component,actor)==0);
    set_class(actor,cls,"Destructible_PlacementActor_DataCache_EXOChip_C");CHECK(((DestroyFunction)destroy)(component,actor)==1);
    set_class(actor,cls,"Debris_Base_01_C");CHECK(((DestroyFunction)destroy)(component,actor)==1);
    set_class(actor,cls,"Dynamite_C");CHECK(((DestroyFunction)destroy)(component,actor)==1);
    set_derived(actor,cls,parent,"Item_PuzzleBox_Top_C","Slottable_Item_C");
    CHECK(((DestroyFunction)destroy)(component,actor)==1);
    unsigned char lidroot[512]={0},lidmount[512]={0};void *lidptr=lidroot;
    memcpy(actor+0x148,&lidptr,8);lidptr=lidmount;memcpy(lidroot+0xC8,&lidptr,8);
    lidptr=component;memcpy(lidmount+0xA8,&lidptr,8);
    CHECK(((DestroyFunction)destroy)(component,actor)==0);
    memset(actor+0x148,0,8);
    set_derived(actor,cls,parent,"ModernPOI_Breadboard_T2_Partial_BASE_C","ModernPOI_Breadboard_T2_BASE_C");CHECK(((DestroyFunction)destroy)(component,actor)==1);
    set_class(actor,cls,"ModernPOI_Breadboard_Partial_T3_BASE_C");CHECK(((DestroyFunction)destroy)(component,actor)==1);
    set_derived(actor,cls,parent,"Item_ModernPOI_Wreckage_GenericT1_C_C","Slottable_Item_C");CHECK(((DestroyFunction)destroy)(component,actor)==1);
    // Direct attachment only; test through the real installed detour.
    unsigned char explosive[384]={0},root[512]={0},mount[512]={0};
    unsigned char vehicle[64]={0},vehicle_class[96]={0},vehicle_base[96]={0};
    unsigned char nearby[64]={0},nearby_class[96]={0},storage[64]={0};
    void *ptr=explosive;memcpy(component+0xA8,&ptr,8);
    ptr=root;memcpy(explosive+0x148,&ptr,8);
    ptr=mount;memcpy(root+0xC8,&ptr,8);
    ptr=vehicle;memcpy(mount+0xA8,&ptr,8);
    set_class(vehicle,vehicle_base,"VehicleBase");
    set_class(nearby,nearby_class,"VehicleBase");
    const char *vehicles[]={"RoverT1_C","RoverT2_C","RoverT2B_C","Rover_Large_C","Shuttle_C","VTOL_BP_C","RailCar_C"};
    for(unsigned i=0;i<sizeof(vehicles)/sizeof(*vehicles);++i) {
        set_class(vehicle,vehicle_class,vehicles[i]);ptr=vehicle_base;memcpy(vehicle_class+0x40,&ptr,8);
        allow_attached_vehicle_destruction=0;CHECK(((DestroyFunction)destroy)(component,vehicle)==0);
        allow_attached_vehicle_destruction=1;CHECK(((DestroyFunction)destroy)(component,vehicle)==1);
        CHECK(((DestroyFunction)destroy)(component,nearby)==0);
    }
    set_class(actor,cls,"LargePlatformA_C");ptr=actor;memcpy(mount+0xA8,&ptr,8);
    CHECK(((DestroyFunction)destroy)(component,actor)==0);
    ptr=storage;memcpy(mount+0xA8,&ptr,8);CHECK(((DestroyFunction)destroy)(component,vehicle)==0);
    memset(root+0xC8,0,8);CHECK(((DestroyFunction)destroy)(component,vehicle)==0);
    CHECK(!is_attached_vehicle(NULL,vehicle));CHECK(!is_attached_vehicle(component,NULL));
    // Wrecked wind-farm Medium Platform B: exact type, direct attachment only.
    set_class(actor,cls,"ModernPOI_WindFarm_B-01_Small_C");
    ptr=mount;memcpy(root+0xC8,&ptr,8);ptr=actor;memcpy(mount+0xA8,&ptr,8);
    CHECK(((DestroyFunction)destroy)(component,actor)==1);
    ptr=storage;memcpy(mount+0xA8,&ptr,8);
    CHECK(((DestroyFunction)destroy)(component,actor)==0);
    memset(root+0xC8,0,8);CHECK(((DestroyFunction)destroy)(component,actor)==0);
    set_class(actor,cls,"ModernPOI_WindFarm_Unverified_C");
    ptr=mount;memcpy(root+0xC8,&ptr,8);ptr=actor;memcpy(mount+0xA8,&ptr,8);
    CHECK(((DestroyFunction)destroy)(component,actor)==0);
    set_class(actor,cls,"Breadboard_T3_C");CHECK(((DestroyFunction)destroy)(component,actor)==0);
    puts("PASS: wind-farm wreck direct attachment; nearby/cargo/unattached exclusions; intact platform protected.");
    // Missing/invalid config preserves protection; only explicit true opts in.
    wchar_t tempdir[MAX_PATH],config[MAX_PATH];
    CHECK(GetTempPathW(MAX_PATH,tempdir));CHECK(GetTempFileNameW(tempdir,L"bpd",0,config));
    load_options(config);CHECK(!allow_attached_vehicle_destruction);
    const wchar_t *settings[]={L"true",L"false",L"TRUE",L"invalid",L"1"};
    const int enabled[]={1,0,1,0,0};
    for(unsigned i=0;i<5;++i) {
        CHECK(WritePrivateProfileStringW(L"Explosions",L"AllowAttachedVehicleDestruction",settings[i],config));
        load_options(config);CHECK(allow_attached_vehicle_destruction==enabled[i]);
    }
    CHECK(DeleteFileW(config));load_options(config);CHECK(!allow_attached_vehicle_destruction);
    puts("PASS: config defaults/true/false/invalid; attached vehicle opt-in; nearby and cargo-mounted exclusions; base protection.");
    puts("PASS: protected equipment; caches/debris/consumables and inheritance; real detour/trampoline execution; exact field writes; atomic mismatch refusal.");
    return 0;
}
