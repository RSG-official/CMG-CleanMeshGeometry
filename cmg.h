/*  cmg.h  —  CMG (Clean Mesh Geometry) single-header reader library
 *
 *  USAGE
 *  -----
 *  In exactly ONE .c / .cpp file, before #include:
 *
 *      #define CMG_IMPLEMENTATION
 *      #include "cmg.h"
 *
 *  All other files just do:
 *
 *      #include "cmg.h"
 *
 *  QUICK EXAMPLE
 *  -------------
 *      char err[256];
 *      CMGMesh *mesh = cmg_load("model.cmg", err, sizeof(err));
 *      if (!mesh) { fprintf(stderr, "CMG error: %s\n", err); return 1; }
 *
 *      printf("Vertices:  %u\n", mesh->vertex_count);
 *      printf("Triangles: %u\n", mesh->triangle_count);
 *
 *      for (uint32_t i = 0; i < mesh->vertex_count; i++) {
 *          CMGVertex v = mesh->vertices[i];
 *          // use v.px, v.py, v.pz  (position)
 *          //     v.nx, v.ny, v.nz  (normal, unit-length)
 *      }
 *      for (uint32_t t = 0; t < mesh->triangle_count; t++) {
 *          uint32_t a = mesh->indices[t*3+0];
 *          uint32_t b = mesh->indices[t*3+1];
 *          uint32_t c = mesh->indices[t*3+2];
 *      }
 *
 *      cmg_free(mesh);
 *
 *  FORMAT NOTES
 *  ------------
 *  A .cmg file is a 12-byte little-endian header followed by N chunks:
 *
 *      Offset  Size  Field
 *      0       4     Magic: "CMG\x00"
 *      4       4     Version (uint32) — this library reads version 0
 *      8       4     ChunkCount (uint32)
 *
 *  Each chunk:
 *      0       4     ChunkID (4 ASCII bytes)
 *      4       4     ChunkSize in bytes, payload only (uint32)
 *      8       N     Payload
 *
 *  Standard chunks parsed by this library:
 *      BBOX  — 6 floats: MinX,MinY,MinZ, MaxX,MaxY,MaxZ
 *      VERT  — N * 6 floats: px,py,pz, nx,ny,nz per vertex
 *      INDX  — N * uint32 triangle vertex indices (3 per triangle)
 *      EXRF  — uint16 length + UTF-8 bytes: sidecar filename (no path)
 *
 *  Unknown chunks are skipped (forward-compatibility rule).
 *
 *  FILE FORMAT VERSION
 *  -------------------
 *  CMG_FORMAT_VERSION == 0  means development/unstable.
 *  This library rejects any file whose version field != CMG_FORMAT_VERSION.
 *  When the spec goes public at version 1, bump both constants together.
 *
 *  ENDIANNESS
 *  ----------
 *  The format is little-endian. On big-endian hosts the library byte-swaps
 *  all multi-byte values automatically. On little-endian hosts (x86/ARM
 *  in LE mode, which covers Windows, Linux, macOS on modern hardware) no
 *  swapping is done.
 *
 *  LICENSE
 *  -------
 *  MIT — see bottom of file.
 */

#ifndef CMG_H
#define CMG_H

#include <stdint.h>   /* uint32_t, uint16_t */
#include <stddef.h>   /* size_t              */

/* -------------------------------------------------------------------------
 * Public constants
 * ---------------------------------------------------------------------- */

#define CMG_FORMAT_VERSION  0u      /* file-format version this lib reads  */
#define CMG_NO_SIDECAR      ""      /* mesh->sidecar_name when EXRF absent */

/* -------------------------------------------------------------------------
 * Public types
 * ---------------------------------------------------------------------- */

/** One vertex as stored in the VERT chunk (position + unit-length normal). */
typedef struct CMGVertex {
    float px, py, pz;   /* world-space position */
    float nx, ny, nz;   /* surface normal (unit-length) */
} CMGVertex;

/**
 * A fully-loaded CMG mesh.
 * Allocate with cmg_load(), release with cmg_free().
 * All pointer fields are NULL if the corresponding chunk was absent or empty.
 */
