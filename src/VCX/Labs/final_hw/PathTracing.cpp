// PathTracing.cpp
#include "Labs/final_hw/PathTracing.h"
#include <algorithm>
#include <cmath>

namespace VCX::Labs::Rendering {

    // 定义线程局部随机数生成器
    thread_local std::mt19937                          RandomGenerator(std::random_device {}());
    thread_local std::uniform_real_distribution<float> RandomDistribution(0.0f, 1.0f);

    // 在单位半球上均匀采样
    glm::vec3 SampleHemisphereUniform(const glm::vec3 & normal) {
        float u1 = RandomFloat();
        float u2 = RandomFloat();

        float r   = sqrt(1.0f - u1 * u1);
        float phi = 2.0f * glm::pi<float>() * u2;

        glm::vec3 local = glm::vec3(
            cos(phi) * r,
            sin(phi) * r,
            u1);

        // 构建局部坐标系
        glm::vec3 z = glm::normalize(normal);
        glm::vec3 h = glm::vec3(1.0f, 0.0f, 0.0f);
        if (fabs(z.x) <= fabs(z.y) && fabs(z.x) <= fabs(z.z))
            h = glm::vec3(1.0f, 0.0f, 0.0f);
        else if (fabs(z.y) <= fabs(z.x) && fabs(z.y) <= fabs(z.z))
            h = glm::vec3(0.0f, 1.0f, 0.0f);
        else
            h = glm::vec3(0.0f, 0.0f, 1.0f);

        glm::vec3 x = glm::normalize(glm::cross(h, z));
        glm::vec3 y = glm::normalize(glm::cross(z, x));

        return local.x * x + local.y * y + local.z * z;
    }

    // 基于余弦权重的半球采样
    glm::vec3 SampleHemisphereCosine(const glm::vec3 & normal) {
        float u1 = RandomFloat();
        float u2 = RandomFloat();

        float r     = sqrt(u1);
        float theta = 2.0f * glm::pi<float>() * u2;

        float x = r * cos(theta);
        float y = r * sin(theta);
        float z = sqrt(1.0f - u1);

        glm::vec3 local = glm::vec3(x, y, z);

        // 构建局部坐标系
        glm::vec3 zAxis = glm::normalize(normal);
        glm::vec3 h     = glm::vec3(1.0f, 0.0f, 0.0f);
        if (fabs(zAxis.x) <= fabs(zAxis.y) && fabs(zAxis.x) <= fabs(zAxis.z))
            h = glm::vec3(1.0f, 0.0f, 0.0f);
        else if (fabs(zAxis.y) <= fabs(zAxis.x) && fabs(zAxis.y) <= fabs(zAxis.z))
            h = glm::vec3(0.0f, 1.0f, 0.0f);
        else
            h = glm::vec3(0.0f, 0.0f, 1.0f);

        glm::vec3 xAxis = glm::normalize(glm::cross(h, zAxis));
        glm::vec3 yAxis = glm::normalize(glm::cross(zAxis, xAxis));

        return local.x * xAxis + local.y * yAxis + local.z * zAxis;
    }

    // 重要性采样（基于粗糙度）
    glm::vec3 SampleHemisphereImportance(const glm::vec3 & normal, float roughness) {
        // 基于粗糙度选择采样策略
        if (roughness > 0.5f) {
            // 粗糙表面使用余弦采样
            return SampleHemisphereCosine(normal);
        } else {
            // 光滑表面使用均匀采样
            return SampleHemisphereUniform(normal);
        }
    }

