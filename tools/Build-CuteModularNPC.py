"""Blender 5.2 authoring tool for the approved cute, modular NPC revision.

Run with Blender -b <rigged.blend> --python tools/Build-CuteModularNPC.py.
Use -- --publish to update the source, static library, GLBs and catalogs after
review. The default writes a review .blend and renders without publishing.
All 37 existing part IDs, bind bones and keyframe actions are retained.
"""
import bpy, bmesh, math, json, csv, sys, hashlib
from pathlib import Path
from mathutils import Vector, Matrix

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'Art/LowPoly_AgeOfEchoes'
WORK = ROOT / 'x64/perf/npc-toy-work'
WORK.mkdir(parents=True, exist_ok=True)
PUBLISH = '--publish' in sys.argv
REV = 4

def linear(v):
    return v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4

def material(key, rgb, metal=0, rough=.78):
    name = 'MAT_AOE_NPC_' + key
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    rgba = (*[linear(v) for v in rgb], 1)
    m.diffuse_color = rgba
    m.use_nodes = True
    bs = m.node_tree.nodes.get('Principled BSDF')
    bs.inputs['Base Color'].default_value = rgba
    bs.inputs['Roughness'].default_value = rough
    bs.inputs['Metallic'].default_value = metal
    m.use_backface_culling = True
    return m

M = {
    'Skin': material('Skin', (.86,.63,.43)),
    'Blush': material('Blush', (.81,.45,.31)),
    'Ear': material('Ear', (.71,.39,.27)),
    'Eye': material('Eye', (.105,.068,.052), rough=.45),
    'Ivory': material('Ivory', (.96,.88,.72)),
    'Glint': material('Glint', (1,.98,.91), rough=.3),
    'Mouth': material('Mouth', (.43,.20,.12)),
    'Hair': material('Hair', (.29,.135,.068)),
    'HairLight': material('HairLight', (.38,.19,.092)),
    'Teal': material('Teal', (.26,.43,.46)),
    'TealTrim': material('TealTrim', (.40,.56,.55)),
    'Linen': material('Linen', (.83,.76,.59)),
    'LinenShade': material('LinenShade', (.66,.59,.44)),
    'Rust': material('Rust', (.62,.30,.18)),
    'Moss': material('Moss', (.34,.43,.22)),
    'MossLight': material('MossLight', (.47,.53,.29)),
    'Wine': material('Wine', (.45,.19,.23)),
    'Leather': material('Leather', (.30,.20,.135)),
    'LeatherLight': material('LeatherLight', (.46,.31,.18)),
    'Sole': material('Sole', (.20,.145,.108)),
    'Trousers': material('Trousers', (.30,.29,.245)),
    'Iron': material('Iron', (.49,.54,.55), .55,.65),
    'IronEdge': material('IronEdge', (.65,.68,.65),.5,.6),
    'IronDark': material('IronDark', (.27,.32,.34),.4),
    'Brass': material('Brass', (.72,.53,.26),.6,.58),
    'Wood': material('Wood', (.48,.31,.16)),
    'Amber': material('Amber', (.98,.68,.22)),
}

