
varying vec3 coords;
flat out int node_indirection_base_idx;

uniform vec3 min;
uniform vec3 invsize;

uniform int first_node_index;
uniform int max_leaf_per_node;

void main()
{
	gl_Position = gl_ModelViewProjectionMatrix * vec4(gl_Vertex.xyz, 1.0);
	coords = (gl_Vertex.xyz - min) * invsize;

	int internal_node_index = first_node_index + (gl_VertexID / 8);
	node_indirection_base_idx = internal_node_index * max_leaf_per_node;
}