    // 菲涅尔项 (Schlick近似)
    glm::vec3 BRDF::FresnelSchlick(float cosTheta) const {
        glm::vec3 F0 = glm::mix(glm::vec3(0.04f), Diffuse, Metallic);
        return F0 + (glm::vec3(1.0f) - F0) * pow(1.0f - cosTheta, 5.0f);
    }
    float DistributionGGX(float ndoth, float roughness) {
        float alpha  = roughness * roughness;
        float alpha2 = alpha * alpha;
        float ndoth2 = ndoth * ndoth;
        float denom  = ndoth2 * (alpha2 - 1.0f) + 1.0f;
        return alpha2 / (glm::pi<float>() * denom * denom);
    }
    // 评估BRDF
    glm::vec3 BRDF::Evaluate(const glm::vec3 & wi, const glm::vec3 & wo, const glm::vec3 & normal) const {
        glm::vec3 h     = glm::normalize(wi + wo);
        float     ndotl = glm::max(0.0f, glm::dot(normal, wi));
        float     ndotv = glm::max(0.0f, glm::dot(normal, wo));
        float     ndoth = glm::max(0.0f, glm::dot(normal, h));
        float     vdoth = glm::max(0.0f, glm::dot(wo, h));

        // 漫反射项 (Lambert)
        glm::vec3 diffuse = Diffuse / glm::pi<float>();

        // 镜面反射项 (Cook-Torrance)
        float D, G, F;

        // 法线分布函数 (GGX)
        float alpha  = Roughness * Roughness;
        float alpha2 = alpha * alpha;
        float ndoth2 = ndoth * ndoth;
        D            = alpha2 / (glm::pi<float>() * (ndoth2 * (alpha2 - 1.0f) + 1.0f) * (ndoth2 * (alpha2 - 1.0f) + 1.0f));

        // 几何遮蔽函数 (Smith)
        float k  = (Roughness + 1.0f) * (Roughness + 1.0f) / 8.0f;
        float G1 = ndotv / (ndotv * (1.0f - k) + k);
        float G2 = ndotl / (ndotl * (1.0f - k) + k);
        G        = G1 * G2;

        // 菲涅尔项 (Schlick)
        glm::vec3 F0 = glm::mix(glm::vec3(0.04f), Diffuse, Metallic);
        F            = glm::mix(F0.x, 1.0f, pow(1.0f - vdoth, 5.0f));

        // Cook-Torrance BRDF
        glm::vec3 specular = glm::vec3(F * D * G) / (4.0f * ndotl * ndotv + 0.001f);

        // 能量守恒
        glm::vec3 kd = glm::vec3(1.0f) - glm::vec3(F);
        kd *= (1.0f - Metallic);

        return kd * diffuse + specular;
    }

    // BRDF重要性采样
    glm::vec3 BRDF::Sample(const glm::vec3 & wo, const glm::vec3 & normal, float & pdf) const {
        float r = RandomFloat();

        // 根据金属度和粗糙度决定采样策略
        if (Metallic > 0.8f && Roughness < 0.1f) {
            // 镜面反射：完美反射
            glm::vec3 wi = glm::reflect(-wo, normal);
            pdf          = 1.0f;
            return wi;
        } else if (r < 0.5f) {
            // 漫反射采样（余弦权重）
            glm::vec3 wi = SampleHemisphereCosine(normal);
            pdf          = glm::max(0.0f, glm::dot(normal, wi)) / glm::pi<float>();
            return wi;
        } else {
            // 镜面反射采样（基于粗糙度）
            float u1 = RandomFloat();
            float u2 = RandomFloat();

            float alpha    = Roughness * Roughness;
            float phi      = 2.0f * glm::pi<float>() * u1;
            float cosTheta = sqrt((1.0f - u2) / (1.0f + (alpha * alpha - 1.0f) * u2));
            float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

            glm::vec3 h = glm::vec3(
                sinTheta * cos(phi),
                sinTheta * sin(phi),
                cosTheta);

            // 转换到世界空间
            glm::vec3 z  = glm::normalize(normal);
            glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
            if (fabs(z.y) > 0.999f) up = glm::vec3(1.0f, 0.0f, 0.0f);
            glm::vec3 x = glm::normalize(glm::cross(up, z));
            glm::vec3 y = glm::cross(z, x);

            h = h.x * x + h.y * y + h.z * z;

            // 反射方向
            glm::vec3 wi = glm::reflect(-wo, h);

            // 计算PDF
            float ndoth = glm::max(0.0f, glm::dot(normal, h));
            float vdoth = glm::max(0.0f, glm::dot(wo, h));
            float D     = DistributionGGX(ndoth, Roughness);
            pdf         = (D * ndoth) / (4.0f * vdoth);

            return wi;
        }
    }

