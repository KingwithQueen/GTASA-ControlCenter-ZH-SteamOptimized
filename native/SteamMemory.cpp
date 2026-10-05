// Steam address adapter for the supplied GTA SA Control Center VB6 sources.
// Addresses below belong ONLY to SHA256 15e3cfed...43efb (classic Steam).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <wincrypt.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <cmath>
#pragma comment(lib,"psapi.lib")
#pragma comment(lib,"advapi32.lib")
static HANDLE game=nullptr;
static DWORD gamePid=0,base=0,remote=0;
static bool enabled=false;
static bool coffeeScriptsVerified=false;
static DWORD lastError=0;
static bool stopRuntime();
static DWORD va(DWORD preferred){return base+preferred-0x400000;}
static bool read(DWORD p,void* b,SIZE_T n){SIZE_T got=0;return ReadProcessMemory(game,(void*)p,b,n,&got)&&got==n;}
static bool write(DWORD p,const void* b,SIZE_T n){SIZE_T got=0;return WriteProcessMemory(game,(void*)p,b,n,&got)&&got==n;}
static DWORD u32(DWORD p){DWORD v=0;read(p,&v,4);return v;}
static BYTE u8(DWORD p){BYTE v=0;read(p,&v,1);return v;}
static bool fingerprint(const wchar_t* path,const char* expected="15e3cfedba9a841df67d8194e7249afb493b0e10d6138fb8ebab2c136e543efb"){
 HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
 if(f==INVALID_HANDLE_VALUE)return false;
 HCRYPTPROV provider=0;HCRYPTHASH hash=0;bool ok=false;
 if(CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)&&CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)){
  BYTE buf[65536];DWORD count;bool good=true;
  for(;;){if(!ReadFile(f,buf,sizeof buf,&count,nullptr)){good=false;break;}if(!count)break;if(!CryptHashData(hash,buf,count,0)){good=false;break;}}
  BYTE digest[32];DWORD n=32;char text[65]={};
  if(good&&CryptGetHashParam(hash,HP_HASHVAL,digest,&n,0)){for(int i=0;i<32;i++)sprintf_s(text+i*2,3,"%02x",digest[i]);ok=strcmp(text,expected)==0;}
 }
 if(hash)CryptDestroyHash(hash);if(provider)CryptReleaseContext(provider,0);CloseHandle(f);return ok;
}
// Each range is an independently located typed array/structure, not a global offset.
struct Range{DWORD oldAddress,steamAddress,size;};
static constexpr Range ranges[]={
 {0xB79380,0xC0C5A8,82*4}, // CStats::StatTypesFloat: verified SetStatValue 0x570900
 {0xB79000,0xC0C228,223*4},// Legacy int index 0 == stat ID 120; Steam indexes raw IDs.
 {0xB7CD98,0xC100D0,0x190},// CWorld::Players[0]: FindPlayerPed and money cheat
 {0x969130,0x9E0C58,92},   // cheat flags: independently matched hash/function table
};
static DWORD mapped(DWORD a,DWORD n){
 for(auto r:ranges)if(a>=r.oldAddress&&n<=r.size&&a-r.oldAddress<=r.size-n)return va(r.steamAddress+a-r.oldAddress);
 switch(a){
 case 0xB6F5F0:return n==4?va(0xC100D0):0;
 case 0xB7CB64:return n==4?va(0xC0FD58):0;
 case 0xB70153:return n==1?va(0xBFD4C7):0;
 case 0xB70152:return n==1?va(0xBFD4C6):0;
 case 0xB7014E:return n==1?va(0xBFD4C3):0;
 case 0xB7015C:return n==4?va(0xBFD4D0):0; // CClock::Update 0x53B020
 default:break;
 }
 // Unmapped legacy globals/code never fall through to a Steam address by accident.
 if(a<0x1400000||!n||n>4096||a>0xffffffff-n)return 0;
 MEMORY_BASIC_INFORMATION m{};
 if(!VirtualQueryEx(game,(void*)a,&m,sizeof m)||m.State!=MEM_COMMIT||m.Type!=MEM_PRIVATE||
    (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))||a+n>(uintptr_t)m.BaseAddress+m.RegionSize)return 0;
 return a;
}
static bool privateWritable(DWORD a,DWORD n);
static bool currentVehicle(DWORD& vehicle){
 DWORD ped=0,rc=0,flags=0;vehicle=0;
 if(!read(va(0xc100d0),&ped,4)||!read(va(0xc10180),&rc,4))return false;
 if(!ped)return true;
 // FindPlayerCoors 0x560F52 gives remote control priority. Keep the UI's
 // existing six-model whitelist and "also control RC vehicles" opt-in.
 if(rc&&privateWritable(rc,0x5a4)){
  WORD model=0;if(!read(rc+0x22,&model,2))return false;
  for(int id:{441,464,594,501,465,564})if(model==id){vehicle=rc;return true;}
 }
 if(!read(ped+0x46c,&flags,4))return false;
 if(flags&0x100)return read(ped+0x58c,&vehicle,4)&&vehicle!=0;
 vehicle=ped;return true;
}
#include "LegacyGarage.h"
static bool featurePause();
#include "Girlfriends.h"
#include "HotCoffee.h"
#include "Feedback.h"

