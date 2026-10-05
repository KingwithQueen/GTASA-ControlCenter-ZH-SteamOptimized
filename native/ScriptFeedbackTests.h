static DWORD feedbackCalls=0;static char displayed[400]={};static bool cancelDuringFeedback=false;
static void __cdecl feedbackCallback(const char* text,DWORD quick,DWORD permanent,DWORD brief){
 assert(quick==1&&permanent==0&&brief==0);feedbackCalls++;strcpy_s(displayed,text);
 // A reentrant producer cannot alter the currently consumed input.
 assert(u32(remote+feedbackOffset)==3||u32(remote+feedbackOffset+feedbackStride)==3);
 if(cancelDuringFeedback){
  cancelDuringFeedback=false;assert(SteamFeedbackEnable(0)==0);
  assert(SteamFeedbackMessage(L"Unsafe replacement",18)==8);
  assert(!strcmp(text,"Cancel in flight"));
 }
}
static void scriptFeedbackTests(){
 DWORD savedPed=u32(va(0xc100d0));DWORD ped=(DWORD)VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);assert(ped);
 *(DWORD*)va(0xc100d0)=ped;*(BYTE*)va(0xc0fd39)=1;
 for(int i=0;i<6;i++)memcpy((void*)(va(0xac12d0)+gfCodePositions[i]),gfScriptSignatures[i],32);
 assert(SteamGirlfriendSupported()==1);
 for(int i=0;i<2;i++)memcpy((void*)(va(0xac12d0)+coffeePositions[i]),coffeeSignatures[i],12);
 assert(SteamCoffeeGet()==255&&SteamCoffeeSet(0)==3);
 coffeeScriptsVerified=true;*(DWORD*)va(0xac25dc)=1;
 assert(SteamCoffeeGet()==1&&SteamCoffeeSet(0)==0&&SteamCoffeeGet()==0);
 assert(SteamCoffeeSet(1)==0&&SteamCoffeeGet()==1&&SteamCoffeeSet(2)==3);
 *(BYTE*)(va(0xac12d0)+coffeePositions[0]+4)^=1;assert(SteamCoffeeGet()==255&&SteamCoffeeSet(0)==3);
 *(BYTE*)(va(0xac12d0)+coffeePositions[0]+4)^=1;
 for(DWORD i=0;i<6;i++){
  assert(SteamGirlfriendSet(i,30+i,2)==0);
  assert(u32(va(0xac12d0)+gfGlobals[i])==30+i&&u32(va(0xc0c048)+gfStats[i]*4)==30+i);
  assert(SteamGirlfriendSet(i,75+i,1)==0);
 }
 auto run=(void(__cdecl*)())(remote+0x100);*(BYTE*)va(0xc0fd39)=0;
 for(int i=0;i<6;i++){*(DWORD*)(va(0xac12d0)+gfGlobals[i])=1;*(DWORD*)(va(0xc0c048)+gfStats[i]*4)=2;}
 run();for(int i=0;i<6;i++)assert(u32(va(0xac12d0)+gfGlobals[i])==75u+i&&u32(va(0xc0c048)+gfStats[i]*4)==75u+i);
 assert(SteamGirlfriendSet(0,40,2)==2);assert(SteamGirlfriendSet(6,0,0)==3&&SteamGirlfriendSet(0,101,1)==3);
 assert(SteamCoffeeSet(0)==2&&SteamCoffeeGet()==1);
 *(DWORD*)(va(0xac12d0)+gfGlobals[0])=12;*(BYTE*)(va(0xac12d0)+gfCodePositions[0]+20)^=1;run();
 assert(u32(va(0xac12d0)+gfGlobals[0])==12&&SteamGirlfriendSupported()==0&&SteamGirlfriendSet(0,50,1)==3);
 *(BYTE*)(va(0xac12d0)+gfCodePositions[0]+20)^=1;*(DWORD*)va(0xc100d0)=ped+4;run();assert(u32(va(0xac12d0)+gfGlobals[0])==12);
 *(DWORD*)va(0xc100d0)=ped;assert(SteamGirlfriendSet(0,0,0)==0);run();assert(u32(va(0xac12d0)+gfGlobals[0])==12);
 for(int i=0;i<6;i++)assert(SteamGirlfriendSet(i,0,0)==0);
 // Test the exact compiled queue calling the same cdecl engine ABI.
 BYTE engine[10]={0x55,0x8b,0xec,0x80,0x3d};DWORD global=va(0xc35fe8);memcpy(engine+5,&global,4);
 memcpy((void*)va(0x594cb0),engine,10);assert(feedbackEngineMatched());assert(SteamFeedbackEnable(1)==0);
 BYTE jump[5]={0xe9};DWORD delta=(DWORD)feedbackCallback-va(0x594cb5);memcpy(jump+1,&delta,4);memcpy((void*)va(0x594cb0),jump,5);
 assert(SteamFeedbackMessage(L"Vehicle flipped",15)==0);assert(SteamFeedbackMessage(L"~r~Timer frozen",15)==0);
 assert(SteamFeedbackMessage(L"Full",4)==8);run();assert(feedbackCalls==2&&strcmp(displayed,"~r~Timer frozen")==0);
 run();assert(feedbackCalls==2);assert(SteamFeedbackMessage(L"",0)==3&&SteamFeedbackMessage(L"\n",1)==3);
 assert(SteamFeedbackMessage(L"\x4e2d\x6587",2)==0);run();assert(feedbackCalls==3&&!memcmp(displayed,"\xd6\xd0\xce\xc4",4));
 assert(SteamFeedbackMessage(L"Cleanup",7)==0);assert(SteamFeedbackEnable(0)==0);run();assert(feedbackCalls==3);
 memcpy((void*)va(0x594cb0),engine,10);assert(SteamFeedbackEnable(1)==0);
 memcpy((void*)va(0x594cb0),jump,5);cancelDuringFeedback=true;
 assert(SteamFeedbackMessage(L"Cancel in flight",16)==0);run();assert(feedbackCalls==4);
 assert(u32(remote+feedbackOffset)==0&&u32(remote+feedbackOffset+feedbackStride)==0);
 DWORD rc=(DWORD)VirtualAlloc(nullptr,8192,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);DWORD vehicle=0;assert(rc);
 *(DWORD*)va(0xc10180)=0;assert(currentVehicle(vehicle)&&vehicle==ped);
 *(DWORD*)(ped+0x46c)=0x100;*(DWORD*)(ped+0x58c)=rc;assert(currentVehicle(vehicle)&&vehicle==rc);
 *(DWORD*)(ped+0x46c)=0;*(DWORD*)va(0xc10180)=rc;
 for(int model:{441,464,594,501,465,564}){*(WORD*)(rc+0x22)=(WORD)model;assert(currentVehicle(vehicle)&&vehicle==rc);}
 *(WORD*)(rc+0x22)=411;assert(currentVehicle(vehicle)&&vehicle==ped);
 *(DWORD*)va(0xc10180)=1;assert(currentVehicle(vehicle)&&vehicle==ped);*(DWORD*)va(0xc10180)=0;
 *(DWORD*)va(0xc100d0)=1;assert(!currentVehicle(vehicle));
 *(DWORD*)va(0xc100d0)=savedPed;VirtualFree((void*)ped,0,MEM_RELEASE);VirtualFree((void*)rc,0,MEM_RELEASE);
 puts("SCRIPT_FEEDBACK_RC=PASS six synchronized girlfriends/locks, script and player identity guards, bounded game-thread feedback, GBK text, cancellation and six RC models");
}
