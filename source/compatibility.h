static HANDLE compatibility_process = (HANDLE)(intptr_t)-1;

static int checked_read(const void *address, void *out, size_t size) {
    SIZE_T copied=0;
    return ReadProcessMemory(compatibility_process,address,out,size,&copied) && copied==size;
}

static int checked_name(unsigned char *pool, uint32_t index, char name[256]) {
    if ((index>>16)>=8192) return 0;
    unsigned char *block;
    if (!checked_read(pool+0x10+(index>>16)*8,&block,8) || !block) return 0;
    unsigned char *entry=block+(index&65535)*2;
    uint16_t header;
    if (!checked_read(entry,&header,2)) return 0;
    unsigned length=header>>6;
    if (!length || length>=256 || (header&1) || !checked_read(entry+2,name,length)) return 0;
    name[length]=0;
    return 1;
}

static unsigned char *rip_address(unsigned char *instruction, unsigned displacement, unsigned length) {
    int32_t relative; memcpy(&relative,instruction+displacement,4);
    return instruction+length+relative;
}

static int image_contains(unsigned char *image, const void *address, size_t size) {
    IMAGE_DOS_HEADER *dos=(IMAGE_DOS_HEADER *)image;
    IMAGE_NT_HEADERS64 *nt=(IMAGE_NT_HEADERS64 *)(image+dos->e_lfanew);
    uintptr_t start=(uintptr_t)image,at=(uintptr_t)address;
    return at>=start && size<=nt->OptionalHeader.SizeOfImage &&
           at-start<=nt->OptionalHeader.SizeOfImage-size;
}

static unsigned char *find_name_pool(unsigned char *image) {
    IMAGE_DOS_HEADER *dos=(IMAGE_DOS_HEADER *)image;
    IMAGE_NT_HEADERS64 *nt=(IMAGE_NT_HEADERS64 *)(image+dos->e_lfanew);
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
    const unsigned char tail[]={0x48,0x8B,0x44,0x24,0x38,0x48,0xC1,0xE8,0x20,0x8D,0x0C,0x00,0x49,0x8B,0x44,0xF8,0x10};
    unsigned char *found=NULL;
    for (unsigned s=0;s<nt->FileHeader.NumberOfSections;++s) {
        if (!(sections[s].Characteristics&IMAGE_SCN_MEM_EXECUTE)) continue;
        unsigned char *start=image+sections[s].VirtualAddress;
        size_t size=sections[s].Misc.VirtualSize;
        if (!image_contains(image,start,size) || size<39) return NULL;
        for (size_t i=0;i<=size-39;++i) {
            unsigned char *p=start+i;
            if (p[0]!=0x48 || p[1]!=0x8D || p[2]!=0x0D || p[7]!=0xE8 ||
                memcmp(p+12,"\x4C\x8B\xC0\xC6\x05",5) || p[21]!=1 ||
                memcmp(p+22,tail,sizeof(tail))) continue;
            unsigned char *candidate=rip_address(p,3,7);
            char name[256];
            if (!image_contains(image,candidate,24) || !checked_name(candidate,0,name) || strcmp(name,"None")) continue;
            if (found && found!=candidate) return NULL;
            found=candidate;
        }
    }
    return found;
}

static unsigned char *find_object_array(unsigned char *image, unsigned char *destroy) {
    if (!image_contains(image,destroy,0xC9) || memcmp(destroy+0xA1,"\x3B\x05",2) ||
        memcmp(destroy+0xC2,"\x48\x8B\x05",3)) return NULL;
    unsigned char *from_count=rip_address(destroy+0xA1,2,6)-0x24;
    unsigned char *from_chunks=rip_address(destroy+0xC2,3,7)-0x10;
    if (from_count!=from_chunks || !image_contains(image,from_count,0x28)) return NULL;
    return from_count;
}

typedef struct RequiredField {
    const char *owner,*name;
    uint32_t offset,size;
} RequiredField;

