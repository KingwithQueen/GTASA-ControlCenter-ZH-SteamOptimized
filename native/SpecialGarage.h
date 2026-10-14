#include "TrainRestore.h"
static bool trainRestorePresent(){
 DWORD branch=u32(va(0x44b1f0));
 if(branch<trainBranchOffset)return false;
 DWORD allocation=branch-trainBranchOffset;
 MEMORY_BASIC_INFORMATION m{};
 if(!VirtualQueryEx(game,(void*)allocation,&m,sizeof m)||m.State!=MEM_COMMIT||m.Type!=MEM_PRIVATE||
  m.AllocationBase!=(void*)allocation||(m.Protect&255)!=PAGE_EXECUTE_READ||
  m.RegionSize<trainAllocationSize||m.Protect&(PAGE_GUARD|PAGE_NOACCESS))return false;
 char tag[sizeof trainRestoreTag];
 if(!read(allocation,tag,sizeof tag)||memcmp(tag,trainRestoreTag,sizeof tag))return false;
 auto code=trainRestoreCode(base,branch);std::vector<BYTE> actual(code.size());
 if(!read(branch,actual.data(),actual.size())||actual!=code)return false;
 BYTE signature[sizeof trainCtorSignature];
 if(!read(va(0x706300),signature,sizeof signature)||memcmp(signature,trainCtorSignature,sizeof signature))return false;
 for(int i=0;i<11;i++)if(i!=trainSwitchIndex&&u32(va(0x44b1dc)+i*4)!=va(trainRestoreTable[i]))return false;
 return true;
}
static bool airRestorePresent(){
 // Original Steam branches allocate actual CHeli and CPlane objects.
 return u32(va(0x44b1e4))==va(0x44ae5a)&&u32(va(0x44b1e8))==va(0x44ae8c)&&
  u8(va(0x6f8a10))==0x55&&u8(va(0x6fdb00))==0x55;
}
static DWORD garageModelKind(WORD model){
 if(model<400||model>611)return 0xffffffff;
 DWORD info=u32(va(0xb12818)+model*4),kind=0;
 if(!(garageData(info,0x4c)||privateWritable(info,0x4c))||!read(info+60,&kind,4)||kind>11)return 0xffffffff;
 return kind;
}
extern "C" DWORD __stdcall SteamGarageCapabilities(){
 if(!game||!base)return 0;
 return (airRestorePresent()?1u:0u)|(trainRestorePresent()?2u:0u);
}
extern "C" DWORD __stdcall SteamGarageModelSupported(DWORD model){
 if(!game||!base||model<400||model>611)return 0;
 DWORD kind=garageModelKind((WORD)model);
 if(kind==3||kind==4)return airRestorePresent()?1:0;
 if(kind==6)return trainRestorePresent()?1:0;
 return kind<=11&&garageCatalog[model-400].parkable?1:0;
}
static bool garageNeedsTrain(const BYTE* selected){
 for(int i=0;i<4;i++)if(garageModelKind(garageWord(selected+i*64+18))==6)return true;
 return false;
}