// Remote queue layout: magic[32], enabled DWORD at +32, car at +36,
// twelve model requests at +40. Code starts at +0x100 and executes on the game thread.
static std::vector<BYTE> stub(DWORD allocation){
 std::vector<BYTE> b;const DWORD start=allocation+0x100;
 auto byte=[&](BYTE v){b.push_back(v);};
 auto word=[&](DWORD v){for(int i=0;i<4;i++)byte(BYTE(v>>(i*8)));};
 auto bytes=[&](std::initializer_list<BYTE> v){b.insert(b.end(),v);};
 auto call=[&](DWORD target){byte(0xe8);word(target-(start+(DWORD)b.size()+4));};
 bytes({0x9c,0x60,0x8b,0xec,0x81,0xec});word(528);bytes({0x83,0xe4,0xf0,0x0f,0xae,0x04,0x24}); // save flags/registers/FX state
 appendGarageCode(b,allocation,start);
 appendGirlfriendCode(b,allocation,start);
 appendFeedbackCode(b,allocation,start);
 bytes({0x83,0x3d});word(allocation+32);byte(0);
 bytes({0x0f,0x84});SIZE_T skip=b.size();word(0);
 // Clear each slot before calling the engine; reentrant requests cannot run twice.
 for(int slot=0;slot<13;slot++){
  DWORD at=allocation+36+slot*4;
  bytes({0x31,0xc0,0x87,0x05});word(at);bytes({0x85,0xc0,0x74});
  SIZE_T j=b.size();byte(0);
  if(slot)bytes({0x6a,0x02});byte(0x50);
  call(va(slot?0x408B00:0x43C620));bytes({0x83,0xc4,BYTE(slot?8:4)});
  b[j]=BYTE(b.size()-j-1);
 }
 DWORD dist=(DWORD)(b.size()-skip-4);memcpy(b.data()+skip,&dist,4);
 bytes({0x0f,0xae,0x0c,0x24,0x8b,0xe5,0x61,0x9d}); // restore FX/registers/flags
 call(va(0x43BF50));byte(0xc3);return b;
}
static const char magic[]="GTASA_CC_STEAM_QUEUE_V2";
static bool installQueue(){
 if(remote)return true;
 BYTE old[5];if(!read(va(0x54BD2B),old,5))return false;
 const BYTE expected[]={0xe8,0x20,0x02,0xef,0xff};
 if(memcmp(old,expected,5)!=0){
  if(old[0]!=0xe8){lastError=3;return false;}
  DWORD candidate=va(0x54BD30)+*(DWORD*)(old+1)-0x100;
  MEMORY_BASIC_INFORMATION region{};
  if(!VirtualQueryEx(game,(void*)candidate,&region,sizeof region)||region.State!=MEM_COMMIT||region.Type!=MEM_PRIVATE||
     region.AllocationBase!=(void*)candidate||candidate>0xffffffff-remoteSize||
     candidate+remoteSize>(uintptr_t)region.BaseAddress+region.RegionSize||
     (region.Protect&255)!=PAGE_EXECUTE_READWRITE){lastError=3;return false;}
  char tag[sizeof magic];if(!read(candidate,tag,sizeof tag)||memcmp(tag,magic,sizeof tag)){lastError=3;return false;}
  auto code=stub(candidate);std::vector<BYTE> actual(code.size());
  if(!read(candidate+0x100,actual.data(),actual.size())||actual!=code){lastError=3;return false;}
  remote=candidate;return true;
 }
 // FrontEndMenuManager + 0x5C remains true when minimizing clears UserPause.
 // The menu prevents normal world processing in that case as well.
 if(!u8(va(0xC0FD39))&&!u8(va(0xC0FD3A))&&!u8(va(0xC33994))){lastError=2;return false;}
 DWORD alloc=(DWORD)VirtualAllocEx(game,nullptr,remoteSize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
 if(!alloc)return false;
 auto code=stub(alloc);
 if(!write(alloc,magic,sizeof magic)||!write(alloc+0x100,code.data(),code.size()))return false;
 DWORD prot;if(!VirtualProtectEx(game,(void*)alloc,remoteSize,PAGE_EXECUTE_READWRITE,&prot))return false;
 FlushInstructionCache(game,(void*)(alloc+0x100),code.size());
 BYTE jump[5]={0xe8};DWORD offset=alloc+0x100-va(0x54BD30);memcpy(jump+1,&offset,4);
 DWORD previous;if(!VirtualProtectEx(game,(void*)va(0x54BD2B),5,PAGE_EXECUTE_READWRITE,&previous))return false;
 const bool ok=write(va(0x54BD2B),jump,5);DWORD unused;
 VirtualProtectEx(game,(void*)va(0x54BD2B),5,previous,&unused);FlushInstructionCache(game,(void*)va(0x54BD2B),5);
 if(!ok)return false;remote=alloc;return true;
}
extern "C" DWORD __stdcall SteamAttach(HANDLE process){
 DWORD pid=GetProcessId(process);if(!pid)return 0;
 if(gamePid==pid&&game==process&&base)return base;
 if(game&&game!=process&&!stopRuntime()){lastError=4;return 0;}
 game=nullptr;gamePid=base=remote=0;enabled=false;coffeeScriptsVerified=false;lastError=0;
 HMODULE module;DWORD needed;wchar_t path[32768];
 if(!EnumProcessModules(process,&module,sizeof module,&needed)||!GetModuleFileNameExW(process,module,path,32768)||!fingerprint(path))return 0;
 game=process;gamePid=pid;base=(DWORD)module;
 // Verify an independent, relocated FindPlayerPed instruction before permitting writes.
 BYTE actual[6],expected[6]={0x8b,0x80};DWORD address=va(0xC100D0);memcpy(expected+2,&address,4);
 if(!read(va(0x560EF7),actual,6)||memcmp(actual,expected,6)){game=nullptr;base=gamePid=0;return 0;}
 std::wstring scriptPath=path;SIZE_T slash=scriptPath.find_last_of(L"\\/");
 if(slash!=std::wstring::npos){scriptPath.resize(slash+1);scriptPath+=L"data\\script\\script.img";
  coffeeScriptsVerified=fingerprint(scriptPath.c_str(),"4ce0cdde60b95b4676d392685b0440a391f1e4bb8ac79db6abb35e379ff82fde");}
 BYTE call[5]={};const BYTE original[]={0xe8,0x20,0x02,0xef,0xff};
 if(read(va(0x54bd2b),call,5)&&memcmp(call,original,5)&&installQueue()){
  // A previous instance may have crashed. Adopt only the exact V2 allocation
  // and release its old controls before the newly loaded UI chooses settings.
  if(!stopRuntime()){lastError=4;return 0;}
 }
 return base;
}
extern "C" DWORD __stdcall SteamStatus(){return lastError;}
#include "FeatureHooks.h"
static bool stopRuntime(){
 if(!game||!remote)return true;DWORD exitCode=STILL_ACTIVE;
 if(GetExitCodeProcess(game,&exitCode)&&exitCode!=STILL_ACTIVE)return true;
 DWORD zero[14]={};bool ok=write(remote+32,zero,sizeof zero);
 ok=write(remote+featureFlags,zero,8)&&ok;
 for(DWORD i=0;i<6;i++)ok=write(remote+gfOffset+i*gfStride,zero,4)&&ok;
 DWORD cancelled=4;for(DWORD i=0;i<2;i++)ok=write(remote+feedbackOffset+i*feedbackStride,&cancelled,4)&&ok;
 for(DWORD i=0;i<17;i++)ok=write(remote+lockOffset+i*lockStride,zero,4)&&ok;
 return ok;
}
extern "C" BOOL __stdcall TrainerCloseHandle(HANDLE process){
 if(process==game){
  // VB6 closes its process handle before DLL_PROCESS_DETACH. Clear the queue now.
  if(!stopRuntime()){lastError=4;return FALSE;}
  game=nullptr;gamePid=base=remote=0;enabled=false;coffeeScriptsVerified=false;lastError=0;
 }
 return CloseHandle(process);
}
extern "C" BOOL __stdcall TrainerReadMemory(HANDLE process,DWORD address,void* out,DWORD count,DWORD* got){
 if(got)*got=0;if(!out||!count||count>4096)return FALSE;
 if(process!=game||!base)return ReadProcessMemory(process,(void*)address,out,count,(SIZE_T*)got);
 memset(out,0,count);
 if(address==0xB6F3B8&&count==4){DWORD vehicle=0;if(!currentVehicle(vehicle))return FALSE;*(DWORD*)out=vehicle;if(got)*got=4;return TRUE;}
 if(address==0x53BF9C&&count==5){const BYTE original[]={0xa0,0x48,0xcb,0xb7,0};const BYTE injected[]={0xe9,0x3f,0xad,0x31,0};memcpy(out,enabled?injected:original,5);if(got)*got=5;return TRUE;}
 if(address==0xA4AC6C&&count==1){*(BYTE*)out=255;if(got)*got=1;return TRUE;} // use the validated SteamCoffee API
 if(address==0x96918C&&count==4){*(DWORD*)out=u8(va(0x9E0CB4));if(got)*got=4;return TRUE;}
 if((address==0xC81318||address==0xC8131C||address==0xC81320)&&count==4){
  short v=0;if(!read(va(address==0xC81318?0xD0D068:address==0xC8131C?0xD0D070:0xD0D06C),&v,2))return FALSE;
  *(int*)out=v;if(got)*got=4;return TRUE;
 }
 DWORD target=mapped(address,count);if(!target)return FALSE;
 bool ok=read(target,out,count);if(ok&&got)*got=count;return ok;
}
extern "C" BOOL __stdcall TrainerWriteMemory(HANDLE process,DWORD address,const void* data,DWORD count,DWORD* got){
 if(got)*got=0;if(!data||!count||count>4096)return FALSE;
 if(process!=game||!base)return WriteProcessMemory(process,(void*)address,data,count,(SIZE_T*)got);
 bool ok=false;
 if(address==0x856CE0&&count==504){ok=true;} // old machine code is never copied into Steam
 else if(address==0x53BF9C&&count==5){
  BYTE op=*(const BYTE*)data;
  if(op==0xa0){DWORD zero[14]={};if(remote)write(remote+32,zero,sizeof zero);enabled=false;ok=true;}
  else if(op==0xe9&&installQueue()){DWORD yes=1;ok=write(remote+32,&yes,4);enabled=ok;}
 }
 else if(address>=0x1301000&&address<=0x1301030&&count==4&&(address%4)==0){
  DWORD value=*(const DWORD*)data;bool car=address==0x1301000;
  if(remote&&enabled&&(value==0||(car?(value>=400&&value<=611):(value>=321&&value<=372))))ok=write(remote+36+address-0x1301000,data,4);
 }
 else if(address==0x96918C&&count==4){BYTE v=*(const DWORD*)data?1:0;ok=write(va(0x9E0CB4),&v,1);}
 else if((address==0xC81318||address==0xC8131C||address==0xC81320)&&count==4){
  int v=*(const int*)data;if(v>=-1&&v<=45){short s=(short)v;ok=write(va(address==0xC81318?0xD0D068:address==0xC8131C?0xD0D070:0xD0D06C),&s,2);}
 }else{DWORD target=mapped(address,count);if(target)ok=write(target,data,count);}
 if(ok&&got)*got=count;return ok;
}
BOOL WINAPI DllMain(HINSTANCE,DWORD reason,LPVOID){
 if(reason==DLL_PROCESS_DETACH)stopRuntime();return TRUE;
}
#include "OfflineUiFixture.h"
#ifdef UNIT_TEST
#include <cassert>
static DWORD carCalls=0,modelCalls=0,originalCalls=0,lastCar=0,lastModel=0;
static void callback(DWORD address,DWORD* counter,DWORD* argument){
 std::vector<BYTE> code={0xff,0x05};DWORD at=(DWORD)counter;
 for(int i=0;i<4;i++)code.push_back(BYTE(at>>(8*i)));
 if(argument){code.insert(code.end(),{0x8b,0x44,0x24,0x04,0xa3});at=(DWORD)argument;for(int i=0;i<4;i++)code.push_back(BYTE(at>>(8*i)));}
 code.push_back(0xc3);memcpy((void*)address,code.data(),code.size());
}
#include "LegacyGarageTests.h"
#include "FeatureHookTests.h"
#include "ScriptFeedbackTests.h"
int main(){
 game=GetCurrentProcess();base=(DWORD)VirtualAlloc(nullptr,0xa00000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
 remote=(DWORD)VirtualAlloc(nullptr,remoteSize,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);assert(base&&remote);
 callback(va(0x43C620),&carCalls,&lastCar);callback(va(0x408B00),&modelCalls,&lastModel);callback(va(0x43BF50),&originalCalls,nullptr);
 auto code=stub(remote);assert(code.size()<4096-0x100);memcpy((void*)(remote+0x100),code.data(),code.size());
 auto run=(void(__cdecl*)())(remote+0x100);
 *(DWORD*)(remote+32)=1;*(DWORD*)(remote+36)=411;for(int i=0;i<12;i++)*(DWORD*)(remote+40+i*4)=321+i;
 double before=123.25,after=0;
 __asm fld qword ptr before
 run();
 __asm fstp qword ptr after
 assert(after==before&&carCalls==1&&modelCalls==12&&originalCalls==1&&lastCar==411&&lastModel==332);
 for(int i=0;i<13;i++)assert(*(DWORD*)(remote+36+i*4)==0);
 for(int i=0;i<10000;i++)run();assert(carCalls==1&&modelCalls==12&&originalCalls==10001);
 *(DWORD*)(remote+32)=0;*(DWORD*)(remote+36)=400;run();assert(carCalls==1&&originalCalls==10002);
 assert(mapped(0xB793D4,4)==va(0xC0C5FC));assert(mapped(0xB79000,4)==va(0xC0C228));
 assert(mapped(0xA4AC6C,1)==0&&mapped(0x53BF9C,5)==0&&mapped(0x96ABD8,256)==0);
 assert(mapped(0xB794C6,4)==0); // crossing the end of the float array must fail
 legacyGarageTests();
 featureHookTests();
 scriptFeedbackTests();
 HANDLE owned=nullptr;assert(DuplicateHandle(GetCurrentProcess(),GetCurrentProcess(),GetCurrentProcess(),&owned,0,FALSE,DUPLICATE_SAME_ACCESS));
 game=owned;DWORD savedRemote=remote;*(DWORD*)(remote+32)=1;*(DWORD*)(remote+36)=411;
 assert(TrainerCloseHandle(owned));assert(!game&&!base&&!remote&&!enabled);
 for(int i=0;i<14;i++)assert(*(DWORD*)(savedRemote+32+i*4)==0);
 for(int i=0;i<2;i++)assert(*(DWORD*)(savedRemote+featureFlags+i*4)==0&&*(DWORD*)(savedRemote+feedbackOffset+i*feedbackStride)==4);
 run();for(int i=0;i<2;i++)assert(*(DWORD*)(savedRemote+feedbackOffset+i*feedbackStride)==0);
 for(int i=0;i<6;i++)assert(*(DWORD*)(savedRemote+gfOffset+i*gfStride)==0);
 for(int i=0;i<17;i++)assert(*(DWORD*)(savedRemote+lockOffset+i*lockStride)==0);
 puts("PASS: real x86 game-thread queue, 10002 calls, one-shot requests, FX state, typed mapping, rejected legacy addresses and queue cleanup before handle close.");return 0;
}
#endif
