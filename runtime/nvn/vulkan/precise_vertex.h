#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "shader_cache.h"

// Generated GLSL keeps guest registers in uints. Marking only gl_Position precise
// does not reach arithmetic behind those bitcasts and helper calls. Qualify the
// registers, outputs and helper return values before shaderc's optimizer runs.
static inline char *precise_vertex_source(const char *src) {
    static const char *const types[] = {"float", "uint", "int", "bool", "vec2", "vec3", "vec4",
        "uvec2", "uvec3", "uvec4", "ivec2", "ivec3", "ivec4", "bvec2", "bvec3", "bvec4",
        "mat2", "mat3", "mat4", "uint64_t", "int64_t"};
    size_t size = strlen(src), lines = 1;
    for (const char *p = src; *p; p++) lines += *p == '\n';
    if (lines > (SIZE_MAX - size - 1) / 8) return NULL;
    char *out = (char *)malloc(size + 8 * lines + 1);
    if (!out) return NULL;
    char *w = out;
    for (const char *p = src; *p;) {
        const char *end = strchr(p, '\n');
        if (!end) end = p + strlen(p);
        const char *text = p;
        while (text < end && (*text == ' ' || *text == '\t')) text++;
        bool qualify = false;
        for (size_t i = 0; i < sizeof types / sizeof *types; i++) {
            size_t n = strlen(types[i]);
            if ((size_t)(end - text) > n && !memcmp(text, types[i], n) && (text[n] == ' ' || text[n] == '\t')) {
                qualify = true; break;
            }
        }
        if ((size_t)(end - text) >= 7 && !memcmp(text, "layout(", 7)) {
            for (const char *q = text; end - q >= 5; q++)
                if (!memcmp(q, " out ", 5)) { qualify = true; break; }
        }
        memcpy(w, p, (size_t)(text - p)); w += text - p;
        if (qualify) { memcpy(w, "precise ", 8); w += 8; }
        memcpy(w, text, (size_t)(end - text)); w += end - text;
        p = end;
        if (*p == '\n') { *w++ = *p++; }
    }
    *w = 0;
    return out;
}

static inline bool precise_vertex_float_arith(const uint32_t *w, uint32_t wc, uint32_t op) {
    switch (op) {
    case 127: case 129: case 131: case 133: case 136:
    case 142: case 143: case 144: case 145: case 146: case 148: return true;
    case 12: return wc >= 5 && w[4] == 50; // Explicit GLSL.std.450 Fma.
    default: return false;
    }
}

// Check the unoptimized qualified module before enabling optimization. Negation
// has no rounding, and explicit Fma is already one operation. All other arithmetic
// must carry NoContraction; an unhandled source construct falls back to baseline.
static inline bool precise_vertex_protected(const uint32_t *words, size_t count) {
    if (!vk_shader_cache_framed(words, count) || words[3] > 4u * 1024 * 1024) return false;
    uint8_t *marked = (uint8_t *)calloc(words[3], 1);
    if (!marked) return false;
    for (size_t i = 5; i < count; i += words[i] >> 16) {
        uint32_t wc = words[i] >> 16, op = words[i] & 65535;
        if (op == 71 && wc >= 3 && words[i + 2] == 42 && words[i + 1] < words[3]) marked[words[i + 1]] = 1;
    }
    bool safe = true;
    for (size_t i = 5; i < count; i += words[i] >> 16) {
        uint32_t wc = words[i] >> 16, op = words[i] & 65535;
        if (op == 127 || op == 12) continue;
        if (precise_vertex_float_arith(words + i, wc, op) &&
            (wc < 3 || words[i + 2] >= words[3] || !marked[words[i + 2]])) { safe = false; break; }
    }
    free(marked);
    return safe;
}
