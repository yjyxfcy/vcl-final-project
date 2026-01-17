// CasePathTracing.cpp
#include "Labs/final_hw/CasePathTracing.h"
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <random>
namespace VCX::Labs::Rendering {

    CasePathTracing::CasePathTracing(std::initializer_list<Assets::ExampleScene> && scenes):
        _scenes(scenes),
        _program(
            Engine::GL::UniqueProgram({ Engine::GL::SharedShader("assets/shaders/flat.vert"), Engine::GL::SharedShader("assets/shaders/flat.frag") })),
        _sceneObject(4),
        _texture({ .MinFilter = Engine::GL::FilterMode::Linear, .MagFilter = Engine::GL::FilterMode::Nearest }) {
        _cameraManager.AutoRotate = false;
        _program.GetUniforms().SetByName("u_Color", glm::vec3(1, 1, 1));
    }

    CasePathTracing::~CasePathTracing() {
        _stopFlag = true;
        if (_task.joinable()) _task.join();
    }

    void CasePathTracing::OnSetupPropsUI() {
        if (ImGui::BeginCombo("Scene", GetSceneName(_sceneIdx))) {
            for (std::size_t i = 0; i < _scenes.size(); ++i) {
                bool selected = i == _sceneIdx;
                if (ImGui::Selectable(GetSceneName(i), selected)) {
                    if (! selected) {
                        _sceneIdx   = i;
                        _sceneDirty = true;
                        _treeDirty  = true;
                        _resetDirty = true;
                    }
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted("Path Tracing is a physically-based global illumination algorithm that simulates light transport for realistic rendering.");
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        if (ImGui::Button("Reset Scene")) _resetDirty = true;
        ImGui::SameLine();
        if (_task.joinable()) {
            if (ImGui::Button("Stop Rendering")) {
                _stopFlag = true;
                if (_task.joinable()) _task.join();
            }
        } else if (ImGui::Button("Start Rendering")) _stopFlag = false;

        ImGui::ProgressBar(float(_pixelIndex) / (_buffer.GetSizeX() * _buffer.GetSizeY()));
        ImGui::SameLine();
        ImGui::Text("%.1f%%", 100.0f * float(_pixelIndex) / (_buffer.GetSizeX() * _buffer.GetSizeY()));

        Common::ImGuiHelper::SaveImage(_texture, GetBufferSize(), true);
        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Path Tracing Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            _resetDirty |= ImGui::SliderInt("Samples/Pixel", &_samplesPerPixel, 1, 512);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Higher values reduce noise but increase render time");
            }

            _resetDirty |= ImGui::SliderInt("Max Bounces", &_maxBounces, 1, 20);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Maximum number of light bounces (indirect illumination)");
            }

            _resetDirty |= ImGui::Checkbox("Direct Lighting", &_enableDirectLighting);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Sample direct lighting from light sources");
            }

            _resetDirty |= ImGui::Checkbox("Russian Roulette", &_enableRussianRoulette);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Terminate low-contributing paths probabilistically");
            }