class Part:
    """Assemble all detail into one mesh with explicit per-vertex bone weights."""
    def __init__(self):
        self.v=[]; self.f=[]; self.mi=[]; self.weights=[]; self.mats=[]
    def add(self, mesh, mat, weight, transform=None):
        offset=len(self.v)
        if mat not in self.mats: self.mats.append(mat)
        idx=self.mats.index(mat)
        for v in mesh.vertices:
            p=transform @ v.co if transform else v.co.copy()
            self.v.append(tuple(p))
            w=weight(p) if callable(weight) else ({weight:1} if isinstance(weight,str) else weight.copy())
            w={n:a for n,a in w.items() if a>1e-6}
            total=sum(w.values())
            self.weights.append({n:a/total for n,a in w.items()})
        self.f.extend([tuple(offset+i for i in p.vertices) for p in mesh.polygons])
        self.mi.extend([idx]*len(mesh.polygons))
        bpy.data.meshes.remove(mesh)
    def raw(self, verts, faces, mat, weight):
        mesh=bpy.data.meshes.new('_raw')
        mesh.from_pydata(verts,[],faces); mesh.update()
        self.add(mesh,mat,weight)
    def box(self, pos, size, mat, weight, bevel=.02, segments=1, rotation=None, taper=1):
        bm=bmesh.new(); bmesh.ops.create_cube(bm,size=1)
        for v in bm.verts:
            v.co.x *= size[0] * (taper if v.co.z>0 else 1)
            v.co.y *= size[1]
            v.co.z *= size[2]
        if bevel>0:
            bmesh.ops.bevel(bm,geom=list(bm.edges),offset=min(bevel,min(size)*.43),segments=segments,profile=.5,affect='EDGES',clamp_overlap=True)
        if mat==M['Skin'] and size[2]>.5:
            # Small face patches keep the eyes readable in the cached painter.
            faces=[face for face in bm.faces if face.calc_area()>.035]
            if faces:
                result=bmesh.ops.poke(bm,faces=faces)
                bmesh.ops.poke(bm,faces=result['faces'])
        bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
        mesh=bpy.data.meshes.new('_box'); bm.to_mesh(mesh); bm.free()
        rot=rotation or Matrix.Identity(3)
        transform=Matrix.Translation(Vector(pos)) @ rot.to_4x4()
        self.add(mesh,mat,weight,transform)
    def ellipsoid(self, pos, size, mat, weight, segments=10, rings=5, rotation=None):
        bm=bmesh.new()
        bmesh.ops.create_uvsphere(bm,u_segments=segments,v_segments=rings,radius=1)
        for v in bm.verts:
            v.co.x*=size[0]; v.co.y*=size[1]; v.co.z*=size[2]
        mesh=bpy.data.meshes.new('_oval'); bm.to_mesh(mesh); bm.free()
        transform=Matrix.Translation(Vector(pos)) @ (rotation or Matrix.Identity(3)).to_4x4()
        self.add(mesh,mat,weight,transform)
    def bar(self, a,b,width,depth,mat,weight,bevel=.008):
        a,b=Vector(a),Vector(b)
        q=Vector((0,0,1)).rotation_difference((b-a).normalized())
        self.box((a+b)/2,(width,depth,(b-a).length),mat,weight,bevel,rotation=q.to_matrix())
    def tube(self, points,radii,mat,weight,sides=8,depth_ratio=1):
        # Broad closed rings, sufficient for shape and skinning without subdivision.
        vs=[]
        for k,p in enumerate(points):
            p=Vector(p)
            tangent=Vector(points[min(k+1,len(points)-1)])-Vector(points[max(k-1,0)])
            tangent.normalize()
            u=Vector((0,1,0)).cross(tangent).normalized()
            if u.length<.01: u=Vector((1,0,0))
            v=tangent.cross(u).normalized()
            rx,ry=radii[k] if isinstance(radii[k],tuple) else (radii[k],radii[k]*depth_ratio)
            for i in range(sides):
                t=2*math.pi*(i+.5)/sides
                vs.append(tuple(p+u*rx*math.cos(t)+v*ry*math.sin(t)))
        fs=[tuple(reversed(range(sides)))]
        for k in range(len(points)-1):
            for i in range(sides):
                a=k*sides+i;b=k*sides+(i+1)%sides
                fs.append((a,b,b+sides,a+sides))
        fs.append(tuple(range((len(points)-1)*sides,len(points)*sides)))
        self.raw(vs,fs,mat,weight)
    def loft(self, rings, mat, weight):
        # Rounded rectangle cross sections: flat front face and beveled corners.
        vs=[]
        for z,x,y,bevel in rings:
            b=bevel
            vs.extend([(a,c,z) for a,c in [(-x+b,-y),(x-b,-y),(x,-y+b),(x,y-b),(x-b,y),(-x+b,y),(-x,y-b),(-x,-y+b)]])
        fs=[tuple(reversed(range(8)))]
        for k in range(len(rings)-1):
            for i in range(8):fs.append((k*8+i,k*8+(i+1)%8,(k+1)*8+(i+1)%8,(k+1)*8+i))
        fs.append(tuple(range((len(rings)-1)*8,len(rings)*8)))
        self.raw(vs,fs,mat,weight)
    def finish(self, old):
        previous_mesh=old.data
        canonical_name=old.name
        mesh=bpy.data.meshes.new(old.name+'_CuteMesh')
        mesh.from_pydata(self.v,[],self.f); mesh.update()
        for m in self.mats:mesh.materials.append(m)
        for p,i in zip(mesh.polygons,self.mi):p.material_index=i;p.use_smooth=False
        bm=bmesh.new();bm.from_mesh(mesh)
        bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(mesh);bm.free()
        # Every shared preview/revenant keeps a consistent group-to-index layout.
        users=[o for o in bpy.data.objects if o.type=='MESH' and o.data==old.data]
        groups=sorted({n for w in self.weights for n in w})
        for o in users:
            o.data=mesh
            o.vertex_groups.clear()
            for n in groups:o.vertex_groups.new(name=n)
            for i,w in enumerate(self.weights):
                for n,a in w.items():o.vertex_groups[n].add([i],a,'REPLACE')
            # Old per-object overrides no longer correspond to the new material slots.
            for s in o.material_slots:s.link='DATA'
            if o.name.startswith('SKM_MOB_07'):
                for s in o.material_slots:
                    key=s.material.name
                    if key.endswith(('_Skin','_Blush','_Ear')): repl=bpy.data.materials['MAT_AOE_GhoulSkin']
                    elif key.endswith(('_Teal','_TealTrim','_Linen')): repl=bpy.data.materials['MAT_AOE_GhoulCloth']
                    else: continue
                    s.link='OBJECT';s.material=repl
            o['quality_revision']=REV
        if previous_mesh.users==0:bpy.data.meshes.remove(previous_mesh)
        mesh.name=canonical_name
        mesh['quality_revision']=REV
        return old

