vec3 v_normal    : NORMAL    = vec3(0.0, 0.0, 1.0);
vec2 v_texcoord0 : TEXCOORD0 = vec2(0.0, 0.0);
vec3 v_worldpos  : TEXCOORD1 = vec3(0.0, 0.0, 0.0);
vec2 v_texcoord1 : TEXCOORD2 = vec2(0.0, 0.0);
vec3 v_localpos  : TEXCOORD3 = vec3(0.0, 0.0, 0.0); // the model's own space (a character's rest pose): fixed to its surface

vec3 a_position  : POSITION;
vec3 a_normal    : NORMAL;
vec2 a_texcoord0 : TEXCOORD0;
vec2 a_texcoord1 : TEXCOORD1;

vec4 i_data0     : TEXCOORD7;
vec4 i_data1     : TEXCOORD6;
vec4 i_data2     : TEXCOORD5;
vec4 i_data3     : TEXCOORD4;
