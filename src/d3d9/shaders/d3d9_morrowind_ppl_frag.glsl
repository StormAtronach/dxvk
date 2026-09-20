#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_spirv_intrinsics : require
#extension GL_EXT_demote_to_helper_invocation : require
#extension GL_EXT_nonuniform_qualifier : require

#ifdef INTERP_MODE
#define PPL_NORMAL_INTERP INTERP_MODE
#define PPL_TEXCOORD_INTERP INTERP_MODE
#define PPL_LIGHT_INTERP INTERP_MODE
#define PPL_COLOR_INTERP INTERP_MODE
#else
#define PPL_NORMAL_INTERP centroid
#define PPL_TEXCOORD_INTERP
#define PPL_LIGHT_INTERP
#define PPL_COLOR_INTERP centroid
#endif

layout(location = 0) PPL_NORMAL_INTERP in vec4 in_NormalFog;
layout(location = 1) PPL_TEXCOORD_INTERP in vec4 in_Texcoord01;
layout(location = 2) PPL_TEXCOORD_INTERP in vec4 in_Texcoord23;
layout(location = 3) PPL_LIGHT_INTERP in vec4 in_ViewPosition;
layout(location = 4) PPL_COLOR_INTERP in vec4 in_Color0;

layout(location = 0) out vec4 out_Color0;

#include "d3d9_morrowind_ppl_common.glsl"

const uint D3DTOP_SELECTARG1          = 2u;
const uint D3DTOP_SELECTARG2          = 3u;
const uint D3DTOP_MODULATE            = 4u;
const uint D3DTOP_MODULATE2X          = 5u;
const uint D3DTOP_MODULATE4X          = 6u;
const uint D3DTOP_ADD                 = 7u;
const uint D3DTOP_ADDSIGNED           = 8u;
const uint D3DTOP_ADDSIGNED2X         = 9u;
const uint D3DTOP_SUBTRACT            = 10u;
const uint D3DTOP_BLENDDIFFUSEALPHA   = 12u;
const uint D3DTOP_BLENDTEXTUREALPHA   = 13u;
const uint D3DTOP_BUMPENVMAP          = 22u;
const uint D3DTOP_BUMPENVMAPLUMINANCE = 23u;
const uint D3DTOP_DOTPRODUCT3         = 24u;
const uint D3DTOP_MULTIPLYADD         = 25u;

const uint D3DTA_DIFFUSE = 0u;
const uint D3DTA_CURRENT = 1u;
const uint D3DTA_TEXTURE = 2u;

const uint VK_COMPARE_OP_LESS             = 1u;
const uint VK_COMPARE_OP_EQUAL            = 2u;
const uint VK_COMPARE_OP_LESS_OR_EQUAL    = 3u;
const uint VK_COMPARE_OP_GREATER          = 4u;
const uint VK_COMPARE_OP_NOT_EQUAL        = 5u;
const uint VK_COMPARE_OP_GREATER_OR_EQUAL = 6u;
const uint VK_COMPARE_OP_ALWAYS           = 7u;

layout(push_constant, scalar, row_major)
uniform RenderStates {
    D3D9SharedPushData global;

    layout(offset = MaxSharedPushDataSize)
    D3D9FfpsPushData ffps;

    uint packedSamplerIndices[TextureStageCount / 2u];
};

layout(set = SRV_SET, binding = SRV_PS_BASE) uniform texture2D pplTextures[TextureStageCount];
layout(set = SAMPLER_SET, binding = 0) uniform sampler sampler_heap[];

uint loadPplSamplerHeapIndex(uint samplerBindingIndex) {
    uint packedSamplerIndex = packedSamplerIndices[samplerBindingIndex / 2u];
    return bitfieldExtract(packedSamplerIndex, 16 * (int(samplerBindingIndex) & 1), 16);
}

vec2 loadPplTexcoord(uint index) {
    vec2 result = in_Texcoord01.xy;
    result = mix(result, in_Texcoord01.zw, bvec2(index == 1u));
    result = mix(result, in_Texcoord23.xy, bvec2(index == 2u));
    result = mix(result, in_Texcoord23.zw, bvec2(index == 3u));
    return result;
}

vec2 getPplStageTexcoord(uint stageIndex) {
    MorrowindPplStage stage = ppl.stages[stageIndex];
    uint coordIndex = stage.texcoordGen != 0u ? ppl.uvSetCount : stage.texcoordIndex;
    vec2 coord = loadPplTexcoord(coordIndex);

    if (stage.texcoordGen != 0u && hasPplFlag(MorrowindPplProjectiveTexgen))
        coord /= loadPplTexcoord(coordIndex + 1u);

    return coord;
}

