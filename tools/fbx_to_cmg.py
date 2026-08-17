#!/usr/bin/env python3
"""
fbx_to_cmg.py -- Convert an FBX file to CMG (.cmg + .cmgex) using assimp.

Mirrors the exact binary layout produced by cmg-blender-addon/addon/cmg.py
so output from this converter is byte-layout-compatible with both that
addon's importer and the CMG C++ library. If you change this file's chunk
packing, keep it in sync with that addon (and the format Spec.md).

Requires: pip install pyassimp --break-system-packages
          plus the system assimp library (e.g. apt install libassimp-dev)

Usage:
    python3 fbx_to_cmg.py input.fbx [output.cmg] [--embed-textures]
"""
import sys
import os
import struct

import pyassimp
import pyassimp.postprocess as pp

# ---------------------------------------------------------------------------
# CMG format constants (mirrors cmg-blender-addon/addon/cmg.py -- keep in sync)
# ---------------------------------------------------------------------------

CMG_MAGIC = b"CMG\x00"
CMG_VERSION = 0
CMGEX_MAGIC = b"CMGX"
CMGEX_VERSION = 0
CMGEX_EXT = ".cmgex"

TEXTURE_ROLE_BASE_COLOR = 0
TEXTURE_ROLE_NORMAL = 1
TEXTURE_ROLE_ROUGHNESS = 2
TEXTURE_ROLE_METALLIC = 3
TEXTURE_ROLE_EMISSION = 4
TEXTURE_ROLES = (
    TEXTURE_ROLE_BASE_COLOR,
    TEXTURE_ROLE_NORMAL,
    TEXTURE_ROLE_ROUGHNESS,
    TEXTURE_ROLE_METALLIC,
    TEXTURE_ROLE_EMISSION,
)

TEXTURE_MODE_EMBEDDED = 0
TEXTURE_MODE_EXTERNAL = 1
NO_TEXTURE = 0xFFFFFFFF

# assimp aiTextureType values, hardcoded to match the installed assimp C
# library's enum (assimp 5.x). The pyassimp Python package bundles an older
# header snapshot that predates the PBR texture-type additions (BASE_COLOR,
# METALNESS, DIFFUSE_ROUGHNESS), but the underlying .so still understands
# these integer values, so we reference them directly rather than relying
# on pyassimp.postprocess/material's (stale) constant list.
AI_TEX_DIFFUSE = 1
AI_TEX_EMISSIVE = 4
AI_TEX_NORMALS = 6
AI_TEX_BASE_COLOR = 12
AI_TEX_METALNESS = 15
AI_TEX_DIFFUSE_ROUGHNESS = 16

ROLE_TO_AI_TEX_TYPES = {
    TEXTURE_ROLE_BASE_COLOR: (AI_TEX_DIFFUSE, AI_TEX_BASE_COLOR),
    TEXTURE_ROLE_NORMAL: (AI_TEX_NORMALS,),
    TEXTURE_ROLE_ROUGHNESS: (AI_TEX_DIFFUSE_ROUGHNESS,),
    TEXTURE_ROLE_METALLIC: (AI_TEX_METALNESS,),
    TEXTURE_ROLE_EMISSION: (AI_TEX_EMISSIVE,),
}


# ---------------------------------------------------------------------------
# Binary packing helpers (identical layout to cmg.py's pack_string/write_chunk)
# ---------------------------------------------------------------------------

def pack_string(s):
    data = s.encode("utf-8")
    if len(data) > 0xFFFF:
        raise ValueError(f"String too long to encode ({len(data)} bytes): {s!r}")
    return struct.pack("<H", len(data)) + data


def _chunk_bytes(chunk_id, data):
    cid = chunk_id.encode("ascii")
    assert len(cid) == 4, f"chunk id must be 4 bytes, got {chunk_id!r}"
    return cid + struct.pack("<I", len(data)) + data


def write_chunked_file(path, magic, version, chunks):
    tmp_path = path + ".tmp"
    with open(tmp_path, "wb") as f:
        f.write(magic)
        f.write(struct.pack("<II", version, len(chunks)))
        for chunk_id, data in chunks:
            f.write(_chunk_bytes(chunk_id, data))
    os.replace(tmp_path, path)  # atomic-ish; avoid leaving a partial file on failure


# ---------------------------------------------------------------------------
# Conversion
# ---------------------------------------------------------------------------

