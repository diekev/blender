
varying vec3 coords;

uniform sampler3D soot_texture;
uniform vec3 N;
uniform float step_size;

vec3 nor;

void compute_normal()
{
    vec3 sample1, sample2;
    sample1.x = texture3D(soot_texture, coords - vec3(step_size, 0.0, 0.0)).r;
    sample2.x = texture3D(soot_texture, coords + vec3(step_size, 0.0, 0.0)).r;
    sample1.y = texture3D(soot_texture, coords - vec3(0.0, step_size, 0.0)).r;
    sample2.y = texture3D(soot_texture, coords + vec3(0.0, step_size, 0.0)).r;
    sample1.z = texture3D(soot_texture, coords - vec3(0.0, 0.0, step_size)).r;
    sample2.z = texture3D(soot_texture, coords + vec3(0.0, 0.0, step_size)).r;
    nor = normalize(sample1 - sample2);
}

void main()
{
	/* compute color and density from volume texture */
	vec4 soot = texture3D(soot_texture, coords).rrrr;

	compute_normal();

	vec3 normalized_normal = normalize(nor);
	float w = 0.5 * (1.0 + dot(normalized_normal, vec3(0.0, 1.0, 0.0)));
	vec4 color = w * soot + (1.0 - w) * (soot * 0.3);

	gl_FragColor = vec4(normalized_normal, 1.0) * soot;
}