def mix(a,b,t):return {a:1-t,b:t}
def torso_weight(p):
    z=p.z
    if z<.79:return {'pelvis':1}
    if z<.98:return mix('pelvis','spine',(z-.79)/.19)
    return mix('spine','chest',min(1,(z-.98)/.12))
def skirt_weight(p):
    if p.z>=.78:return torso_weight(p)
    side='R' if p.x>=0 else 'L'
    return mix('pelvis','skirt.'+side,min(.85,max(0,(.78-p.z)/.45)))

def head(female=False):
    p=Part(); w='head'
    # Almost cuboid, with generous cheek/jaw bevels rather than a polygon cylinder.
    p.box((0,0,1.505),(.61 if female else .63,.49,.59),M['Skin'],w,.138,4)
    for side in [-1,1]:
        p.ellipsoid((side*.321,0,1.50),(.053,.047,.077),M['Skin'],w,8,4)
        p.ellipsoid((side*.348,-.018,1.50),(.015,.028,.039),M['Ear'],w,8,4)
        x=side*.119
        # Larger oval eyes are sculpted onto the flat face, legible at game scale.
        p.ellipsoid((x,-.247,1.536),(.043,.013,.062),M['Eye'],w,12,6)
        p.ellipsoid((x-.011,-.260,1.563),(.011,.004,.014),M['Glint'],w,8,4)
        p.bar((x-.041,-.251,1.627),(x+.038,-.250,1.634+side*.005),.020 if female else .025,.014,M['Hair'],w,.005)
        if female:
            p.bar((x+side*.030,-.252,1.58),(x+side*.050,-.252,1.59),.012,.009,M['Eye'],w,.003)
        p.ellipsoid((side*.188,-.244,1.426),(.040,.004,.016),M['Blush'],w,8,4)
    p.ellipsoid((0,-.257,1.457),(.035,.037,.027),M['Skin'],w,10,5)
    # Upturned smile: tiny closed tube, no thin overlapping coplanar mouth decal.
    pts=[(-.065,-.247,1.394),(-.040,-.251,1.380),(0,-.252,1.375),(.040,-.251,1.380),(.065,-.247,1.394)]
    p.tube(pts,[.005]*len(pts),M['Mouth'],w,6)
    p.tube([(0,0,1.15),(0,0,1.26)],[.085,.085],M['Skin'],'neck',8)
    return p

def hair(style):
    p=Part();w='head'
    # Closed sculpted cap, varying lower rim preserves an open forehead.
    n=16;vs=[]
    for row in range(4):
        for i in range(n):
            a=2*math.pi*i/n
            x,y=math.cos(a),math.sin(a)
            x=math.copysign(abs(x)**.5,x);y=math.copysign(abs(y)**.5,y)
            if row==0:
                z=1.57-.12*max(0,y)+.12*max(0,-y)
                rx,ry=.326,.265
            elif row==1:z=1.755;rx,ry=.327,.264
            elif row==2:z=1.833;rx,ry=.247,.204
            else:z=1.872;rx,ry=.052,.048
            vs.append((x*rx,y*ry,z))
    fs=[tuple(reversed(range(n)))]
    for row in range(3):
        for i in range(n):fs.append((row*n+i,row*n+(i+1)%n,(row+1)*n+(i+1)%n,(row+1)*n+i))
    fs.append(tuple(range(3*n,4*n)))
    p.raw(vs,fs,M['Hair'],w)
    # Broad swept forelocks with pointed tips, no strand geometry.
    for i in range(5):
        x=-.24+i*.105
        tip=x-.060 if style in [1,5] else x+(.04 if x<0 else -.035)
        points=[(x+.052,-.15,1.81),(x+.02,-.233,1.774),(tip,-.276,1.66+(i%2)*.028)]
        p.tube(points,[(.083,.033),(.076,.028),(.008,.010)],M['HairLight'] if i%2==0 else M['Hair'],w,6)
    if style==5:
        p.tube([(-.12,-.05,1.83),(-.20,-.16,1.86),(-.29,-.23,1.76)],[(.13,.06),(.10,.05),(.02,.018)],M['HairLight'],w,6)
    if style in [2,3]:
        # Side masses slope outward around ears and taper at their ends.
        for s in [-1,1]:
            z=1.32 if style==2 else 1.14
            p.tube([(s*.29,.06,1.70),(s*.34,.10,1.44),(s*.31,.13,z)],[(.073,.12),(.078,.12),(.054,.10)],M['Hair'],w,8)
        p.box((0,.225,1.45 if style==2 else 1.35),(.55,.14,.40 if style==2 else .60),M['Hair'],w,.058,1)
    if style==4:
        p.tube([(.21,.13,1.57),(.31,.09,1.39),(.33,-.04,1.24)],[(.10,.09),(.085,.075),(.07,.065)],M['Hair'],w,8)
        for i in range(5):
            p.ellipsoid((.33+(.022 if i%2 else -.018),-.09-i*.012,1.27-i*.074),(.077-i*.006,.064-i*.005,.068),M['HairLight'] if i%2 else M['Hair'],w,8,4)
        p.box((.33,-.145,.91),(.091,.074,.035),M['Linen'],w,.01)
    return p

