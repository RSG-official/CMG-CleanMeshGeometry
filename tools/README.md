# Converters

Standalone scripts that convert other 3D formats into `.cmg`/`.cmgex`,
independent of the Blender addon. Useful for batch conversion, CI pipelines,
or environments where running Blender isn't practical.

Every converter here must produce byte-for-byte the same chunk layout as
[`cmg-blender-addon`](https://github.com/RSG-official/cmg-blender-addon)'s
`export_cmg`, and output should be verified against this repo's C++ reader,
not just "does the addon accept it back." See each script's docstring for
specifics.

## fbx_to_cmg.py

Converts an FBX file to `.cmg` + `.cmgex` using [assimp](https://github.com/assimp/assimp)
for FBX parsing.

### Setup

```bash
apt install libassimp-dev        # or your platform's equivalent
pip install pyassimp --break-system-packages
```

### Usage

```bash
python3 tools/fbx_to_cmg.py input.fbx                    # -> input.cmg, input.cmgex, external texture refs
python3 tools/fbx_to_cmg.py input.fbx output.cmg          # explicit output path
python3 tools/fbx_to_cmg.py input.fbx --embed-textures    # embed texture bytes instead of referencing paths
```

### What's supported

- Geometry: positions, normals (generated if missing), triangulated faces
  (n-gons/quads are triangulated automatically)
- Multiple objects, preserved as `OBJS` ranges with original mesh names
- Materials: base color + opacity, emissive color. Metallic/roughness are
  written with sane defaults (0.0 / 0.5) since classic FBX materials
  (Phong/Lambert) don't model them — only glTF-derived FBX materials will
  populate these correctly
- Textures: diffuse/base-color, normal, emissive. Roughness/metalness
  textures only if the source FBX uses glTF-style PBR material properties
  (rare in classic FBX exports)
- UV coordinates (first UV channel only), globally deduplicated
- Vertex colors (first color channel), defaulting to white where absent

### Known limitations

- Only the first UV channel and first vertex-color channel are read; a
  second UV set or color layer in the source file is silently ignored
- No skeleton/skinning/animation support (matches the current `.cmg`/`.cmgex`
  format — see the addon repo's roadmap; `SKEL` is spec-only, not implemented
  anywhere yet)
- Material property extraction relies on hardcoded assimp `aiTextureType`
  integer values for PBR slots (see comments in the script) since the
  `pyassimp` package's own Python-side constants predate those additions;
  if you upgrade assimp and something seems off with roughness/metalness
  textures, check those constants first
- Not tested against every FBX exporter's quirks (tested here against
  assimp's own FBX exporter as a stand-in; if you hit a real-world FBX file
  that doesn't convert cleanly, please file an issue with the file if you
  can share it)

### Verifying output

Build and run this repo's example against the converted file:

```bash
g++ -std=c++17 -Iinclude examples/main.cpp src/cmg_format.cpp -o test_cmg
./test_cmg output.cmg
```

Counts printed (vertices, triangles, objects, materials, textures) should
match what the converter printed on the way out.
