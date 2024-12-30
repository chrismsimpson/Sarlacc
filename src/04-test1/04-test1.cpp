#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define MTK_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#include <AppKit/AppKit.hpp>
#include <Metal/Metal.hpp>
#include <MetalKit/MetalKit.hpp>
#include <simd/simd.h>

struct shader_types {
    struct InstanceData {
        simd::float4x4 instanceTransform;
        simd::float4 instanceColor;
    };

    struct CameraData {
        simd::float4x4 perspectiveTransform;
        simd::float4x4 worldTransform;
    };
};

// Constants
static constexpr size_t kMaxFramesInFlight = 3;

// Namespace for mathematical utilities
namespace math {
constexpr simd::float3 add(const simd::float3& a, const simd::float3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

simd::float4x4 makeIdentity()
{
    return simd_matrix(
        simd::float4 { 1.f, 0.f, 0.f, 0.f },
        simd::float4 { 0.f, 1.f, 0.f, 0.f },
        simd::float4 { 0.f, 0.f, 1.f, 0.f },
        simd::float4 { 0.f, 0.f, 0.f, 1.f });
}

simd::float4x4 makePerspective(float fovRadians, float aspect, float znear, float zfar)
{
    float ys = 1.f / tanf(fovRadians * 0.5f);
    float xs = ys / aspect;
    float zs = zfar / (znear - zfar);
    return simd_matrix(
        simd::float4 { xs, 0.0f, 0.0f, 0.0f },
        simd::float4 { 0.0f, ys, 0.0f, 0.0f },
        simd::float4 { 0.0f, 0.0f, zs, znear * zs },
        simd::float4 { 0.0f, 0.0f, -1.0f, 0.0f });
}

simd::float4x4 makeXRotate(float angleRadians)
{
    float a = angleRadians;
    return simd_matrix(
        simd::float4 { 1.0f, 0.0f, 0.0f, 0.0f },
        simd::float4 { 0.0f, cosf(a), sinf(a), 0.0f },
        simd::float4 { 0.0f, -sinf(a), cosf(a), 0.0f },
        simd::float4 { 0.0f, 0.0f, 0.0f, 1.0f });
}

simd::float4x4 makeYRotate(float angleRadians)
{
    float a = angleRadians;
    return simd_matrix(
        simd::float4 { cosf(a), 0.0f, sinf(a), 0.0f },
        simd::float4 { 0.0f, 1.0f, 0.0f, 0.0f },
        simd::float4 { -sinf(a), 0.0f, cosf(a), 0.0f },
        simd::float4 { 0.0f, 0.0f, 0.0f, 1.0f });
}

simd::float4x4 makeZRotate(float angleRadians)
{
    float a = angleRadians;
    return simd_matrix(
        simd::float4 { cosf(a), sinf(a), 0.0f, 0.0f },
        simd::float4 { -sinf(a), cosf(a), 0.0f, 0.0f },
        simd::float4 { 0.0f, 0.0f, 1.0f, 0.0f },
        simd::float4 { 0.0f, 0.0f, 0.0f, 1.0f });
}

simd::float4x4 makeTranslate(const simd::float3& v)
{
    return simd_matrix(
        simd::float4 { 1.0f, 0.0f, 0.0f, 0.0f },
        simd::float4 { 0.0f, 1.0f, 0.0f, 0.0f },
        simd::float4 { 0.0f, 0.0f, 1.0f, 0.0f },
        simd::float4 { v.x, v.y, v.z, 1.0f });
}

simd::float4x4 makeScale(const simd::float3& v)
{
    return simd_matrix(
        simd::float4 { v.x, 0, 0, 0 },
        simd::float4 { 0, v.y, 0, 0 },
        simd::float4 { 0, 0, v.z, 0 },
        simd::float4 { 0, 0, 0, 1.0f });
}
}

// Structure to hold vertex data
struct Vertex {
    simd::float3 position;
};

// Structure to hold triangle data
struct Triangle {
    Vertex vertices[3];
    simd::float4 color;

    Triangle()
        : color { 1.0f, 1.0f, 1.0f, 1.0f }
    {
    }
    Triangle(Vertex v0, Vertex v1, Vertex v2, simd::float4 c = { 1.0f, 1.0f, 1.0f, 1.0f })
    {
        vertices[0] = v0;
        vertices[1] = v1;
        vertices[2] = v2;
        color = c;
    }
};

// Function to load OBJ file
std::optional<std::vector<Triangle>> loadOBJ(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open OBJ file: " << filename << std::endl;
        return std::nullopt;
    }

