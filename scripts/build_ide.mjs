// Compiler-only CDP driver. Never runs the trainer or uses an existing IDE.
import fs from 'node:fs';import path from 'node:path';
const [project,port,reportPath]=process.argv.slice(2);
const pause=ms=>new Promise(r=>setTimeout(r,ms));let page;
for(let i=0;i<120;i++){try{const pages=await(await fetch('http://127.0.0.1:'+port+'/json/list')).json();page=pages.find(x=>x.url.startsWith('http://localhost:')&&x.url.endsWith('/main.htm'));if(page)break;}catch{}await pause(500);}
if(!page)throw Error('Dedicated compiler page unavailable');
const ws=new WebSocket(page.webSocketDebuggerUrl);await new Promise((r,j)=>{ws.onopen=r;ws.onerror=j});
let id=0;const waiters=new Map();ws.onmessage=e=>{const m=JSON.parse(e.data);if(waiters.has(m.id)){waiters.get(m.id)(m);waiters.delete(m.id)}};
async function evaluate(expression){const n=++id;const p=new Promise(r=>waiters.set(n,r));ws.send(JSON.stringify({id:n,method:'Runtime.evaluate',params:{expression,returnByValue:true,awaitPromise:true}}));const r=await Promise.race([p,pause(20000).then(()=>{throw Error('Compiler request timeout')})]);if(r.result?.exceptionDetails)throw Error(JSON.stringify(r.result.exceptionDetails));return r.result?.result?.value;}
const report={trainerLaunched:false,privateCompiler:true};
try{
 let state='';
 for(let i=0;i<160;i++){state=await evaluate('document.body?.innerText ?? ""');if(state?.includes('IMPORT FINISHED')&&state.includes('MAIN_THREAD [IDLE]'))break;await pause(500);}
 report.importState=state;
 if(!state?.includes('IMPORT FINISHED WITH 0 ERRORS/WARNINGS'))throw Error('VBP import did not settle cleanly');
 await evaluate('new Promise(resolve=>root.saveProjectAs('+JSON.stringify(path.join(project,'GTASAConsole.twinproj').replaceAll('\\','/'))+',()=>resolve("saved")))');
 await evaluate('executeIdeCommand("tbBuild_Start"); "requested"');
 let success=false;
 for(let i=0;i<160;i++){state=await evaluate('document.body?.innerText ?? ""');if(state.includes('[LINKER] SUCCESS')){success=true;break;}await pause(500);}
 report.buildState=state;report.success=success;
 if(!success)throw Error('Compiler did not link successfully');
 report.bytes=fs.statSync(path.join(project,'GTASA_Center_ZH.exe')).size;
 console.log(JSON.stringify({success:true,bytes:report.bytes,trainerLaunched:false}));
}catch(e){report.error=String(e);process.exitCode=1;console.error(e)}
finally{fs.writeFileSync(reportPath,JSON.stringify(report,null,2));ws.close();}