    // 计算采样PDF
    float BRDF::PDF(const glm::vec3 & wi, const glm::vec3 & wo, const glm::vec3 & normal) const {
        glm::vec3 h     = glm::normalize(wi + wo);
        float     ndoth = glm::max(0.0f, glm::dot(normal, h));
        float     vdoth = glm::max(0.0f, glm::dot(wo, h));

        // 混合PDF
        float diffuseWeight  = glm::length(Diffuse) * (1.0f - Metallic);
        float specularWeight = glm::length(Specular);
        float totalWeight    = diffuseWeight + specularWeight;

        if (totalWeight < 1e-5f) return 1.0f / (2.0f * glm::pi<float>());

        float diffuseProb = diffuseWeight / totalWeight;

        // 漫反射PDF（余弦权重）
        float diffusePdf = glm::max(0.0f, glm::dot(normal, wi)) / glm::pi<float>();

        // 镜面反射PDF（GGX）
        float alpha   = Roughness * Roughness;
        float alpha2  = alpha * alpha;
        float ndoth2  = ndoth * ndoth;
        float D       = alpha2 / (glm::pi<float>() * (ndoth2 * (alpha2 - 1.0f) + 1.0f) * (ndoth2 * (alpha2 - 1.0f) + 1.0f));
        float specPdf = (D * ndoth) / (4.0f * vdoth);

        return diffuseProb * diffusePdf + (1.0f - diffuseProb) * specPdf;
    }

    // 从场景材质创建BRDF
    BRDF CreateBRDFFromMaterial(const glm::vec4 & albedo, const glm::vec4 & metaSpec) {
        BRDF brdf;

        // 从albedo提取漫反射颜色
        brdf.Diffuse = glm::vec3(albedo);

        // 从metaSpec提取镜面反射信息
        brdf.Specular   = glm::vec3(metaSpec);
        float shininess = metaSpec.a * 256.0f;

        // 将shininess转换为roughness
        brdf.Roughness = glm::clamp(1.0f - (shininess / 256.0f), 0.01f, 1.0f);

        // 根据镜面反射强度估算金属度
        float specIntensity = glm::length(brdf.Specular);
        brdf.Metallic       = glm::clamp(specIntensity * 2.0f - 1.0f, 0.0f, 1.0f);

        // 如果金属度高，调整漫反射和镜面反射
        if (brdf.Metallic > 0.5f) {
            brdf.Diffuse  = glm::mix(brdf.Diffuse, glm::vec3(0.0f), brdf.Metallic);
            brdf.Specular = glm::mix(glm::vec3(0.04f), brdf.Diffuse, brdf.Metallic);
        }

        brdf.IOR = 1.5f; // 默认折射率

        return brdf;
    }