    std::vector<simd::float3> temp_vertices;
    std::vector<Triangle> triangles;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string type;
        iss >> type;
        if (type == "v") {
            float x, y, z;
            iss >> x >> y >> z;
            temp_vertices.emplace_back(simd::float3 { x, y, z });
        } else if (type == "f") {
            int v1, v2, v3;
            iss >> v1 >> v2 >> v3;
            // OBJ indices start at 1
            if (v1 <= 0 || v2 <= 0 || v3 <= 0 || v1 > temp_vertices.size() || v2 > temp_vertices.size() || v3 > temp_vertices.size()) {
                std::cerr << "Invalid face in OBJ file: " << filename << std::endl;
                return std::nullopt;
            }
            Vertex vert1 = { temp_vertices[v1 - 1] };
            Vertex vert2 = { temp_vertices[v2 - 1] };
            Vertex vert3 = { temp_vertices[v3 - 1] };
            // Simple coloring based on vertex heights
            float z1 = vert1.position.z;
            float z2 = vert2.position.z;
            float z3 = vert3.position.z;
            simd::float4 color;
            if (z1 == 0 && z2 == 0 && z3 == 0) {
                color = simd::float4 { 0.0f, 0.0f, 1.0f, 0.8f }; // Blue (water)
            } else if (z1 == 0 || z2 == 0 || z3 == 0) {
                color = simd::float4 { 1.0f, 1.0f, 0.44f, 0.6f }; // Yellow (sand)
            } else {
                color = simd::float4 { 0.0f, 0.44f, 0.0f, 0.6f }; // Green (grass)
            }
            triangles.emplace_back(vert1, vert2, vert3, color);
        }
    }

    file.close();
    return triangles;
}

// Renderer class handling Metal rendering
class Renderer {
public:
    Renderer(MTL::Device* pDevice, const std::vector<Triangle>& meshTriangles)
        : _pDevice(pDevice->retain())
        , _frame(0)
        , _angle(0.0f)
    {
        _pCommandQueue = _pDevice->newCommandQueue();
        buildShaders();
        buildDepthStencilStates();
        buildBuffers(meshTriangles);
        _semaphore = dispatch_semaphore_create(kMaxFramesInFlight);
    }

    ~Renderer()
    {
        _pShaderLibrary->release();
        _pPSO->release();
        _pDepthStencilState->release();
        _pVertexBuffer->release();
        _pIndexBuffer->release();
        for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
            _pInstanceBuffers[i]->release();
            _pCameraBuffers[i]->release();
        }
        _pCommandQueue->release();
        _pDevice->release();
    }

    void draw(MTK::View* pView)
    {
        dispatch_semaphore_wait(_semaphore, DISPATCH_TIME_FOREVER);
        MTL::CommandBuffer* pCmdBuffer = _pCommandQueue->commandBuffer();
        pCmdBuffer->addCompletedHandler([this](MTL::CommandBuffer* buffer) {
            dispatch_semaphore_signal(_semaphore);
        });

        // Update angle for rotation
        _angle += 0.01f;

        // Update instance data
        shader_types::InstanceData* pInstanceData = reinterpret_cast<shader_types::InstanceData*>(_pInstanceBuffers[_frame]->contents());
        for (size_t i = 0; i < _triangles.size(); ++i) {
            // Simple rotation around Y-axis
            simd::float4x4 rotation = math::makeYRotate(_angle);
            simd::float4x4 scale = math::makeScale(simd::float3 { 1.0f, 1.0f, 1.0f });
            simd::float4x4 translate = math::makeTranslate(simd::float3 { 0.0f, 0.0f, -5.0f });

            pInstanceData[i].instanceTransform = rotation * scale * translate;
            pInstanceData[i].instanceColor = _triangles[i].color;
        }
        _pInstanceBuffers[_frame]->didModifyRange(NS::Range::Make(0, _pInstanceBuffers[_frame]->length()));

        // Update camera data
        shader_types::CameraData* pCameraData = reinterpret_cast<shader_types::CameraData*>(_pCameraBuffers[_frame]->contents());
        pCameraData->perspectiveTransform = math::makePerspective(90.0f * (M_PI / 180.0f), pView->drawableSize().width / pView->drawableSize().height, 0.1f, 1000.0f);
        pCameraData->worldTransform = math::makeIdentity();
        _pCameraBuffers[_frame]->didModifyRange(NS::Range::Make(0, sizeof(shader_types::CameraData)));

        // Begin Render Pass
        MTL::RenderPassDescriptor* pRenderPass = pView->currentRenderPassDescriptor();
        if (!pRenderPass) {
            pCmdBuffer->commit();
            pCmdBuffer->release();
            return;
        }

        MTL::RenderCommandEncoder* pEncoder = pCmdBuffer->renderCommandEncoder(pRenderPass);
        pEncoder->setRenderPipelineState(_pPSO);
        pEncoder->setDepthStencilState(_pDepthStencilState);
        pEncoder->setVertexBuffer(_pVertexBuffer, 0, 0);
        pEncoder->setVertexBuffer(_pInstanceBuffers[_frame], 0, 1);
        pEncoder->setVertexBuffer(_pCameraBuffers[_frame], 0, 2);
        pEncoder->drawIndexedPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle, _indexCount, MTL::IndexType::IndexTypeUInt16, _pIndexBuffer, 0);
        pEncoder->endEncoding();

        pCmdBuffer->presentDrawable(pView->currentDrawable());
        pCmdBuffer->commit();

        _frame = (_frame + 1) % kMaxFramesInFlight;
    }

