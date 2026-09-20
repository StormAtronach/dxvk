#extension GL_EXT_scalar_block_layout : require

#include "d3d9_fixed_function_common.glsl"
#include "../dxvk_morrowind_limits.h"

const uint MorrowindPplMaxStages = 6u;
const uint MorrowindPplMaxLights = uint(DXVK_MORROWIND_PPL_MAX_LIGHTS);

const uint MorrowindPplUseSkinning      = 1u << 0;
const uint MorrowindPplVertexColor      = 1u << 1;
const uint MorrowindPplUseBumpmap       = 1u << 2;
const uint MorrowindPplUseTexgen        = 1u << 3;
const uint MorrowindPplProjectiveTexgen = 1u << 4;

const uint MorrowindStageAlphaMatchesColor = 1u << 0;
const uint MorrowindStageAlphaSelectArg1   = 1u << 1;

struct MorrowindPplStage {
    uint colorOp;
    uint colorArg1;
    uint colorArg2;
    uint colorArg0;
    uint texcoordIndex;
    uint texcoordGen;
    uint flags;
    uint reserved;
};

struct MorrowindPplData {
    uint flags;
    uint uvSetCount;
    uint vertexBlendState;
    uint vertexMaterialMode;
    uint fogMode;
    uint activeStageCount;
    uint lightSlotCount;
    uint bumpmapStage;
    uint texgenStage;

    MorrowindPplStage stages[MorrowindPplMaxStages];

    mat4 projection;
    mat4 worldView[4];
    mat4 texgenTransform;

    vec4 materialDiffuse;
    vec4 materialAmbient;
    vec4 materialEmissive;

    vec4 sceneAmbient;
    vec4 sunDiffuse;
    vec4 sunDirection;

    vec4 lightDiffuse[MorrowindPplMaxLights];
    float lightAmbient[MorrowindPplMaxLights];
    float lightPosition[3][MorrowindPplMaxLights];
    float lightFalloffQuadratic[MorrowindPplMaxLights];
    float lightFalloffConstant;

    vec4 fogColor;
    float nearFogStart;
    float nearFogRange;

    vec4 bumpMatrix;
    vec2 bumpLumiScaleBias;
    uvec2 reserved;

    float lightFadeInvRadius[MorrowindPplMaxLights];
};

#define CBV_MORROWIND_PPL 7

layout(set = CBV_SET, binding = CBV_MORROWIND_PPL, scalar, row_major)
uniform MorrowindPplConstants {
    MorrowindPplData ppl;
};

bool hasPplFlag(uint flag) {
    return (ppl.flags & flag) != 0u;
}