def torso(style):
    p=Part()
    base={1:'Teal',2:'IronDark',3:'Moss',4:'Linen',5:'Wine',6:'Linen'}[style]
    p.loft([(.70,.232,.145,.035),(.79,.218,.145,.03),(.92,.225,.15,.035),(1.075,.258,.145,.045),(1.145,.235,.125,.040)],M[base],torso_weight)
    # Neck opening and folded collars: broad deliberate planes.
    p.box((0,0,1.16),(.18,.17,.066),M['Skin'],'neck',.025)
    if style in [1,5,6]:
        for s in [-1,1]:
            p.raw([(s*.025,-.15,1.14),(s*.15,-.15,1.12),(s*.087,-.174,1.055)],[(0,1,2),(2,1,0)],M['Linen'] if style!=1 else M['TealTrim'],'chest')
    if style==1:
        p.loft([(.693,.235,.148,.035),(.722,.235,.148,.035)],M['TealTrim'],torso_weight)
        p.bar((-.21,-.159,.84),(.16,-.157,1.12),.058,.022,M['Leather'],torso_weight,.006)
        p.box((-.03,-.181,.98),(.065,.018,.054),M['Brass'],'spine',.005)
    elif style==2:
        p.loft([(.84,.225,.162,.055),(1.02,.258,.181,.055),(1.11,.23,.16,.045)],M['Iron'],torso_weight)
        p.bar((0,-.184,.87),(0,-.184,1.09),.023,.016,M['IronEdge'],torso_weight,.004)
        for s in [-1,1]:
            p.box((s*.145,-.16,.75),(.16,.055,.19),M['Iron'],skirt_weight,.022)
            p.box((s*.21,-.145,1.065),(.025,.025,.025),M['Brass'],'chest',.006)
    elif style==3:
        p.box((0,-.164,.955),(.37,.06,.30),M['Leather'],torso_weight,.025)
        # Small mantle around the collar, not another large asset/slot.
        for s in [-1,1]:
            p.bar((s*.04,-.13,1.145),(s*.21,-.155,1.055),.102,.036,M['MossLight'],'chest',.012)
        p.box((0,-.19,1.10),(.049,.023,.049),M['Brass'],'chest',.012)
        p.bar((-.18,-.204,.84),(.17,-.198,1.11),.043,.015,M['LeatherLight'],torso_weight,.004)
    elif style==4:
        p.loft([(.27,.295,.192,.045),(.48,.275,.177,.04),(.70,.230,.151,.035),(.78,.218,.145,.03)],M['Linen'],skirt_weight)
        p.box((0,-.18,.68),(.13,.024,.74),M['Rust'],skirt_weight,.015)
        for s in [-1,1]:p.bar((s*.09,-.148,1.14),(s*.09,-.16,.81),.052,.026,M['Leather'],torso_weight,.007)
    elif style==5:
        p.box((0,-.166,.96),(.37,.052,.29),M['Rust'],torso_weight,.023)
        for z in [.89,.98,1.065]:p.box((0,-.199,z),(.027,.018,.027),M['Brass'],torso_weight,.006)
    elif style==6:
        p.loft([(.40,.28,.176,.04),(.68,.235,.151,.035),(.78,.22,.15,.03)],M['Rust'],skirt_weight)
        p.box((0,-.171,.975),(.29,.025,.23),M['Rust'],torso_weight,.017)
        apron=[(-.145,-.195,.50),(.145,-.195,.50),(.145,-.172,.78),(-.145,-.172,.78)]
        p.raw(apron+[(x,y+.010,z) for x,y,z in apron],[(0,1,2,3),(7,6,5,4),(0,4,5,1),(1,5,6,2),(2,6,7,3),(3,7,4,0)],M['Rust'],skirt_weight)
        for s in [-1,1]:p.bar((s*.11,-.151,1.13),(s*.11,-.168,.86),.037,.020,M['Rust'],torso_weight,.005)
        p.box((.09,-.194,.72),(.11,.022,.095),M['LeatherLight'],skirt_weight,.01)
    return p

