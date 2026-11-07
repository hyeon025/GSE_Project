"""Read-only Blender source audit; does not render, save or export assets."""
import bpy
import math
import heapq
from mathutils import Vector
from mathutils.bvhtree import BVHTree

rig = bpy.data.objects['RIG_NPC_Humanoid_v1']
for name in ['Idle', 'Walk', 'Run']:
    track = next(t for t in rig.animation_data.nla_tracks if t.name == name)
    strip = track.strips[0]
    rig.animation_data.action = strip.action
    for t in rig.animation_data.nla_tracks:
        t.mute = True
    print('AUDIT_CLIP', name, 'frames', tuple(strip.action.frame_range), flush=True)
    for fraction in [0, .125, .25, .375, .5, .625, .75, .875, 1]:
        start, end = strip.action.frame_range
        frame = start + (end - start) * fraction
        bpy.context.scene.frame_set(int(frame), subframe=frame % 1)
        points = {}
        for bone in ['pelvis', 'thigh.L', 'shin.L', 'foot.L', 'foot.R', 'hand.L', 'hand.R']:
            b = rig.pose.bones[bone]
            points[bone] = tuple(round(v, 4) for v in b.head)
        print('AUDIT_POSE', name, fraction, points, flush=True)

rig.animation_data.action = None
for b in rig.pose.bones:
    b.matrix_basis.identity()
bpy.context.view_layer.update()
heads = [o for o in bpy.data.objects if o.type == 'MESH' and o.name.startswith('SKM_NPC_Head_')]
hairs = [o for o in bpy.data.objects if o.type == 'MESH' and o.name.startswith('SKM_NPC_Hair_')]
for hair in hairs:
    mesh = hair.data
    mesh.calc_loop_triangles()
    tree = BVHTree.FromPolygons([v.co for v in mesh.vertices], [list(t.vertices) for t in mesh.loop_triangles], all_triangles=True)
    for head in heads:
        exposed = []
        covered = 0
        inward = 0
        for vertex in head.data.vertices:
            p = vertex.co
            # Crown/temples that should be under the hair cap, excluding ears and face.
            if p.z < 1.71:
                continue
            origin = Vector((0, 0, 1.50))
            direction = (p - origin).normalized()
            outer = p + direction * .20
            hit, normal, _, _ = tree.ray_cast(outer, -direction, .20)
            if hit is None:
                exposed.append(tuple(round(v, 5) for v in p))
            else:
                covered += 1
                inward += normal.dot(direction) < 0
        print('AUDIT_HAIR', hair.name, head.name, 'covered', covered, 'exposed', len(exposed), 'inward', inward, 'examples', exposed[:8], flush=True)
    bad = []
    for poly in mesh.polygons:
        if len(poly.vertices) < 4:
            continue
        first = mesh.vertices[poly.vertices[0]].co
        distance = max(abs(poly.normal.dot(mesh.vertices[i].co - first)) for i in poly.vertices)
        if distance > .002:
            bad.append((poly.index, len(poly.vertices), round(distance, 5)))
    print('AUDIT_NONPLANAR', hair.name, bad[:20], flush=True)

# Numerical visibility comparison for the former centroid-only preview path.
# This audits triangle coverage; it creates no image and does not launch the game.
view = Vector((-.5411961001461970, -1.3065629648763766, 2 * .65 / 1.0747093)).normalized()

