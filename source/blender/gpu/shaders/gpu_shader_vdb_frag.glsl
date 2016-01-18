
varying vec3 coords;

uniform sampler3D soot_texture;

void main()
{
	/* compute color and density from volume texture */
	vec4 soot = texture3D(soot_texture, coords).rrrr;
	vec4 color = vec4(0.7, 0.7, 0.7, 1.0) * soot;

	gl_FragColor = color;
}
