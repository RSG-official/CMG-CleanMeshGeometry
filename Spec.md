# CMG Format Specification — v1.0

This document defines the binary layout of the CMG (Clean Mesh Geometry) format:
the core `.cmg` geometry file and its optional `.cmgex` sidecar. It is the
authoritative reference for anyone implementing a CMG reader or writer in any
language.

---

## 1. Byte Order

All multi-byte values are **little-endian**. This applies to every integer and
float in both `.cmg` and `.cmgex`. There is currently no support for big-endian
platforms; a big-endian host must byte-swap on load/save.

---

## 2. File Header

Every CMG-family file (`.cmg` and `.cmgex`) begins with a fixed 12-byte header:

| Offset | Size | Field         | Type     | Notes                          |
|--------|------|---------------|----------|---------------------------------|
| 0      | 4    | `magic`       | char[4]  | `"CMG\0"` for `.cmg`, `"CMGX"` for `.cmgex` |
| 4      | 4    | `version`     | uint32   | Format version. Currently `0`.  |
| 8      | 4    | `chunk_count` | uint32   | Number of chunks that follow.   |

---

## 3. Chunk Structure

Immediately following the header, `chunk_count` chunks appear back-to-back.
Each chunk has this layout:

| Offset | Size | Field   | Type    | Notes                          |
|--------|------|---------|---------|----------------------------------|
| 0      | 4    | `id`    | char[4] | ASCII chunk identifier, NOT null-terminated |
| 4      | 4    | `size`  | uint32  | Byte length of `data` that follows |
| 8      | size | `data`  | bytes   | Chunk payload, format depends on `id` |

**Reader rule:** an unrecognized `id` MUST be skipped (using `size` to jump past
its `data`), not treated as an error. This is what allows the format to gain
new chunk types in future versions without breaking older readers.

---

## 4. Chunk IDs

### `.cmg` (core geometry) — always written in this order:

| ID     | Required? | Contents                                  |
|--------|-----------|--------------------------------------------|
| `BBOX` | **Yes**   | Bounding box (min/max corners)             |
| `VERT` | **Yes**   | Vertex positions + normals                 |
| `INDX` | **Yes**   | Triangle indices                           |
| `EXRF` | Optional  | Sidecar filename reference. Present if and only if a `.cmgex` sidecar was written alongside this file. |

### `.cmgex` (sidecar) — always written in this order, **as a single unit**:

| ID     | Required?              | Contents                              |
|--------|-------------------------|----------------------------------------|
| `OBJS` | Yes, if sidecar exists  | Named object → vertex/triangle range map |
| `MTRL` | Yes, if sidecar exists  | Material definitions                   |
| `MIDX` | Yes, if sidecar exists  | Per-triangle material index             |
| `VCOL` | Yes, if sidecar exists  | Per-vertex color (may be zero-length if no vertex colors exist) |
| `UVMP` | Yes, if sidecar exists  | Pool of unique UV coordinates (may be zero-length if no UVs exist) |
| `UVIX` | Yes, if sidecar exists  | Per-triangle-corner index into `UVMP` (may be zero-length) |
| `TXTR` | Yes, if sidecar exists  | Texture pool (`count` may be `0`)      |

**Important distinction:** the `.cmgex` file itself is optional — a `.cmg`
with no `EXRF` chunk has no sidecar and no materials/UVs/colors/textures. But
if a sidecar *does* exist, all 7 chunks above are present in it, even if some
contain zero entries. There is no case where, e.g., `MTRL` exists but `MIDX`
doesn't.

---

## 5. Ordering Rules

- Chunks in `.cmg` MUST appear in the order `BBOX`, `VERT`, `INDX`, `[EXRF]`.
- Chunks in `.cmgex` MUST appear in the order `OBJS`, `MTRL`, `MIDX`, `VCOL`, `UVMP`, `UVIX`, `TXTR`.
- **Readers should not rely on position** — always dispatch by `id`, not index — but writers targeting v1.0 compatibility should preserve this order for consistency across implementations.

---

