// PathTracing.h
#pragma once

#include "Engine/Scene.h"
#include "Labs/Common/ImageRGB.h"
#include "Labs/final_hw/Ray.h"
#include "Labs/final_hw/tasks.h"
#include <glm/glm.hpp>
#include <random>
namespace VCX::Labs::Rendering {

    // 线程局部随机数生成器
    thread_local extern std::mt19937                          RandomGenerator;
    thread_local extern std::uniform_real_distribution<float> RandomDistribution;

    inline float RandomFloat() {
        return RandomDistribution(RandomGenerator);
    }

    // 采样函数
    glm::vec3 SampleHemisphereUniform(const glm::vec3 & normal);
    glm::vec3 SampleHemisphereCosine(const glm::vec3 & normal);
    glm::vec3 SampleHemisphereImportance(const glm::vec3 & normal, float roughness);

    // BRDF结构
    struct BRDF {
        glm::vec3 Diffuse;
        glm::vec3 Specular;
        float     Roughness;
        float     Metallic;
        float     IOR; // 折射率

        // 评估BRDF
        glm::vec3 Evaluate(const glm::vec3 & wi, const glm::vec3 & wo, const glm::vec3 & normal) const;

        // 重要性采样
        glm::vec3 Sample(const glm::vec3 & wo, const glm::vec3 & normal, float & pdf) const;

        // 计算采样PDF
        float PDF(const glm::vec3 & wi, const glm::vec3 & wo, const glm::vec3 & normal) const;

        // 菲涅尔项 (Schlick近似)
        glm::vec3 FresnelSchlick(float cosTheta) const;

        // 检查是否为镜面反射
        bool IsSpecular() const { return Roughness < 0.01f && Metallic > 0.9f; }

        // 检查是否为透明材质
        bool IsTransparent() const { return false; } // 扩展后可支持透明材质
    };

    // 从场景材质创建BRDF
    BRDF CreateBRDFFromMaterial(const glm::vec4 & albedo, const glm::vec4 & metaSpec);

    // 直接光照采样 (Next Event Estimation)
    glm::vec3 SampleDirectLighting(
        const RayIntersector & intersector,
        const glm::vec3 &      position,
        const glm::vec3 &      normal,
        const BRDF &           brdf,
        const glm::vec3 &      wo);

    // 环境光采样 (天空光)
    glm::vec3 SampleEnvironmentLight(
        const Ray &       ray,
        float             intensity,
        const glm::vec3 & color);

    // Path Tracing核心函数
    glm::vec3 PathTrace(
        const RayIntersector & intersector,
        Ray                    ray,
        int                    maxBounces,
        bool                   enableDirectLighting,
        bool                   enableRussianRoulette,
        bool                   enableNextEventEstimation,
        float                  skyLightIntensity,
        const glm::vec3 &      skyLightColor);

    // 渐进式Path Tracing (用于交互式渲染)
    class ProgressivePathTracer {
    public:
        ProgressivePathTracer(int width, int height);

        // 渲染一帧
        void RenderFrame(
            const RayIntersector & intersector,
            const Engine::Camera & camera,
            int                    samplesPerPixel,
            int                    maxBounces,
            bool                   enableDirectLighting,
            bool                   enableRussianRoulette,
            bool                   enableNextEventEstimation,
            float                  skyLightIntensity,
            const glm::vec3 &      skyLightColor);

        // 获取当前渲染结果
        const Common::ImageRGB & GetBuffer() const { return _buffer; }

        // 重置渲染
        void Reset() { _sampleCount = 0; }

        // 获取样本数
        int GetSampleCount() const { return _sampleCount; }

    private:
        Common::ImageRGB _buffer;
        Common::ImageRGB _accumulator;
        int              _width;
        int              _height;
        int              _sampleCount;
    };

} // namespace VCX::Labs::Rendering