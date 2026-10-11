import json,re,sys,difflib,collections
S=sys.argv[1]
hits=json.load(open(S+'/table_hits.json'))
src=open('vendor/flarial/src/Utils/Memory/Game/Sig/SigInit.cpp',encoding='utf-8').read()
hist=collections.defaultdict(list)
for m in re.finditer(r'ADD_SIG\("([^"]+)",\s*"([^"]+)"\)',src): hist[m.group(1)].append(m.group(2))
# effective (newest) set
blocks=re.split(r"void SigInit::init\w+\(\)\s*\{",src)[1:]
eff={}
for block in reversed(blocks):
    for m in re.finditer(r'(ADD_SIG|DEPRECATE_SIG)\("([^"\n]+)"(?:,\s*"([^"\n]+)")?\)',block):
        k,n,p=m.groups()
        if k=="DEPRECATE_SIG": eff.pop(n,None)
        else: eff[n]=p
norm=lambda p:['?' if '?' in t else t for t in p.split()]
uniq=[h for h in hits if len(h["hits"])==1]
exact={}
for h in uniq:
    for n,ps in hist.items():
        if h["pattern"] in ps: exact.setdefault(n,[]).append(h)
result={}
for n in eff:
    if n in exact:
        # prefer the newest table entry among exact matches
        h=max(exact[n],key=lambda x:x["i"]); result[n]={"rva":h["hits"][0],"pattern":h["pattern"],"how":"exact"}
used={r["pattern"] for r in result.values()}
cands=[h for h in uniq if h["i"]>=683 and h["pattern"] not in used]
scores=[]
for n in eff:
    if n in result: continue
    for h in cands:
        s=max(difflib.SequenceMatcher(None,norm(p),norm(h["pattern"])).ratio() for p in hist[n])
        scores.append((s,n,h["i"]))
scores.sort(reverse=True)
taken=set()
byi={h["i"]:h for h in cands}
for s,n,i in scores:
    if n in result or i in taken or s<0.55: continue
    result[n]={"rva":byi[i]["hits"][0],"pattern":byi[i]["pattern"],"how":f"similar {s:.2f}"}
    taken.add(i)
json.dump(result,open(S+'/named.json','w'),indent=1)
print(len(eff),'needed,',sum(r["how"]=="exact" for r in result.values()),'exact,',sum(r["how"]!="exact" for r in result.values()),'by similarity,',len(eff)-len(result),'missing')
print('missing:',sorted(set(eff)-set(result)))
print('leftover new unique unpaired:',len(cands)-len(taken))
