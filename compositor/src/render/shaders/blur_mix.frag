#version 450
// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#extension GL_GOOGLE_include_directive : require

// Live blur (§8.3): the blurred background under a panel, with Windows 11's
// acrylic recipe (WinUI's AcrylicBrush, the luminosity one): the background's
// brightness replaced by the tint's (a Luminosity blend at flags bits 8-15 /
// 255), then its hue and saturation pulled toward the tint (a Color blend at
// the tint's alpha), then 2% noise (no banding in gradients). Before that the
// theme's saturation (shape.y). The panel itself gives the shape: where its surface
// is transparent (outside the rounded corners) nothing is blurred, unless
// flags bit 3 says the whole region. The panel is then drawn on top.
//
// binding 0: the panel surface (uv); binding 1: the blurred background, read
// at the output point: (pixel - pad) * shape.zw. color: the tint (linear,
// straight: alpha = the Color blend's opacity).

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

vec3 srgbToLinear(vec3 c)
{
    const bvec3 low = lessThanEqual(c, vec3(0.04045));
    return mix(pow((c + 0.055) / 1.055, vec3(2.4)), c / 12.92, low);
}

vec3 linearToSrgb(vec3 c)
{
    const bvec3 low = lessThanEqual(c, vec3(0.0031308));
    return mix(1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, c * 12.92, low);
}

// The non-separable blend modes (W3C compositing, what Direct2D does).
float lum(vec3 c)
{
    return dot(c, vec3(0.3, 0.59, 0.11));
}

vec3 setLum(vec3 c, float l)
{
    c += l - lum(c);
    l = lum(c);
    const float n = min(c.r, min(c.g, c.b));
    const float x = max(c.r, max(c.g, c.b));
    if (n < 0.0) {
        c = l + (c - l) * l / (l - n);
    }
    if (x > 1.0) {
        c = l + (c - l) * (1.0 - l) / (x - l);
    }
    return c;
}

void main()
{
    vec3 c = texture(blurred, (gl_FragCoord.xy - pc.pad) * pc.shape.zw).rgb;
    const float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = mix(vec3(luma), c, pc.shape.y); // saturation (themes: blur-saturation)
    // The background read already has the output's filter (if any): only the
    // tint takes it here. Blends in sRGB, like Windows.
    c = linearToSrgb(clamp(c, 0.0, 1.0));
    const vec3 tint = linearToSrgb(colorFilter(vec4(pc.color.rgb, 1.0)).rgb);
    const float luminosity = float((pc.flags >> 8) & 255u) / 255.0;
    c = mix(c, setLum(c, lum(tint)), luminosity);
    c = mix(c, setLum(tint, lum(c)), pc.color.a);
    c = srgbToLinear(mix(c, vec3(noise(gl_FragCoord.xy)), 0.02));
    c = max(c, 0.0);
    // flags bit 3: the whole region (window-blur: apps that don't ask for blur
    // often draw nearly or fully transparent pixels, or draw in a subsurface
    // above a transparent main surface, like Firefox). Otherwise a nearly
    // transparent panel is enough (the shell's acrylic surfaces: 4%).
    const float shape = (pc.flags & 8u) != 0u ? 1.0 : smoothstep(0.0, 0.04, texture(mask, uv).a);
    const float coverage = shape * shapeClip() * pc.alpha;
    outColor = vec4(c, 1.0) * coverage;
}
