// Samplers whose values are numbers (depth, normals, shadow depth, light volume) rather than colors.
// On OpenGL ES a sampler is lowp unless said otherwise, which makes everything computed from it lowp (8 bits).
#ifndef PAKAL_PRECISION_SH
#define PAKAL_PRECISION_SH

#if BGFX_SHADER_LANGUAGE_GLSL
	#define PAKAL_SAMPLER2D_HIGH(_name, _reg) uniform highp sampler2D _name
	#define PAKAL_SAMPLER3D_HIGH(_name, _reg) uniform highp sampler3D _name
	#define PAKAL_SAMPLER2DSHADOW_HIGH(_name, _reg) uniform highp sampler2DShadow _name
#else
	#define PAKAL_SAMPLER2D_HIGH(_name, _reg) SAMPLER2D(_name, _reg)
	#define PAKAL_SAMPLER3D_HIGH(_name, _reg) SAMPLER3D(_name, _reg)
	#define PAKAL_SAMPLER2DSHADOW_HIGH(_name, _reg) SAMPLER2DSHADOW(_name, _reg)
#endif

#endif // PAKAL_PRECISION_SH
