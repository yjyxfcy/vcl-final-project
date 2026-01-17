#include "Assets/bundled.h"
#include "Labs/final_hw/App.h"

namespace VCX::Labs::Rendering {
    using namespace Assets;

    App::App() :
        _ui(Labs::Common::UIOptions { }),
        _caseRayTracing({ ExampleScene::Floor, ExampleScene::CornellBox}), 
        _casePathTracing({ ExampleScene::Floor, ExampleScene::CornellBox}) 
        {
    }

    void App::OnFrame() {
        _ui.Setup(_cases, _caseId);
    }
}
