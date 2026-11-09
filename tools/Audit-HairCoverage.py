"""Read-only dense scalp coverage audit in Blender. Does not modify/export sources.

blender -b Art/.../AgeOfEchoes_Characters_Rigged.blend --python tools/Audit-HairCoverage.py
The report separates crown coverage from the deliberately open lower hairline.
"""
import bpy
import json
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree

root = Path(__file__).resolve().parents[1]
output = root / 'x64/hair-fix'
output.mkdir(parents=True, exist_ok=True)
heads = sorted((o for o in bpy.data.objects if o.name.startswith('SKM_NPC_Head_')), key=lambda o: o.name)
hairs = sorted((o for o in bpy.data.objects if o.name.startswith('SKM_NPC_Hair_')), key=lambda o: o.name)
assert len(heads) == 2 and len(hairs) == 5
report = {'read_only': True, 'samples_per_triangle_edge': 16, 'results': []}
for head in heads:
    mesh = head.data
    mesh.calc_loop_triangles()
    samples = {'crown': [], 'lower_hairline': []}
    for tri in mesh.loop_triangles:
        if mesh.materials[tri.material_index].name != 'MAT_AOE_NPC_Skin':
            continue
        points = [mesh.vertices[i].co for i in tri.vertices]
        for i in range(1, 17):
            for j in range(1, 17 - i):
                point = points[0] * (1 - (i + j) / 16) + points[1] * i / 16 + points[2] * j / 16
                if point.z >= 1.71:
                    samples['crown'].append(point)
                # This broad mask includes intentionally exposed hairline/forehead.
                # Report it separately; do not call those samples missing polygons.
                elif point.z >= (1.70 if point.y < -.16 else 1.60):
                    samples['lower_hairline'].append(point)
    for hair in hairs:
        mesh = hair.data
        mesh.calc_loop_triangles()
        tree = BVHTree.FromPolygons([v.co for v in mesh.vertices],
                                   [tuple(t.vertices) for t in mesh.loop_triangles], all_triangles=True)
        record = {'head': head.name, 'hair': hair.name}
        for region, points in samples.items():
            missing, clearance = [], []
            for point in points:
                direction = (point - Vector((0, 0, 1.50))).normalized()
                hit, _, _, _ = tree.ray_cast(point + direction * .3, -direction, .31)
                if hit is None:
                    missing.append([round(v, 6) for v in point])
                else:
                    clearance.append((hit - point).dot(direction))
            record[region] = {'tested': len(points), 'exposed': len(missing),
                              'minimum_covered_clearance_m': min(clearance) if clearance else None,
                              'example_exposed_points': missing[:8]}
        assert record['crown']['exposed'] == 0, record
        report['results'].append(record)
        print('HAIR_DENSE_COVERAGE', head.name, hair.name, record['crown'], flush=True)
report['crown_samples_total'] = sum(r['crown']['tested'] for r in report['results'])
report['crown_exposed_total'] = sum(r['crown']['exposed'] for r in report['results'])
path = output / 'source-coverage.json'
path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print('HAIR_DENSE_COVERAGE_PASS', path, flush=True)