def arms(style):
    p=Part()
    for s,side in [(1,'R'),(-1,'L')]:
        shoulder=Vector((s*.25,0,1.105)); elbow=Vector((s*.38,0,.93));wrist=Vector((s*.47,-.025,.77))
        upper='upper_arm.'+side;fore='forearm.'+side;hand='hand.'+side
        points=[shoulder.lerp(elbow,-.02),shoulder.lerp(elbow,.30),shoulder.lerp(elbow,.85),elbow,elbow.lerp(wrist,.20),elbow.lerp(wrist,.75),wrist]
        def weight(co):
            # Distance along the rest chain, smooth across the elbow rings.
            t=max(0,min(1,(1.00-co.z)/.12))
            if co.z>.85:return mix(upper,fore,t)
            return mix(fore,hand,max(0,min(.85,(.82-co.z)/.065)))
        base={1:'Teal',2:'IronDark',3:'Linen',4:'Linen'}[style]
        p.tube(points,[(.104,.096),(.105,.095),(.089,.085),(.085,.079),(.081,.073),(.075,.071),(.073,.067)],M[base],weight,8)
        # Cuff and bracer are broad beveled solids; the joint surface stays continuous.
        q=Vector((0,0,1)).rotation_difference((wrist-elbow).normalized()).to_matrix()
        if style==2:
            qu=Vector((0,0,1)).rotation_difference((elbow-shoulder).normalized()).to_matrix()
            p.box(shoulder.lerp(elbow,.13),(.25,.24,.15),M['Iron'],upper,.025,2,rotation=qu)
            p.box(elbow.lerp(wrist,.56),(.178,.17,.145),M['Iron'],fore,.027,1,rotation=q)
            p.box(elbow,(.176,.17,.108),M['IronEdge'],fore,.023,1,rotation=q)
        else:
            p.box(elbow.lerp(wrist,.63),(.162,.155,.124),M['Leather'] if style==3 else M['TealTrim'] if style==1 else M['LinenShade'],fore,.019,rotation=q)
        p.box(wrist,(.158,.145,.047),M['IronEdge'] if style==2 else M['LeatherLight'],fore,.012,rotation=q)
        palm=Vector((s*.481,-.032,.704))
        skin=M['Leather'] if style==2 else M['Skin']
        p.box(palm,(.135,.133,.125),skin,hand,.036,2)
        p.ellipsoid((s*.425,-.078,.729),(.036,.039,.048),skin,hand,8,4)
    return p

def legs(style):
    p=Part();base=M['IronDark'] if style==3 else M['Teal'] if style==2 else M['Trousers']
    p.box((0,0,.712),(.417,.255,.16),base,'pelvis',.034)
    for s,side in [(1,'R'),(-1,'L')]:
        def weight(co):
            if co.z>.65:return mix('pelvis','thigh.'+side,max(0,min(1,(.75-co.z)/.10)))
            return mix('thigh.'+side,'shin.'+side,max(0,min(1,(.50-co.z)/.14)))
        p.tube([(s*.12,0,.72),(s*.12,0,.63),(s*.12,-.01,.48),(s*.12,-.012,.43),(s*.12,-.006,.36),(s*.12,0,.23)],[(.099,.12),(.101,.119),(.091,.109),(.088,.106),(.081,.102),(.075,.09)],base,weight,8)
        if style==2:
            p.box((s*.12,-.111,.52),(.123,.018,.109),M['Linen'],weight,.009)
        if style==3:
            p.box((s*.12,-.110,.445),(.173,.067,.145),M['Iron'],'shin.'+side,.035,2)
            p.box((s*.12,-.089,.30),(.145,.048,.15),M['Iron'],'shin.'+side,.022)
    return p

def feet(style):
    p=Part()
    for s,side in [(1,'R'),(-1,'L')]:
        w='foot.'+side
        p.box((s*.12,-.059,.032),(.214,.327,.062),M['Sole'],w,.022,1)
        p.box((s*.12,-.06,.108),(.204,.302,.143),M['Leather'],w,.044,2)
        p.box((s*.12,.005,.228),(.177,.185,.185),M['Leather'] if style==1 else M['LinenShade'],'shin.'+side,.026)
        p.box((s*.12,.004,.305),(.193,.200,.048),M['LeatherLight'] if style==1 else M['Linen'],'shin.'+side,.012)
        if style!=1:
            for z in [.205,.252]:p.box((s*.12,-.093,z),(.177,.016,.023),M['Linen'],'shin.'+side,.004)
    return p

def belt(style):
    p=Part();w=torso_weight
    p.loft([(.785,.237,.164,.035),(.845,.237,.164,.035)],M['Leather'],w)
    p.box((0,-.185,.815),(.082,.025,.077),M['Brass'],w,.012)
    p.box((0,-.201,.815),(.045,.009,.040),M['Leather'],w,.004)
    if style==2:
        for s in [-1,1]:
            p.box((s*.205,-.122,.737),(.139,.102,.151),M['Leather'],w,.024)
            p.box((s*.205,-.178,.772),(.131,.019,.060),M['LeatherLight'],w,.012)
    return p

def facial(style):
    p=Part();w='head'
    # Curved beard surrounds the smile instead of covering the entire face.
    pts=[(-.238,-.262,1.455),(-.204,-.275,1.358),(-.12,-.278,1.30),(0,-.279,1.285),(.12,-.278,1.30),(.204,-.275,1.358),(.238,-.262,1.455)]
    p.tube(pts,[.014 if style==1 else .025]*len(pts),M['Hair'],w,6)
    if style==2:
        for s in [-1,1]:p.bar((s*.025,-.283,1.418),(s*.077,-.269,1.402),.027,.022,M['Hair'],w,.006)
    return p