private:
    // Shader-related structures
    // struct shader_types {
    //     struct InstanceData {
    //         simd::float4x4 instanceTransform;
    //         simd::float4 instanceColor;
    //     };

    //     struct CameraData {
    //         simd::float4x4 perspectiveTransform;
    //         simd::float4x4 worldTransform;
    //     };
    // };

    void buildShaders()
    {
        using NS::StringEncoding::UTF8StringEncoding;

        const char* shaderSrc = R"(
            #include <metal_stdlib>
            using namespace metal;

            struct v2f {
                float4 position [[position]];
                float4 color;
            };

            struct VertexData {
                float3 position;
            };

            struct InstanceData {
                float4x4 instanceTransform;
                float4 instanceColor;
            };

            struct CameraData {
                float4x4 perspectiveTransform;
                float4x4 worldTransform;
            };

            vertex v2f vertexMain(
                device const VertexData* vertexData [[buffer(0)]],
                device const InstanceData* instanceData [[buffer(1)]],
                device const CameraData& cameraData [[buffer(2)]],
                uint vertexId [[vertex_id]],
                uint instanceId [[instance_id]]
            ) {
                v2f out;
                float4 pos = float4(vertexData[vertexId].position, 1.0);
                pos = instanceData[instanceId].instanceTransform * pos;
                pos = cameraData.perspectiveTransform * cameraData.worldTransform * pos;
                out.position = pos;
                out.color = instanceData[instanceId].instanceColor;
                return out;
            }

            fragment float4 fragmentMain(v2f in [[stage_in]]) {
                return in.color;
            }
        )";

        NS::Error* pError = nullptr;
        MTL::Library* pLibrary = _pDevice->newLibrary(NS::String::string(shaderSrc, UTF8StringEncoding), nullptr, &pError);
        if (!pLibrary) {
            std::cerr << "Failed to create shader library: " << pError->localizedDescription()->utf8String() << std::endl;
            assert(false);
        }

        MTL::Function* pVertexFn = pLibrary->newFunction(NS::String::string("vertexMain", UTF8StringEncoding));
        MTL::Function* pFragFn = pLibrary->newFunction(NS::String::string("fragmentMain", UTF8StringEncoding));

        MTL::RenderPipelineDescriptor* pDesc = MTL::RenderPipelineDescriptor::alloc()->init();
        pDesc->setVertexFunction(pVertexFn);
        pDesc->setFragmentFunction(pFragFn);
        pDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB);
        pDesc->setDepthAttachmentPixelFormat(MTL::PixelFormat::PixelFormatDepth32Float);

        _pPSO = _pDevice->newRenderPipelineState(pDesc, &pError);
        if (!_pPSO) {
            std::cerr << "Failed to create pipeline state: " << pError->localizedDescription()->utf8String() << std::endl;
            assert(false);
        }

        pVertexFn->release();
        pFragFn->release();
        pDesc->release();
        _pShaderLibrary = pLibrary;
    }

    void buildDepthStencilStates()
    {
        MTL::DepthStencilDescriptor* pDesc = MTL::DepthStencilDescriptor::alloc()->init();
        pDesc->setDepthCompareFunction(MTL::CompareFunction::CompareFunctionLess);
        pDesc->setDepthWriteEnabled(true);
        _pDepthStencilState = _pDevice->newDepthStencilState(pDesc);
        pDesc->release();
    }

    void buildBuffers(const std::vector<Triangle>& meshTriangles)
    {
        // Flatten vertex data
        std::vector<Vertex> vertices;
        std::vector<uint16_t> indices;
        size_t index = 0;
        for (const auto& tri : meshTriangles) {
            vertices.push_back(tri.vertices[0]);
            vertices.push_back(tri.vertices[1]);
            vertices.push_back(tri.vertices[2]);
            indices.push_back(index++);
            indices.push_back(index++);
            indices.push_back(index++);
        }
        _indexCount = indices.size();

        // Create vertex buffer
        size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
        _pVertexBuffer = _pDevice->newBuffer(vertexBufferSize, MTL::ResourceStorageModeManaged);
        memcpy(_pVertexBuffer->contents(), vertices.data(), vertexBufferSize);
        _pVertexBuffer->didModifyRange(NS::Range::Make(0, vertexBufferSize));

        // Create index buffer
        size_t indexBufferSize = indices.size() * sizeof(uint16_t);
        _pIndexBuffer = _pDevice->newBuffer(indexBufferSize, MTL::ResourceStorageModeManaged);
        memcpy(_pIndexBuffer->contents(), indices.data(), indexBufferSize);
        _pIndexBuffer->didModifyRange(NS::Range::Make(0, indexBufferSize));

        // Create instance buffers
        _triangles = meshTriangles;
        size_t instanceBufferSize = _triangles.size() * sizeof(shader_types::InstanceData);
        for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
            _pInstanceBuffers[i] = _pDevice->newBuffer(instanceBufferSize, MTL::ResourceStorageModeManaged);
        }

        // Create camera buffers
        size_t cameraBufferSize = sizeof(shader_types::CameraData);
        for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
            _pCameraBuffers[i] = _pDevice->newBuffer(cameraBufferSize, MTL::ResourceStorageModeManaged);
        }
    }

    MTL::Device* _pDevice = nullptr;
    MTL::CommandQueue* _pCommandQueue = nullptr;
    MTL::Library* _pShaderLibrary = nullptr;
    MTL::RenderPipelineState* _pPSO = nullptr;
    MTL::DepthStencilState* _pDepthStencilState = nullptr;
    MTL::Buffer* _pVertexBuffer = nullptr;
    MTL::Buffer* _pIndexBuffer = nullptr;
    MTL::Buffer* _pInstanceBuffers[kMaxFramesInFlight] = { nullptr };
    MTL::Buffer* _pCameraBuffers[kMaxFramesInFlight] = { nullptr };
    dispatch_semaphore_t _semaphore;
    size_t _indexCount = 0;
    size_t _frame;
    float _angle;

    // Shader types
    // struct shader_types {
    //     struct InstanceData {
    //         simd::float4x4 instanceTransform;
    //         simd::float4 instanceColor;
    //     };

    //     struct CameraData {
    //         simd::float4x4 perspectiveTransform;
    //         simd::float4x4 worldTransform;
    //     };
    // };

    // Mesh data
    std::vector<Triangle> _triangles;
};

