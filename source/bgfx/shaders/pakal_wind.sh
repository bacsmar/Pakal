// The wind in the foliage (MeshComponent3D_Bgfx::set_wind): an alpha-tested model (leaves, palms, grass) bends with
// gusts, more the higher above its own origin, each model on a phase of its own by where it stands. Vertex work only.
uniform vec4 u_wind;   // x: bend (meters at 1 m above the origin), y: speed, z: clock (seconds), w: 1 on

vec4 wind_bend(vec4 worldPos, vec3 origin)
{
	if (u_wind.w <= 0.0)
	{
		return worldPos;
	}
	float h = max(0.0, worldPos.y - origin.y);
	float phase = dot(origin.xz, vec2(0.37, 0.53)) + u_wind.z * u_wind.y;
	float gust = sin(phase) * 0.7 + sin(phase * 2.3 + worldPos.x * 0.5) * 0.3;
	float bend = u_wind.x * h * h / (h + 1.5);
	worldPos.xz += vec2(gust, gust * 0.6 + cos(phase * 1.7) * 0.3) * bend;
	return worldPos;
}