def hat(style):
    p=Part();w='head'
    base=M['Linen'] if style==1 else M['Iron'] if style==2 else M['Wine']
    # Top shell with open front leaves the whole expression unobstructed.
    n=16;vs=[]
    for row,(z,rx,ry) in enumerate([(1.70,.354,.294),(1.81,.35,.29),(1.885,.235,.202),(1.91,.10,.09)]):
        for i in range(n):
            t=2*math.pi*i/n;x,y=math.cos(t),math.sin(t)
            vs.append((math.copysign(abs(x)**.6,x)*rx,math.copysign(abs(y)**.6,y)*ry,z))
    fs=[tuple(reversed(range(n)))]
    for r in range(3):
        for i in range(n):fs.append((r*n+i,r*n+(i+1)%n,(r+1)*n+(i+1)%n,(r+1)*n+i))
    fs.append(tuple(range(3*n,4*n)));p.raw(vs,fs,base,w)
    if style in [1,2]:
        for s in [-1,1]:p.box((s*.334,.032,1.52),(.07,.26,.35),base,w,.029,2)
        p.box((0,.255,1.54),(.57,.06,.31),base,w,.025)
        p.box((0,-.28,1.706),(.55,.035,.045),M['IronEdge'] if style==2 else M['LinenShade'],w,.011)
    else:
        p.box((0,-.23,1.716),(.56,.16,.043),M['Leather'],w,.018)
    return p

def back(style):
    p=Part()
    if style==1:
        # Thin closed mantle with enough rows for cape deformation.
        rings=[(1.13,.20,.17),(.98,.24,.23),(.75,.275,.28),(.50,.30,.32)]
        vs=[]
        for thickness in [0,.025]:
            for z,x,y in rings:
                for t in [-1,0,1]:vs.append((x*t,y+.035*(1-t*t)+thickness,z))
        fs=[]
        for off in [0,12]:
            for r in range(3):
                for c in range(2):
                    a=off+r*3+c;fs.append((a,a+1,a+4,a+3) if off==0 else (a+3,a+4,a+1,a))
        edge=[0,1,2,5,8,11,10,9,6,3]
        for a,b in zip(edge,edge[1:]+edge[:1]):fs.append((a,b,b+12,a+12))
        p.raw(vs,fs,M['Moss'],lambda co:mix('chest','cape',min(.95,max(0,(1.13-co.z)/.35))))
    elif style==2:
        p.tube([(.19,.22,.73),(.24,.23,1.13)],[(.082,.075),(.092,.084)],M['Leather'],'chest',10)
        p.box((.24,.23,1.13),(.193,.18,.042),M['LeatherLight'],'chest',.013)
        for i in range(3):
            x=.17+i*.063
            p.bar((x,.23,.98),(x+.035,.23,1.44),.014,.014,M['Wood'],'chest',.003)
            p.box((x+.03,.23,1.37),(.053,.025,.09),M['Linen'],'chest',.006)
    else:
        p.box((0,.25,.94),(.36,.25,.40),M['Leather'],'chest',.046,2)
        p.box((0,.385,1.075),(.34,.055,.145),M['LeatherLight'],'chest',.024)
        p.box((0,.35,1.19),(.37,.15,.12),M['Linen'],'chest',.035,2)
        for s in [-1,1]:p.bar((s*.10,.405,.80),(s*.10,.405,1.14),.035,.025,M['Leather'],'chest',.005)
    return p

