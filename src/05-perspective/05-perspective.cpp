/*
 *
 * Copyright 2022 Apple Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cassert>

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define MTK_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#include <AppKit/AppKit.hpp>
#include <Metal/Metal.hpp>
#include <MetalKit/MetalKit.hpp>

#include <simd/simd.h>

static constexpr size_t NUM_INSTANCES = 32;
static constexpr size_t MAX_FRAMES_IN_FLIGHT = 3;

#pragma region Declarations {

namespace math {
constexpr simd::float3 add(const simd::float3& a, const simd::float3& b);
constexpr simd_float4x4 makeIdentity();
simd::float4x4 makePerspective();
simd::float4x4 makeXRotate(float angleRadians);
simd::float4x4 makeYRotate(float angleRadians);
simd::float4x4 makeZRotate(float angleRadians);
simd::float4x4 makeTranslate(const simd::float3& v);
simd::float4x4 makeScale(const simd::float3& v);
}

class Renderer {
public:
    Renderer(MTL::Device* device);
    ~Renderer();
    void buildShaders();
    void buildDepthStencilStates();
    void buildBuffers();
    void draw(MTK::View* view);

private:
    MTL::Device* m_device;
    MTL::CommandQueue* m_commandQueue;
    MTL::Library* m_shaderLibrary;
    MTL::RenderPipelineState* m_renderPipelineState;
    MTL::DepthStencilState* m_depthStencilState;
    MTL::Buffer* m_vertexDataBuffer;
    MTL::Buffer* m_instanceDataBuffer[MAX_FRAMES_IN_FLIGHT];
    MTL::Buffer* m_cameraDataBuffer[MAX_FRAMES_IN_FLIGHT];
    MTL::Buffer* m_indexBuffer;
    float m_angle;
    int m_frame;
    dispatch_semaphore_t m_semaphore;
    static const int MAX_FRAMES_IN_FLIGHT;
};

class MyMTKViewDelegate : public MTK::ViewDelegate {
public:
    MyMTKViewDelegate(MTL::Device* device);
    virtual ~MyMTKViewDelegate() override;
    virtual void drawInMTKView(MTK::View* view) override;

private:
    Renderer* m_renderer;
};

class MyAppDelegate : public NS::ApplicationDelegate {
public:
    ~MyAppDelegate();

    NS::Menu* createMenuBar();

    virtual void applicationWillFinishLaunching(NS::Notification* notification) override;
    virtual void applicationDidFinishLaunching(NS::Notification* notification) override;
    virtual bool applicationShouldTerminateAfterLastWindowClosed(NS::Application* sender) override;

private:
    NS::Window* m_window;
    MTK::View* m_mtkView;
    MTL::Device* m_device;
    MyMTKViewDelegate* m_viewDelegate = nullptr;
};

#pragma endregion Declarations }

int main(int argc, char* argv[])
{
    NS::AutoreleasePool* autoreleasePool = NS::AutoreleasePool::alloc()->init();

    MyAppDelegate del;

    NS::Application* sharedApplication = NS::Application::sharedApplication();
    sharedApplication->setDelegate(&del);
    sharedApplication->run();

    autoreleasePool->release();

    return 0;
}

#pragma mark - AppDelegate
#pragma region AppDelegate {

MyAppDelegate::~MyAppDelegate()
{
    m_mtkView->release();
    m_window->release();
    m_device->release();
    delete m_viewDelegate;
}

NS::Menu* MyAppDelegate::createMenuBar()
{
    using NS::StringEncoding::UTF8StringEncoding;

    NS::Menu* mainMenu = NS::Menu::alloc()->init();
    NS::MenuItem* appMenuItem = NS::MenuItem::alloc()->init();
    NS::Menu* appMenu = NS::Menu::alloc()->init(NS::String::string("Appname", UTF8StringEncoding));

    NS::String* appName = NS::RunningApplication::currentApplication()->localizedName();
    NS::String* quitItemName = NS::String::string("Quit ", UTF8StringEncoding)->stringByAppendingString(appName);
    SEL quitCallback = NS::MenuItem::registerActionCallback("appQuit", [](void*, SEL, const NS::Object* sender) {
        auto app = NS::Application::sharedApplication();
        app->terminate(sender);
    });

    NS::MenuItem* appQuitItem = appMenu->addItem(quitItemName, quitCallback, NS::String::string("q", UTF8StringEncoding));
    appQuitItem->setKeyEquivalentModifierMask(NS::EventModifierFlagCommand);
    appMenuItem->setSubmenu(appMenu);

    NS::MenuItem* windowMenuItem = NS::MenuItem::alloc()->init();
    NS::Menu* windowMenu = NS::Menu::alloc()->init(NS::String::string("Window", UTF8StringEncoding));

    SEL closeWindowCallback = NS::MenuItem::registerActionCallback("windowClose", [](void*, SEL, const NS::Object*) {
        auto app = NS::Application::sharedApplication();
        app->windows()->object<NS::Window>(0)->close();
    });
    NS::MenuItem* closeWindowItem = windowMenu->addItem(NS::String::string("Close Window", UTF8StringEncoding), closeWindowCallback, NS::String::string("w", UTF8StringEncoding));
    closeWindowItem->setKeyEquivalentModifierMask(NS::EventModifierFlagCommand);

    windowMenuItem->setSubmenu(windowMenu);

    mainMenu->addItem(appMenuItem);
    mainMenu->addItem(windowMenuItem);

    appMenuItem->release();
    windowMenuItem->release();
    appMenu->release();
    windowMenu->release();

    return mainMenu->autorelease();
}

void MyAppDelegate::applicationWillFinishLaunching(NS::Notification* notification)
{
    NS::Menu* menu = createMenuBar();
    NS::Application* app = reinterpret_cast<NS::Application*>(notification->object());
    app->setMainMenu(menu);
    app->setActivationPolicy(NS::ActivationPolicy::ActivationPolicyRegular);
}

void MyAppDelegate::applicationDidFinishLaunching(NS::Notification* notification)
{
    CGRect frame = (CGRect) { { 100.0, 100.0 }, { 512.0, 512.0 } };

    m_window = NS::Window::alloc()->init(
        frame,
        NS::WindowStyleMaskClosable | NS::WindowStyleMaskTitled,
        NS::BackingStoreBuffered,
        false);

    m_device = MTL::CreateSystemDefaultDevice();

    m_mtkView = MTK::View::alloc()->init(frame, m_device);
    m_mtkView->setColorPixelFormat(MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB);
    m_mtkView->setClearColor(MTL::ClearColor::Make(0.1, 0.1, 0.1, 1.0));
    m_mtkView->setDepthStencilPixelFormat(MTL::PixelFormat::PixelFormatDepth16Unorm);
    m_mtkView->setClearDepth(1.0f);

    m_viewDelegate = new MyMTKViewDelegate(m_device);
    m_mtkView->setDelegate(m_viewDelegate);

    m_window->setContentView(m_mtkView);
    m_window->setTitle(NS::String::string("05 - Perspective", NS::StringEncoding::UTF8StringEncoding));

    m_window->makeKeyAndOrderFront(nullptr);

    NS::Application* app = reinterpret_cast<NS::Application*>(notification->object());
    app->activateIgnoringOtherApps(true);
}

bool MyAppDelegate::applicationShouldTerminateAfterLastWindowClosed(NS::Application* sender)
{
    return true;
}

#pragma endregion AppDelegate }

#pragma mark - ViewDelegate
#pragma region ViewDelegate {

MyMTKViewDelegate::MyMTKViewDelegate(MTL::Device* device)
    : MTK::ViewDelegate()
    , m_renderer(new Renderer(device))
{
}

MyMTKViewDelegate::~MyMTKViewDelegate()
{
    delete m_renderer;
}

void MyMTKViewDelegate::drawInMTKView(MTK::View* view)
{
    m_renderer->draw(view);
}

#pragma endregion ViewDelegate }

#pragma mark - Math

namespace math {
constexpr simd::float3 add(const simd::float3& a, const simd::float3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

constexpr simd_float4x4 makeIdentity()
{
    using simd::float4;
    return (simd_float4x4) { (float4) { 1.f, 0.f, 0.f, 0.f },
        (float4) { 0.f, 1.f, 0.f, 0.f },
        (float4) { 0.f, 0.f, 1.f, 0.f },
        (float4) { 0.f, 0.f, 0.f, 1.f } };
}

simd::float4x4 makePerspective(float fovRadians, float aspect, float znear, float zfar)
{
    using simd::float4;
    float ys = 1.f / tanf(fovRadians * 0.5f);
    float xs = ys / aspect;
    float zs = zfar / (znear - zfar);
    return simd_matrix_from_rows((float4) { xs, 0.0f, 0.0f, 0.0f },
        (float4) { 0.0f, ys, 0.0f, 0.0f },
        (float4) { 0.0f, 0.0f, zs, znear * zs },
        (float4) { 0, 0, -1, 0 });
}

simd::float4x4 makeXRotate(float angleRadians)
{
    using simd::float4;
    const float a = angleRadians;
    return simd_matrix_from_rows((float4) { 1.0f, 0.0f, 0.0f, 0.0f },
        (float4) { 0.0f, cosf(a), sinf(a), 0.0f },
        (float4) { 0.0f, -sinf(a), cosf(a), 0.0f },
        (float4) { 0.0f, 0.0f, 0.0f, 1.0f });
}

simd::float4x4 makeYRotate(float angleRadians)
{
    using simd::float4;
    const float a = angleRadians;
    return simd_matrix_from_rows((float4) { cosf(a), 0.0f, sinf(a), 0.0f },
        (float4) { 0.0f, 1.0f, 0.0f, 0.0f },
        (float4) { -sinf(a), 0.0f, cosf(a), 0.0f },
        (float4) { 0.0f, 0.0f, 0.0f, 1.0f });
}

simd::float4x4 makeZRotate(float angleRadians)
{
    using simd::float4;
    const float a = angleRadians;
    return simd_matrix_from_rows((float4) { cosf(a), sinf(a), 0.0f, 0.0f },
        (float4) { -sinf(a), cosf(a), 0.0f, 0.0f },
        (float4) { 0.0f, 0.0f, 1.0f, 0.0f },
        (float4) { 0.0f, 0.0f, 0.0f, 1.0f });
}

simd::float4x4 makeTranslate(const simd::float3& v)
{
    using simd::float4;
    const float4 col0 = { 1.0f, 0.0f, 0.0f, 0.0f };
    const float4 col1 = { 0.0f, 1.0f, 0.0f, 0.0f };
    const float4 col2 = { 0.0f, 0.0f, 1.0f, 0.0f };
    const float4 col3 = { v.x, v.y, v.z, 1.0f };
    return simd_matrix(col0, col1, col2, col3);
}

simd::float4x4 makeScale(const simd::float3& v)
{
    using simd::float4;
    return simd_matrix((float4) { v.x, 0, 0, 0 },
        (float4) { 0, v.y, 0, 0 },
        (float4) { 0, 0, v.z, 0 },
        (float4) { 0, 0, 0, 1.0 });
}

}

#pragma mark - Renderer
#pragma region Renderer {

const int Renderer::MAX_FRAMES_IN_FLIGHT = 3;

Renderer::Renderer(MTL::Device* device)
    : m_device(device->retain())
    , m_angle(0.f)
    , m_frame(0)
{
    m_commandQueue = m_device->newCommandQueue();
    buildShaders();
    buildDepthStencilStates();
    buildBuffers();

    m_semaphore = dispatch_semaphore_create(Renderer::MAX_FRAMES_IN_FLIGHT);
}

Renderer::~Renderer()
{
    m_shaderLibrary->release();
    m_depthStencilState->release();
    m_vertexDataBuffer->release();
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_instanceDataBuffer[i]->release();
    }
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_cameraDataBuffer[i]->release();
    }
    m_indexBuffer->release();
    m_renderPipelineState->release();
    m_commandQueue->release();
    m_device->release();
}

namespace shader_types {
struct InstanceData {
    simd::float4x4 instanceTransform;
    simd::float4 instanceColor;
};

struct CameraData {
    simd::float4x4 perspectiveTransform;
    simd::float4x4 worldTransform;
};
}

void Renderer::buildShaders()
{
    using NS::StringEncoding::UTF8StringEncoding;

    const char* shaderSrc = R"(
        #include <metal_stdlib>
        using namespace metal;

        struct v2f
        {
            float4 position [[position]];
            half3 color;
        };

        struct VertexData
        {
            float3 position;
        };

        struct InstanceData
        {
            float4x4 instanceTransform;
            float4 instanceColor;
        };

        struct CameraData
        {
            float4x4 perspectiveTransform;
            float4x4 worldTransform;
        };

        v2f vertex vertexMain( device const VertexData* vertexData [[buffer(0)]],
                               device const InstanceData* instanceData [[buffer(1)]],
                               device const CameraData& cameraData [[buffer(2)]],
                               uint vertexId [[vertex_id]],
                               uint instanceId [[instance_id]] )
        {
            v2f o;
            float4 pos = float4( vertexData[ vertexId ].position, 1.0 );
            pos = instanceData[ instanceId ].instanceTransform * pos;
            pos = cameraData.perspectiveTransform * cameraData.worldTransform * pos;
            o.position = pos;
            o.color = half3( instanceData[ instanceId ].instanceColor.rgb );
            return o;
        }

        half4 fragment fragmentMain( v2f in [[stage_in]] )
        {
            return half4( in.color, 1.0 );
        }
    )";

    NS::Error* error = nullptr;
    MTL::Library* pLibrary = m_device->newLibrary(NS::String::string(shaderSrc, UTF8StringEncoding), nullptr, &error);
    if (!pLibrary) {
        __builtin_printf("%s", error->localizedDescription()->utf8String());
        assert(false);
    }

    MTL::Function* pVertexFn = pLibrary->newFunction(NS::String::string("vertexMain", UTF8StringEncoding));
    MTL::Function* pFragFn = pLibrary->newFunction(NS::String::string("fragmentMain", UTF8StringEncoding));

    MTL::RenderPipelineDescriptor* renderPipelineDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    renderPipelineDescriptor->setVertexFunction(pVertexFn);
    renderPipelineDescriptor->setFragmentFunction(pFragFn);
    renderPipelineDescriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB);
    renderPipelineDescriptor->setDepthAttachmentPixelFormat(MTL::PixelFormat::PixelFormatDepth16Unorm);

    m_renderPipelineState = m_device->newRenderPipelineState(renderPipelineDescriptor, &error);
    if (!m_renderPipelineState) {
        __builtin_printf("%s", error->localizedDescription()->utf8String());
        assert(false);
    }

    pVertexFn->release();
    pFragFn->release();
    renderPipelineDescriptor->release();
    m_shaderLibrary = pLibrary;
}

void Renderer::buildDepthStencilStates()
{
    MTL::DepthStencilDescriptor* pDsDesc = MTL::DepthStencilDescriptor::alloc()->init();
    pDsDesc->setDepthCompareFunction(MTL::CompareFunction::CompareFunctionLess);
    pDsDesc->setDepthWriteEnabled(true);

    m_depthStencilState = m_device->newDepthStencilState(pDsDesc);

    pDsDesc->release();
}

void Renderer::buildBuffers()
{
    using simd::float3;
    const float s = 0.5f;

    float3 verts[] = {
        { -s, -s, +s },
        { +s, -s, +s },
        { +s, +s, +s },
        { -s, +s, +s },

        { -s, -s, -s },
        { -s, +s, -s },
        { +s, +s, -s },
        { +s, -s, -s }
    };

    uint16_t indices[] = {
        0, 1, 2, /* front */
        2, 3, 0,

        1, 7, 6, /* right */
        6, 2, 1,

        7, 4, 5, /* back */
        5, 6, 7,

        4, 0, 3, /* left */
        3, 5, 4,

        3, 2, 6, /* top */
        6, 5, 3,

        4, 7, 1, /* bottom */
        1, 0, 4
    };

    const size_t vertexDataSize = sizeof(verts);
    const size_t indexDataSize = sizeof(indices);

    MTL::Buffer* pVertexBuffer = m_device->newBuffer(vertexDataSize, MTL::ResourceStorageModeManaged);
    MTL::Buffer* pIndexBuffer = m_device->newBuffer(indexDataSize, MTL::ResourceStorageModeManaged);

    m_vertexDataBuffer = pVertexBuffer;
    m_indexBuffer = pIndexBuffer;

    memcpy(m_vertexDataBuffer->contents(), verts, vertexDataSize);
    memcpy(m_indexBuffer->contents(), indices, indexDataSize);

    m_vertexDataBuffer->didModifyRange(NS::Range::Make(0, m_vertexDataBuffer->length()));
    m_indexBuffer->didModifyRange(NS::Range::Make(0, m_indexBuffer->length()));

    const size_t instanceDataSize = MAX_FRAMES_IN_FLIGHT * NUM_INSTANCES * sizeof(shader_types::InstanceData);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_instanceDataBuffer[i] = m_device->newBuffer(instanceDataSize, MTL::ResourceStorageModeManaged);
    }

    const size_t cameraDataSize = MAX_FRAMES_IN_FLIGHT * sizeof(shader_types::CameraData);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_cameraDataBuffer[i] = m_device->newBuffer(cameraDataSize, MTL::ResourceStorageModeManaged);
    }
}

