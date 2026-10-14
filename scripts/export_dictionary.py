"""Export the exact latest UTF-8 dictionary; no source rewriting."""
from pathlib import Path
import argparse,re
ROOT=Path(__file__).resolve().parents[1]
def export(destination):
    # Keep legacy designer literals ASCII; Unicode comes from the UTF-8 dictionary.
    for source in (ROOT/'GTASAConsole').iterdir():
        if source.suffix.lower() not in ('.frm','.ctl'):continue
        for number,line in enumerate(source.read_bytes().splitlines(),1):
            match=re.match(rb'^\s*(?:Caption|ToolTipText|ButtonCaption|Text)\s*=\s*"([^"]*)"',line)
            if match and any(byte>=128 for byte in match[1]):
                raise ValueError(f'{source.name}:{number}: non-ASCII designer text; use an ASCII dictionary key')
    data=(ROOT/'localization/zh-CN.master.tsv').read_bytes()
    seen=set()
    for i,line in enumerate(data.decode('utf-8-sig').splitlines(),1):
        if not line:continue
        parts=line.split('\t')
        if len(parts)!=2 or not parts[0] or parts[0] in seen:
            raise ValueError(f'Invalid/duplicate dictionary row {i}')
        seen.add(parts[0])
    destination=Path(destination);destination.parent.mkdir(parents=True,exist_ok=True)
    destination.write_bytes(data)
    return len(seen)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=ROOT/'GTASAConsole/zh-CN.tsv')
    a=p.parse_args();print(f'Exported {export(a.output)} dictionary entries')
