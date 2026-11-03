"""Executed by Build-CuteModularNPC.py after the authoring/review stage."""
import struct, shutil

CAT=ART/'Catalog'
def read_json(name):return json.loads((CAT/name).read_text(encoding='utf8'))
def write_json(name,value):(CAT/name).write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
def glb_json(path):
    with open(path,'rb') as f:
        magic,version,total=struct.unpack('<III',f.read(12));assert magic==0x46546C67 and version==2
        length,kind=struct.unpack('<II',f.read(8));assert kind==0x4E4F534A
        return json.loads(f.read(length))

def export(path,objects,armature=None):
    bpy.ops.object.select_all(action='DESELECT')
    for ob in objects:
        ob.hide_set(False);ob.hide_viewport=False;ob.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]
    tracks=[]
    if armature:
        armature.animation_data.action=None
        for t in armature.animation_data.nla_tracks:
            tracks.append((t,t.mute));t.mute=False
        for b in armature.pose.bones:b.matrix_basis=Matrix.Identity(4)
    bpy.context.scene.frame_set(1)
    bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,
        export_extras=True,export_animations=bool(armature),export_animation_mode='NLA_TRACKS',
        export_force_sampling=True,export_frame_range=False,export_def_bones=False,
        export_skins=bool(armature),export_yup=True,export_cameras=False,export_lights=False,
        export_materials='EXPORT',export_apply=False,export_morph=True,export_morph_normal=False,
        export_morph_tangent=False,export_morph_animation=False)
    for t,mute in tracks:t.mute=mute
    if armature:
        js=glb_json(path)
        actual={a['name'] for a in js.get('animations',[])}
        expected={t.name for t in armature.animation_data.nla_tracks}
        assert actual==expected,(path,actual,expected)
        assert len(js.get('skins',[]))==1,(path,'skins')
        print('ANIMATED_GLB_VERIFIED',path.name,len(actual),flush=True)

review_path=ART/'Previews/Rigged/08_Cute_Modular_NPC_Lineup.png'
shutil.copy2(WORK/'Cute_Modular_Lineup.png',review_path)
# Keep all pre-existing assets and preview rigs; new review copies share the meshes.
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'AgeOfEchoes_Characters_Rigged.blend'))
export(ART/'Exports/Rigged/NPC_Modular_Rigged.glb',[rig]+assets,rig)
recipes=read_json('npc_appearance_recipes.json')['recipes']
resident=recipes[0]['parts']
selected=[o for o in assets if resident.get(o['slot'])==o['part_index']]
export(ART/'Exports/Rigged/NPC_Resident_Animated.glb',[rig]+selected,rig)
revenant=bpy.data.objects['RIG_MOB_07_AshenRevenant']
reused=[o for o in bpy.data.objects if o.type=='MESH' and o.name.startswith('SKM_MOB_07_AshenRevenant__')]
assert len(reused)==7
export(ART/'Exports/Rigged/MOB_07_AshenRevenant.glb',[revenant]+reused,revenant)

palette=read_json('material_palette.json')
existing={r['material_id']:r for r in palette['materials']}
for m in M.values():
    if m.name.endswith('ReviewFloor'):continue
    bs=m.node_tree.nodes.get('Principled BSDF')
    existing[m.name]={'material_id':m.name,'palette_key':m.name.removeprefix('MAT_AOE_'),
        'base_color_linear_rgba':list(bs.inputs['Base Color'].default_value),
        'roughness':bs.inputs['Roughness'].default_value,'metallic':bs.inputs['Metallic'].default_value,
        'emission_linear_rgba':list(bs.inputs['Emission Color'].default_value),
        'emission_strength':bs.inputs['Emission Strength'].default_value,'double_sided':False}
palette['materials']=list(existing.values());write_json('material_palette.json',palette)

# Append only asset objects from the approved rigged source into the static source.
rigged_path=ART/'AgeOfEchoes_Characters_Rigged.blend'
names=[o.name for o in assets]
new_meshes={o['source_asset_id']:o.data.name for o in assets}
bpy.ops.wm.open_mainfile(filepath=str(ART/'AgeOfEchoes_LowPoly_Modular.blend'))
with bpy.data.libraries.load(str(rigged_path),link=False) as (src,dst):dst.meshes=list(new_meshes.values())
loaded={m.name:m for m in dst.meshes}
for asset_id,mesh_name in new_meshes.items():
    # Names may acquire .001 if a former preview used the same name.
    match=next(m for m in dst.meshes if m.name==mesh_name or m.name.startswith(mesh_name+'.'))
    old=bpy.data.objects[asset_id];oldmesh=old.data
    users=[o for o in bpy.data.objects if o.type=='MESH' and o.data==oldmesh]
    for o in users:
        o.data=match;o.vertex_groups.clear();o['quality_revision']=REV
        for slot in o.material_slots:slot.link='DATA'
    if oldmesh.users==0:bpy.data.meshes.remove(oldmesh)
    match.name=asset_id