    // 直接光照采样 (Next Event Estimation)
    glm::vec3 SampleDirectLighting(
        const RayIntersector & intersector,
        const glm::vec3 &      position,
        const glm::vec3 &      normal,
        const BRDF &           brdf,
        const glm::vec3 &      wo) {
        glm::vec3    directLight(0.0f);
        const auto & scene  = *intersector.InternalScene;
        const auto & lights = scene.Lights;

        if (lights.empty()) {
            return directLight;
        }

        // 随机选择一个光源
        int          lightIndex = int(RandomFloat() * lights.size());
        const auto & light      = lights[lightIndex];

        glm::vec3 lightDir;
        float     lightDistance;
        glm::vec3 lightIntensity;

        if (light.Type == Engine::LightType::Point) {
            // 点光源
            lightDir      = light.Position - position;
            lightDistance = glm::length(lightDir);
            lightDir      = glm::normalize(lightDir);

            // 平方反比衰减
            float attenuation = 1.0f / (lightDistance * lightDistance);
            lightIntensity    = light.Intensity * attenuation;

        } else if (light.Type == Engine::LightType::Directional) {
            // 方向光
            lightDir       = -light.Direction;
            lightDistance  = 1e6f;
            lightIntensity = light.Intensity;

        } else if (light.Type == Engine::LightType::Spot) {
            // 聚光灯
            lightDir      = light.Position - position;
            lightDistance = glm::length(lightDir);
            lightDir      = glm::normalize(lightDir);

            // 聚光灯衰减
            float cosTheta = glm::dot(lightDir, -light.Direction);
            float cosPhi   = cos(light.OuterCutOff);
            float cosGamma = cos(light.CutOff);

            if (cosTheta > cosGamma) {
                lightIntensity = light.Intensity;
            } else if (cosTheta > cosPhi) {
                float falloff  = (cosTheta - cosPhi) / (cosGamma - cosPhi);
                lightIntensity = light.Intensity * falloff * falloff;
            } else {
                lightIntensity = glm::vec3(0.0f);
            }

            lightIntensity /= (lightDistance * lightDistance);
        }

        // 检查遮挡
        Ray  shadowRay(position + normal * EPS1, lightDir);
        auto shadowHit = intersector.IntersectRay(shadowRay);

        if (shadowHit.IntersectState) {
            if (shadowHit.IntersectAlbedo.w >= 0.2f) {
                glm::vec3 shadowPos  = shadowHit.IntersectPosition;
                float     shadowDist = glm::length(shadowPos - position);
                if (shadowDist < lightDistance - EPS1) {
                    return directLight;
                }
            }
        }

        // 计算直接光照贡献
        float ndotl = glm::max(0.0f, glm::dot(normal, lightDir));
        if (ndotl > 0.0f) {
            glm::vec3 brdfValue = brdf.Evaluate(-lightDir, wo, normal);

            // MIS (Multiple Importance Sampling) 权重
            float lightPdf = 1.0f / lights.size();
            float brdfPdf  = brdf.PDF(-lightDir, wo, normal);
            float weight   = 1.0f;
            if (brdfPdf > 0.0f) {
                weight = lightPdf * lightPdf / (lightPdf * lightPdf + brdfPdf * brdfPdf);
            }

            directLight = brdfValue * lightIntensity * ndotl * weight / lightPdf;
        }

        // 乘以光源选择概率的倒数
        directLight *= lights.size();

        return directLight;
    }

    // 环境光采样 (天空光)
    glm::vec3 SampleEnvironmentLight(
        const Ray &       ray,
        float             intensity,
        const glm::vec3 & color) {
        // 简单的均匀天空光
        return color * intensity;
    }

    // Path Tracing核心函数
    glm::vec3 PathTrace(
        const RayIntersector & intersector,
        Ray                    ray,
        int                    maxBounces,
        bool                   enableDirectLighting,
        bool                   enableRussianRoulette,
        bool                   enableNextEventEstimation,
        float                  skyLightIntensity,
        const glm::vec3 &      skyLightColor) {
        glm::vec3 throughput(1.0f);
        glm::vec3 radiance(0.0f);

        for (int bounce = 0; bounce <= maxBounces; bounce++) {
            auto rayHit = intersector.IntersectRay(ray);

            if (! rayHit.IntersectState) {
                // 命中天空，添加环境光
                radiance += throughput * SampleEnvironmentLight(ray, skyLightIntensity, skyLightColor);
                break;
            }

            const glm::vec3 pos      = rayHit.IntersectPosition;
            glm::vec3       normal   = glm::normalize(rayHit.IntersectNormal);
            const glm::vec4 albedo   = rayHit.IntersectAlbedo;
            const glm::vec4 metaSpec = rayHit.IntersectMetaSpec;

            // 确保法线朝向入射方向
            if (glm::dot(normal, -ray.Direction) < 0.0f) {
                normal = -normal;
            }

            // 创建BRDF
            BRDF brdf = CreateBRDFFromMaterial(albedo, metaSpec);

            // 自发光（如果有）
            // 注意：当前场景格式不支持自发光，这里为0
            glm::vec3 emission = glm::vec3(0.0f);
            radiance += throughput * emission;

            // 直接光照 (Next Event Estimation)
            if (enableDirectLighting && enableNextEventEstimation) {
                glm::vec3 directLight = SampleDirectLighting(intersector, pos, normal, brdf, -ray.Direction);
                radiance += throughput * directLight;
            }

            // 重要性采样下一个方向
            glm::vec3 wo = -ray.Direction;
            float     pdf;
            glm::vec3 wi = brdf.Sample(wo, normal, pdf);

            if (pdf < 1e-6f) {
                break;
            }

            // 计算BRDF值
            glm::vec3 brdfValue = brdf.Evaluate(wi, wo, normal);

            // 更新吞吐量
            float ndotl = glm::max(0.0f, glm::dot(normal, wi));
            throughput *= brdfValue * ndotl / pdf;

            // 俄罗斯轮盘赌终止
            if (enableRussianRoulette && bounce > 2) {
                float surviveProb = glm::min(1.0f, glm::max(glm::max(throughput.x, throughput.y), throughput.z));
                if (RandomFloat() > surviveProb) {
                    break;
                }
                throughput /= surviveProb;
            }

            // 准备下一次反弹
            ray = Ray(pos + normal * EPS1, wi);

            // 如果吞吐量太小，提前终止
            if (glm::max(glm::max(throughput.x, throughput.y), throughput.z) < 1e-3f) {
                break;
            }
        }

        return radiance;
    }