            _resetDirty |= ImGui::Checkbox("Next Event Estimation", &_enableNextEventEstimation);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Sample direct lighting explicitly (reduces noise)");
            }

            _resetDirty |= ImGui::SliderFloat("Sky Light", &_skyLightIntensity, 0.0f, 2.0f);
            _resetDirty |= ImGui::ColorEdit3("Sky Color", glm::value_ptr(_skyLightColor), ImGuiColorEditFlags_Float);

            _resetDirty |= ImGui::SliderInt("Super Sample", &_superSampleRate, 1, 4);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Anti-aliasing quality (ray samples per pixel)");
            }
        }
        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Statistics")) {
            auto const width        = _buffer.GetSizeX();
            auto const height       = _buffer.GetSizeY();
            auto const totalPixels  = width * height;
            auto const totalSamples = totalPixels * _samplesPerPixel * _superSampleRate * _superSampleRate;

            ImGui::Text("Resolution: %d x %d", width, height);
            ImGui::Text("Total Rays: ~%dM", totalSamples / 1000000);
            ImGui::Text("Progress: %d / %d pixels", _pixelIndex, totalPixels);

            if (_task.joinable()) {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "Rendering...");
            } else if (_pixelIndex == totalPixels) {
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "Render Complete");
            } else {
                ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Ready to Render");
            }
        }
        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Control")) {
            ImGui::Checkbox("Zoom Tooltip", &_enableZoom);
        }
        ImGui::Spacing();
    }

    Common::CaseRenderResult CasePathTracing::OnRender(std::pair<std::uint32_t, std::uint32_t> const desiredSize) {
        if (_resetDirty) {
            _stopFlag = true;
            if (_task.joinable()) _task.join();
            _pixelIndex = 0;
            _resizable  = true;
            _resetDirty = false;
        }

        if (_sceneDirty) {
            _sceneObject.ReplaceScene(GetScene(_sceneIdx));
            _cameraManager.Save(_sceneObject.Camera);
            _sceneDirty = false;
        }

        if (_resizable) {
            _frame.Resize(desiredSize);
            _cameraManager.Update(_sceneObject.Camera);
            _program.GetUniforms().SetByName("u_Projection", _sceneObject.Camera.GetProjectionMatrix((float(desiredSize.first) / desiredSize.second)));
            _program.GetUniforms().SetByName("u_View", _sceneObject.Camera.GetViewMatrix());

            gl_using(_frame);

            glEnable(GL_DEPTH_TEST);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            for (auto const & model : _sceneObject.OpaqueModels)
                model.Mesh.Draw({ _program.Use() });
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glDisable(GL_DEPTH_TEST);
        }

        if (! _stopFlag && ! _task.joinable()) {
            if (_pixelIndex == 0) {
                _resizable = false;
                _buffer    = _frame.GetColorAttachment().Download<Engine::Formats::RGB8>();
            }

            _task = std::thread([&]() {
                auto const width  = _buffer.GetSizeX();
                auto const height = _buffer.GetSizeY();

                if (_pixelIndex == 0 && _treeDirty) {
                    Engine::Scene const & scene = GetScene(_sceneIdx);
                    _intersector.InitScene(&scene);
                    _treeDirty = false;
                }

                // Path Tracing渲染循环
                while (_pixelIndex < std::size_t(width) * height) {
                    int i = _pixelIndex % width;
                    int j = _pixelIndex / width;

                    glm::vec3 accumulatedColor(0.0f);

                    // 每像素多次采样
                    for (int sample = 0; sample < _samplesPerPixel; ++sample) {
                        // 像素内随机采样（抗锯齿）
                        for (int dy = 0; dy < _superSampleRate; ++dy) {
                            for (int dx = 0; dx < _superSampleRate; ++dx) {
                                float step = 1.0f / _superSampleRate;
                                float di = step * (0.5f + dx), dj = step * (0.5f + dy);

                                // 添加随机抖动以减少规则采样
                                di += (RandomFloat() - 0.5f) * step;
                                dj += (RandomFloat() - 0.5f) * step;

                                auto const & camera    = _sceneObject.Camera;
                                glm::vec3    lookDir   = glm::normalize(camera.Target - camera.Eye);
                                glm::vec3    rightDir  = glm::normalize(glm::cross(lookDir, camera.Up));
                                glm::vec3    upDir     = glm::normalize(glm::cross(rightDir, lookDir));
                                float const  aspect    = width * 1.f / height;
                                float const  fovFactor = std::tan(glm::radians(camera.Fovy) / 2);

                                glm::vec3 pixelLookDir = lookDir;
                                pixelLookDir += fovFactor * (2.0f * (j + dj) / height - 1.0f) * upDir;
                                pixelLookDir += fovFactor * aspect * (2.0f * (i + di) / width - 1.0f) * rightDir;

                                Ray initialRay(camera.Eye, glm::normalize(pixelLookDir));

                                // 使用Path Tracing
                                glm::vec3 sampleColor = PathTrace(
                                    _intersector,
                                    initialRay,
                                    _maxBounces,
                                    _enableDirectLighting,
                                    _enableRussianRoulette,
                                    _enableNextEventEstimation,
                                    _skyLightIntensity,
                                    _skyLightColor);

                                accumulatedColor += sampleColor;
                            }
                        }
                    }

                    // 平均所有采样，应用gamma校正
                    float     totalSubSamples = _samplesPerPixel * _superSampleRate * _superSampleRate;
                    glm::vec3 finalColor      = accumulatedColor / totalSubSamples;
                    finalColor                = glm::pow(finalColor, glm::vec3(1.0f / 2.2f));

                    _buffer.At(i, j) = finalColor;
                    ++_pixelIndex;

                    if (_stopFlag) return;
                }
            });
        }

        if (! _resizable) {
            if (! _stopFlag) _texture.Update(_buffer);
            if (_task.joinable() && _pixelIndex == _buffer.GetSizeX() * _buffer.GetSizeY()) {
                _stopFlag = true;
                _task.join();
            }
        }

        return Common::CaseRenderResult {
            .Fixed     = false,
            .Flipped   = true,
            .Image     = _resizable ? _frame.GetColorAttachment() : _texture,
            .ImageSize = _resizable ? desiredSize : GetBufferSize(),
        };
    }

    void CasePathTracing::OnProcessInput(ImVec2 const & pos) {
        auto         window  = ImGui::GetCurrentWindow();
        bool         hovered = false;
        bool         anyHeld = false;
        ImVec2 const delta   = ImGui::GetIO().MouseDelta;
        ImGui::ButtonBehavior(window->Rect(), window->GetID("##io"), &hovered, &anyHeld);

        if (! hovered) return;

        if (_resizable) {
            _cameraManager.ProcessInput(_sceneObject.Camera, pos);
        } else {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && delta.x != 0.f)
                ImGui::SetScrollX(window, window->Scroll.x - delta.x);
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && delta.y != 0.f)
                ImGui::SetScrollY(window, window->Scroll.y - delta.y);
        }

        if (_enableZoom && ! anyHeld && ImGui::IsItemHovered())
            Common::ImGuiHelper::ZoomTooltip(_resizable ? _frame.GetColorAttachment() : _texture, GetBufferSize(), pos, true);
    }

} // namespace VCX::Labs::Rendering