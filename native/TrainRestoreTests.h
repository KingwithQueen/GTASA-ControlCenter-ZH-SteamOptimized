static DWORD trainTestVehicle=0,trainTestSize=0,trainTestCalls=0;
static bool trainTestAllocationFails=false;
static void* __cdecl trainTestAllocate(DWORD size){
 trainTestSize=size;trainTestCalls++;return trainTestAllocationFails?nullptr:(void*)trainTestVehicle;
}
static DWORD seedTrainRestore(){
 for(int i=0;i<11;i++)*(DWORD*)(va(0x44b1dc)+i*4)=va(trainRestoreTable[i]);
 memcpy((void*)va(0x706300),trainCtorSignature,sizeof trainCtorSignature);
 DWORD allocation=(DWORD)VirtualAlloc(nullptr,trainAllocationSize,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 assert(allocation);auto code=trainRestoreCode(base,allocation+trainBranchOffset);
 memcpy((void*)allocation,trainRestoreTag,sizeof trainRestoreTag);
 memcpy((void*)(allocation+trainBranchOffset),code.data(),code.size());
 DWORD old=0;assert(VirtualProtect((void*)allocation,trainAllocationSize,PAGE_EXECUTE_READ,&old));
 *(DWORD*)va(0x44b1f0)=allocation+trainBranchOffset;return allocation;
}
static void trainRestoreTests(){
 DWORD allocation=seedTrainRestore();assert(trainRestorePresent());
 *(DWORD*)va(0x44b1dc)=0;assert(!trainRestorePresent());*(DWORD*)va(0x44b1dc)=va(trainRestoreTable[0]);
 DWORD old=0;assert(VirtualProtect((void*)allocation,4096,PAGE_EXECUTE_READWRITE,&old));
 assert(!trainRestorePresent());((BYTE*)allocation)[0]^=1;
 assert(VirtualProtect((void*)allocation,4096,PAGE_EXECUTE_READ,&old));assert(!trainRestorePresent());
 assert(VirtualProtect((void*)allocation,4096,PAGE_READWRITE,&old));((BYTE*)allocation)[0]^=1;
 assert(VirtualProtect((void*)allocation,4096,PAGE_EXECUTE_READ,&old));assert(trainRestorePresent());
 trainTestVehicle=(DWORD)VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);assert(trainTestVehicle);
 std::vector<BYTE> alloc={0xb8};DWORD target=(DWORD)&trainTestAllocate;
 for(int i=0;i<4;i++)alloc.push_back(BYTE(target>>(8*i)));alloc.insert(alloc.end(),{0xff,0xe0});
 memcpy((void*)va(0x71d270),alloc.data(),alloc.size());
 // A thiscall constructor with the same return/stack ABI, leaving flags/linkage observable.
 const BYTE ctor[]={0x8b,0xc1,0xc7,0x40,0x40,0x00,0x00,0x02,0x00,
  0xc7,0x80,0x90,5,0,0,6,0,0,0,0x8b,0x4c,0x24,4,0x66,0x89,0x48,0x22,0xc2,8,0};
 memcpy((void*)va(0x706300),ctor,sizeof ctor);*(BYTE*)va(0x44afb8)=0xc3;*(BYTE*)va(0x44b1c8)=0xc3;
 BYTE frame[64]={},stored[64]={};*(WORD*)(stored+18)=537;DWORD result=0;
 std::vector<BYTE> wrapper={0x9c,0x60};
 auto word=[&](DWORD v){for(int i=0;i<4;i++)wrapper.push_back(BYTE(v>>(8*i)));};
 wrapper.push_back(0xbd);word((DWORD)(frame+32));wrapper.push_back(0xbf);word((DWORD)stored);
 wrapper.push_back(0xb8);word(allocation+trainBranchOffset);wrapper.insert(wrapper.end(),{0xff,0xd0,0xa3});word((DWORD)&result);
 wrapper.insert(wrapper.end(),{0x61,0x9d,0xc3});
 DWORD entry=(DWORD)VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);assert(entry);
 memcpy((void*)entry,wrapper.data(),wrapper.size());auto run=(void(__cdecl*)())entry;
 run();assert(result==trainTestVehicle&&trainTestSize==0x6ac&&trainTestCalls==1);
 assert(u32(trainTestVehicle+0x590)==6&&*(WORD*)(trainTestVehicle+0x22)==537);
 assert((*(WORD*)(trainTestVehicle+0x5b8)&0x100)&&!(u32(trainTestVehicle+0x40)&0x20000));
 assert(!u32(trainTestVehicle+0x5d0)&&!u32(trainTestVehicle+0x5d4));
 assert(*(DWORD*)(frame+8)==trainTestVehicle&&*(DWORD*)(frame+28)==0xffffffff);
 trainTestAllocationFails=true;run();assert(result==0&&trainTestCalls==2);trainTestAllocationFails=false;
 *(DWORD*)va(0x44b1f0)=va(0x44af89);
 VirtualFree((void*)allocation,0,MEM_RELEASE);VirtualFree((void*)entry,0,MEM_RELEASE);
 VirtualFree((void*)trainTestVehicle,0,MEM_RELEASE);
 puts("TRAIN_RESTORE=PASS actual x86 allocation/constructor/stack ABI, null allocation, off-rail physics, signature and read-only component gates");
}