def weapon(slot,style):
    p=Part();w='hand.R' if slot=='Mainhand' else 'hand.L'
    x=.49 if slot=='Mainhand' else -.49
    if slot=='Mainhand' and style==1:
        p.bar((x,-.04,.65),(x,-.04,.84),.048,.048,M['Leather'],w,.01)
        p.box((x,-.04,.847),(.23,.055,.052),M['Brass'],w,.012)
        # Diamond section blade, blunt enough to suit the toy character style.
        vs=[(x-.055,-.04,.87),(x,-.068,.87),(x+.055,-.04,.87),(x,-.012,.87),(x-.036,-.04,1.31),(x,-.060,1.31),(x+.036,-.04,1.31),(x,-.020,1.31),(x,-.04,1.41)]
        fs=[(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7),(4,5,8),(5,6,8),(6,7,8),(7,4,8),(3,2,1,0)]
        p.raw(vs,fs,M['IronEdge'],w)
        p.box((x,-.04,.625),(.071,.067,.047),M['Brass'],w,.015)
    elif slot=='Mainhand' and style==2:
        pts=[(x,-.02,.32),(x+.09,-.02,.47),(x+.145,-.02,.71),(x+.09,-.02,.95),(x,-.02,1.10)]
        p.tube(pts,[.024,.027,.030,.027,.024],M['Wood'],w,8)
        p.bar(pts[0],pts[-1],.006,.006,M['Linen'],w,.001)
        p.bar((x+.143,-.02,.66),(x+.143,-.02,.77),.065,.055,M['Leather'],w,.008)
    elif slot=='Mainhand':
        p.bar((x,-.025,.10),(x,-.025,1.31),.035,.035,M['Wood'],w,.007)
        p.ellipsoid((x,-.025,1.37),(.08,.07,.11),M['MossLight'],w,8,4)
        p.box((x,-.025,1.29),(.085,.07,.040),M['Brass'],w,.008)
    elif style==1:
        p.box((x,-.12,.79),(.34,.085,.43),M['IronDark'],w,.055,2)
        p.box((x,-.169,.79),(.29,.025,.37),M['Wood'],w,.039)
        p.bar((x,-.19,.64),(x,-.19,.94),.04,.024,M['IronEdge'],w,.006)
        p.box((x,-.195,.79),(.25,.026,.040),M['IronEdge'],w,.009)
        p.ellipsoid((x,-.218,.79),(.055,.025,.055),M['Iron'],w,8,4)
    else:
        p.box((x,-.05,.49),(.14,.14,.20),M['Amber'],w,.019)
        for z in [.375,.60]:p.box((x,-.05,z),(.18,.18,.035),M['IronDark'],w,.012)
        for sx in [-1,1]:
            for sy in [-1,1]:p.bar((x+sx*.07,-.05+sy*.07,.38),(x+sx*.07,-.05+sy*.07,.60),.018,.018,M['IronDark'],w,.003)
        p.tube([(x-.06,-.05,.62),(x-.06,-.05,.70),(x+.06,-.05,.70),(x+.06,-.05,.62)],[.012]*4,M['IronDark'],w,6)
    return p

BUILDERS={'Head':head,'Hair':hair,'Torso':torso,'Arms':arms,'Legs':legs,'Feet':feet,'Belt':belt,'FacialHair':facial,'Headgear':hat,'Back':back}
assets=sorted([o for o in bpy.data.objects if o.name.startswith('SKM_NPC_') and o.get('kind')=='npc'],key=lambda o:o.name)
assert len(assets)==37,len(assets)
rig=bpy.data.objects['RIG_NPC_Humanoid_v1']
before_bones=[(b.name,tuple(b.head_local),tuple(b.tail_local)) for b in rig.data.bones]
before_actions=[(a.name,tuple(a.frame_range)) for a in bpy.data.actions]
def keyframe_hash(action):
    channels=[]
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for fc in bag.fcurves:
                    channels.append((fc.data_path,fc.array_index,[(tuple(k.co),k.interpolation) for k in fc.keyframe_points]))
    return hashlib.sha256(repr(channels).encode()).hexdigest()
before_keyframes={a.name:keyframe_hash(a) for a in bpy.data.actions}
for o in assets:
    slot=o['slot'];index=o['part_index']
    p=head(index==2) if slot=='Head' else weapon(slot,index) if slot in ['Mainhand','Offhand'] else BUILDERS[slot](index)
    p.finish(o)
import importlib.util
expression_spec=importlib.util.spec_from_file_location('npc_expressions',ROOT/'tools/Add-NPCExpressions.py')
expression_module=importlib.util.module_from_spec(expression_spec)
expression_spec.loader.exec_module(expression_module)
expression_module.add_expressions(assets)
rig['character_quality_revision']=REV

