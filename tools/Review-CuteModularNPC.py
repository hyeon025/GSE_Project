"""Verify new NPC skin/export contracts and render actual keyframe previews."""
import bpy, json, math, hashlib, sys
from pathlib import Path
from mathutils.kdtree import KDTree

ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'Art/LowPoly_AgeOfEchoes'; WORK=ROOT/'x64/perf/npc-toy-work'
rig=bpy.data.objects['RIG_NPC_Humanoid_v1']
def set_action(ob,action):
    ob.animation_data.action=action
    ob.animation_data.action_slot=action.slots[0]
assets=[o for o in bpy.data.objects if o.name.startswith('SKM_NPC_') and o.get('kind')=='npc']
report=json.loads((ART/'Catalog/cute_modular_revision.json').read_text(encoding='utf8'))

def action_hash(a):
    channels=[]
    for layer in a.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for fc in bag.fcurves:
                    channels.append((fc.data_path,fc.array_index,[(tuple(k.co),k.interpolation) for k in fc.keyframe_points]))
    return hashlib.sha256(repr(channels).encode()).hexdigest()

current={a.name:action_hash(a) for a in bpy.data.actions}
baseline=ROOT/'x64/perf/npc-toy-before/AgeOfEchoes_Characters_Rigged.blend'
if baseline.exists():
    with bpy.data.libraries.load(str(baseline),link=False) as (src,dst):
        names=tuple(src.actions);dst.actions=list(names)
    unchanged=all(current[n]==action_hash(a) for n,a in zip(names,dst.actions))
else:
    unchanged=current==report['keyframe_action_hashes']
assert unchanged,'Animation keyframes changed'
report['all_144_animation_keyframe_hashes_unchanged']=unchanged
report['keyframe_action_hashes']=current
main_rig_names=['RIG_NPC_Humanoid_v1']+['RIG_MOB_'+str(i+1).zfill(2)+'_'+name for i,name in enumerate(['Wolf','Boar','Bear','Stag','Goat','Hare','AshenRevenant'])]
bound_actions={s.action.name for name in main_rig_names for t in bpy.data.objects[name].animation_data.nla_tracks for s in t.strips}
assert len(bound_actions)==144
report['bound_animation_action_count']=len(bound_actions)
report['source_action_datablocks']=len(current)
report['legacy_unbound_action_datablocks']=sorted(set(current)-bound_actions)
report['all_source_action_keyframes_unchanged']=unchanged

samples=[]
for track in rig.animation_data.nla_tracks:track.mute=True
for clip in ['Idle','Walk','Run','Turn_L45','Turn_R45','Turn_L90','Turn_R90','WalkTurn_L45','WalkTurn_R45','RunTurn_L45','RunTurn_R45','Jump','JumpStart','JumpLoop','JumpLand','Attack','Hit','Death']:
    set_action(rig,bpy.data.actions[clip])
    end=int(rig.animation_data.action.frame_range[1]);count=0;min_z=100;max_z=-100
    for frame in sorted({1,end,max(1,end//4),max(1,end//2),max(1,3*end//4)}):
        bpy.context.scene.frame_set(frame);dg=bpy.context.evaluated_depsgraph_get()
        for o in assets:
            ev=o.evaluated_get(dg);mesh=ev.to_mesh()
            for v in mesh.vertices:
                assert all(math.isfinite(a) for a in v.co),(clip,o.name,frame)
                min_z=min(min_z,v.co.z);max_z=max(max_z,v.co.z)
            count+=len(mesh.vertices);ev.to_mesh_clear()
    samples.append({'clip':clip,'sampled_vertices':count,'z_bounds_m':[min_z,max_z]})
report['deformed_clip_checks']=samples
rig.animation_data.action=None

# Verify actual exported skinning, not just source weights/clip labels.
original_objects=set(bpy.data.objects)
bpy.ops.import_scene.gltf(filepath=str(ART/'Exports/Rigged/NPC_Resident_Animated.glb'))
imported=[o for o in bpy.data.objects if o not in original_objects]
irig=next(o for o in imported if o.type=='ARMATURE')
import_actions={}
for t in irig.animation_data.nla_tracks:
    t.mute=True
    for s in t.strips:import_actions[t.name]=s.action
if not import_actions:
    # Blender can use action names rather than NLA track names on import.
    import_actions={a.name.split('.')[0]:a for a in bpy.data.actions if a.name not in current}
roundtrips=[]
recipes=json.loads((ART/'Catalog/npc_appearance_recipes.json').read_text(encoding='utf8'))['recipes'][0]['parts']
source_parts=[o for o in assets if recipes.get(o['slot'])==o['part_index']]
for clip in ['Walk','Run','Attack','Jump','Turn_L90']:
    set_action(rig,bpy.data.actions[clip])
    action=import_actions.get(clip)
    if action is None:
        action=next(a for n,a in import_actions.items() if n.startswith(clip))
    set_action(irig,action)
    bpy.context.scene.frame_set(7);dg=bpy.context.evaluated_depsgraph_get();max_error=0;checked=0
    for src in source_parts:
        target=next(o for o in imported if o.type=='MESH' and (o.name==src.name or o.name.startswith(src.name+'.')))
        se=src.evaluated_get(dg);sm=se.to_mesh();te=target.evaluated_get(dg);tm=te.to_mesh()
        tree=KDTree(len(sm.vertices))
        for i,v in enumerate(sm.vertices):tree.insert(se.matrix_world@v.co,i)
        tree.balance()
        for v in tm.vertices:
            co,index,distance=tree.find(te.matrix_world@v.co)
            max_error=max(max_error,distance);checked+=1
        se.to_mesh_clear();te.to_mesh_clear()
    assert max_error<2e-5,(clip,max_error)
    roundtrips.append({'clip':clip,'frame':7,'imported_vertices_checked':checked,'max_position_error_m':max_error})
report['resident_glb_roundtrip']=roundtrips
for o in imported:bpy.data.objects.remove(o,do_unlink=True)
rig.animation_data.action=None

if '--checks-only' in sys.argv:
    (ART/'Catalog/cute_modular_revision.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print('CUTE_MODULAR_REVIEW_PASS',flush=True)
    raise SystemExit(0)

scene=bpy.context.scene
review_rigs=[o for o in bpy.data.collections['08_CUTE_MODULAR_REVIEW'].objects if o.type=='ARMATURE']
scene.cycles.samples=8
scene.render.resolution_x=1120;scene.render.resolution_y=587
scene.render.resolution_percentage=100
for clip in ['Walk','Run','Attack']:
    for r in review_rigs:set_action(r,bpy.data.actions[clip])
    end=int(bpy.data.actions[clip].frame_range[1]);period=end-1
    out=WORK/clip;out.mkdir(exist_ok=True)
    for i in range(12):
        scene.frame_set(1+round(i*period/12))
        scene.render.filepath=str(out/f'{i:02d}.png')
        bpy.ops.render.render(write_still=True)
    print('MOTION_RENDER_COMPLETE',clip,flush=True)
(ART/'Catalog/cute_modular_revision.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('CUTE_MODULAR_REVIEW_PASS',flush=True)
