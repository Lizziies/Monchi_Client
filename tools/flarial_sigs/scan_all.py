import json,re,sys
S=sys.argv[1]
meta=json.load(open(S+"/mc.bin.json")); img=open(S+"/mc.bin","rb").read()
text=next(s for s in meta["sections"] if s["name"]==".text")
code=img[text["rva"]:text["rva"]+text["size"]]
table=json.load(open(S+'/table.json'))
def find(tok,limit=2):
    expr=b"".join(b"." if t is None else re.escape(bytes([t])) for t in tok)
    runs,cur=[],None
    for i,t in enumerate(tok):
        if t is None: cur=None; continue
        if cur is None: cur=[i,bytearray()]; runs.append(cur)
        cur[1].append(t)
    if not runs: return []
    off,anchor=max(runs,key=lambda r:len(r[1])); anchor=bytes(anchor)
    rx=re.compile(expr,re.DOTALL); found=[]; p=code.find(anchor)
    while p>=0:
        b0=p-off
        if b0>=0 and rx.match(code,b0):
            found.append(text["rva"]+b0)
            if len(found)>=limit: break
        p=code.find(anchor,p+1)
    return found
out=[]
for i,p in enumerate(table):
    tok=[None if "?" in t else int(t,16) for t in p.split()]
    h=find(tok)
    out.append({"i":i,"pattern":p,"hits":h})
json.dump(out,open(S+'/table_hits.json','w'))
print(sum(len(o["hits"])==1 for o in out),"unique of",len(out),"; new block 683+:",sum(len(o["hits"])==1 for o in out[683:]),"of",len(out)-683)