## 6. Binary Layout of Every Structure

### `BBOX`
6 × float32: `min.x, min.y, min.z, max.x, max.y, max.z` (24 bytes total)

### `VERT`
Flat array of float32, 6 per vertex: `pos.x, pos.y, pos.z, normal.x, normal.y, normal.z`.
Vertex count = `size / 24`.

### `INDX`
Flat array of uint32, 3 per triangle. Triangle count = `size / 12`.

### `EXRF`
One length-prefixed string (see §7): the sidecar's filename, resolved relative
to the directory containing the `.cmg` file.

### `OBJS`
```
uint32   object_count
repeated object_count times:
    string   name
    uint32   vstart   (first vertex index belonging to this object)
    uint32   vcount   (number of vertices)
    uint32   tstart   (first triangle index belonging to this object)
    uint32   tcount   (number of triangles)
```

### `MTRL`
```
uint32   material_count
repeated material_count times:
    string   name
    float32  base_color[4]       (RGBA)
    float32  metallic
    float32  roughness
    float32  emission_color[3]   (RGB)
    float32  emission_strength
    repeated 5 times (fixed roles: base_color, normal, roughness, metallic, emission):
        uint32   texture_index   (0xFFFFFFFF = no texture assigned)
        float32  uv_offset[2]
        float32  uv_scale[2]
        float32  uv_rotation
```
Per-material size: variable (due to name) + 16 + 4 + 4 + 12 + 4 + (5 × 24) = variable + 160 bytes.

### `MIDX`
Flat array of uint16, one per triangle: index into the `MTRL` material list.
Triangle count = `size / 2`.

### `VCOL`
Flat array of float32, 4 per vertex (RGBA). Count = `size / 16`.

### `UVMP`
Flat array of float32, 2 per UV coordinate (U, V). Count = `size / 8`.

### `UVIX`
Flat array of uint32, one per triangle-corner: index into `UVMP`.
Count = `size / 4` (= 3 × triangle_count).

### `TXTR`
```
uint32   texture_count
repeated texture_count times:
    string   name
    uint8    role      (0=base_color, 1=normal, 2=roughness, 3=metallic, 4=emission)
    uint8    mode       (0=embedded, 1=external)
    if mode == embedded:
        uint32   byte_length
        bytes    raw_image_bytes   (e.g. PNG/JPEG data, as-is)
    if mode == external:
        string   path
```

### String encoding (used by `EXRF`, `OBJS` names, `MTRL` names, `TXTR` names/paths)
```
uint16   byte_length   (NOT character count — UTF-8 byte length)
bytes    utf8_data      (no null terminator)
```

---

## 7. Limits

These are not currently enforced by the reference reader/writer, but are
documented here as the practical ceilings implied by the field types chosen:

| Quantity                     | Limit                  | Reason                          |
|-------------------------------|--------------------------|-----------------------------------|
| Vertices per file              | 4,294,967,295 (uint32)   | `VERT`/`INDX` indices are uint32 |
| Triangles per file              | 4,294,967,295 (uint32)   | `INDX` count field is uint32     |
| Materials per file               | 65,535 (uint16)          | `MIDX` stores material index as uint16 |
| String length (names/paths)        | 65,535 bytes (uint16)    | String length prefix is uint16   |
| Texture roles per material         | Fixed at 5               | Format defines exactly 5 fixed slots |
| Embedded texture size                | 4,294,967,295 bytes (uint32) | `TXTR` byte_length field is uint32 |

**Practical recommendation for implementers:** treat >65,535 materials or
>65,535-byte names as malformed/unsupported rather than attempting to handle
overflow, since these ceilings are far beyond any reasonable real-world asset.

---

## 8. Version History

| Version | Status                | Notes |
|---------|--------------------------|-------|
| 0       | Current (development)   | Format may still change before v1.0 is finalized. No backward-compatibility guarantees yet. |

Once this document's contents are locked and implemented consistently across
the Blender addon and C++ library, version should be bumped to `1` and this
spec frozen as CMG v1.0.