void Renderer::draw(MTK::View* view)
{
    using simd::float3;
    using simd::float4;
    using simd::float4x4;

    NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();

    m_frame = (m_frame + 1) % Renderer::MAX_FRAMES_IN_FLIGHT;
    MTL::Buffer* pInstanceDataBuffer = m_instanceDataBuffer[m_frame];

    MTL::CommandBuffer* commandBuffer = m_commandQueue->commandBuffer();
    dispatch_semaphore_wait(m_semaphore, DISPATCH_TIME_FOREVER);
    Renderer* pRenderer = this;
    commandBuffer->addCompletedHandler(^void(MTL::CommandBuffer* commandBuffer) {
        dispatch_semaphore_signal(pRenderer->m_semaphore);
    });

    m_angle += 0.01f;

    const float scl = 0.1f;
    shader_types::InstanceData* pInstanceData = reinterpret_cast<shader_types::InstanceData*>(pInstanceDataBuffer->contents());

    float3 objectPosition = { 0.f, 0.f, -5.f };

    // Update instance positions:

    float4x4 rt = math::makeTranslate(objectPosition);
    float4x4 rr = math::makeYRotate(-m_angle);
    float4x4 rtInv = math::makeTranslate({ -objectPosition.x, -objectPosition.y, -objectPosition.z });
    float4x4 fullObjectRot = rt * rr * rtInv;

    for (size_t i = 0; i < NUM_INSTANCES; ++i) {
        float iDivNumInstances = i / (float)NUM_INSTANCES;
        float xoff = (iDivNumInstances * 2.0f - 1.0f) + (1.f / NUM_INSTANCES);
        float yoff = sin((iDivNumInstances + m_angle) * 2.0f * M_PI);

        // Use the tiny math library to apply a 3D transformation to the instance.
        float4x4 scale = math::makeScale((float3) { scl, scl, scl });
        float4x4 zrot = math::makeZRotate(m_angle);
        float4x4 yrot = math::makeYRotate(m_angle);
        float4x4 translate = math::makeTranslate(math::add(objectPosition, { xoff, yoff, 0.f }));

        pInstanceData[i].instanceTransform = fullObjectRot * translate * yrot * zrot * scale;

        float r = iDivNumInstances;
        float g = 1.0f - r;
        float b = sinf(M_PI * 2.0f * iDivNumInstances);
        pInstanceData[i].instanceColor = (float4) { r, g, b, 1.0f };
    }
    pInstanceDataBuffer->didModifyRange(NS::Range::Make(0, pInstanceDataBuffer->length()));

    // Update camera state:

    MTL::Buffer* pCameraDataBuffer = m_cameraDataBuffer[m_frame];
    shader_types::CameraData* pCameraData = reinterpret_cast<shader_types::CameraData*>(pCameraDataBuffer->contents());
    pCameraData->perspectiveTransform = math::makePerspective(45.f * M_PI / 180.f, 1.f, 0.03f, 500.0f);
    pCameraData->worldTransform = math::makeIdentity();
    pCameraDataBuffer->didModifyRange(NS::Range::Make(0, sizeof(shader_types::CameraData)));

    // Begin render pass:

    MTL::RenderPassDescriptor* pRpd = view->currentRenderPassDescriptor();
    MTL::RenderCommandEncoder* renderCommandEncoder = commandBuffer->renderCommandEncoder(pRpd);

    renderCommandEncoder->setRenderPipelineState(m_renderPipelineState);
    renderCommandEncoder->setDepthStencilState(m_depthStencilState);

    renderCommandEncoder->setVertexBuffer(m_vertexDataBuffer, /* offset */ 0, /* index */ 0);
    renderCommandEncoder->setVertexBuffer(pInstanceDataBuffer, /* offset */ 0, /* index */ 1);
    renderCommandEncoder->setVertexBuffer(pCameraDataBuffer, /* offset */ 0, /* index */ 2);

    renderCommandEncoder->setCullMode(MTL::CullModeBack);
    renderCommandEncoder->setFrontFacingWinding(MTL::Winding::WindingCounterClockwise);

    renderCommandEncoder->drawIndexedPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle,
        6 * 6, MTL::IndexType::IndexTypeUInt16,
        m_indexBuffer,
        0,
        NUM_INSTANCES);

    renderCommandEncoder->endEncoding();
    commandBuffer->presentDrawable(view->currentDrawable());
    commandBuffer->commit();

    pPool->release();
}

#pragma endregion Renderer }