catalog=read_json('mesh_catalog.json')
static_assets=[bpy.data.objects[a['asset_id']] for a in catalog['assets'] if a['kind']=='npc']
for o in static_assets:
    assert o.location.length<1e-6 and all(abs(s-1)<1e-6 for s in o.scale)
    o.data.calc_loop_triangles()
for a in catalog['assets']:
    if a['kind']!='npc':continue
    o=bpy.data.objects[a['asset_id']];mesh=o.data
    a.update(vertices=len(mesh.vertices),triangles=len(mesh.loop_triangles),
        bounds_m=[[round(fn(v.co[i] for v in mesh.vertices),5) for i in range(3)] for fn in [min,max]],
        materials=[m.name for m in mesh.materials],quality_revision=REV)
    export(ART/a['glb_file'],[o])
export(ART/'Exports/NPC_Modular_Library.glb',static_assets)
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'AgeOfEchoes_LowPoly_Modular.blend'))
write_json('mesh_catalog.json',catalog)
creatures=read_json('creature_catalog.json')
aliases=read_json('shared_geometry_aliases.json')['aliases']
triangles_by_id={r['asset_id']:r['triangles'] for r in report['parts']}
for creature in creatures['species']:
    if creature['species_index']==7:
        creature['mesh_triangles']=sum(triangles_by_id[a['source_mesh'].replace('SKM_','SM_',1)] for a in aliases)
        creature['quality_revision']=REV
write_json('creature_catalog.json',creatures)
with (CAT/'mesh_catalog.csv').open('w',encoding='utf8',newline='') as f:
    keys=['asset_id','kind','slot','index','label_ko','vertices','triangles','glb_file']
    writer=csv.DictWriter(f,fieldnames=keys,extrasaction='ignore');writer.writeheader();writer.writerows(catalog['assets'])
counts={(a['slot'],a['index']):a['triangles'] for a in catalog['assets'] if a['kind']=='npc'}
recipe_counts=[{'appearance_id':r['appearance_id'],'triangles':sum(counts[(s,i)] for s,i in r['parts'].items() if i)} for r in recipes]
stats=read_json('asset_statistics.json')
before_counts=read_json('cute_modular_revision.json')['previous_recipe_triangles'] if (CAT/'cute_modular_revision.json').exists() else stats['npc_triangles_by_recipe']
stats.update(npc_triangles_by_recipe=recipe_counts,character_quality_revision=REV)
stats['rigging_extension']['character_quality_revision']=REV
write_json('asset_statistics.json',stats)
rs=read_json('rigging_statistics.json');rs['character_quality_revision']=REV;write_json('rigging_statistics.json',rs)
validation=read_json('validation_report.json')
validation.update(npc_triangles_min=min(r['triangles'] for r in recipe_counts),npc_triangles_max=max(r['triangles'] for r in recipe_counts),character_quality_revision=REV)
by_id={r['asset_id']:r for r in report['parts']}
for item in validation['meshes']:
    if item['asset_id'] in by_id:
        item.update(closed_components=by_id[item['asset_id']]['closed_components'],zero_area_faces=by_id[item['asset_id']]['zero_area_triangles'])
        item.pop('surface_note',None)
write_json('validation_report.json',validation)
report.update(date='2026-11-01',approved_concept='References/Approved_Cute_Modular_Concept.png',
    npc_triangles_min=min(r['triangles'] for r in recipe_counts),npc_triangles_max=max(r['triangles'] for r in recipe_counts),
    previous_recipe_triangles=before_counts,current_recipe_triangles=recipe_counts,
    source_axes={'up':'+Z','front':'-Y'},npc_height_m=1.8,
    exports_updated=['Exports/Rigged/NPC_Modular_Rigged.glb','Exports/Rigged/NPC_Resident_Animated.glb','Exports/Rigged/MOB_07_AshenRevenant.glb','Exports/NPC_Modular_Library.glb']+[a['glb_file'] for a in catalog['assets'] if a['kind']=='npc'],
    unchanged_contracts=['37 part IDs and indices','40 appearance recipes','21 bind bones','18 humanoid keyframe actions','6 quadruped meshes, rigs and exports','environment meshes and buildings'],
    render_notes='Closed NPC solids use single-sided materials. Unneeded rear faces are culled by the runtime; no textures or new slots.')
write_json('cute_modular_revision.json',report)
print('CUTE_NPC_PUBLICATION_COMPLETE',flush=True)
