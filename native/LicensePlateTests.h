static DWORD plateTestNew=0,plateTestCreates=0,plateTestDestroys=0;
static bool plateTestFail=false,plateTestCancelDuring=false,plateTestChangeDuring=false;
static DWORD __cdecl plateTestCreate(const char* text,DWORD type){
 assert(type<=2||type==255);plateTestCreates++;
 if(plateTestFail)return 0;
 if(plateTestCancelDuring)*(DWORD*)(remote+plateOffset)=4;
 if(plateTestChangeDuring)*(DWORD*)(u32(va(0xc100d0))+0x58c)=0;
 memcpy((void*)(plateTestNew+16),text,9);*(DWORD*)(plateTestNew+0x54)=1;return plateTestNew;
}
static void __cdecl plateTestDestroy(DWORD texture){
 assert(*(DWORD*)(texture+0x54)>0);--*(DWORD*)(texture+0x54);plateTestDestroys++;
}
static void licensePlateTests(){
 DWORD ped=(DWORD)VirtualAlloc(nullptr,8192,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 DWORD car=(DWORD)VirtualAlloc(nullptr,8192,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 DWORD info=(DWORD)VirtualAlloc(nullptr,256,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 DWORD old=(DWORD)VirtualAlloc(nullptr,256,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 plateTestNew=(DWORD)VirtualAlloc(nullptr,256,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 assert(ped&&car&&info&&old&&plateTestNew);
 *(DWORD*)va(0xc100d0)=ped;*(DWORD*)(ped+0x46c)=0x100;*(DWORD*)(ped+0x58c)=car;
 *(WORD*)(car+0x22)=411;*(DWORD*)(car+0x588)=old;*(DWORD*)(old+0x54)=2;
 *(DWORD*)(va(0xb12818)+411*4)=info;*(DWORD*)(info+0x24)=1;*(BYTE*)(info+0x31)=255;
 BYTE create[]={0x55,0x8b,0xec,0x56,0x68,4,6,0,0,0x83,0xc4,4,0x5e,0x5d,0xe9,0,0,0,0};
 DWORD d=(DWORD)&plateTestCreate-va(0x732c10+sizeof create);memcpy(create+15,&d,4);memcpy((void*)va(0x732c10),create,sizeof create);
 BYTE destroy[]={0x56,0x8b,0x74,0x24,8,0x8b,0x56,0x54,0x5e,0xe9,0,0,0,0};
 d=(DWORD)&plateTestDestroy-va(0x828060+sizeof destroy);memcpy(destroy+10,&d,4);memcpy((void*)va(0x828060),destroy,sizeof destroy);
 assert(plateEngineMatched());memset((void*)(remote+plateOffset),0,40);
 std::vector<BYTE> code={0x9c,0x60};const DWORD entry=remote+0x3800;
 appendPlateCode(code,remote,entry);code.insert(code.end(),{0x61,0x9d,0xc3});assert(code.size()<512);
 memcpy((void*)entry,code.data(),code.size());auto run=(void(__cdecl*)())entry;
 assert(SteamPlateSet(car,L"cj 123",6)==0&&SteamPlateStatus()==8);
 assert(*(DWORD*)(car+0x588)==old&&plateTestCreates==0);
 assert(SteamPlateSet(car,L"SECOND",6)==8);run();
 assert(SteamPlateStatus()==0&&plateTestCreates==1&&plateTestDestroys==1);
 assert(*(DWORD*)(car+0x588)==plateTestNew&&*(DWORD*)(old+0x54)==1);
 assert(!memcmp((void*)(plateTestNew+16),"CJ 123  ",8));
 assert(SteamPlateSet(car,L"CJ 123",6)==0);run();assert(plateTestCreates==1);
 assert(SteamPlateSet(car,L"\x4e2d",1)==3&&SteamPlateSet(car,L"123456789",9)==3&&SteamPlateSet(car,L" ",1)==3);
 assert(SteamPlateSet(car,L"STALE",5)==0);*(DWORD*)(ped+0x58c)=0;run();
 assert(SteamPlateStatus()==7&&plateTestCreates==1);*(DWORD*)(ped+0x58c)=car;
 assert(SteamPlateSet(car,L"FAILED",6)==0);plateTestFail=true;run();plateTestFail=false;
 assert(SteamPlateStatus()==4&&*(DWORD*)(car+0x588)==plateTestNew&&plateTestDestroys==1);
 assert(SteamPlateSet(car,L"CANCEL",6)==0);*(DWORD*)(remote+plateOffset)=4;run();
 assert(SteamPlateStatus()==0&&plateTestCreates==2);
 DWORD kept=plateTestNew;plateTestNew=(DWORD)VirtualAlloc(nullptr,256,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);assert(plateTestNew);
 assert(SteamPlateSet(car,L"MID CALL",8)==0);plateTestCancelDuring=true;run();plateTestCancelDuring=false;
 assert(SteamPlateStatus()==7&&u32(car+0x588)==kept&&u32(kept+0x54)==1&&u32(plateTestNew+0x54)==0);
 assert(SteamPlateSet(car,L"NEW CAR",7)==0);plateTestChangeDuring=true;run();plateTestChangeDuring=false;
 assert(SteamPlateStatus()==7&&u32(car+0x588)==kept&&u32(kept+0x54)==1&&u32(plateTestNew+0x54)==0);
 *(DWORD*)(ped+0x58c)=car;
 *(DWORD*)(info+0x24)=0;assert(SteamPlateSet(car,L"NO PLATE",8)==3);
 puts("PASS: plate game-thread dispatch, private raster ownership, shared old texture reference, stale vehicle, cancellation, invalid input and failed generation.");
}
