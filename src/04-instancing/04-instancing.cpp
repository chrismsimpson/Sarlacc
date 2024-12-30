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

static constexpr size_t kNumInstances = 32;
static constexpr size_t MAX_FRAMES_IN_FLIGHT = 3;

#pragma region Declarations {

class Renderer {
public:
    Renderer(MTL::Device* device);
    ~Renderer();
    void buildShaders();
    void buildBuffers();
    void draw(MTK::View* view);

private:
    MTL::Device* m_device;
    MTL::CommandQueue* m_commandQueue;
    MTL::Library* m_shaderLibrary;
    MTL::RenderPipelineState* m_renderPipelineState;
    MTL::Buffer* m_vertexDataBuffer;
    MTL::Buffer* m_instanceDataBuffer[MAX_FRAMES_IN_FLIGHT];
    MTL::Buffer* m_indexBuffer;
    float m_angle;
    int m_frame;
    dispatch_semaphore_t _semaphore;
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
    SEL quitCb = NS::MenuItem::registerActionCallback("appQuit", [](void*, SEL, const NS::Object* sender) {
        auto app = NS::Application::sharedApplication();
        app->terminate(sender);
    });

    NS::MenuItem* appQuitItem = appMenu->addItem(quitItemName, quitCb, NS::String::string("q", UTF8StringEncoding));
    appQuitItem->setKeyEquivalentModifierMask(NS::EventModifierFlagCommand);
    appMenuItem->setSubmenu(appMenu);

    NS::MenuItem* windowMenuItem = NS::MenuItem::alloc()->init();
    NS::Menu* windowMenu = NS::Menu::alloc()->init(NS::String::string("Window", UTF8StringEncoding));

    SEL closeWindowCb = NS::MenuItem::registerActionCallback("windowClose", [](void*, SEL, const NS::Object*) {
        auto app = NS::Application::sharedApplication();
        app->windows()->object<NS::Window>(0)->close();
    });
    NS::MenuItem* closeWindowItem = windowMenu->addItem(NS::String::string("Close Window", UTF8StringEncoding), closeWindowCb, NS::String::string("w", UTF8StringEncoding));
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
    NS::Menu* pMenu = createMenuBar();
    NS::Application* app = reinterpret_cast<NS::Application*>(notification->object());
    app->setMainMenu(pMenu);
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
    m_mtkView->setClearColor(MTL::ClearColor::Make(1.0, 0.0, 0.0, 1.0));

    m_viewDelegate = new MyMTKViewDelegate(m_device);
    m_mtkView->setDelegate(m_viewDelegate);

    m_window->setContentView(m_mtkView);
    m_window->setTitle(NS::String::string("04 - Instancing", NS::StringEncoding::UTF8StringEncoding));

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
    buildBuffers();

    _semaphore = dispatch_semaphore_create(Renderer::MAX_FRAMES_IN_FLIGHT);
}

Renderer::~Renderer()
{
    m_shaderLibrary->release();
    m_vertexDataBuffer->release();
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_instanceDataBuffer[i]->release();
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

        v2f vertex vertexMain( device const VertexData* vertexData [[buffer(0)]],
                               device const InstanceData* instanceData [[buffer(1)]],
                               uint vertexId [[vertex_id]],
                               uint instanceId [[instance_id]] )
        {
            v2f o;
            float4 pos = float4( vertexData[ vertexId ].position, 1.0 );
            o.position = instanceData[ instanceId ].instanceTransform * pos;
            o.color = half3( instanceData[ instanceId ].instanceColor.rgb );
            return o;
        }

        half4 fragment fragmentMain( v2f in [[stage_in]] )
        {
            return half4( in.color, 1.0 );
        }
    )";

    NS::Error* pError = nullptr;
    MTL::Library* pLibrary = m_device->newLibrary(NS::String::string(shaderSrc, UTF8StringEncoding), nullptr, &pError);
    if (!pLibrary) {
        __builtin_printf("%s", pError->localizedDescription()->utf8String());
        assert(false);
    }

    MTL::Function* pVertexFn = pLibrary->newFunction(NS::String::string("vertexMain", UTF8StringEncoding));
    MTL::Function* pFragFn = pLibrary->newFunction(NS::String::string("fragmentMain", UTF8StringEncoding));

    MTL::RenderPipelineDescriptor* pDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    pDesc->setVertexFunction(pVertexFn);
    pDesc->setFragmentFunction(pFragFn);
    pDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB);

    m_renderPipelineState = m_device->newRenderPipelineState(pDesc, &pError);
    if (!m_renderPipelineState) {
        __builtin_printf("%s", pError->localizedDescription()->utf8String());
        assert(false);
    }

    pVertexFn->release();
    pFragFn->release();
    pDesc->release();
    m_shaderLibrary = pLibrary;
}

