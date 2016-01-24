
varying vec3 coords;
flat in int node_indirection_base_idx;

uniform sampler3D soot_texture;
uniform isampler1D nodeIndirectionSampler;

uniform ivec3 maxLeafCountPerAtlasDimension;
uniform int maxLeafCountPerInternalNodeDimension;

vec3 get_leaf_atlas_offset(int atlas_texture_index)
{
	vec3 atlasCoord;
	atlasCoord.x = atlas_texture_index % maxLeafCountPerAtlasDimension.x;
	atlasCoord.y = (atlas_texture_index / maxLeafCountPerAtlasDimension.x) % maxLeafCountPerAtlasDimension.y;
	atlasCoord.z = atlas_texture_index / (maxLeafCountPerAtlasDimension.x * maxLeafCountPerAtlasDimension.y);

	return atlasCoord / maxLeafCountPerAtlasDimension;
}

int normalizedCoordToLinearOffset(vec3 pos)
{
	ivec3 coord = clamp(ivec3(pos * float(maxLeafCountPerInternalNodeDimension)), ivec3(0), ivec3(15));
	// VDB is indexed in zyx order
	return coord.z + coord.y * maxLeafCountPerInternalNodeDimension + coord.x * maxLeafCountPerInternalNodeDimension * maxLeafCountPerInternalNodeDimension;
}

void main()
{
	vec3 pos = coords;

	int atlasTextureIndex = texture1D(nodeIndirectionSampler, node_indirection_base_idx + normalizedCoordToLinearOffset(pos)).r;
	vec3 texCoordScale =  vec3(1.0 / maxLeafCountPerAtlasDimension);

	vec4 soot = vec4(0.0);

	if (atlasTextureIndex != -1) {
		vec3 coordFloat = min(pos, 0.9999) * float(maxLeafCountPerInternalNodeDimension);
		ivec3 coordInt = ivec3(coordFloat);
		vec3 frac = coordFloat - coordInt;

		vec3 texCoordOffset = get_leaf_atlas_offset(atlasTextureIndex);

		vec3 sampleTexCoord = clamp(frac, 0.5/8.0, 7.5/8.0) * texCoordScale + texCoordOffset;
		soot = texture3D(soot_texture, sampleTexCoord).rrrr;
	}

	gl_FragColor = soot;
}