vec4 samplePplTexture(uint stageIndex, vec2 coord) {
    return texture(sampler2D(
        pplTextures[stageIndex],
        sampler_heap[loadPplSamplerHeapIndex(stageIndex)]), coord);
}

vec4 resolvePplArg(uint arg, vec4 current, vec4 diffuse, vec4 textureValue) {
    switch (arg) {
        case D3DTA_DIFFUSE: return diffuse;
        case D3DTA_CURRENT: return current;
        case D3DTA_TEXTURE: return textureValue;
        default:            return vec4(0.0);
    }
}

vec3 calculatePplPointLighting(vec3 normal) {
    vec3 result = vec3(0.0);

    for (uint light = 0u; light < ppl.lightSlotCount; light++) {
        vec3 lightVector = vec3(
            ppl.lightPosition[0][light],
            ppl.lightPosition[1][light],
            ppl.lightPosition[2][light]) - in_ViewPosition.xyz;

        float distanceSquared = dot(lightVector, lightVector);
        float lambert = clamp(dot(normal, lightVector) / sqrt(distanceSquared), 0.0, 1.0);
        float attenuation = 1.0
            / (ppl.lightFalloffQuadratic[light] * distanceSquared + ppl.lightFalloffConstant);

        // Fade to zero over the last quarter of the light's cutoff distance.
        // An inverse radius of 0 leaves the light unchanged.
        float fade = clamp(4.0 * sqrt(distanceSquared) * ppl.lightFadeInvRadius[light] - 3.0, 0.0, 1.0);
        fade = 1.0 - fade * fade;
        attenuation *= fade * fade;

        result += (lambert + ppl.lightAmbient[light])
            * attenuation
            * ppl.lightDiffuse[light].rgb;
    }

    return result;
}

vec3 tonemapPpl(vec3 color) {
    color = clamp(color, 0.0, 2.2);
    return (((0.0548303 * color - 0.189786) * color - 0.154732) * color + 1.12969) * color;
}

bool alphaTestPpl(float alpha) {
    uint compareOp = bitfieldExtract(alphaTestAndModeArgs, 0, 3);
    uint precisionBits = bitfieldExtract(alphaTestAndModeArgs, 4, 4);
    uint alphaRefInitial = bitfieldExtract(global.packedFogColorAndAlphaRef, 24, 8);

    if (compareOp < VK_COMPARE_OP_ALWAYS) {
        float alphaRef;

        if (precisionBits <= 8u) {
            uint alphaRefInt = (alphaRefInitial << precisionBits)
                | (alphaRefInitial >> (8u - precisionBits));
            alphaRef = float(alphaRefInt);
            float alphaFactor = float((256u << precisionBits) - 1u);
            alpha = roundEven(alpha * alphaFactor);
        } else {
            alphaRef = float(alphaRefInitial) / 255.0;
        }

        switch (compareOp) {
            case VK_COMPARE_OP_LESS:             return alpha <  alphaRef;
            case VK_COMPARE_OP_EQUAL:            return alpha == alphaRef;
            case VK_COMPARE_OP_LESS_OR_EQUAL:    return alpha <= alphaRef;
            case VK_COMPARE_OP_GREATER:          return alpha >  alphaRef;
            case VK_COMPARE_OP_NOT_EQUAL:        return alpha != alphaRef;
            case VK_COMPARE_OP_GREATER_OR_EQUAL: return alpha >= alphaRef;
            default:                             return false;
        }
    }

    return true;
}

