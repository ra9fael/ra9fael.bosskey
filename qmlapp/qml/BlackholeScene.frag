// BlackholeScene.frag - a Schwarzschild black hole over a dark sky.
//
// Ported from XboxNahida/ghostty-blackhole-main (MIT), whose shader follows Eric
// Bruneton's "Real-time High-Quality Rendering of Non-Rotating Black Holes"
// (BSD-3-Clause). The physics is unchanged: every pixel's null geodesic is
// integrated with a = -(3/2) h^2 x / r^5, the disk is crossed with interpolation
// and lit by a Shakura-Sunyaev profile with relativistic beaming. The shadow, the
// photon ring, the disk's upper and lower arcs and the sky bent into arcs around
// the hole are all results of the integration, not things painted on.
//
// The backdrop is a dark sky of dust and stars, not the desktop the cover sits on.
// Two reasons: a boss key that lenses the screen still shows the screen, and the
// disk only pops against near-black. The sky is procedural because a flat colour
// has nothing for the lens to bend — the filaments and stars wound into arcs
// around the hole are most of what makes the lensing read. Nothing behind the
// cover reaches the fragment stage at all, so showing this scene no longer has to
// clear the desktop first.
//
// The output is opaque, and premultiplied all the same: D3D11's swapchain
// composites the fragment colour straight onto whatever is under the window, and
// one premultiply is what keeps that identical to an opaque result.
//
// Differences from the reference: no struct or uniform arrays (the look is a flat
// set of uniforms so it runs as a QML ShaderEffect) and no work-area shield
// (nothing here to protect). The black hole starts as a small central point, then
// the accretion disk spreads before the shadow grows into it. Its centre follows
// a slow random walk supplied by QML.
//
// Baked with qsb (see CMakeLists.txt): GLSL 100 es / 120 / 150 + HLSL 50 + MSL 12.

#version 440

layout(location = 1) in vec2 qt_MultiTexCoord0;
layout(location = 0) out vec4 fragColor;

// ShaderEffect properties land here: Qt requires the user uniforms in a std140
// block (a Vulkan/SPIR-V rule that the GLSL ES targets inherit), and Qt's own
// qt_Matrix/qt_Opacity must sit at the head of that block or it binds nothing
// and the effect silently draws nothing.
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float uAspect;
    float uTime;
    float uCenterX;
    float uCenterY;
    float uHoleRadius;
    float uDiskInner;
    float uDiskOuter;
    float uIncl;
    float uRoll;
    float uTemp;
    float uGain;
    float uExposure;
    float uSpeed;
    float uWind;
    float uContrast;
    float uDoppler;
    float uBeam;
    float uOpacity;
    float uDilation;
    float uBgGain;   // gain on the sky's dust and stars (0: just uSky)
    float uBloom;    // how much light the disk throws into the dust
    float uVignette; // corner darkening, in [0,1]
    vec4  uSky;      // the sky's darkest tone; vec4 because QML's `color` is
                     // vec4 -- only .rgb is read
};

#define B_CRIT 2.5980762   // 3*sqrt(3)/2: the shadow's angular radius, in r_s
#define N_STEPS 48         // geodesic integration steps per pixel
#define TAU 6.2831853
#define LENS_DEPTH 13.0    // hole-to-desktop distance in r_s; bigger bends harder

float hash21(vec2 p) {
    p = fract(p * vec2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return fract(p.x * p.y);
}

// GLSL mod is floor-based, HLSL fmod truncates: write it once for both.
float glmod(float x, float y) { return x - y * floor(x / y); }

// Value noise whose y lattice wraps every perY cells, so the disk's angular
// dimension tiles seamlessly across the atan branch cut.
float vnoiseWrapY(vec2 p, float perY) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float y0 = glmod(i.y, perY), y1 = glmod(i.y + 1.0, perY);
    return mix(mix(hash21(vec2(i.x, y0)), hash21(vec2(i.x + 1.0, y0)), f.x),
               mix(hash21(vec2(i.x, y1)), hash21(vec2(i.x + 1.0, y1)), f.x),
               f.y);
}

vec2 rot(vec2 v, float a) {
    float c = cos(a), s = sin(a);
    return vec2(c * v.x - s * v.y, s * v.x + c * v.y);
}

// Value noise without the wrap, for the sky's dust.
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash21(i), hash21(i + vec2(1.0, 0.0)), f.x),
               mix(hash21(i + vec2(0.0, 1.0)), hash21(i + vec2(1.0, 1.0)), f.x), f.y);
}

