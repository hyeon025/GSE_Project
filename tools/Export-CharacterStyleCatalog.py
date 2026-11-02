"""Export the C++ palette source and stable-ID policy for downstream asset work."""
from pathlib import Path
import re,json
root=Path(__file__).resolve().parents[1]
text=(root/'SimpleGame/CharacterStyle.h').read_text(encoding='utf8')
def colors(name):
    block=re.search(r'static const Rgb '+name+r'\[[^\]]+\]=\{(.*?)\};',text,re.S).group(1)
    values=[float(v) for v in re.findall(r'(\.\d+)f',block)]
    return [values[i:i+3] for i in range(0,len(values),3)]
catalog={'version':1,'date':'2026-11-02','color_space':'sRGB; runtime converts to linear',
    'skin_colors':colors('skin'),'hair_colors':colors('hair'),'cloth_colors':colors('cloth'),'trim_colors':colors('trim'),
    'expressions':[{'index':i,'name':name,'morph_target':None if i==0 else 'Expression_'+name} for i,name in enumerate(['Gentle','Neutral','Determined','Concerned','Sleepy','Cheerful'])],
    'player':{'skin':1,'hair':7,'cloth':0,'expression':2,'parts':{'Head':1,'Hair':5,'Torso':1,'Arms':1,'Legs':1,'Feet':1,'Back':1,'Belt':2}},
    'npc_policy':{'period':2880,'unique_color_expression_combinations_per_period':2880,'reserved_player_cloth_index':0,
       'algorithm':'serial=((max(id,1)-1)*137)%2880; cloth=1+serial%12; serial/=12; hair=serial%8; serial/=8; skin=serial%5; serial/=5; expression=(serial+cloth*5+hair*3+skin*2)%6; all divisions are integer',
       'saved_fields':'artCatalogVersion=2; artPalettes=[skin,hair,cloth]; artParts retains existing IDs',
       'expression_persistence':'derived from the persistent NPC ID; independent of frame, job and position'},
    'mesh_policy':'37 existing shared part meshes, no new meshes for colors or expressions; two head meshes each have five relative morph targets',
    'runtime_policy':'Four linear RGB palette colors are instance attributes. Color indices are excluded from pose cache keys; only expression affects geometry.'}
assert list(map(len,[catalog['skin_colors'],catalog['hair_colors'],catalog['cloth_colors'],catalog['trim_colors']]))==[5,8,13,13]
dest=root/'Art/LowPoly_AgeOfEchoes/Catalog/character_style_catalog.json'
dest.write_text(json.dumps(catalog,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print(dest)