// Custom MTKViewDelegate
class MyMTKViewDelegate : public MTK::ViewDelegate {
public:
    MyMTKViewDelegate(MTL::Device* pDevice, const std::vector<Triangle>& meshTriangles)
        : m_renderer(new Renderer(pDevice, meshTriangles))
    {
    }

    virtual ~MyMTKViewDelegate() override
    {
        delete m_renderer;
    }

    virtual void drawInMTKView(MTK::View* pView) override
    {
        m_renderer->draw(pView);
    }

private:
    Renderer* m_renderer;
};

// Application Delegate
class MyAppDelegate : public NS::ApplicationDelegate {
public:
    ~MyAppDelegate()
    {
        if (_pMtkView)
            _pMtkView->release();
        if (_pWindow)
            _pWindow->release();
        if (_pDevice)
            _pDevice->release();
        delete _pViewDelegate;
    }

    NS::Menu* createMenuBar()
    {
        using NS::StringEncoding::UTF8StringEncoding;

        NS::Menu* pMainMenu = NS::Menu::alloc()->init();
        NS::MenuItem* pAppMenuItem = NS::MenuItem::alloc()->init();
        NS::Menu* pAppMenu = NS::Menu::alloc()->init(NS::String::string("Appname", UTF8StringEncoding));

        NS::String* appName = NS::RunningApplication::currentApplication()->localizedName();
        NS::String* quitItemName = NS::String::string("Quit ", UTF8StringEncoding)->stringByAppendingString(appName);
        SEL quitCb = NS::MenuItem::registerActionCallback("appQuit", [](void*, SEL, const NS::Object* pSender) {
            auto pApp = NS::Application::sharedApplication();
            pApp->terminate(pSender);
        });

        NS::MenuItem* pAppQuitItem = pAppMenu->addItem(quitItemName, quitCb, NS::String::string("q", UTF8StringEncoding));
        pAppQuitItem->setKeyEquivalentModifierMask(NS::EventModifierFlagCommand);
        pAppMenuItem->setSubmenu(pAppMenu);

        NS::MenuItem* pWindowMenuItem = NS::MenuItem::alloc()->init();
        NS::Menu* pWindowMenu = NS::Menu::alloc()->init(NS::String::string("Window", UTF8StringEncoding));

        SEL closeWindowCb = NS::MenuItem::registerActionCallback("windowClose", [](void*, SEL, const NS::Object*) {
            auto pApp = NS::Application::sharedApplication();
            pApp->windows()->object<NS::Window>(0)->close();
        });
        NS::MenuItem* pCloseWindowItem = pWindowMenu->addItem(NS::String::string("Close Window", UTF8StringEncoding), closeWindowCb, NS::String::string("w", UTF8StringEncoding));
        pCloseWindowItem->setKeyEquivalentModifierMask(NS::EventModifierFlagCommand);

        pWindowMenuItem->setSubmenu(pWindowMenu);

        pMainMenu->addItem(pAppMenuItem);
        pMainMenu->addItem(pWindowMenuItem);

        pAppMenuItem->release();
        pWindowMenuItem->release();
        pAppMenu->release();
        pWindowMenu->release();

        return pMainMenu->autorelease();
    }

