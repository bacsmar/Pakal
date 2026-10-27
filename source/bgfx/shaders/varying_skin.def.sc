vec3 v_normal    : NORMAL    = vec3(0.0, 0.0, 1.0);
vec2 v_texcoord0 : TEXCOORD0 = vec2(0.0, 0.0);
vec3 v_worldpos  : TEXCOORD1 = vec3(0.0, 0.0, 0.0);
vec2 v_texcoord1 : TEXCOORD2 = vec2(0.0, 0.0); // fs_mesh reads it (the lightmap); characters have none
vec3 v_localpos  : TEXCOORD3 = vec3(0.0, 0.0, 0.0); // the rest pose, before skinning: fixed to the surface as it moves

vec3 a_position  : POSITION;
vec3 a_normal    : NORMAL;
vec2 a_texcoord0 : TEXCOORD0;
vec4 a_indices   : BLENDINDICES;
vec4 a_weight    : BLENDWEIGHT;
