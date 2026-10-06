#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <vector>
#include "TrainRestore.h"

// The installer verifies the complete EXE hash. Recheck the loaded image before patching.
// Only memory APIs run here; no loader, crypto, file, engine, or background thread calls.
static bool installTrainRestore(DWORD image){
 const auto* dos=(const IMAGE_DOS_HEADER*)image;
 if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<=0||dos->e_lfanew>4096)return false;
 const auto* pe=(const IMAGE_NT_HEADERS32*)(image+dos->e_lfanew);
 if(pe->Signature!=IMAGE_NT_SIGNATURE||pe->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386||
  pe->FileHeader.TimeDateStamp!=0x547cd2e9||pe->OptionalHeader.SizeOfImage!=0x985000||
  pe->OptionalHeader.CheckSum!=0x5bf017||
  memcmp((void*)trainAddress(image,0x706300),trainCtorSignature,sizeof trainCtorSignature))return false;
 auto* table=(DWORD*)trainAddress(image,0x44b1dc);
 for(int i=0;i<11;i++)if(table[i]!=trainAddress(image,trainRestoreTable[i]))return false;
 const BYTE common[]={0xd9,0x07,0x8b,0xf0};
 if(memcmp((void*)trainAddress(image,0x44afb8),common,sizeof common))return false;
 DWORD allocation=(DWORD)VirtualAlloc(nullptr,trainAllocationSize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
 if(!allocation)return false;
 auto code=trainRestoreCode(image,allocation+trainBranchOffset);
 memcpy((void*)allocation,trainRestoreTag,sizeof trainRestoreTag);
 memcpy((void*)(allocation+trainBranchOffset),code.data(),code.size());
 DWORD old=0;
 if(!VirtualProtect((void*)allocation,trainAllocationSize,PAGE_EXECUTE_READ,&old)){
  VirtualFree((void*)allocation,0,MEM_RELEASE);return false;
 }
 FlushInstructionCache(GetCurrentProcess(),(void*)allocation,trainAllocationSize);
 if(!VirtualProtect(table+trainSwitchIndex,4,PAGE_EXECUTE_READWRITE,&old)){
  VirtualFree((void*)allocation,0,MEM_RELEASE);return false;
 }
 InterlockedExchange((LONG*)(table+trainSwitchIndex),(LONG)(allocation+trainBranchOffset));
 DWORD ignored=0;VirtualProtect(table+trainSwitchIndex,4,old,&ignored);
 // The self-contained code survives component unload. Keep it until process exit.
 return true;
}
#ifndef TRAIN_COMPONENT_TEST
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){
 if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(instance);installTrainRestore((DWORD)GetModuleHandleW(nullptr));}
 return TRUE;
}
#endif
