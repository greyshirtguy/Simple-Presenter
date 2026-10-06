#version 440

// Ported from gl-transitions "FilmBurn" (https://gl-transitions.com), under the MIT licence
// in the LICENSE file beside this one. Its seed is fixed at its default. To keep it light on
// modest graphics hardware, the blur takes 8 samples where the original took 50, and the
// second of the two sums that make the light leaks, the drifting spots, is left out. The
// original's own `texture` function is `blended` here, a name GLSL now keeps for itself.
// What follows the declarations is otherwise the original, whose credit is:
// Author: Anastasia Dunbar
// License: MIT

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float progress;
};

layout(binding = 1) uniform sampler2D fromTex;
layout(binding = 2) uniform sampler2D toTex;

// gl-transitions measures y upwards from the bottom; Qt measures it downwards.
vec4 getFromColor(vec2 uv) { return texture(fromTex, vec2(uv.x, 1.0 - uv.y)); }
vec4 getToColor(vec2 uv) { return texture(toTex, vec2(uv.x, 1.0 - uv.y)); }
const float Seed = 2.31;
float sigmoid(float x, float a) {
    float b = pow(x*2.,a)/2.;
    if (x > .5) {
        b = 1.-pow(2.-(x*2.),a)/2.;
    }
	return b;
}
float rand(float co){
    return fract(sin((co*24.9898)+Seed)*43758.5453);
}
float rand(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}
float apow(float a,float b) { return pow(abs(a),b)*sign(b); }
vec3 pow3(vec3 a,vec3 b) { return vec3(apow(a.r,b.r),apow(a.g,b.g),apow(a.b,b.b)); }
float smooth_mix(float a,float b,float c) { return mix(a,b,sigmoid(c,2.)); }
float random(vec2 co, float shft){
    co += 10.;
    return smooth_mix(fract(sin(dot(co.xy ,vec2(12.9898+(floor(shft)*.5),78.233+Seed))) * 43758.5453),fract(sin(dot(co.xy ,vec2(12.9898+(floor(shft+1.)*.5),78.233+Seed))) * 43758.5453),fract(shft));
}
float smooth_random(vec2 co, float shft) {
	return smooth_mix(smooth_mix(random(floor(co),shft),random(floor(co+vec2(1.,0.)),shft),fract(co.x)),smooth_mix(random(floor(co+vec2(0.,1.)),shft),random(floor(co+vec2(1.,1.)),shft),fract(co.x)),fract(co.y));
}
vec4 blended(vec2 p) {
    return mix(getFromColor(p), getToColor(p), sigmoid(progress,10.));
}
#define pi 3.14159265358979323
#define clamps(x) clamp(x,0.,1.)

vec4 transition(vec2 p) {
  vec3 f = vec3(0.);
  for (float i = 0.; i < 13.; i++) {
    f += sin(((p.x*rand(i)*6.)+(progress*8.))+rand(i+1.43))*sin(((p.y*rand(i+4.4)*6.)+(progress*6.))+rand(i+2.4));
  }
  f += 4.;
  f /= 11.;
  f = pow3(f*vec3(1.,0.7,0.6),vec3(1.,2.-sin(progress*pi),1.3));
  f *= sin(progress*pi);

  p -= .5;
  p *= 1.+(smooth_random(vec2(progress*5.),6.3)*sin(progress*pi)*.05);
  p += .5;

  vec4 blurred_image = vec4(0.);
  float bluramount = sin(progress*pi)*.03;
  #define repeats  8.
  for (float i = 0.; i < repeats; i++) {
      vec2 q = vec2(cos(degrees((i/repeats)*360.)),sin(degrees((i/repeats)*360.))) *  (rand(vec2(i,p.x+p.y))+bluramount);
      vec2 uv2 = p+(q*bluramount);
      blurred_image += blended(uv2);
  }
  blurred_image /= repeats;

  return blurred_image+vec4(f,0.);
}

void main()
{
    fragColor = transition(vec2(qt_TexCoord0.x, 1.0 - qt_TexCoord0.y)) * qt_Opacity;
}
