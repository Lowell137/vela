#version 450
// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#extension GL_GOOGLE_include_directive : require

// Live blur (§8.3): the blurred background under a panel, with Windows 11's
// acrylic recipe: a bit more saturation, a tint and a veil of noise (no
// banding in gradients). The panel itself gives the shape: where its surface
// is transparent (outside the rounded corners) nothing is blurred, unless
// flags bit 3 says the whole region. The panel is then drawn on top.
//
// binding 0: the panel surface (uv); binding 1: the blurred background, read
// at the output point: (pixel - pad) * shape.zw. color: the tint (linear,
// premultiplied: alpha = how much).

#include "push.glsl"
#include "shape.glsl"

layout(set = 0, binding = 0) uniform sampler2D mask;
layout(set = 0, binding = 1) uniform sampler2D blurred;
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

float noise(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

// Liquid glass (flags bit 4, blur-style=liquid): how close to the glass's
// edge (0 inside, 1 on it) and which way the edge is (outward). From the
// rounded clip when there is one (an exact SDF), else from the panel's alpha
// around the pixel.
const float EDGE = 22.0; // the bevel's width, in pixels

float maskAt(vec2 offset)
{
    const vec2 duv = pc.uvX * (offset.x / pc.dst.z) + pc.uvY * (offset.y / pc.dst.w);
    const vec2 p = gl_FragCoord.xy + offset - pc.dst.xy;
    const vec2 screen = gl_FragCoord.xy + offset;
    if (any(lessThan(screen, vec2(0.0))) || any(greaterThan(screen, pc.target))) {
        return 1.0; // beyond the output: the glass goes on
    }
    if (any(lessThan(p, vec2(0.0))) || any(greaterThan(p, pc.dst.zw))) {
        return 0.0; // beyond the surface: outside the glass
    }
    return smoothstep(0.0, 0.06, texture(mask, uv + duv).a);
}

vec3 bevel()
{
    // The whole region (window-blur): its rectangle, the window's alpha says nothing.
    const bool exact = (pc.flags & 2u) != 0u || (pc.flags & 8u) != 0u;
    if (exact) {
        const vec4 rect = (pc.flags & 2u) != 0u ? pc.shapeRect : pc.dst;
        const vec2 halfSize = rect.zw * 0.5;
        const float r = (pc.flags & 2u) != 0u ? min(pc.shape.x, min(halfSize.x, halfSize.y)) : 0.0;
        const vec2 d = gl_FragCoord.xy - (rect.xy + halfSize);
        const vec2 q = abs(d) - halfSize + r;
        const float dist = min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
        vec2 n = q.x > 0.0 && q.y > 0.0 ? normalize(q) : (q.x > q.y ? vec2(1.0, 0.0) : vec2(0.0, 1.0));
        n *= sign(d + 1e-4);
        return vec3(n, 1.0 - clamp(-dist / EDGE, 0.0, 1.0));
    }
    // 16 directions, 4 distances: how much of the ring around is outside.
    vec2 n = vec2(0.0);
    float outside = 0.0;
    for (int i = 0; i < 16; ++i) {
        const float a = float(i) * 0.392699 + 0.196350;
        const vec2 dir = vec2(cos(a), sin(a));
        float o = 0.0;
        for (int k = 1; k <= 4; ++k) {
            o += (1.0 - maskAt(dir * EDGE * float(k) * 0.25)) * (1.25 - 0.25 * float(k));
        }
        n += dir * o;
        outside += o;
    }
    return vec3(length(n) > 1e-3 ? normalize(n) : vec2(0.0), smoothstep(0.0, 1.0, outside / 9.0));
}

vec3 readBlurred(vec2 p)
{
    // Inside the panel and the output: what was blurred (the lens reads beside the pixel).
    p = clamp(p, max(pc.dst.xy, vec2(0.5)), min(pc.dst.xy + pc.dst.zw, pc.target) - 0.5);
    return texture(blurred, (p - pc.pad) * pc.shape.zw).rgb;
}

void main()
{
    const bool liquid = (pc.flags & 16u) != 0u;
    vec3 c;
    vec3 b = vec3(0.0);
    if (liquid) {
        // A lens at the edges: the background pulled in from outside, a little
        // more for red than for blue.
        b = bevel();
        const float e = b.z * b.z;
        const vec2 shift = -b.xy * e * 28.0;
        c = vec3(readBlurred(gl_FragCoord.xy + shift * 1.08).r, readBlurred(gl_FragCoord.xy + shift).g,
            readBlurred(gl_FragCoord.xy + shift * 0.92).b);
    } else {
        c = readBlurred(gl_FragCoord.xy);
    }
    const float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = mix(vec3(luma), c, liquid ? 1.15 : 1.25); // saturation
    if (liquid) {
        // Clear glass: a faint veil of the tint, brighter at the rim where the
        // light from the top left catches it (and its echo across).
        c = c * (1.0 - 0.35 * pc.color.a) + colorFilter(pc.color).rgb * 0.35;
        const vec2 light = normalize(vec2(-0.6, -0.8));
        const float rim = pow(b.z, 6.0);
        c += vec3(0.30) * rim * max(dot(b.xy, light), 0.0) + vec3(0.12) * rim * max(dot(b.xy, -light), 0.0);
        c += vec3(0.04) * b.z;
    } else {
        // The background read already has the output's filter (if any): only
        // the tint takes it here.
        c = c * (1.0 - pc.color.a) + colorFilter(pc.color).rgb; // tint
    }
    c += (noise(gl_FragCoord.xy) - 0.5) * (2.0 / 255.0);
    c = max(c, 0.0);
    // flags bit 3: the whole region (window-blur: apps that don't ask for blur
    // often draw nearly or fully transparent pixels, or draw in a subsurface
    // above a transparent main surface, like Firefox). Liquid glass: even a
    // faint panel is glass.
    const float a = texture(mask, uv).a;
    const float shape = (pc.flags & 8u) != 0u ? 1.0 : liquid ? smoothstep(0.0, 0.06, a) : smoothstep(0.0, 0.5, a);
    const float coverage = shape * shapeClip() * pc.alpha;
    outColor = vec4(c, 1.0) * coverage;
}
