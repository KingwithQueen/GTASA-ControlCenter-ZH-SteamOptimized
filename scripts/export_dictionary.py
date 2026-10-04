"""Export the exact latest UTF-8 dictionary; no source rewriting."""
from pathlib import Path
import argparse
ROOT=Path(__file__).resolve().parents[1]
def export(destination):
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
