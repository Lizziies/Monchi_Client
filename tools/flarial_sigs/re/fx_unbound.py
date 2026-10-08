import json,re,glob
src=open('dll/src/sdk/Effects.cpp',encoding='utf8').read()
hdr=open('dll/src/sdk/Effects.hpp',encoding='utf8').read()
body=hdr[hdr.index('enum class Id'):]
body=body[body.index('{')+1:body.index('Count')]
ids=[x.strip() for x in body.replace('\n',' ').split(',') if x.strip()]
tab=re.findall(r'\{"(fx\.[A-Za-z0-9]+)",\s*"([^"]*)",\s*Kind::(\w+)\}',src)
print(len(ids),len(tab))
sj=json.load(open('sigs/1.26.52.json',encoding='utf8'))
sig=set(sj['sigs']); off=sj['offsets']
def bound(s,kind):
    if s in sig: return 'sig'
    if off.get('has.'+s,0)>0: return 'has'
    if (s+'.option') in off: return 'option'
    if kind=='Data' and any(k.startswith(s) for k in off): return 'data'
    return ''
n=min(len(ids),len(tab))
state={ids[i]:(tab[i][0],tab[i][2],bound(tab[i][0],tab[i][2])) for i in range(n)}
for i in range(n):
    if ids[i].lower()!=tab[i][0][3:].lower(): print('order differs at',i,ids[i],tab[i][0]); break
print('bound:',sorted(k for k,v in state.items() if v[2]))
use={}
for f in glob.glob('dll/src/modules/**/*.hpp',recursive=True)+glob.glob('dll/src/modules/**/*.cpp',recursive=True):
    t=open(f,encoding='utf8',errors='replace').read()
    for m in re.finditer(r'\nclass (\w+)[^{;]*\{',t):
        nxt=re.search(r'\nclass \w+[^{;]*\{',t[m.end():]); end=m.end()+(nxt.start() if nxt else len(t)-m.end())
        b=t[m.start():end]
        nm=re.search(r'(?:Module|HudModule|GameText|StickyKey)\(\s*"([^"]+)"',b)
        if not nm: continue
        for x in set(re.findall(r'fx::Id::(\w+)',b)): use.setdefault(nm.group(1),set()).add(x)
bad={}
for mod,xs in sorted(use.items()):
    miss=sorted(x for x in xs if x in state and not state[x][2]); ok=sorted(x for x in xs if x in state and state[x][2])
    if miss: bad[mod]=(miss,ok)
print(len(use),'modules use effects;',len(bad),'ask for at least one that is not bound:')
for k,(m,o) in bad.items(): print(f'  {k}: NOT BOUND {", ".join(m)}'+(f' | bound {", ".join(o)}' if o else ' | NOTHING bound'))
