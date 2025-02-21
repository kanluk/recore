#version 460

#include "lpv.glslh"

#include "lpv.glsl"

#extension GL_EXT_debug_printf : require

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

PUSH_CONSTANT(PropagationPush);

/*
 * Light Propagation Volumes:
 *
 * Papers / Articles:
 * - https://cg.ivd.kit.edu/publications/p2010/CLPVFRII_Kaplanyan_2010/CLPVFRII_Kaplanyan_2010.pdf
 * - https://advances.realtimerendering.com/s2009/Light_Propagation_Volumes.pdf
 * - https://ericpolman.com/2016/06/28/light-propagation-volumes/
 * - https://blog.blackhc.net/2010/07/light-propagation-volumes/
 *
 * Code / Repositories used:
 * - https://github.com/djbozkosz/Light-Propagation-Volumes/
 * - https://github.com/mafian89/Light-Propagation-Volumes
 * - https://github.com/Global-Illuminati/Light-Propagation-Volumes
 * - http://data.blog.blackhc.net/LPVPrototype.zip
 */

const ivec3 PROPAGATION_DIRECTIONS[6] = {
	//+Z
	ivec3(0,0,1),
	//-Z
	ivec3(0,0,-1),
	//+X
	ivec3(1,0,0),
	//-X
	ivec3(-1,0,0),
	//+Y
	ivec3(0,1,0),
	//-Y
	ivec3(0,-1,0)
};

const ivec2 CELL_SIDES[4] = { ivec2( 1, 0 ), ivec2( 0, 1 ), ivec2( -1, 0 ), ivec2( 0, -1 ) };


const float SOLID_ANGLE_DIRECT = 0.4006696846f / M_PI;
const float SOLID_ANGLE_SIDE = 0.4234413544f / M_PI;


//Get side direction
vec3 getEvalSideDirection( int index, ivec3 orientation ) {
	const float smallComponent = 0.4472135; // 1 / sqrt(5)
	const float bigComponent = 0.894427; // 2 / sqrt(5)

	const ivec2 side = CELL_SIDES[ index ];
	vec3 tmp = vec3( side.x * smallComponent, side.y * smallComponent, bigComponent );
	return vec3(orientation.x * tmp.x, orientation.y * tmp.y, orientation.z * tmp.z);
}

vec3 getReprojSideDirection( int index, ivec3 orientation ) {
	const ivec2 side = CELL_SIDES[ index ];
	return vec3( orientation.x*side.x, orientation.y*side.y, 0 );
}


// #define SH_USE_VEC4

#define PROPAGATE_OCCLUSION