typedef struct CMGMesh {
    /* Core geometry — always present on a successful load */
    uint32_t    vertex_count;
    uint32_t    triangle_count;
    CMGVertex  *vertices;       /* [vertex_count]     */
    uint32_t   *indices;        /* [triangle_count*3] */

    /* Bounding box — present when the file contains a BBOX chunk */
    int         has_bbox;
    float       bbox_min[3];    /* MinX, MinY, MinZ */
    float       bbox_max[3];    /* MaxX, MaxY, MaxZ */

    /* Sidecar reference — set when the file contains an EXRF chunk.
     * Contains only the filename (e.g. "model.cmgex"), not a full path.
     * Look for the sidecar next to the .cmg file. */
    char        sidecar_name[256];
} CMGMesh;

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Load a .cmg file from disk.
 *
 * @param filepath   Path to the .cmg file (UTF-8 on all platforms).
 * @param err        Caller-supplied buffer for an error message on failure.
 * @param err_size   Size of the error buffer in bytes.
 * @return           Pointer to a heap-allocated CMGMesh on success,
 *                   NULL on failure (err is populated in that case).
 *
 * The returned mesh must be released with cmg_free().
 */
CMGMesh *cmg_load(const char *filepath, char *err, size_t err_size);

/**
 * Release all memory owned by a CMGMesh returned from cmg_load().
 * Passing NULL is safe (no-op).
 */
void cmg_free(CMGMesh *mesh);

/**
 * Return the file-format version this library was compiled to read.
 * Useful for runtime assertions in host applications.
 */
uint32_t cmg_format_version(void);

#ifdef __cplusplus
}
#endif


/* =========================================================================
 * IMPLEMENTATION
 * ======================================================================= */

#ifdef CMG_IMPLEMENTATION

#if defined(__GNUC__) || defined(__clang__)
    #define CMG__MAYBE_UNUSED __attribute__((unused))
#else
    #define CMG__MAYBE_UNUSED
#endif

#include <stdio.h>    /* FILE, fopen, fread, fclose */
#include <stdlib.h>   /* malloc, free               */
#include <string.h>   /* memcmp, memcpy, memset, snprintf */

/* ---- endianness -------------------------------------------------------- */

/* Detect little-endian at compile time.
 * Covers MSVC (_M_IX86 / _M_X64 / _M_ARM*), GCC/Clang (__BYTE_ORDER__). */
#if defined(_MSC_VER)
    /* MSVC always targets LE architectures it supports (x86, x64, ARM). */
    #define CMG_LITTLE_ENDIAN 1
#elif defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        #define CMG_LITTLE_ENDIAN 1
    #else
        #define CMG_LITTLE_ENDIAN 0
    #endif
#else
    /* Conservative fallback: assume LE. If you target BE hardware (SPARC,
     * old MIPS, etc.) define CMG_LITTLE_ENDIAN=0 manually. */
    #define CMG_LITTLE_ENDIAN 1
#endif

CMG__MAYBE_UNUSED static uint32_t cmg__swap32(uint32_t x) {
    return ((x & 0xFF000000u) >> 24)
         | ((x & 0x00FF0000u) >>  8)
         | ((x & 0x0000FF00u) <<  8)
         | ((x & 0x000000FFu) << 24);
}
CMG__MAYBE_UNUSED static uint16_t cmg__swap16(uint16_t x) {
    return (uint16_t)(((x & 0xFF00u) >> 8) | ((x & 0x00FFu) << 8));
}

/* Read a uint32 from raw bytes, applying LE->host byte-swap if needed. */
static uint32_t cmg__read_u32(const unsigned char *p) {
    uint32_t v;
    memcpy(&v, p, 4);
#if !CMG_LITTLE_ENDIAN
    v = cmg__swap32(v);
#endif
    return v;
}

static uint16_t cmg__read_u16(const unsigned char *p) {
    uint16_t v;
    memcpy(&v, p, 2);
#if !CMG_LITTLE_ENDIAN
    v = cmg__swap16(v);
#endif
    return v;
}

static float cmg__read_f32(const unsigned char *p) {
    float v;
#if CMG_LITTLE_ENDIAN
    memcpy(&v, p, 4);
#else
    uint32_t u;
    memcpy(&u, p, 4);
    u = cmg__swap32(u);
    memcpy(&v, &u, 4);
#endif
    return v;
}

/* ---- internal error helper --------------------------------------------- */

static void cmg__err(char *buf, size_t n, const char *msg) {
    if (buf && n > 0) {
        strncpy(buf, msg, n - 1);
        buf[n - 1] = '\0';
    }
}

/* ---- magic ------------------------------------------------------------- */

static const unsigned char CMG__MAGIC[4] = { 'C', 'M', 'G', '\0' };

/* ---- file reading helpers ---------------------------------------------- */

/* Read exactly `n` bytes from `f` into `dst`. Returns 1 on success. */
static int cmg__read(FILE *f, void *dst, size_t n) {
    return fread(dst, 1, n, f) == n;
}

/* Skip exactly `n` bytes forward in `f`. Returns 1 on success. */
static int cmg__skip(FILE *f, uint32_t n) {
    return fseek(f, (long)n, SEEK_CUR) == 0;
}

