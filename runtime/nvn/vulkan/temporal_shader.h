// Conservative image-space history filter. No camera jitter / engine velocity is available, so do not describe
// this as temporal supersampling. Changes and disocclusions fall back towards the current image.
#pragma once
static const char temporal_shader_src[] =
    "#version 450\n"
    "layout(set=0,binding=0) uniform sampler2D currentImage;\n"
    "layout(set=0,binding=1) uniform sampler2D historyImage;\n"
    "layout(push_constant) uniform P { vec4 a; vec4 b; } pc;\n"
    "layout(location=0) out vec4 o;\n"
    "void main() {\n"
    "  ivec2 sz=textureSize(currentImage,0), p=ivec2(gl_FragCoord.xy);\n"
    "  vec4 c=texelFetch(currentImage,p,0);\n"
    "  if(pc.a.z<0.5) { o=c; return; }\n"
    "  vec3 lo=c.rgb, hi=c.rgb;\n"
    "  for(int y=-1;y<=1;y++) for(int x=-1;x<=1;x++) {\n"
    "    vec3 s=texelFetch(currentImage,clamp(p+ivec2(x,y),ivec2(0),sz-1),0).rgb;\n"
    "    lo=min(lo,s); hi=max(hi,s);\n"
    "  }\n"
    "  vec3 h=texelFetch(historyImage,p,0).rgb;\n"
    "  vec3 delta=abs(h-c.rgb);\n"
    "  float change=max(delta.r,max(delta.g,delta.b));\n"
    "  float weight=0.35*(1.0-smoothstep(0.025,0.12,change));\n"
    "  o=vec4(mix(c.rgb,clamp(h,lo,hi),weight),c.a);\n"
    "}\n";
