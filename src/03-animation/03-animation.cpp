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

#pragma region Declarations {

class Renderer {
public:
    Renderer(MTL::Device* device);
    ~Renderer();
    void buildShaders();
    void buildBuffers();
    void buildFrameData();
    void draw(MTK::View* view);

private:
    MTL::Device* m_device;
    MTL::CommandQueue* m_commandQueue;
    MTL::Library* m_shaderLibrary;
    MTL::RenderPipelineState* m_renderPipelineState;
    MTL::Buffer* _pArgBuffer;
    MTL::Buffer* m_vertexPositionsBuffer;
    MTL::Buffer* m_vertexColorsBuffer;
    MTL::Buffer* _pFrameData[3];
    float _angle;
    int _frame;
    dispatch_semaphore_t _semaphore;
    static const int kMaxFramesInFlight;
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
    m_window->setTitle(NS::String::string("03 - Animation", NS::StringEncoding::UTF8StringEncoding));

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

const int Renderer::kMaxFramesInFlight = 3;

Renderer::Renderer(MTL::Device* device)
    : m_device(device->retain())
    , _angle(0.f)
    , _frame(0)
{
    m_commandQueue = m_device->newCommandQueue();
    buildShaders();
    buildBuffers();
    buildFrameData();

    _semaphore = dispatch_semaphore_create(Renderer::kMaxFramesInFlight);
}

Renderer::~Renderer()
{
    m_shaderLibrary->release();
    _pArgBuffer->release();
    m_vertexPositionsBuffer->release();
    m_vertexColorsBuffer->release();
    for (int i = 0; i < Renderer::kMaxFramesInFlight; ++i) {
        _pFrameData[i]->release();
    }
    m_renderPipelineState->release();
    m_commandQueue->release();
    m_device->release();
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
            device float3* positions [[id(0)]];
            device float3* colors [[id(1)]];
        };

        struct FrameData
        {
            float angle;
        };

        v2f vertex vertexMain( device const VertexData* vertexData [[buffer(0)]], constant FrameData* frameData [[buffer(1)]], uint vertexId [[vertex_id]] )
        {
            float a = frameData->angle;
            float3x3 rotationMatrix = float3x3( sin(a), cos(a), 0.0, cos(a), -sin(a), 0.0, 0.0, 0.0, 1.0 );
            v2f o;
            o.position = float4( rotationMatrix * vertexData->positions[ vertexId ], 1.0 );
            o.color = half3(vertexData->colors[ vertexId ]);
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
    const size_t NumVertices = 3;

    simd::float3 positions[NumVertices] = {
        { -0.8f, 0.8f, 0.0f },
        { 0.0f, -0.8f, 0.0f },
        { +0.8f, 0.8f, 0.0f }
    };

    simd::float3 colors[NumVertices] = {
        { 1.0, 0.3f, 0.2f },
        { 0.8f, 1.0, 0.0f },
        { 0.8f, 0.0f, 1.0 }
    };

    const size_t positionsDataSize = NumVertices * sizeof(simd::float3);
    const size_t colorDataSize = NumVertices * sizeof(simd::float3);

    MTL::Buffer* pVertexPositionsBuffer = m_device->newBuffer(positionsDataSize, MTL::ResourceStorageModeManaged);
    MTL::Buffer* pVertexColorsBuffer = m_device->newBuffer(colorDataSize, MTL::ResourceStorageModeManaged);

    m_vertexPositionsBuffer = pVertexPositionsBuffer;
    m_vertexColorsBuffer = pVertexColorsBuffer;

    memcpy(m_vertexPositionsBuffer->contents(), positions, positionsDataSize);
    memcpy(m_vertexColorsBuffer->contents(), colors, colorDataSize);

    m_vertexPositionsBuffer->didModifyRange(NS::Range::Make(0, m_vertexPositionsBuffer->length()));
    m_vertexColorsBuffer->didModifyRange(NS::Range::Make(0, m_vertexColorsBuffer->length()));

    using NS::StringEncoding::UTF8StringEncoding;
    assert(m_shaderLibrary);

    MTL::Function* pVertexFn = m_shaderLibrary->newFunction(NS::String::string("vertexMain", UTF8StringEncoding));
    MTL::ArgumentEncoder* pArgEncoder = pVertexFn->newArgumentEncoder(0);

    MTL::Buffer* pArgBuffer = m_device->newBuffer(pArgEncoder->encodedLength(), MTL::ResourceStorageModeManaged);
    _pArgBuffer = pArgBuffer;

    pArgEncoder->setArgumentBuffer(_pArgBuffer, 0);

    pArgEncoder->setBuffer(m_vertexPositionsBuffer, 0, 0);
    pArgEncoder->setBuffer(m_vertexColorsBuffer, 0, 1);

    _pArgBuffer->didModifyRange(NS::Range::Make(0, _pArgBuffer->length()));

    pVertexFn->release();
    pArgEncoder->release();
}

struct FrameData {
    float angle;
};

void Renderer::buildFrameData()
{
    for (int i = 0; i < Renderer::kMaxFramesInFlight; ++i) {
        _pFrameData[i] = m_device->newBuffer(sizeof(FrameData), MTL::ResourceStorageModeManaged);
    }
}

void Renderer::draw(MTK::View* view)
{
    NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();

    _frame = (_frame + 1) % Renderer::kMaxFramesInFlight;
    MTL::Buffer* frameDataBuffer = _pFrameData[_frame];

    MTL::CommandBuffer* commandBuffer = m_commandQueue->commandBuffer();
    dispatch_semaphore_wait(_semaphore, DISPATCH_TIME_FOREVER);
    Renderer* pRenderer = this;
    commandBuffer->addCompletedHandler(^void(MTL::CommandBuffer* commandBuffer) {
        dispatch_semaphore_signal(pRenderer->_semaphore);
    });

    reinterpret_cast<FrameData*>(frameDataBuffer->contents())->angle = (_angle += 0.01f);
    frameDataBuffer->didModifyRange(NS::Range::Make(0, sizeof(FrameData)));

    MTL::RenderPassDescriptor* pRpd = view->currentRenderPassDescriptor();
    MTL::RenderCommandEncoder* renderCommandEncoder = commandBuffer->renderCommandEncoder(pRpd);

    renderCommandEncoder->setRenderPipelineState(m_renderPipelineState);
    renderCommandEncoder->setVertexBuffer(_pArgBuffer, 0, 0);
    renderCommandEncoder->useResource(m_vertexPositionsBuffer, MTL::ResourceUsageRead);
    renderCommandEncoder->useResource(m_vertexColorsBuffer, MTL::ResourceUsageRead);

    renderCommandEncoder->setVertexBuffer(frameDataBuffer, 0, 1);
    renderCommandEncoder->drawPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));

    renderCommandEncoder->endEncoding();
    commandBuffer->presentDrawable(view->currentDrawable());
    commandBuffer->commit();

    pPool->release();
}

#pragma endregion Renderer }