/* ---- VERT chunk parser ------------------------------------------------- */

static CMGVertex *cmg__parse_vert(const unsigned char *data, uint32_t size,
                                   uint32_t *out_count) {
    *out_count = 0;
    if (size % (6 * 4) != 0) return NULL;   /* must be a whole number of vertices */
    uint32_t count = size / (6 * 4);
    if (count == 0) return NULL;

    CMGVertex *verts = (CMGVertex *)malloc(count * sizeof(CMGVertex));
    if (!verts) return NULL;

    const unsigned char *p = data;
    for (uint32_t i = 0; i < count; i++) {
        verts[i].px = cmg__read_f32(p +  0);
        verts[i].py = cmg__read_f32(p +  4);
        verts[i].pz = cmg__read_f32(p +  8);
        verts[i].nx = cmg__read_f32(p + 12);
        verts[i].ny = cmg__read_f32(p + 16);
        verts[i].nz = cmg__read_f32(p + 20);
        p += 24;
    }
    *out_count = count;
    return verts;
}

/* ---- INDX chunk parser ------------------------------------------------- */

static uint32_t *cmg__parse_indx(const unsigned char *data, uint32_t size,
                                   uint32_t *out_triangle_count) {
    *out_triangle_count = 0;
    if (size % (3 * 4) != 0) return NULL;  /* must be complete triangles */
    uint32_t index_count = size / 4;
    if (index_count == 0) return NULL;

    uint32_t *idx = (uint32_t *)malloc(index_count * sizeof(uint32_t));
    if (!idx) return NULL;

    const unsigned char *p = data;
    for (uint32_t i = 0; i < index_count; i++) {
        idx[i] = cmg__read_u32(p);
        p += 4;
    }
    *out_triangle_count = index_count / 3;
    return idx;
}

/* ---- BBOX chunk parser ------------------------------------------------- */

static int cmg__parse_bbox(const unsigned char *data, uint32_t size,
                             float out_min[3], float out_max[3]) {
    if (size != 6 * 4) return 0;
    out_min[0] = cmg__read_f32(data +  0);
    out_min[1] = cmg__read_f32(data +  4);
    out_min[2] = cmg__read_f32(data +  8);
    out_max[0] = cmg__read_f32(data + 12);
    out_max[1] = cmg__read_f32(data + 16);
    out_max[2] = cmg__read_f32(data + 20);
    return 1;
}

/* ---- EXRF chunk parser ------------------------------------------------- */

/* Returns 1 if the sidecar filename was successfully read into out_name
 * (truncated to name_size-1 bytes if needed). */
static int cmg__parse_exrf(const unsigned char *data, uint32_t size,
                             char *out_name, size_t name_size) {
    if (size < 2) return 0;
    uint16_t len = cmg__read_u16(data);
    if ((uint32_t)len + 2u > size) return 0;
    if (name_size == 0) return 0;
    uint32_t copy = len < (uint32_t)(name_size - 1) ? len : (uint32_t)(name_size - 1);
    memcpy(out_name, data + 2, copy);
    out_name[copy] = '\0';
    return 1;
}

/* ---- public API -------------------------------------------------------- */