    virtual void applicationWillFinishLaunching(NS::Notification* pNotification) override
    {
        NS::Menu* pMenu = createMenuBar();
        NS::Application* pApp = reinterpret_cast<NS::Application*>(pNotification->object());
        pApp->setMainMenu(pMenu);
        pApp->setActivationPolicy(NS::ActivationPolicy::ActivationPolicyRegular);
    }

    virtual void applicationDidFinishLaunching(NS::Notification* pNotification) override
    {
        // Load mesh
        // std::string objFilename = "path_to_your_obj_file.obj"; // Replace with your OBJ file path
        std::string objFilename = "/Users/chris/teapot.obj"; // Replace with your OBJ file path
        auto meshOpt = loadOBJ(objFilename);
        if (!meshOpt.has_value()) {
            std::cerr << "Failed to load mesh. Exiting." << std::endl;
            NS::Application::sharedApplication()->terminate(nullptr);
            return;
        }
        std::vector<Triangle> meshTriangles = meshOpt.value();

        CGRect frame = (CGRect) { { 100.0, 100.0 }, { 800.0, 600.0 } };
        _pWindow = NS::Window::alloc()->init(
            frame,
            NS::WindowStyleMaskClosable | NS::WindowStyleMaskTitled | NS::WindowStyleMaskResizable,
            NS::BackingStoreBuffered,
            false);

        _pDevice = MTL::CreateSystemDefaultDevice();
        if (!_pDevice) {
            std::cerr << "Metal is not supported on this device." << std::endl;
            NS::Application::sharedApplication()->terminate(nullptr);
            return;
        }

        _pMtkView = MTK::View::alloc()->init(frame, _pDevice);
        _pMtkView->setColorPixelFormat(MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB);
        _pMtkView->setClearColor(MTL::ClearColor::Make(0.1, 0.1, 0.1, 1.0));
        _pMtkView->setDepthStencilPixelFormat(MTL::PixelFormat::PixelFormatDepth32Float);
        _pMtkView->setClearDepth(1.0f);
        _pMtkView->setSampleCount(1);

        _pViewDelegate = new MyMTKViewDelegate(_pDevice, meshTriangles);
        _pMtkView->setDelegate(_pViewDelegate);

        _pWindow->setContentView(_pMtkView);
        _pWindow->setTitle(NS::String::string("Metal 3D Renderer", NS::StringEncoding::UTF8StringEncoding));
        _pWindow->makeKeyAndOrderFront(nullptr);

        NS::Application* pApp = reinterpret_cast<NS::Application*>(pNotification->object());
        pApp->activateIgnoringOtherApps(true);
    }

    virtual bool applicationShouldTerminateAfterLastWindowClosed(NS::Application* pSender) override
    {
        return true;
    }

private:
    NS::Window* _pWindow = nullptr;
    MTK::View* _pMtkView = nullptr;
    MTL::Device* _pDevice = nullptr;
    MyMTKViewDelegate* _pViewDelegate = nullptr;
};

// Main function
int main(int argc, const char* argv[])
{
    NS::AutoreleasePool* pAutoreleasePool = NS::AutoreleasePool::alloc()->init();

    MyAppDelegate delegate;
    NS::Application* pApp = NS::Application::sharedApplication();
    pApp->setDelegate(&delegate);
    pApp->run();

    pAutoreleasePool->release();
    return 0;
}
