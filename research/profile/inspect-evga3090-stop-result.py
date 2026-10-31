# GPL-2.0-only. Offline EVGA05 before/after evidence inspection; no hardware access.
import argparse
import hashlib
import json
import re
from pathlib import Path

GOP = "9042a9de-23dc-4a38-96fb-7aded080516a"
TEXT = "387477c2-69c7-11d2-8e39-00a0c969723b"

def graph(part):
    result = {}
    handle = protocol = None
    for line in part.splitlines():
        if line.startswith("handle id="):
            handle = line[10:]
            if handle in result:
                raise ValueError("duplicate handle")
            result[handle] = {}
        elif line.startswith("protocol guid=") and handle:
            protocol = line[14:]
            if protocol in result[handle]:
                raise ValueError("duplicate protocol")
            result[handle][protocol] = []
        elif line.startswith("open agent=") and handle and protocol:
            m = re.fullmatch(r"open agent=(0x[0-9a-f]{16}) controller=(0x[0-9a-f]{16}) attributes=(0x[0-9a-f]{16}) count=(0x[0-9a-f]{16})", line)
            if not m:
                raise ValueError("malformed open")
            a,c,attrs,n = m.groups()
            if int(attrs,16) not in (1,2,4):
                result[handle][protocol].append((a,c,int(attrs,16),int(n,16)))
    return result

def inspect(raw):
    s = raw.decode("utf-8").replace("\r\n","\n")
    if len(raw)>4*1024*1024 or "incomplete reason=" in s:
        raise ValueError("oversized or incomplete capture")
    events = ["evga-text-stop-before", "evga-text-stop-return", "evga-text-stop-post-graph", "evga-text-stop-capture-after", "evga-text-stop-ready-windows"]
    positions=[]
    for e in events:
        token=e+" status=0x0000000000000000\n"
        if s.count(token)!=1:
            raise ValueError("missing, repeated or failed phase: "+e)
        positions.append(s.index(token))
    if positions!=sorted(positions) or s.count("capture-complete=1\n")!=2:
        raise ValueError("phase order or capture count")
    parts=s.split("gpu-console-graph-v1")
    if len(parts)!=3:
        raise ValueError("expected two graphs")
    before,after=map(graph,parts[1:])
    if set(before)!=set(after):
        raise ValueError("handle membership changed")
    gops={h for h,p in before.items() if GOP in p}
    if len(gops)!=2 or gops!={h for h,p in after.items() if GOP in p}:
        raise ValueError("GOP inventory changed")
    removed=[];added=[];changes=[]
    for h in before:
        for p in before[h].keys()-after[h].keys(): removed.append(p)
        for p in after[h].keys()-before[h].keys(): added.append(p)
        for p in before[h].keys() & after[h].keys():
            a,b=sorted(before[h][p]),sorted(after[h][p])
            if a!=b: changes.append((h,p,a,b))
    if added or len(changes)!=1:
        raise ValueError("unexpected active edge or protocol addition")
    h,p,a,b=changes[0]
    if h not in gops or p!=TEXT or b or len(a)!=2 or sorted(x[2] for x in a)!=[8,16] or any(x[3]!=1 for x in a) or a[0][0]!=a[1][0]:
        raise ValueError("not the expected single text membership removal")
    return {"test":"offline-evga-one-membership-result-v1","sha256":hashlib.sha256(raw).hexdigest(),"bytes":len(raw),"both_captures_complete":True,"selected_active_opens_removed":2,"gop_handles_preserved":2,"all_other_active_opens_preserved":True,"protocol_guids_removed":sorted(removed),"protocol_guids_added":added,"hardware_accessed":False,"unlock_proven":False,"windows_return":"requires operator report; log proves ready only"}

if __name__=="__main__":
    ap=argparse.ArgumentParser();ap.add_argument("capture",type=Path);args=ap.parse_args()
    try: print(json.dumps(inspect(args.capture.read_bytes()),indent=2))
    except (OSError,ValueError) as e: ap.exit(1,str(e)+"\n")
