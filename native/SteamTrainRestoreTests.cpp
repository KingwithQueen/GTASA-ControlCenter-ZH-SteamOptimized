#define TRAIN_COMPONENT_TEST
#include "SteamTrainRestore.cpp"
#include <cassert>
#include <cstdio>
int main(){
 _set_error_mode(_OUT_TO_STDERR);_set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
 DWORD image=(DWORD)VirtualAlloc(nullptr,0xa00000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);assert(image);
 auto* dos=(IMAGE_DOS_HEADER*)image;dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=128;
 auto* pe=(IMAGE_NT_HEADERS32*)(image+128);pe->Signature=IMAGE_NT_SIGNATURE;
 pe->FileHeader.Machine=IMAGE_FILE_MACHINE_I386;pe->FileHeader.TimeDateStamp=0x547cd2e9;
 pe->OptionalHeader.SizeOfImage=0x985000;pe->OptionalHeader.CheckSum=0x5bf017;
 auto* table=(DWORD*)trainAddress(image,0x44b1dc);
 for(int i=0;i<11;i++)table[i]=trainAddress(image,trainRestoreTable[i]);
 memcpy((void*)trainAddress(image,0x706300),trainCtorSignature,sizeof trainCtorSignature);
 const BYTE common[]={0xd9,7,0x8b,0xf0};memcpy((void*)trainAddress(image,0x44afb8),common,sizeof common);
 pe->OptionalHeader.CheckSum=0;assert(!installTrainRestore(image));pe->OptionalHeader.CheckSum=0x5bf017;
 table[0]=0;assert(!installTrainRestore(image));table[0]=trainAddress(image,trainRestoreTable[0]);
 *(BYTE*)trainAddress(image,0x706300)=0;assert(!installTrainRestore(image));
 memcpy((void*)trainAddress(image,0x706300),trainCtorSignature,sizeof trainCtorSignature);
 assert(installTrainRestore(image));DWORD allocation=table[trainSwitchIndex]-trainBranchOffset;
 assert(!memcmp((void*)allocation,trainRestoreTag,sizeof trainRestoreTag));
 auto code=trainRestoreCode(image,table[trainSwitchIndex]);
 assert(!memcmp((void*)table[trainSwitchIndex],code.data(),code.size()));
 MEMORY_BASIC_INFORMATION m{};assert(VirtualQuery((void*)allocation,&m,sizeof m)&&m.Protect==PAGE_EXECUTE_READ);
 for(int i=0;i<11;i++)if(i!=trainSwitchIndex)assert(table[i]==trainAddress(image,trainRestoreTable[i]));
 assert(!installTrainRestore(image)&&table[trainSwitchIndex]==allocation+trainBranchOffset);
 VirtualFree((void*)allocation,0,MEM_RELEASE);VirtualFree((void*)image,0,MEM_RELEASE);
 puts("TRAIN_COMPONENT=PASS startup installer, incompatible PE/code/branches rejected, only train branch patched, immutable self-contained code");
}
