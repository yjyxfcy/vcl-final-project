// CasePathTracing.h
#pragma once

#include "Engine/GL/Frame.hpp"
#include "Engine/GL/Program.h"
#include "Labs/Common/ICase.h"
#include "Labs/Common/ImageRGB.h"
#include "Labs/Common/OrbitCameraManager.h"
#include "Labs/final_hw/Content.h"
#include "Labs/final_hw/PathTracing.h"
#include "Labs/final_hw/SceneObject.h"

namespace VCX::Labs::Rendering {

    class CasePathTracing : public Common::ICase {
    public:
        CasePathTracing(std::initializer_list<Assets::ExampleScene> && scenes);
        ~CasePathTracing();

        virtual std::string_view const GetName() override { return "Path Tracing (Global Illumination)"; }

        virtual void                     OnSetupPropsUI() override;
        virtual Common::CaseRenderResult OnRender(std::pair<std::uint32_t, std::uint32_t> const desiredSize) override;
        virtual void                     OnProcessInput(ImVec2 const & pos) override;

    private:
        std::vector<Assets::ExampleScene> const _scenes;
        Engine::GL::UniqueProgram               _program;
        Engine::GL::UniqueRenderFrame           _frame;
        SceneObject                             _sceneObject;
        Common::OrbitCameraManager              _cameraManager;

        Engine::GL::UniqueTexture2D _texture;
        RayIntersector              _intersector;

        std::size_t _sceneIdx { 0 };
        bool        _enableZoom { true };
        bool        _resetDirty { true };
        bool        _sceneDirty { true };
        bool        _treeDirty { true };

        // Path Tracing 参数
        int       _samplesPerPixel { 16 };
        int       _maxBounces { 5 };
        bool      _enableDirectLighting { true };
        bool      _enableRussianRoulette { true };
        bool      _enableNextEventEstimation { true };
        float     _skyLightIntensity { 0.8f };
        glm::vec3 _skyLightColor { 0.7f, 0.8f, 1.0f };
        int       _superSampleRate { 1 }; // 用于抗锯齿

        std::size_t      _pixelIndex { 0 };
        bool             _stopFlag { true };
        Common::ImageRGB _buffer;
        bool             _resizable { true };

        std::thread _task;

        auto GetBufferSize() const { return std::pair(std::uint32_t(_buffer.GetSizeX()), std::uint32_t(_buffer.GetSizeY())); }

        char const *          GetSceneName(std::size_t const i) const { return Content::SceneNames[std::size_t(_scenes[i])].c_str(); }
        Engine::Scene const & GetScene(std::size_t const i) const { return Content::Scenes[std::size_t(_scenes[i])]; }
    };
} // namespace VCX::Labs::Rendering