def _find_texture(mat, role):
    """Return (path, uv_offset, uv_scale, uv_rotation) for the first assimp
    texture type mapped to `role` that this material actually has, or
    (None, ...) if none is set."""
    for ai_type in ROLE_TO_AI_TEX_TYPES[role]:
        path = mat.properties.get(("file", ai_type))
        if path:
            trafo = mat.properties.get(("uvtrafo", ai_type))
            if trafo and len(trafo) >= 5:
                return path, (trafo[0], trafo[1]), (trafo[2], trafo[3]), trafo[4]
            return path, (0.0, 0.0), (1.0, 1.0), 0.0
    return None, (0.0, 0.0), (1.0, 1.0), 0.0


def convert(fbx_path, cmg_path=None, texture_mode=TEXTURE_MODE_EXTERNAL):
    if not os.path.isfile(fbx_path):
        raise FileNotFoundError(fbx_path)

    if cmg_path is None:
        cmg_path = os.path.splitext(fbx_path)[0] + ".cmg"
    sidecar_path = os.path.splitext(cmg_path)[0] + CMGEX_EXT
    sidecar_name = os.path.basename(sidecar_path)
    fbx_dir = os.path.dirname(os.path.abspath(fbx_path))

    flags = pp.aiProcess_Triangulate | pp.aiProcess_GenNormals

    with pyassimp.load(fbx_path, processing=flags) as scene:
        if not scene.meshes:
            raise ValueError(f"{fbx_path}: no meshes found")

        global_vertices = []   # flat pos.xyz + normal.xyz per vertex (6 floats/vert)
        global_indices = []
        global_colors = []     # flat rgba per vertex (4 floats/vert)
        global_uv_pool = []
        global_uv_lookup = {}
        global_uv_indices = []
        material_indices = []  # per-triangle, global
        object_ranges = []     # (name, vstart, vcount, tstart, tcount)

        bmin = [float("inf")] * 3
        bmax = [float("-inf")] * 3

        for mesh_i, mesh in enumerate(scene.meshes):
            vstart = len(global_vertices) // 6
            tstart = len(material_indices)
            vcount = len(mesh.vertices)

            has_uv = mesh.texturecoords is not None and len(mesh.texturecoords) > 0
            has_color = mesh.colors is not None and len(mesh.colors) > 0

            for vi in range(vcount):
                px, py, pz = (float(c) for c in mesh.vertices[vi])
                if mesh.normals is not None:
                    nx, ny, nz = (float(c) for c in mesh.normals[vi])
                else:
                    nx, ny, nz = 0.0, 0.0, 1.0
                global_vertices.extend((px, py, pz, nx, ny, nz))

                for axis, val in enumerate((px, py, pz)):
                    if val < bmin[axis]:
                        bmin[axis] = val
                    if val > bmax[axis]:
                        bmax[axis] = val

                if has_color:
                    c = mesh.colors[0][vi]
                    r, g, b = float(c[0]), float(c[1]), float(c[2])
                    a = float(c[3]) if len(c) > 3 else 1.0
                else:
                    r, g, b, a = 1.0, 1.0, 1.0, 1.0
                global_colors.extend((r, g, b, a))

            for face in mesh.faces:
                for local_idx in face:
                    global_indices.append(int(local_idx) + vstart)

                for local_idx in face:
                    if has_uv:
                        uv0 = mesh.texturecoords[0][int(local_idx)]
                        key = (round(float(uv0[0]), 6), round(float(uv0[1]), 6))
                    else:
                        key = (0.0, 0.0)
                    idx = global_uv_lookup.get(key)
                    if idx is None:
                        idx = len(global_uv_lookup)
                        global_uv_lookup[key] = idx
                        global_uv_pool.extend(key)
                    global_uv_indices.append(idx)

                material_indices.append(int(mesh.materialindex))

            name = mesh.name or f"Mesh{mesh_i}"
            object_ranges.append((name, vstart, vcount, tstart, len(mesh.faces)))

        vertex_count = len(global_vertices) // 6
        triangle_count = len(global_indices) // 3

        if vertex_count == 0:
            bmin = bmax = [0.0, 0.0, 0.0]

        # ---- core .cmg ----
        cmg_chunks = [
            ("BBOX", struct.pack("<6f", *bmin, *bmax)),
            ("VERT", struct.pack(f"<{len(global_vertices)}f", *global_vertices)),
            ("INDX", struct.pack(f"<{len(global_indices)}I", *global_indices)),
            ("EXRF", pack_string(sidecar_name)),
        ]
        write_chunked_file(cmg_path, CMG_MAGIC, CMG_VERSION, cmg_chunks)

        # ---- sidecar .cmgex ----
        texture_pool = []
        texture_lookup = {}

        def texture_index(path, role):
            if not path:
                return NO_TEXTURE
            key = (path, role, texture_mode)
            if key in texture_lookup:
                return texture_lookup[key]
            idx = len(texture_pool)
            texture_lookup[key] = idx
            tex_name = os.path.basename(path)
            if texture_mode == TEXTURE_MODE_EMBEDDED:
                full = path if os.path.isabs(path) else os.path.join(fbx_dir, path)
                try:
                    with open(full, "rb") as tf:
                        img_bytes = tf.read()
                except OSError as e:
                    print(f"warning: could not read texture '{full}' ({e}); "
                          f"embedding 0 bytes", file=sys.stderr)
                    img_bytes = b""
                payload = (pack_string(tex_name) + struct.pack("<BB", role, texture_mode)
                           + struct.pack("<I", len(img_bytes)) + img_bytes)
            else:
                payload = (pack_string(tex_name) + struct.pack("<BB", role, texture_mode)
                           + pack_string(path))
            texture_pool.append(payload)
            return idx

        mtrl_data = struct.pack("<I", len(scene.materials))
        for mat in scene.materials:
            name = mat.properties.get(("name", 0), "")
            diffuse = mat.properties.get(("diffuse", 0), (1.0, 1.0, 1.0))
            opacity = float(mat.properties.get(("opacity", 0), 1.0))
            base_color = (float(diffuse[0]), float(diffuse[1]), float(diffuse[2]), opacity)
            # Classic FBX materials (Phong/Lambert) have no metallic/roughness
            # concept; only glTF-derived materials set these. Default to a
            # reasonable non-metal mid-roughness look when absent.
            metallic = float(mat.properties.get(("metallicFactor", 0), 0.0))
            roughness = float(mat.properties.get(("roughnessFactor", 0), 0.5))
            emissive = mat.properties.get(("emissive", 0), (0.0, 0.0, 0.0))
            emissive = tuple(float(c) for c in emissive)

            data = (pack_string(name)
                    + struct.pack("<4f", *base_color)
                    + struct.pack("<f", metallic)
                    + struct.pack("<f", roughness)
                    + struct.pack("<3f", *emissive)
                    + struct.pack("<f", 0.0))  # emission strength: not in classic FBX

            for role in TEXTURE_ROLES:
                path, uv_offset, uv_scale, uv_rot = _find_texture(mat, role)
                idx = texture_index(path, role)
                data += (struct.pack("<I", idx)
                         + struct.pack("<2f", *uv_offset)
                         + struct.pack("<2f", *uv_scale)
                         + struct.pack("<f", uv_rot))
            mtrl_data += data

        objs_data = struct.pack("<I", len(object_ranges))
        for name, vstart, vcount, tstart, tcount in object_ranges:
            objs_data += pack_string(name) + struct.pack("<4I", vstart, vcount, tstart, tcount)

        midx_data = struct.pack(f"<{len(material_indices)}H", *material_indices)
        vcol_data = struct.pack(f"<{len(global_colors)}f", *global_colors)
        uvmp_data = struct.pack(f"<{len(global_uv_pool)}f", *global_uv_pool)
        uvix_data = struct.pack(f"<{len(global_uv_indices)}I", *global_uv_indices)
        txtr_data = struct.pack("<I", len(texture_pool)) + b"".join(texture_pool)

        cmgex_chunks = [
            ("OBJS", objs_data),
            ("MTRL", mtrl_data),
            ("MIDX", midx_data),
            ("VCOL", vcol_data),
            ("UVMP", uvmp_data),
            ("UVIX", uvix_data),
            ("TXTR", txtr_data),
        ]
        write_chunked_file(sidecar_path, CMGEX_MAGIC, CMGEX_VERSION, cmgex_chunks)

        n_materials = len(scene.materials)

    print(f"Converted {fbx_path}:")
    print(f"  vertices: {vertex_count}  triangles: {triangle_count}")
    print(f"  objects: {len(object_ranges)}  materials: {n_materials}  "
          f"textures: {len(texture_pool)} ({'embedded' if texture_mode == TEXTURE_MODE_EMBEDDED else 'external'})")
    print(f"  -> {cmg_path}")
    print(f"  -> {sidecar_path}")
    return cmg_path, sidecar_path


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: fbx_to_cmg.py input.fbx [output.cmg] [--embed-textures]", file=sys.stderr)
        sys.exit(1)

    args = sys.argv[1:]
    embed = "--embed-textures" in args
    positional = [a for a in args if not a.startswith("--")]
    in_path = positional[0]
    out_path = positional[1] if len(positional) > 1 else None

    try:
        convert(in_path, out_path, TEXTURE_MODE_EMBEDDED if embed else TEXTURE_MODE_EXTERNAL)
    except Exception as e:
        print(f"error: {e}", file=sys.stderr)
        sys.exit(1)