def review_collection():
    for c in list(bpy.data.collections):
        if c.name.startswith('08_CUTE_MODULAR_REVIEW'):
            for o in list(c.objects):bpy.data.objects.remove(o,do_unlink=True)
            bpy.data.collections.remove(c)
    for o in bpy.data.objects:o.hide_render=True
    col=bpy.data.collections.new('08_CUTE_MODULAR_REVIEW');bpy.context.scene.collection.children.link(col)
    looks=[{'Head':1,'Hair':5,'Torso':1,'Arms':1,'Legs':1,'Feet':1,'Belt':2,'Back':3},
           {'Head':2,'Hair':4,'Torso':6,'Arms':4,'Legs':1,'Feet':1,'Belt':1},
           {'Head':1,'Headgear':2,'Torso':2,'Arms':2,'Legs':3,'Feet':1,'Belt':1,'Mainhand':1,'Offhand':1,'FacialHair':2},
           {'Head':2,'Hair':3,'Torso':3,'Arms':3,'Legs':1,'Feet':1,'Belt':2,'Back':2,'Mainhand':2}]
    for i,look in enumerate(looks):
        r=rig.copy();r.data=rig.data.copy();col.objects.link(r);r.name='REVIEW_Cute_Rig_'+str(i);r.hide_render=False
        r.animation_data_clear();r.animation_data_create();r.animation_data.action=bpy.data.actions['Idle']
        r.animation_data.action_slot=bpy.data.actions['Idle'].slots[0]
        for b in r.pose.bones:b.matrix_basis=Matrix.Identity(4)
        r.location=((i-1.5)*1.30,0,0)
        for slot,idx in look.items():
            src=next(o for o in assets if o['slot']==slot and o['part_index']==idx)
            ob=src.copy();ob.data=src.data;col.objects.link(ob);ob.name='REVIEW_Cute_'+str(i)+'_'+slot
            ob.parent=r;ob.matrix_parent_inverse=Matrix.Identity(4);ob.location=(0,0,0);ob.hide_render=False
            for mod in ob.modifiers:
                if mod.type=='ARMATURE':mod.object=r
    scene=bpy.context.scene;scene.frame_set(1)
    floor=bpy.data.meshes.new('CuteReviewFloor');floor.from_pydata([(-200,-200,-.009),(200,-200,-.009),(200,200,-.009),(-200,200,-.009)],[],[(0,1,2,3)])
    ob=bpy.data.objects.new('REVIEW_Cute_Floor',floor);col.objects.link(ob)
    floor.materials.append(material('ReviewFloor',(.78,.75,.68)))
    cam=bpy.data.cameras.new('CuteReviewCamera');ob=bpy.data.objects.new('REVIEW_Cute_Camera',cam);col.objects.link(ob)
    ob.location=(3,-10,4.6);target=Vector((0,0,.94));ob.rotation_euler=(target-ob.location).to_track_quat('-Z','Y').to_euler();cam.type='ORTHO';cam.ortho_scale=6.35;scene.camera=ob
    for name,loc,power,size in [('Key',(-3,-4,6),650,5),('Fill',(4,-2,4),360,4),('Rim',(0,3,5),550,3)]:
        light=bpy.data.lights.new('CuteReview'+name,'AREA');light.energy=power;light.shape='DISK';light.size=size
        ob=bpy.data.objects.new('REVIEW_Cute_'+name,light);col.objects.link(ob);ob.location=loc;ob.rotation_euler=(Vector((0,0,1))-ob.location).to_track_quat('-Z','Y').to_euler()
    scene.world=bpy.data.worlds.new('CuteReviewWorld');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.65,.69,.75,1);scene.world.node_tree.nodes['Background'].inputs[1].default_value=.45
    scene.render.engine='CYCLES';scene.cycles.samples=40;scene.cycles.use_denoising=True
    scene.render.resolution_x=1680;scene.render.resolution_y=880;scene.render.resolution_percentage=100
    scene.view_settings.view_transform='AgX';scene.render.image_settings.file_format='PNG';scene.render.film_transparent=False
    return col

def validate():
    report={'revision':REV,'status':'PASS','npc_parts':len(assets),'bones_unchanged':before_bones==[(b.name,tuple(b.head_local),tuple(b.tail_local)) for b in rig.data.bones],'action_names_ranges_unchanged':before_actions==[(a.name,tuple(a.frame_range)) for a in bpy.data.actions],'parts':[],'errors':[]}
    report['keyframe_action_hashes']=before_keyframes
    report['all_144_animation_keyframe_hashes_unchanged']=before_keyframes=={a.name:keyframe_hash(a) for a in bpy.data.actions}
    report['source_action_datablocks']=len(before_keyframes)
    report['all_source_action_keyframes_unchanged']=report['all_144_animation_keyframe_hashes_unchanged']
    for o in assets:
        mesh=o.data;mesh.calc_loop_triangles()
        zero=sum(t.area<1e-10 for t in mesh.loop_triangles)
        bad=[];max_weights=0
        for v in mesh.vertices:
            groups=[g for g in v.groups if g.weight>0]
            max_weights=max(max_weights,len(groups))
            if not groups or abs(sum(g.weight for g in groups)-1)>1e-5 or any(o.vertex_groups[g.group].name not in rig.data.bones for g in groups):bad.append(v.index)
        bm=bmesh.new();bm.from_mesh(mesh);closed=all(e.is_manifold for e in bm.edges);bm.free()
        report['parts'].append({'asset_id':o['source_asset_id'],'vertices':len(mesh.vertices),'triangles':len(mesh.loop_triangles),'zero_area_triangles':zero,'closed_components':closed,'max_weights':max_weights,'invalid_weights':len(bad)})
        if zero or bad or max_weights>4:report['errors'].append(o.name)
    if not report['bones_unchanged'] or not report['action_names_ranges_unchanged'] or not report['all_144_animation_keyframe_hashes_unchanged']:report['errors'].append('rig/action contract changed')
    report['status']='FAIL' if report['errors'] else 'PASS'
    (WORK/'geometry_validation.json').write_text(json.dumps(report,indent=2),encoding='utf8')
    assert not report['errors'],report['errors']
    return report

report=validate()
review_collection()
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(WORK/'Cute_Modular_Review.blend'))
bpy.context.scene.render.filepath=str(WORK/'Cute_Modular_Lineup.png');bpy.ops.render.render(write_still=True)
print('CUTE_REVIEW_READY',str(WORK/'Cute_Modular_Lineup.png'),flush=True)

if PUBLISH:
    # Publication is implemented separately so default review never touches deliverables.
    exec((ROOT/'tools/Publish-CuteModularNPC.py').read_text(encoding='utf8'))