static const RequiredField required_fields[]={
    {"Actor","RootComponent",0x148,8},
    {"SceneComponent","AttachParent",0xC8,8},
    {"PhysicalItem","ItemComponent",0x3D8,8},
    {"ItemComponent","storedSubItemType",0x160,8},
    {"ItemType","PickupActor",0xC8,8},
    {"ExplosiveComponent","Power",0xB8,4},
    {"ExplosiveComponent","CombineExplosionsRadiusRatio",0xC0,4},
    {"ExplosiveComponent","ExplosionRadiusCraterScale",0xC4,4},
    {"ExplosiveComponent","PowerToRadiusScale",0xC8,4},
    {"ExplosiveComponent","CombinedPower",0x148,4}
};

static int validate_fields(unsigned char *cls, unsigned char *pool, const char *owner, uint32_t *seen) {
    const char *super_name=!strcmp(owner,"Actor") ? "Object" :
        !strcmp(owner,"PhysicalItem") ? "Actor" : !strcmp(owner,"ItemType") ? "ControlSymbol" : "ActorComponent";
    unsigned char *super;uint32_t super_index;char actual_super[256];
    if (!checked_read(cls+0x40,&super,8) || !super || !checked_read(super+0x18,&super_index,4) ||
        !checked_name(pool,super_index,actual_super) || strcmp(actual_super,super_name)) return 0;
    unsigned char *property;
    if (!checked_read(cls+0x50,&property,8)) return 0;
    unsigned count=0;
    while (property) {
        if (++count>2048) return 0;
        unsigned char data[0x50];char name[256];uint32_t index,offset,size;
        if (!checked_read(property,data,sizeof(data))) return 0;
        memcpy(&index,data+0x28,4);memcpy(&offset,data+0x4C,4);memcpy(&size,data+0x3C,4);
        if (!checked_name(pool,index,name)) return 0;
        for (unsigned i=0;i<sizeof(required_fields)/sizeof(*required_fields);++i) {
            const RequiredField *field=required_fields+i;
            if (strcmp(owner,field->owner) || strcmp(name,field->name)) continue;
            if (offset!=field->offset || size!=field->size) return 0;
            *seen|=1u<<i;
        }
        memcpy(&property,data+0x20,8);
    }
    // Check the UClass default-object offset used for packaged item types.
    unsigned char *cdo,*cdo_class;uint32_t flags;
    return checked_read(cls+0x118,&cdo,8) && cdo &&
           checked_read(cdo+0x10,&cdo_class,8) && cdo_class==cls &&
           checked_read(cdo+8,&flags,4) && (flags&0x10);
}

static int validate_layout(unsigned char *objects, unsigned char *pool) {
    unsigned char **chunks;int32_t count;
    if (!checked_read(objects+0x10,&chunks,8) || !chunks ||
        !checked_read(objects+0x24,&count,4) || count<=0 || count>2000000) return 0;
    uint32_t seen=0,all=(1u<<(sizeof(required_fields)/sizeof(*required_fields)))-1;
    for (int32_t i=0;i<count && seen!=all;++i) {
        unsigned char *chunk,*object,*meta;uint32_t index;char owner[256],kind[256];
        if (!checked_read(chunks+((unsigned)i>>16),&chunk,8) || !chunk ||
            !checked_read(chunk+((unsigned)i&65535)*24,&object,8)) return 0;
        if (!object || !checked_read(object+0x18,&index,4) || !checked_name(pool,index,owner)) continue;
        int wanted=0;
        for (unsigned f=0;f<sizeof(required_fields)/sizeof(*required_fields);++f)
            if (!strcmp(owner,required_fields[f].owner)) {wanted=1;break;}
        if (!wanted) continue;
        if (!checked_read(object+0x10,&meta,8) || !meta || !checked_read(meta+0x18,&index,4) ||
            !checked_name(pool,index,kind) || strcmp(kind,"Class")) continue;
        if (!validate_fields(object,pool,owner,&seen)) return 0;
    }
    return seen==all;
}