void Renderer::buildBuffers()
{
    using simd::float3;

    const float s = 0.5f;

    float3 verts[] = {
        { -s, -s, +s },
        { +s, -s, +s },
        { +s, +s, +s },
        { -s, +s, +s }
    };

    uint16_t indices[] = {
        0,
        1,
        2,
        2,
        3,
        0,
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

    const size_t instanceDataSize = MAX_FRAMES_IN_FLIGHT * kNumInstances * sizeof(shader_types::InstanceData);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_instanceDataBuffer[i] = m_device->newBuffer(instanceDataSize, MTL::ResourceStorageModeManaged);
    }
}

void Renderer::draw(MTK::View* view)
{
    using simd::float4;
    using simd::float4x4;

    NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();

    m_frame = (m_frame + 1) % Renderer::MAX_FRAMES_IN_FLIGHT;
    MTL::Buffer* pInstanceDataBuffer = m_instanceDataBuffer[m_frame];

    MTL::CommandBuffer* commandBuffer = m_commandQueue->commandBuffer();
    dispatch_semaphore_wait(_semaphore, DISPATCH_TIME_FOREVER);
    Renderer* pRenderer = this;
    commandBuffer->addCompletedHandler(^void(MTL::CommandBuffer* commandBuffer) {
        dispatch_semaphore_signal(pRenderer->_semaphore);
    });

    m_angle += 0.01f;

    const float scl = 0.1f;
    shader_types::InstanceData* pInstanceData = reinterpret_cast<shader_types::InstanceData*>(pInstanceDataBuffer->contents());
    for (size_t i = 0; i < kNumInstances; ++i) {
        float iDivNumInstances = i / (float)kNumInstances;
        float xoff = (iDivNumInstances * 2.0f - 1.0f) + (1.f / kNumInstances);
        float yoff = sin((iDivNumInstances + m_angle) * 2.0f * M_PI);
        pInstanceData[i].instanceTransform = (float4x4) { (float4) { scl * sinf(m_angle), scl * cosf(m_angle), 0.f, 0.f },
            (float4) { scl * cosf(m_angle), scl * -sinf(m_angle), 0.f, 0.f },
            (float4) { 0.f, 0.f, scl, 0.f },
            (float4) { xoff, yoff, 0.f, 1.f } };

        float r = iDivNumInstances;
        float g = 1.0f - r;
        float b = sinf(M_PI * 2.0f * iDivNumInstances);
        pInstanceData[i].instanceColor = (float4) { r, g, b, 1.0f };
    }
    pInstanceDataBuffer->didModifyRange(NS::Range::Make(0, pInstanceDataBuffer->length()));

    MTL::RenderPassDescriptor* pRpd = view->currentRenderPassDescriptor();
    MTL::RenderCommandEncoder* renderCommandEncoder = commandBuffer->renderCommandEncoder(pRpd);

    renderCommandEncoder->setRenderPipelineState(m_renderPipelineState);
    renderCommandEncoder->setVertexBuffer(m_vertexDataBuffer, /* offset */ 0, /* index */ 0);
    renderCommandEncoder->setVertexBuffer(pInstanceDataBuffer, /* offset */ 0, /* index */ 1);

    //
    // void drawIndexedPrimitives( PrimitiveType primitiveType, NS::UInteger indexCount, IndexType indexType,
    //                             const class Buffer* pIndexBuffer, NS::UInteger indexBufferOffset, NS::UInteger instanceCount );
    renderCommandEncoder->drawIndexedPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle,
        6, MTL::IndexType::IndexTypeUInt16,
        m_indexBuffer,
        0,
        kNumInstances);

    renderCommandEncoder->endEncoding();
    commandBuffer->presentDrawable(view->currentDrawable());
    commandBuffer->commit();

    pPool->release();
}

#pragma endregion Renderer }