// Four octaves: the low ones are the filaments the lens winds into arcs, the high
// ones only keep the dust from looking like a smooth gradient.
float fbm(vec2 p) {
    float sum = 0.0, amp = 0.5;
    for (int i = 0; i < 4; i++) {
        sum += amp * vnoise(p);
        p *= 2.03;
        amp *= 0.5;
    }
    return sum;
}

// One layer of stars: a cell either holds one or not, and the star is a gaussian
// dot inside it. Dots rather than pixels, so the lens magnifies them into soft
// blobs and arcs instead of aliasing a hard square.
float starLayer(vec2 p, float keep, float size, float t) {
    vec2 i = floor(p);
    vec2 f = fract(p) - 0.5;
    vec2 o = (vec2(hash21(i + vec2(7.31, 1.71)), hash21(i + vec2(2.93, 11.37))) - 0.5) * 0.72;
    float d = length(f - o);
    float spot = exp(-d * d / (size * size));
    float seed = hash21(i + vec2(31.7, 5.13));
    float shimmer = 0.88 + 0.12 * sin(t * mix(0.7, 1.9, seed) + TAU * seed);
    return spot * step(hash21(i), keep) * mix(0.35, 1.0, seed) * shimmer;
}

// The sky: near-black, with bluish dust and three moving particle layers. Sampled at the
// coordinates the deflection of the pixel worked out to, which is the whole point
// — it is the structure in here that the hole bends, and without it the lens
// would have nothing to show.
vec3 backdrop(vec2 uv, float t) {
    vec2 q = vec2(uv.x * uAspect, uv.y);            // square pixels
    vec2 drift = vec2(0.0035, 0.0018) * t;
    float dust = smoothstep(0.30, 0.95, fbm(q * 3.3 + vec2(11.7, 4.3) + drift));
    // Independent velocities in sky coordinates: about 1.2, 3.7 and 8.6 px/s
    // at 1080p. All layers are sampled after the same gravitational mapping.
    float fine = starLayer((q + vec2(0.0010, -0.0005) * t) * 92.0, 0.055, 0.13, t);
    float coarse = starLayer((q + vec2(0.0030, -0.0017) * t) * 33.0 + 17.0, 0.030, 0.10, t);
    float motes = starLayer((q + vec2(0.0070, -0.0038) * t) * 24.0 + 43.0, 0.012, 0.075, t * 0.65);
    vec3 col = uSky.rgb * (0.55 + 2.4 * dust * dust);
    // Two dust tints at once, so the field is not monochrome: cool where it is
    // thin, warm where it is dense.
    col += mix(vec3(0.013, 0.017, 0.032), vec3(0.032, 0.021, 0.013), dust)
         * (dust * dust * dust) * uBgGain;
    // Far stars are cooler and fainter than the closer ones.
    col += (vec3(0.70, 0.80, 1.00) * (fine * 0.50)
            + vec3(1.00, 0.86, 0.70) * (coarse * 0.90)
            + vec3(0.65, 0.75, 0.90) * (motes * 0.65)) * uBgGain;
    return col;
}

// Blackbody colour from temperature in Kelvin (Tanner Helland fit, normalized).
vec3 blackbody(float T) {
    float t = clamp(T, 1500.0, 40000.0) / 100.0;
    float r = t <= 66.0 ? 1.0
                        : clamp(1.292936 * pow(t - 60.0, -0.1332047), 0.0, 1.0);
    float g = t <= 66.0 ? clamp(0.3900816 * log(t) - 0.6318414, 0.0, 1.0)
                        : clamp(1.1298909 * pow(t - 60.0, -0.0755148), 0.0, 1.0);
    float b = t >= 66.0 ? 1.0
                        : (t <= 19.0 ? 0.0
                                     : clamp(0.5432068 * log(t - 10.0) - 1.1962540, 0.0, 1.0));
    return vec3(r, g, b);
}

