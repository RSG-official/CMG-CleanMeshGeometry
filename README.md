# CMG (Clean Mesh Geometry)

> A modern, chunk-based binary mesh format designed for fast loading, extensibility, and clean asset pipelines.

CMG is a lightweight 3D mesh format that separates **core geometry** from **asset metadata**, making it suitable for game engines, rendering applications, asset pipelines, and custom tooling.

The project currently consists of:

- **CMG** – Core geometry file (`.cmg`)
- **CMGEX** – Sidecar metadata file (`.cmgex`)
- Native C++ reader/writer library
- Blender Import/Export addon

---

## Why CMG?

Many existing formats are designed to solve every possible use case.

CMG has a different philosophy:

- Simple
- Fast
- Easy to extend
- Easy to parse
- Versioned
- Chunk-based

The format is intended to be easy to implement in any language while remaining flexible enough for future expansion.

---

# Features

### Core Geometry (.cmg)

- Vertex positions
- Vertex normals
- Triangle indices
- Bounding box
- Versioned binary format
- Chunk-based storage

### Metadata (.cmgex)

- Multi-object support
- Object names
- Materials
- UV coordinates
- Vertex colors
- PBR material properties
- Texture references
- Embedded textures
- External textures
- UV transforms

---

# File Structure

```
model.cmg
│
├── BBOX
├── VERT
├── INDX
└── EXRF (optional)
```

```
model.cmgex
│
├── OBJS
├── MTRL
├── MIDX
├── UVMP
├── UVIX
├── VCOL
└── TXTR
```

---

# Why Two Files?

Geometry changes infrequently.

Metadata changes often.

Separating them provides several advantages:

- Smaller geometry files
- Faster loading
- Optional metadata
- Easier streaming
- Better asset management

Applications that only need geometry can ignore `.cmgex` entirely.

---

# Blender Add-on

https://github.com/RSG-official/cmg-blender-addon

 Above repository includes a Blender add-on supporting:

- Import `.cmg`
- Export `.cmg`
- Multi-object export
- Apply modifiers
- World-space export
- Custom normals
- UVs
- Vertex colors
- Materials
- Embedded textures
- External textures

---

# C++ Library

The repository provides a native C++ implementation for reading CMG files. Write support is planned but not yet implemented.

The library is intentionally lightweight and has minimal dependencies, making it easy to integrate into custom engines or tools.

---

# Design Goals

- Clean binary layout
- Chunk-based extensibility
- Fast loading
- Small runtime footprint
- Human-maintainable implementation
- Long-term format evolution

---

# Current Status

CMG is currently under active development.

The binary format is still evolving and may change until the first stable release.

Current format version:

```
Version 0 (Development)
```

---

# Planned Features

- Skeletal animation
- Skinning
- Morph targets
- Level of Detail (LOD)
- Collision meshes
- Scene serialization
- Mesh compression
- Instancing
- Animation clips

---

# Example

```cpp
CmgMesh mesh;

if (mesh.load("model.cmg"))
{
    // Ready to use
}
```

---

# Repository Structure

```
CMG
├── include/
├── src/
├── examples/
├── blender_addon/
├── LICENSE
└── README.md
```

---

# Project Goals

CMG is intended to become a modern, lightweight asset format suitable for:

- Game engines
- Rendering engines
- Visualization software
- CAD viewers
- Custom asset pipelines
- Research projects

---

# Contributing

Contributions, suggestions, bug reports, and feature requests are welcome.

If you'd like to improve the format or tooling, feel free to open an issue or submit a pull request.

---

# License

See the [LICENSE](LICENSE) file for licensing information.
