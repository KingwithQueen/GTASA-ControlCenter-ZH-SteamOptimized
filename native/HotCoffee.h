// The flag is read by streamed gf_date/gf_sex scripts in script.img, rather
// than main.scm. Pin the package and verify loaded main-script definitions.
static constexpr DWORD coffeePositions[2]={0x226e4,0x25afc};
static constexpr BYTE coffeeSignatures[2][12]={
 {0x04,0,0x02,0x0c,0x13,0x04,1,0x4f,0,1,0xf5,0x26},
 {0x04,0,0x02,0x0c,0x13,0x04,0,0x1b,0x09,0x51,0,0xd1}
};
static bool coffeeSupported(){
 if(!coffeeScriptsVerified||!girlfriendSupported())return false;
 for(int i=0;i<2;i++){BYTE actual[12]={};if(!read(va(0xac12d0)+coffeePositions[i],actual,12)||memcmp(actual,coffeeSignatures[i],12))return false;}
 return true;
}
extern "C" DWORD __stdcall SteamCoffeeGet(){
 if(!coffeeSupported())return 255;DWORD value=255;
 return read(va(0xac25dc),&value,4)&&value<=1?value:255;
}
extern "C" DWORD __stdcall SteamCoffeeSet(DWORD censored){
 if(!game||!base)return 1;if(censored>1||!coffeeSupported())return 3;if(!featurePause())return 2;
 DWORD before=SteamCoffeeGet(),after=255;if(before>1)return 3;
 bool ok=write(va(0xac25dc),&censored,4);
 if(!read(va(0xac25dc),&after,4))return 6;
 if(ok&&after==censored)return 0;
 if(!coffeeSupported()||!featurePause()||(after!=before&&after!=censored))return 6;
 if(!write(va(0xac25dc),&before,4)||!read(va(0xac25dc),&after,4)||after!=before)return 6;return 5;
}