void main() {
    vec2  uv     = qt_MultiTexCoord0;
    float t      = uTime;

    // QML supplies a bounded random walk: the hole is born at the centre and
    // begins to wander only after the accretion disk has appeared.
    vec2 center = vec2(0.5 + uCenterX, 0.5 + uCenterY);

    // The floor only keeps W finite: the arrival starts below a pixel, then
    // brings up a small shadow before the disk spreads around it.
    float rh    = max(uHoleRadius, 0.0008);
    float dil   = clamp(uDilation, 0.05, 1.0);
    float small = step(rh, 0.085);   // 1 while the hole is small on screen

    // Screen to world: 1 screen unit is W Schwarzschild radii, so the shadow
    // shows up exactly rh tall. plen is the same frame in screen units.
    vec2  p    = (uv - center) * vec2(uAspect, 1.0);
    float plen = length(p);
    float W    = B_CRIT / rh;
    vec2  pr   = rot(vec2(p.x, -p.y), uRoll) * W;
    float b    = length(pr);

    float rin  = max(uDiskInner, 1.6);
    float rout = max(uDiskOuter, rin + 0.5);
    // Small holes put the disk's projection closer to the critical edge: widen
    // the near field so its upper and lower bands are not cut off early.
    float bmax = rout + mix(3.0, 8.0, small);
    float Z0   = max(14.0, rout + 5.0);
    // Below ~0.085 the inner edge is blended toward the photon sphere so the
    // band does not break apart when the inclination changes.
    float diskRin = mix(1.60, rin, smoothstep(0.035, 0.085, rh));

    // The displacement fades out a few disk diameters away, so the far field can
    // skip the integration: where the lensing is imperceptible the pixel simply
    // samples the sky where it is.
    float window = exp(-pow(plen / (7.0 * rh), 2.0));

    vec3 col; // the sky, the disk's light, or the shadow -- composited right here

    if (b >= bmax) {
        // Far field: no integration. The deflection is the same finite-camera
        // mapping the integrator uses, fitted so the handoff circle has no
        // displacement jump (the textbook alpha = 2 r_s / b is ~20% off here).
        float u    = Z0 * inversesqrt(Z0 * Z0 + b * b);
        float defl = (2.0 / (W * W)) / max(plen, 1e-4)
                   * (1.29 * u + 0.07) * max(LENS_DEPTH - 2.14 * u + 0.75, 0.0)
                   * window;
        vec2  dir  = p / max(plen, 1e-5);
        // Mild chromatic aberration: blue bends a touch more than red, faded in
        // away from the handoff circle (the geodesic side has none).
        float ab = 0.035 * smoothstep(1.0, 2.0, b / bmax);
        for (int i = 0; i < 3; i++) {
            float k  = 1.0 + (float(i) - 1.0) * ab;
            vec2  sp = p - dir * defl * k;
            vec3  sky = backdrop(center + sp / vec2(uAspect, 1.0), t);
            col[i] = sky[i];
        }
    } else {
        // Near field: integrate x'' = -(3/2) h^2 x / r^5, the exact Schwarzschild
        // photon bending. h^2 = b^2 is conserved, so it is computed once.
        vec3  x  = vec3(pr, Z0);
        vec3  v  = vec3(0.0, 0.0, -1.0);
        float h2 = dot(pr, pr);

        float ci = cos(uIncl), si = sin(uIncl);
        vec3  n  = vec3(0.0, si, ci);   // disk normal
        vec3  e2 = vec3(0.0, ci, -si);  // in-plane axis completing (x, e2, n)
        float sdir = uSpeed < 0.0 ? -1.0 : 1.0;
        float spd  = abs(uSpeed);

        vec3  emitc = vec3(0.0);        // accumulated disk light (HDR)
        float trans = 1.0;              // transmittance toward the desktop
        bool  captured = false;
        float sPrev = dot(x, n);
        vec3  xPrev = x;

        for (int i = 0; i < N_STEPS; i++) {
            float r2 = dot(x, x);
            bool insideHorizon = r2 < 1.0;
            // A small hole finishes this step's disk crossing before ending, so
            // its inner edge is not cut off.
            if (insideHorizon && small < 0.5) { captured = true; break; }
            if (x.z < -Z0 && v.z < 0.0) break;          // escaped out the back
            if (r2 > 4.0 * Z0 * Z0) break;              // flung far sideways
            float r  = sqrt(r2);
            float dt = clamp(0.16 * r, 0.03, 1.5);      // fine near, coarse far
            vec3  a  = -1.5 * h2 * x / (r2 * r2 * r);
            v += a * (0.5 * dt);
            x += v * dt;                                // leapfrog: kick-drift-kick
            r2 = dot(x, x);
            r  = sqrt(r2);
            a  = -1.5 * h2 * x / (r2 * r2 * r);
            v += a * (0.5 * dt);

            // Did this step pierce the disk plane?
            float s = dot(x, n);
            if (s * sPrev < 0.0 && trans > 0.02) {
                float tc = sPrev / (sPrev - s);
                vec3  xc = mix(xPrev, x, tc);
                float rc = length(xc);
                if (rc > diskRin && rc < rout) {
                    float band = smoothstep(diskRin, diskRin * 1.25, rc)
                               * (1.0 - smoothstep(rout * 0.70, rout, rc));
                    float phi   = atan(dot(xc, e2), xc.x);
                    float turns = phi / TAU;
                    float kep   = pow(diskRin / rc, 1.5);
                    // sqrt(1 - 1.5/r): inner orbits run slow, and dil winds the
                    // whole pattern down as the hole gets heavier.
                    float gloc  = sqrt(max(1.0 - 1.5 / rc, 0.02));
                    float swirl = rc * uWind * 0.12 - t * kep * spd * gloc * dil * sdir;
                    float fine  = vnoiseWrapY(vec2(rc * 2.8, turns * 19.0 + swirl * 3.0), 19.0);
                    float broad = vnoiseWrapY(vec2(rc * 1.0, turns * 9.0 + swirl * 1.5 + 7.0), 9.0);
                    float streaks = mix(0.52, 0.48 + 0.22 * broad + 0.10 * fine,
                                        clamp(uContrast * 0.55, 0.0, 1.0));

                    // g = sqrt(1 - 1.5/r) / (1 - beta.k): relativistic shift and
                    // beaming of gas on a circular geodesic.
                    vec3  gasdir = normalize(cross(n, xc)) * sdir;
                    float beta   = clamp(inversesqrt(max(2.0 * (rc - 1.0), 0.2)), 0.0, 0.99);
                    float g      = gloc / max(1.0 + beta * dot(gasdir, normalize(v)), 0.05);
                    g = mix(1.0, g, uDoppler);

                    // Shakura-Sunyaev temperature profile, peak normalized to 1.
                    float xpr   = max(1.0 - sqrt(diskRin / rc), 0.0);
                    float tprof = pow(diskRin / rc, 0.75) * pow(xpr, 0.25) / 0.488;
                    vec3  cbb   = blackbody(uTemp * tprof * g);
                    float boost = pow(g, uBeam);

                    float density = band * streaks;
                    emitc += trans * cbb * (uGain * 2.2 * density * tprof * tprof * boost);
                    // Seen at an angle the front band covers the back one: keep it
                    // glowing but less opaque so the horizon edge is not blacked out.
                    trans *= 1.0 - clamp(uOpacity * density * mix(1.0, 0.58, small), 0.0, 1.0);
                }
            }
            if (insideHorizon) { captured = true; break; }
            sPrev = s;
            xPrev = x;
        }
        // Rays still wound up near the photon sphere when the budget ran out are
        // as good as captured.
        if (!captured && dot(x, x) < 4.0)
            captured = true;

        // Disk light is HDR: tonemap it onto the sky behind it.
        vec3 emit = vec3(1.0) - exp(-emitc * uExposure);

        if (captured) {
            // The shadow swallows the sky outright: nothing shows through. What
            // draws its edge is the lensed sky around it, not a painted circle.
            col = vec3(0.0);
        } else {
            // Where did the escaped ray come from? Project it back onto the sky
            // plane behind the hole and read the sky there. Only the displacement
            // is faded by the window, never the colour, so the warp stays
            // continuous into the far field.
            vec3  d      = normalize(v);
            vec3  behind = vec3(0.0);
            if (d.z < -0.05) {
                float tpl = (-LENS_DEPTH - x.z) / d.z;
                vec3  hp  = x + d * tpl;
                vec2  q   = rot(hp.xy, -uRoll) / W;
                vec2  sp  = vec2(q.x, -q.y);
                vec2  suv = center + (p + (sp - p) * window) / vec2(uAspect, 1.0);
                // Rays bent past ~90 degrees never reach that plane; they fade
                // out instead of sampling the wrong part of the sky.
                float toward = smoothstep(0.05, 0.35, -d.z);
                behind = backdrop(suv, t) * toward;
            }
            col = behind * trans + emit;
        }
    }

    // A soft outer disk glow, limited to far-field pixels. The captured shadow
    // remains pure black.
    float dh   = (b - 0.70 * rout) / (0.45 * rout);
    float halo = exp(-dh * dh) * smoothstep(B_CRIT, B_CRIT * 1.35, b);
    if (b >= bmax)
        col += blackbody(uTemp * 0.85) * (halo * uBloom * 0.035);

    // Corner darkening: keeps the eye on the hole, and stops the sky from ending
    // squarely at the edges of the screen.
    float vr = length((uv - 0.5) * vec2(uAspect, 1.0)) / (0.5 * length(vec2(uAspect, 1.0)));
    col *= 1.0 - uVignette * smoothstep(0.10, 1.0, vr);

    // Opaque output covers the window underneath with the rendered sky.
    fragColor = vec4(col * qt_Opacity, qt_Opacity);
}