    // 渐进式Path Tracing实现
    ProgressivePathTracer::ProgressivePathTracer(int width, int height): _width(width), _height(height), _sampleCount(0) {
        _buffer      = Common::ImageRGB(width, height);
        _accumulator = Common::ImageRGB(width, height);
    }

    void ProgressivePathTracer::RenderFrame(
        const RayIntersector & intersector,
        const Engine::Camera & camera,
        int                    samplesPerPixel,
        int                    maxBounces,
        bool                   enableDirectLighting,
        bool                   enableRussianRoulette,
        bool                   enableNextEventEstimation,
        float                  skyLightIntensity,
        const glm::vec3 &      skyLightColor) {
        _sampleCount++;

        // 并行渲染所有像素（简化版，实际应该使用并行算法）
        for (int y = 0; y < _height; y++) {
            for (int x = 0; x < _width; x++) {
                glm::vec3 accumulatedColor(0.0f);

                // 每像素多次采样
                for (int s = 0; s < samplesPerPixel; s++) {
                    // 像素内随机采样
                    float di = RandomFloat() - 0.5f;
                    float dj = RandomFloat() - 0.5f;

                    glm::vec3 lookDir   = glm::normalize(camera.Target - camera.Eye);
                    glm::vec3 rightDir  = glm::normalize(glm::cross(lookDir, camera.Up));
                    glm::vec3 upDir     = glm::normalize(glm::cross(rightDir, lookDir));
                    float     aspect    = _width * 1.f / _height;
                    float     fovFactor = std::tan(glm::radians(camera.Fovy) / 2);

                    glm::vec3 pixelLookDir = lookDir;
                    pixelLookDir += fovFactor * (2.0f * (y + dj) / _height - 1.0f) * upDir;
                    pixelLookDir += fovFactor * aspect * (2.0f * (x + di) / _width - 1.0f) * rightDir;

                    Ray ray(camera.Eye, glm::normalize(pixelLookDir));

                    // Path Tracing
                    glm::vec3 sampleColor = PathTrace(
                        intersector,
                        ray,
                        maxBounces,
                        enableDirectLighting,
                        enableRussianRoulette,
                        enableNextEventEstimation,
                        skyLightIntensity,
                        skyLightColor);

                    accumulatedColor += sampleColor;
                }

                // 累积并平均
                glm::vec3 newSample   = accumulatedColor / float(samplesPerPixel);
                glm::vec3 oldColor    = _accumulator.At(x, y);
                glm::vec3 newColor    = (oldColor * float(_sampleCount - 1) + newSample) / float(_sampleCount);
                _accumulator.At(x, y) = newColor;
                _buffer.At(x, y)      = glm::pow(newColor, glm::vec3(1.0f / 2.2f));
            }
        }
    }

} // namespace VCX::Labs::Rendering