def precise_order(faces):
    def cross(a, b, p):
        return (b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0])
    def depth(t, p):
        d = cross(t[0], t[1], t[2])
        return t[0][2] + cross(t[0], p, t[2])/d*(t[1][2]-t[0][2]) + cross(t[0], t[1], p)/d*(t[2][2]-t[0][2])
    def overlap(a, b):
        if abs(cross(*a)) < 1e-8 or abs(cross(*b)) < 1e-8:
            return None
        polygon = a[:]
        sign = 1 if cross(*b) > 0 else -1
        for edge in range(3):
            if not polygon:
                return None
            output = []
            previous = polygon[-1]
            last = sign*cross(b[edge], b[(edge+1)%3], previous)
            for current in polygon:
                distance = sign*cross(b[edge], b[(edge+1)%3], current)
                if (last >= 0) != (distance >= 0):
                    t = last/(last-distance)
                    output.append((previous[0]+(current[0]-previous[0])*t, previous[1]+(current[1]-previous[1])*t, 0))
                if distance >= 0:
                    output.append(current)
                previous, last = current, distance
            polygon = output
        if len(polygon) < 3:
            return None
        area = sum(p[0]*q[1]-p[1]*q[0] for p, q in zip(polygon, polygon[1:]+polygon[:1]))
        if abs(area) < 1e-6:
            return None
        return tuple(sum(p[k] for p in polygon)/len(polygon) for k in [0, 1])
    n = len(faces)
    bounds = [(min(p[0] for p in t), max(p[0] for p in t), min(p[1] for p in t), max(p[1] for p in t)) for _, _, t in faces]
    sweep = sorted(range(n), key=lambda i: bounds[i][0])
    incoming, edges = [0]*n, [[] for _ in faces]
    for si, i in enumerate(sweep):
        a = bounds[i]
        for j in sweep[si+1:]:
            b = bounds[j]
            if b[0] >= a[1]:
                break
            if b[1] <= a[0] or b[3] <= a[2] or a[3] <= b[2]:
                continue
            p = overlap(faces[i][2], faces[j][2])
            if p is None:
                continue
            delta = depth(faces[i][2], p)-depth(faces[j][2], p)
            if abs(delta) < 1e-4:
                continue
            far, near = (i, j) if delta < 0 else (j, i)
            edges[far].append(near)
            incoming[near] += 1
    ready = [i for i in range(n) if not incoming[i]]
    heapq.heapify(ready)
    result, done, cycles = [], set(), 0
    while len(result) < n:
        if not ready:
            heapq.heappush(ready, next(i for i in range(n) if i not in done))
            cycles += 1
        i = heapq.heappop(ready)
        if i in done:
            continue
        done.add(i)
        result.append(faces[i])
        for j in edges[i]:
            if j not in done and incoming[j]:
                incoming[j] -= 1
                if not incoming[j]:
                    heapq.heappush(ready, j)
    return result, cycles

c, s = math.cos(-math.pi / 8), math.sin(-math.pi / 8)
head = next(o for o in heads if o.name == 'SKM_NPC_Head_02_Feminine')
for hair in hairs:
    faces = []
    for ob in [head, hair]:
        mesh = ob.data
        mesh.calc_loop_triangles()
        for tri in mesh.loop_triangles:
            points = []
            for i in tri.vertices:
                p = mesh.vertices[i].co
                points.append(Vector(((p.x * c - p.y * s) * 30, (p.x * s + p.y * c) * 30, p.z * 30)))
            if (points[1] - points[0]).cross(points[2] - points[0]).dot(view) <= 0:
                continue
            projected = [(p.x * 1.3065629648763766 - p.y * .5411961001461970,
                          -(p.x * .5411961001461970 + p.y * 1.3065629648763766) * .65 - p.z * 1.0747093,
                          p.dot(view)) for p in points]
            faces.append((sum(p[2] for p in projected), ob == hair, projected))
    faces.sort(key=lambda f: f[0])
    precise, cycles = precise_order(faces)
    reference = {}
    counts = []
    for ordered in [faces, precise]:
        actual = {}
        for _, is_hair, t in ordered:
            a, b, c0 = t
            denominator = (b[0] - a[0]) * (c0[1] - a[1]) - (b[1] - a[1]) * (c0[0] - a[0])
            if abs(denominator) < 1e-9:
                continue
            for iy in range(math.floor(min(p[1] for p in t) * 4), math.ceil(max(p[1] for p in t) * 4)):
                for ix in range(math.floor(min(p[0] for p in t) * 4), math.ceil(max(p[0] for p in t) * 4)):
                    x, y = (ix + .5) / 4, (iy + .5) / 4
                    u = ((x - a[0]) * (c0[1] - a[1]) - (y - a[1]) * (c0[0] - a[0])) / denominator
                    v = ((b[0] - a[0]) * (y - a[1]) - (b[1] - a[1]) * (x - a[0])) / denominator
                    if u < 0 or v < 0 or u + v > 1:
                        continue
                    pixel = ix, iy
                    actual[pixel] = is_hair
                    depth = a[2] + u * (b[2] - a[2]) + v * (c0[2] - a[2])
                    if pixel not in reference or depth > reference[pixel][0]:
                        reference[pixel] = depth, is_hair
        counts.append(sum(is_hair and not actual[p] for p, (_, is_hair) in reference.items()))
    total = sum(is_hair for _, is_hair in reference.values())
    print('AUDIT_HAIR_ORDER', hair.name, 'centroidHidden', counts[0], 'preciseHidden', counts[1], 'hairSamples', total, 'cycles', cycles, flush=True)
