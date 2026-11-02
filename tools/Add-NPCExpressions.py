"""Add shared facial morphs, preserve the 37 meshes, and publish named GLBs.

Blender -b Art/.../AgeOfEchoes_Characters_Rigged.blend --python this-file
Also callable via add_expressions(objects) by the modular character authoring tool.
"""
import bpy, json, math, struct
from pathlib import Path

EXPRESSIONS=['Gentle','Neutral','Determined','Concerned','Sleepy','Cheerful']

def add_expressions(objects):
    for obj in objects:
        if obj.get('slot')!='Head':continue
        mesh=obj.data
        if mesh.shape_keys:
            obj.shape_key_clear()
        base=[v.co.copy() for v in mesh.vertices]
        vertices_by_material={}
        for face in mesh.polygons:
            name=mesh.materials[face.material_index].name
            vertices_by_material.setdefault(name,set()).update(face.vertices)
        eyes=vertices_by_material.get('MAT_AOE_NPC_Eye',set()) | vertices_by_material.get('MAT_AOE_NPC_Glint',set())
        brows=vertices_by_material.get('MAT_AOE_NPC_Hair',set())
        mouth=vertices_by_material.get('MAT_AOE_NPC_Mouth',set())
        assert eyes and brows and mouth,(obj.name,'facial feature materials missing')
        obj.shape_key_add(name='Basis',from_mix=False)
        for expression in EXPRESSIONS[1:]:
            key=obj.shape_key_add(name='Expression_'+expression,from_mix=False)
            key.value=0
            for i,co in enumerate(base):
                p=co.copy()
                if i in eyes:
                    factor={'Neutral':1,'Determined':.82,'Concerned':.94,'Sleepy':.43,'Cheerful':.78}[expression]
                    p.z=1.536+(co.z-1.536)*factor
                if i in brows:
                    dx=abs(co.x)-.119
                    if expression=='Determined':p.z+=.40*dx-.004
                    elif expression=='Concerned':p.z-=.38*dx-.006
                    elif expression=='Sleepy':p.z-=.014
                    elif expression=='Cheerful':p.z+=.009
                if i in mouth:
                    t=min(1.2,abs(co.x)/.065)**1.5
                    p.z+={'Neutral':-.019*t+.010,'Determined':-.022*t+.004,
                          'Concerned':-.042*t+.020,'Sleepy':-.019*t+.010,
                          'Cheerful':.013*t-.009}[expression]
                    if expression=='Cheerful':p.x*=1.07
                key.data[i].co=p
        obj['expression_schema_version']=1
        obj['expression_names']=json.dumps(EXPRESSIONS)
        assert len(mesh.shape_keys.key_blocks)==6

def export_glb(path,objects,rig=None):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objects:o.hide_set(False);o.hide_viewport=False;o.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]
    states=[]
    if rig:
        rig.animation_data.action=None
        for track in rig.animation_data.nla_tracks:states.append((track,track.mute));track.mute=False
    bpy.context.scene.frame_set(1)
    bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,export_extras=True,
        export_animations=bool(rig),export_animation_mode='NLA_TRACKS',export_force_sampling=True,
        export_frame_range=False,export_skins=bool(rig),export_def_bones=False,
        export_morph=True,export_morph_normal=False,export_morph_tangent=False,export_morph_animation=False,
        export_yup=True,export_cameras=False,export_lights=False,export_apply=False)
    for track,mute in states:track.mute=mute
    data=Path(path).read_bytes();size=struct.unpack_from('<I',data,12)[0];doc=json.loads(data[20:20+size])
    if rig:assert len(doc['animations'])==18 and len(doc['skins'])==1
    heads=[m for m in doc['meshes'] if 'NPC_Head_' in m.get('name','')]
    for m in heads:
        assert m['extras']['targetNames']==['Expression_'+n for n in EXPRESSIONS[1:]]
        assert all(len(p['targets'])==5 for p in m['primitives'])

def publish():
    root=Path(__file__).resolve().parents[1];art=root/'Art/LowPoly_AgeOfEchoes'
    objects=[o for o in bpy.data.objects if o.name.startswith('SKM_NPC_') and o.get('kind')=='npc']
    assert len(objects)==37
    add_expressions(objects)
    heads=[o for o in objects if o['slot']=='Head']
    rig=bpy.data.objects['RIG_NPC_Humanoid_v1']
    bpy.context.preferences.filepaths.save_version=0
    bpy.ops.wm.save_as_mainfile(filepath=str(art/'AgeOfEchoes_Characters_Rigged.blend'))
    export_glb(art/'Exports/Rigged/NPC_Modular_Rigged.glb',[rig]+objects,rig)
    recipes=json.loads((art/'Catalog/npc_appearance_recipes.json').read_text(encoding='utf8'))['recipes']
    recipe=recipes[0]['parts']
    export_glb(art/'Exports/Rigged/NPC_Resident_Animated.glb',[rig]+[o for o in objects if recipe.get(o['slot'])==o['part_index']],rig)
    revenant=bpy.data.objects['RIG_MOB_07_AshenRevenant']
    reused=[o for o in bpy.data.objects if o.type=='MESH' and o.name.startswith('SKM_MOB_07_AshenRevenant__')]
    export_glb(art/'Exports/Rigged/MOB_07_AshenRevenant.glb',[revenant]+reused,revenant)
    # Static heads get the same targets; the body geometry/materials stay shared.
    bpy.ops.wm.open_mainfile(filepath=str(art/'AgeOfEchoes_LowPoly_Modular.blend'))
    static_heads=[bpy.data.objects['SM_NPC_Head_0'+str(i)+'_'+name] for i,name in [(1,'Masculine'),(2,'Feminine')]]
    add_expressions(static_heads)
    for o in static_heads:export_glb(art/'Exports/NPC_Parts'/(o.name+'.glb'),[o])
    catalog=json.loads((art/'Catalog/mesh_catalog.json').read_text(encoding='utf8'))
    all_parts=[bpy.data.objects[a['asset_id']] for a in catalog['assets'] if a['kind']=='npc']
    export_glb(art/'Exports/NPC_Modular_Library.glb',all_parts)
    bpy.ops.wm.save_as_mainfile(filepath=str(art/'AgeOfEchoes_LowPoly_Modular.blend'))
    for a in catalog['assets']:
        if a['kind']=='npc' and a['slot']=='Head':a['expression_targets']=['Expression_'+n for n in EXPRESSIONS[1:]]
    (art/'Catalog/mesh_catalog.json').write_text(json.dumps(catalog,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print('NPC_EXPRESSION_EXPORT_PASS',flush=True)

if __name__=='__main__':publish()