void main() {
  ivec3 voxel = ivec3(gl_GlobalInvocationID.xyz);
  uvec3 dimensions = p.lpvGrid.dimensions;

  if (any(greaterThanEqual(voxel, dimensions))) {
    return;
  }

#ifdef SH_USE_VEC4
  vec4 shRedContribution = vec4(0.0);
  vec4 shGreenContribution = vec4(0.0);
  vec4 shBlueContribution = vec4(0.0);
#else
  SHRGBFunction shRGB = SHRGBFunction_init();
#endif

  // Gather from all 6 neighbor cells
  for (uint neighborIdx = 0; neighborIdx < 6; neighborIdx++) {
    // Direction of propagation (neighbor -> current cell)
    ivec3 mainDirection = PROPAGATION_DIRECTIONS[neighborIdx];

    // Query neighbor cell
    ivec3 neighborIndex = voxel - ivec3(mainDirection);
    if (any(lessThan(neighborIndex, ivec3(0))) || any(greaterThanEqual(neighborIndex, ivec3(dimensions)))) {
      continue;
    }
    uint index = LPVGrid_getIndex(p.lpvGrid, uvec3(neighborIndex));
    RadianceVolumeCell cell = deref(p.lpvGrid.volume, index);

    // Continue of cell is empty
    if (cell.counter == 0) {
      continue;
    }

    // Take average cell contributions
    // This should only matter in the first iteration after injection,
    // when one cell is injected by multiple texels of the RSM
    #ifdef SH_USE_VEC4
    vec4 cellSHRed = cell.shRed / cell.counter;
    vec4 cellSHGreen = cell.shGreen / cell.counter;
    vec4 cellSHBlue = cell.shBlue / cell.counter;
    #else

    if (p.iteration == 0) {
      SHRGBFunction_mul(cell.shRGB, 1.0f / float(cell.counter));
    }

    #endif

    // First: direct contribution in main direction

    // Occlusion using the geometry volume
    float transmittanceValue = 1.0;
    SHFunction shNegMainDirection = SHFunction_eval(-mainDirection);

    #ifdef PROPAGATE_OCCLUSION
    if (p.iteration > 0) {
      uvec3 occlusionIndex = uvec3(vec3(neighborIndex) + 0.5 * mainDirection);
      GeometryVolumeCell geometryCell = deref(p.lpvGrid.geometryVolume, LPVGrid_getIndex(p.lpvGrid, occlusionIndex));

      #ifdef SH_USE_VEC4
      vec4 shOcclusion = geometryCell.shGeometry / geometryCell.counter;
      transmittanceValue = 1.0 - clamp(dot(shOcclusion, SH_eval(-mainDirection)), 0.0, 1.0);
      #else
      SHFunction_mul(geometryCell.shGV, 1.0f / float(geometryCell.counter));
      transmittanceValue = 1.0f - clamp(SHFunction_dot(geometryCell.shGV, shNegMainDirection), 0.0f, 1.0f);
      #endif
    }
    #endif

    // Calculate contribution and reproject into SH base
    float occludedDirectFaceContribution = transmittanceValue * SOLID_ANGLE_DIRECT;

    #ifdef SH_USE_VEC4
    vec4 shMainDirection = SH_eval(mainDirection);
    vec4 shMainDirectionCos = SH_evalCos(mainDirection);

    shRedContribution += occludedDirectFaceContribution *  max(0, dot(cellSHRed, shMainDirection)) * shMainDirectionCos;
    shGreenContribution += occludedDirectFaceContribution * max(0, dot(cellSHGreen, shMainDirection)) * shMainDirectionCos;
    shBlueContribution += occludedDirectFaceContribution * max(0, dot(cellSHBlue, shMainDirection)) * shMainDirectionCos;

    #else
    SHFunction shMainDirection = SHFunction_eval(mainDirection);
    SHFunction shMainDirectionCos = SHFunction_evalCos(mainDirection);

    vec3 value = occludedDirectFaceContribution * max(vec3(0.0f, 0.0f, 0.0f), SHRGBFunction_dot(cell.shRGB, shMainDirection));
    SHRGBFunction_mad(shRGB, shMainDirectionCos, value);

    #endif

    // Second: side face contributions
    for (int faceIdx = 0; faceIdx < 4; faceIdx++) {
      // Get directions for side faces
      vec3 evalDirection = getEvalSideDirection(faceIdx, mainDirection);
      vec3 reprojDirection = getReprojSideDirection(faceIdx, mainDirection);

      // Occlusion using the geometry volume
      #ifdef PROPAGATE_OCCLUSION
      // TODO: is this correct? seems to be the same value for each faceIdx?
      if (p.iteration > 0) {
        uvec3 occlusionIndex = uvec3(vec3(neighborIndex) + 0.5 * evalDirection);
        GeometryVolumeCell geometryCell = deref(p.lpvGrid.geometryVolume, LPVGrid_getIndex(p.lpvGrid, occlusionIndex));
        #ifdef SH_USE_VEC4
        vec4 shOcclusion = geometryCell.shGeometry / geometryCell.counter;
        transmittanceValue = 1.0 - clamp(dot(shOcclusion, SH_eval(-evalDirection)), 0.0, 1.0);
        #else
        SHFunction_mul(geometryCell.shGV, 1.0f / float(geometryCell.counter));
        SHFunction shNegEvalDirection = SHFunction_eval(-evalDirection);
        transmittanceValue = 1.0f - clamp(SHFunction_dot(geometryCell.shGV, shNegEvalDirection), 0.0f, 1.0f);
        #endif
      }
      #endif


      // Calculate contribution and reproject into SH base
      float occludedSideFaceContribution = transmittanceValue * SOLID_ANGLE_SIDE;

      #ifdef SH_USE_VEC4
      vec4 shReprojDirectionCos = SH_evalCos(reprojDirection);
      vec4 shEvalDirection = SH_eval(evalDirection);

      shRedContribution += occludedSideFaceContribution * max(0, dot(cellSHRed, shEvalDirection)) * shReprojDirectionCos;
      shGreenContribution += occludedSideFaceContribution * max(0, dot(cellSHGreen, shEvalDirection)) * shReprojDirectionCos;
      shBlueContribution += occludedSideFaceContribution * max(0, dot(cellSHBlue, shEvalDirection)) * shReprojDirectionCos;

      #else
      SHFunction shEvalDirection = SHFunction_eval(evalDirection);
      SHFunction shReprojDirectionCos = SHFunction_evalCos(reprojDirection);

      vec3 value = occludedSideFaceContribution * max(vec3(0.0f, 0.0f, 0.0f), SHRGBFunction_dot(cell.shRGB, shEvalDirection));
      SHRGBFunction_mad(shRGB, shReprojDirectionCos, value);
      #endif
    }
  }

  // Update current cell
  uint index = LPVGrid_getIndex(p.lpvGrid, uvec3(voxel));
  RadianceVolumeCell cell;


#ifdef SH_USE_VEC4
  cell.shRed = shRedContribution;
  cell.shGreen = shGreenContribution;
  cell.shBlue = shBlueContribution;
  cell.counter = 1;
  deref(p.lpvGrid.volumeSwap, index) = cell;


  // Accumulate contributions of each iteration
  atomicAddVec4(deref(p.lpvGrid.volumeAccu, index).shRed, shRedContribution);
  atomicAddVec4(deref(p.lpvGrid.volumeAccu, index).shGreen, shGreenContribution);
  atomicAddVec4(deref(p.lpvGrid.volumeAccu, index).shBlue, shBlueContribution);
  atomicAdd(deref(p.lpvGrid.volumeAccu, index).counter, 1);

#else
  cell.shRGB = shRGB;
  cell.counter = 1;
  deref(p.lpvGrid.volumeSwap, index) = cell;

  for (uint i = 0; i < SH_NUM_COEFFICIENTS; i++) {
    atomicAdd(deref(p.lpvGrid.volumeAccu, index).shRGB.r[i], shRGB.r[i]);
    atomicAdd(deref(p.lpvGrid.volumeAccu, index).shRGB.g[i], shRGB.g[i]);
    atomicAdd(deref(p.lpvGrid.volumeAccu, index).shRGB.b[i], shRGB.b[i]);
  }
  atomicAdd(deref(p.lpvGrid.volumeAccu, index).counter, 1);
#endif

}
