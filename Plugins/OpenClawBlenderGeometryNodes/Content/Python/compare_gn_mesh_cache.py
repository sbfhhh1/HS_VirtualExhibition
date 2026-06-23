import argparse
import json
import math
import struct
from pathlib import Path


def read_u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0], offset + 4


def read_string(data, offset):
    length, offset = read_u32(data, offset)
    return data[offset : offset + length].decode("utf-8"), offset + length


def read_float_array(data, offset, count):
    values = list(struct.unpack_from("<" + "f" * count, data, offset)) if count else []
    return values, offset + count * 4


def read_int_array(data, offset, count):
    values = list(struct.unpack_from("<" + "i" * count, data, offset)) if count else []
    return values, offset + count * 4


def load_cache(path):
    path = Path(path)
    if path.suffix.lower() == ".json":
        with path.open("r", encoding="utf-8-sig") as handle:
            return json.load(handle)

    data = path.read_bytes()
    if data[:8] != b"OCGNMESH":
        raise RuntimeError(f"Unsupported cache format: {path}")

    offset = 8
    version, offset = read_u32(data, offset)
    if version not in (1, 2, 3):
        raise RuntimeError(f"Unsupported binary cache version: {version}")

    object_name, offset = read_string(data, offset)
    vertex_count, offset = read_u32(data, offset)
    normal_count, offset = read_u32(data, offset)
    uv_count, offset = read_u32(data, offset)
    triangle_count, offset = read_u32(data, offset)
    material_index_count, offset = read_u32(data, offset)
    vertices, offset = read_float_array(data, offset, vertex_count)
    normals, offset = read_float_array(data, offset, normal_count)
    uvs, offset = read_float_array(data, offset, uv_count)
    triangles, offset = read_int_array(data, offset, triangle_count)
    material_indices, offset = read_int_array(data, offset, material_index_count)

    material_names = []
    if offset + 4 <= len(data):
        material_name_count, offset = read_u32(data, offset)
        for _ in range(material_name_count):
            name, offset = read_string(data, offset)
            material_names.append(name)

    return {
        "objectName": object_name,
        "vertices": vertices,
        "normals": normals,
        "uvs": uvs,
        "triangles": triangles,
        "materialIndices": material_indices,
        "materialNames": material_names,
    }


def triples(values):
    return [tuple(values[i : i + 3]) for i in range(0, len(values) - 2, 3)]


def vec_sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def vec_cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def vec_dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def vec_norm(a):
    length = math.sqrt(vec_dot(a, a))
    if length <= 1.0e-12:
        return (0.0, 0.0, 0.0)
    return (a[0] / length, a[1] / length, a[2] / length)


def convert_arrays(cache, mode, scale):
    raw_vertices = triples(cache.get("vertices", []))
    raw_normals = triples(cache.get("normals", []))

    if mode == "unity":
        vertices = [(x * scale, z * scale, y * scale) for x, y, z in raw_vertices]
        normals = [vec_norm((x, z, y)) for x, y, z in raw_normals]
    elif mode == "ue":
        vertices = [(x * scale, -y * scale, z * scale) for x, y, z in raw_vertices]
        normals = [vec_norm((x, -y, z)) for x, y, z in raw_normals]
    else:
        raise RuntimeError(f"Unknown conversion mode: {mode}")

    triangles = []
    raw_triangles = cache.get("triangles", [])
    for i in range(0, len(raw_triangles) - 2, 3):
        triangles.extend([raw_triangles[i], raw_triangles[i + 2], raw_triangles[i + 1]])

    return vertices, normals, triangles


def align_winding(vertices, normals, triangles):
    aligned = list(triangles)
    flips = 0
    if len(normals) != len(vertices):
        return aligned, flips
    for i in range(0, len(aligned) - 2, 3):
        a, b, c = aligned[i], aligned[i + 1], aligned[i + 2]
        if a >= len(vertices) or b >= len(vertices) or c >= len(vertices):
            continue
        face = vec_norm(vec_cross(vec_sub(vertices[b], vertices[a]), vec_sub(vertices[c], vertices[a])))
        imported = vec_norm(
            (
                normals[a][0] + normals[b][0] + normals[c][0],
                normals[a][1] + normals[b][1] + normals[c][1],
                normals[a][2] + normals[b][2] + normals[c][2],
            )
        )
        if vec_dot(face, imported) < 0.0:
            aligned[i + 1], aligned[i + 2] = aligned[i + 2], aligned[i + 1]
            flips += 1
    return aligned, flips


def dot_stats(vertices, normals, triangles):
    dots = []
    invalid = 0
    degenerate = 0
    if len(normals) != len(vertices):
        return {"hasNormals": False}
    for i in range(0, len(triangles) - 2, 3):
        a, b, c = triangles[i], triangles[i + 1], triangles[i + 2]
        if a >= len(vertices) or b >= len(vertices) or c >= len(vertices):
            invalid += 1
            continue
        face = vec_norm(vec_cross(vec_sub(vertices[b], vertices[a]), vec_sub(vertices[c], vertices[a])))
        if face == (0.0, 0.0, 0.0):
            degenerate += 1
            continue
        imported = vec_norm(
            (
                normals[a][0] + normals[b][0] + normals[c][0],
                normals[a][1] + normals[b][1] + normals[c][1],
                normals[a][2] + normals[b][2] + normals[c][2],
            )
        )
        if imported != (0.0, 0.0, 0.0):
            dots.append(vec_dot(face, imported))
    return {
        "hasNormals": True,
        "comparedTriangles": len(dots),
        "invalidTriangles": invalid,
        "degenerateTriangles": degenerate,
        "negativeDotTriangles": sum(1 for value in dots if value < 0.0),
        "lowDotTriangles": sum(1 for value in dots if value < 0.2),
        "minDot": min(dots) if dots else 0.0,
        "maxDot": max(dots) if dots else 0.0,
        "averageDot": sum(dots) / len(dots) if dots else 0.0,
    }


def main():
    parser = argparse.ArgumentParser(description="Compare Unity and Unreal final conversions for an OpenClaw GN mesh cache.")
    parser.add_argument("--cache", required=True, help="Raw sidecar cache path (.json or .bin)")
    parser.add_argument("--scale", type=float, default=100.0, help="Blender-to-Unreal scale used by the UE component")
    parser.add_argument("--out", help="Optional JSON report path")
    args = parser.parse_args()

    cache = load_cache(args.cache)
    report = {
        "objectName": cache.get("objectName", ""),
        "raw": {
            "vertices": len(cache.get("vertices", [])) // 3,
            "triangles": len(cache.get("triangles", [])) // 3,
            "normals": len(cache.get("normals", [])) // 3,
            "uvs": len(cache.get("uvs", [])) // 2,
        },
    }
    for mode in ("unity", "ue"):
        vertices, normals, triangles = convert_arrays(cache, mode, args.scale)
        raw_stats = dot_stats(vertices, normals, triangles)
        aligned_triangles, flips = align_winding(vertices, normals, triangles)
        aligned_stats = dot_stats(vertices, normals, aligned_triangles)
        report[mode] = {
            "beforeAlignment": raw_stats,
            "afterAlignment": aligned_stats,
            "alignmentFlips": flips,
        }

    text = json.dumps(report, indent=2)
    if args.out:
        Path(args.out).parent.mkdir(parents=True, exist_ok=True)
        Path(args.out).write_text(text, encoding="utf-8")
    print(text)


if __name__ == "__main__":
    main()