void main() {
    vec3 normal = normalize(in_NormalFog.xyz);
    float fog = in_NormalFog.w;

    vec3 diffuseLight = ppl.sunDiffuse.rgb
        * clamp(dot(normal, -ppl.sunDirection.xyz), 0.0, 1.0);
    vec3 ambientLight = ppl.sceneAmbient.rgb;
    diffuseLight += calculatePplPointLighting(normal);

    vec4 diffuse;
    switch (ppl.vertexMaterialMode) {
        case 0u:
            diffuse = hasPplFlag(MorrowindPplVertexColor) ? in_Color0 : vec4(1.0);
            break;
        case 1u:
            diffuse = vec4(
                ppl.materialDiffuse.rgb * diffuseLight
                    + ppl.materialAmbient.rgb * ambientLight
                    + ppl.materialEmissive.rgb,
                ppl.materialDiffuse.a);
            break;
        case 2u:
            diffuse = vec4(
                in_Color0.rgb * (diffuseLight + ambientLight) + ppl.materialEmissive.rgb,
                in_Color0.a);
            break;
        default:
            diffuse = vec4(
                ppl.materialDiffuse.rgb * diffuseLight
                    + ppl.materialAmbient.rgb * ambientLight
                    + in_Color0.rgb,
                ppl.materialDiffuse.a);
            break;
    }

    vec4 current = diffuse;
    vec4 cachedTexture = vec4(0.0);
    uint cachedTextureStage = ~0u;

    for (uint stageIndex = 0u; stageIndex < ppl.activeStageCount; stageIndex++) {
        MorrowindPplStage stage = ppl.stages[stageIndex];
        vec2 texcoord = getPplStageTexcoord(stageIndex);
        vec4 textureValue = cachedTextureStage == stageIndex
            ? cachedTexture
            : samplePplTexture(stageIndex, texcoord);

        if (stage.colorOp == D3DTOP_BUMPENVMAP
         || stage.colorOp == D3DTOP_BUMPENVMAPLUMINANCE) {
            vec2 offset = vec2(
                textureValue.r * ppl.bumpMatrix.x + textureValue.g * ppl.bumpMatrix.z,
                textureValue.r * ppl.bumpMatrix.y + textureValue.g * ppl.bumpMatrix.w);
            vec4 bump = samplePplTexture(stageIndex + 1u,
                getPplStageTexcoord(stageIndex + 1u) + offset);
            bump.a = textureValue.a;

            if (stage.colorOp == D3DTOP_BUMPENVMAPLUMINANCE) {
                bump.rgb *= clamp(
                    textureValue.b * ppl.bumpLumiScaleBias.x + ppl.bumpLumiScaleBias.y,
                    0.0,
                    1.0);
            }

            cachedTexture = bump;
            cachedTextureStage = stageIndex + 1u;
            continue;
        }

        vec4 arg1 = resolvePplArg(stage.colorArg1, current, diffuse, textureValue);
        vec4 arg2 = resolvePplArg(stage.colorArg2, current, diffuse, textureValue);
        vec4 value = current;
        bool writeValue = true;
        bool rgbOnly = (stage.flags & MorrowindStageAlphaMatchesColor) == 0u;

        switch (stage.colorOp) {
            case D3DTOP_SELECTARG1:        value = arg1; break;
            case D3DTOP_SELECTARG2:        value = arg2; break;
            case D3DTOP_MODULATE:          value = arg1 * arg2; break;
            case D3DTOP_MODULATE2X:        value = 2.0 * arg1 * arg2; break;
            case D3DTOP_MODULATE4X:        value = 4.0 * arg1 * arg2; break;
            case D3DTOP_ADD:               value = arg1 + arg2; break;
            case D3DTOP_ADDSIGNED:         value = arg1 + arg2 - 0.5; break;
            case D3DTOP_ADDSIGNED2X:       value = 2.0 * (arg1 + arg2) - 1.0; break;
            case D3DTOP_SUBTRACT:          value = arg1 - arg2; break;
            case D3DTOP_BLENDDIFFUSEALPHA: value = mix(arg1, arg2, diffuse.a); break;
            case D3DTOP_BLENDTEXTUREALPHA:
                writeValue = false;
                break;
            case D3DTOP_DOTPRODUCT3:
                value = vec4(dot(arg1.rgb, arg2.rgb));
                rgbOnly = true;
                break;
            case D3DTOP_MULTIPLYADD:
                value = vec4(arg1.rgb * arg2.rgb
                    + resolvePplArg(stage.colorArg0, current, diffuse, textureValue).rgb, current.a);
                rgbOnly = true;
                break;
            default:
                writeValue = false;
                break;
        }

        if (writeValue) {
            if (rgbOnly)
                current.rgb = value.rgb;
            else
                current = value;
        }

        if ((stage.flags & MorrowindStageAlphaSelectArg1) != 0u) {
            if (stage.colorArg1 == D3DTA_DIFFUSE)
                current.a = diffuse.a;
            else if (stage.colorArg1 == D3DTA_TEXTURE)
                current.a = textureValue.a;
        }
    }

    current.rgb = tonemapPpl(current.rgb);
    if (ppl.fogMode == 1u)
        current.rgb = mix(ppl.fogColor.rgb, current.rgb, fog);
    else if (ppl.fogMode == 2u)
        current.rgb *= fog;

    out_Color0 = current;

    if (!alphaTestPpl(current.a))
        demote;
}