CMGMesh *cmg_load(const char *filepath, char *err, size_t err_size) {
    if (!filepath) {
        cmg__err(err, err_size, "filepath is NULL");
        return NULL;
    }

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Cannot open file: %s", filepath);
        cmg__err(err, err_size, msg);
        return NULL;
    }

    unsigned char hdr[12];
    if (!cmg__read(f, hdr, 12)) {
        fclose(f);
        cmg__err(err, err_size, "File too short to contain a CMG header");
        return NULL;
    }

    /* Magic check */
    if (memcmp(hdr, CMG__MAGIC, 4) != 0) {
        fclose(f);
        cmg__err(err, err_size, "Not a CMG file (magic mismatch)");
        return NULL;
    }

    /* Version check */
    uint32_t version     = cmg__read_u32(hdr + 4);
    uint32_t chunk_count = cmg__read_u32(hdr + 8);
    if (version != CMG_FORMAT_VERSION) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "Unsupported CMG version %u (this library reads version %u)",
                 version, CMG_FORMAT_VERSION);
        fclose(f);
        cmg__err(err, err_size, msg);
        return NULL;
    }

    /* Allocate result struct */
    CMGMesh *mesh = (CMGMesh *)malloc(sizeof(CMGMesh));
    if (!mesh) {
        fclose(f);
        cmg__err(err, err_size, "Out of memory");
        return NULL;
    }
    memset(mesh, 0, sizeof(CMGMesh));
    mesh->sidecar_name[0] = '\0';

    /* Walk chunks */
    unsigned char chunk_hdr[8];
    for (uint32_t ci = 0; ci < chunk_count; ci++) {
        if (!cmg__read(f, chunk_hdr, 8)) {
            /* Truncated file — stop walking, but don't fail if we already
             * have VERT + INDX (partial files with missing trailing chunks
             * may still be usable). */
            break;
        }
        char     chunk_id[5];
        memcpy(chunk_id, chunk_hdr, 4);
        chunk_id[4] = '\0';
        uint32_t chunk_size = cmg__read_u32(chunk_hdr + 4);

        if (memcmp(chunk_id, "BBOX", 4) == 0) {
            /* BBOX: exactly 24 bytes — read directly, no temp allocation */
            if (chunk_size == 24) {
                unsigned char bbox_data[24];
                if (cmg__read(f, bbox_data, 24)) {
                    mesh->has_bbox = cmg__parse_bbox(
                        bbox_data, chunk_size, mesh->bbox_min, mesh->bbox_max);
                } else {
                    cmg__skip(f, 0);   /* stream position already advanced by fread */
                }
            } else {
                cmg__skip(f, chunk_size);
            }

        } else if (memcmp(chunk_id, "VERT", 4) == 0) {
            unsigned char *data = (unsigned char *)malloc(chunk_size);
            if (!data || !cmg__read(f, data, chunk_size)) {
                free(data);
                cmg__err(err, err_size, "Failed to read VERT chunk");
                fclose(f);
                cmg_free(mesh);
                return NULL;
            }
            /* Free any previously loaded VERT (duplicate chunk = last wins) */
            free(mesh->vertices);
            mesh->vertices = cmg__parse_vert(data, chunk_size, &mesh->vertex_count);
            free(data);
            if (!mesh->vertices) {
                cmg__err(err, err_size, "VERT chunk is malformed or empty");
                fclose(f);
                cmg_free(mesh);
                return NULL;
            }

        } else if (memcmp(chunk_id, "INDX", 4) == 0) {
            unsigned char *data = (unsigned char *)malloc(chunk_size);
            if (!data || !cmg__read(f, data, chunk_size)) {
                free(data);
                cmg__err(err, err_size, "Failed to read INDX chunk");
                fclose(f);
                cmg_free(mesh);
                return NULL;
            }
            free(mesh->indices);
            mesh->indices = cmg__parse_indx(data, chunk_size, &mesh->triangle_count);
            free(data);
            if (!mesh->indices) {
                cmg__err(err, err_size, "INDX chunk is malformed or empty");
                fclose(f);
                cmg_free(mesh);
                return NULL;
            }

        } else if (memcmp(chunk_id, "EXRF", 4) == 0) {
            unsigned char *data = (unsigned char *)malloc(chunk_size + 1);
            if (data && cmg__read(f, data, chunk_size)) {
                cmg__parse_exrf(data, chunk_size,
                                mesh->sidecar_name, sizeof(mesh->sidecar_name));
            } else {
                cmg__skip(f, 0);
            }
            free(data);

        } else {
            /* Unknown chunk — skip per forward-compatibility rule */
            if (!cmg__skip(f, chunk_size)) {
                break;   /* seek failed (truncated?), stop gracefully */
            }
        }
    }

    fclose(f);

    /* Both VERT and INDX are required */
    if (!mesh->vertices) {
        cmg__err(err, err_size, "File is missing the required VERT chunk");
        cmg_free(mesh);
        return NULL;
    }
    if (!mesh->indices) {
        cmg__err(err, err_size, "File is missing the required INDX chunk");
        cmg_free(mesh);
        return NULL;
    }

    /* Optional: validate that no index exceeds vertex_count.
     * This catches corrupt/truncated files before they cause out-of-bounds
     * reads in the caller's rendering code. */
    uint32_t index_count = mesh->triangle_count * 3;
    for (uint32_t i = 0; i < index_count; i++) {
        if (mesh->indices[i] >= mesh->vertex_count) {
            char msg[128];
            snprintf(msg, sizeof(msg),
                     "INDX[%u] = %u is out of range (vertex_count = %u)",
                     i, mesh->indices[i], mesh->vertex_count);
            cmg__err(err, err_size, msg);
            cmg_free(mesh);
            return NULL;
        }
    }

    return mesh;
}

void cmg_free(CMGMesh *mesh) {
    if (!mesh) return;
    free(mesh->vertices);
    free(mesh->indices);
    free(mesh);
}

uint32_t cmg_format_version(void) {
    return CMG_FORMAT_VERSION;
}

#endif /* CMG_IMPLEMENTATION */
#endif /* CMG_H */

/*
 * MIT License
 *
 * Copyright (c) 2026 A.G gaming king
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
