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

static constexpr size_t kInstanceRows = 10;
static constexpr size_t kInstanceColumns = 10;
static constexpr size_t kInstanceDepth = 10;
static constexpr size_t kNumInstances = (kInstanceRows * kInstanceColumns * kInstanceDepth);
static constexpr size_t MAX_FRAMES_IN_FLIGHT = 3;
static constexpr uint32_t kTextureWidth = 128;
static constexpr uint32_t kTextureHeight = 128;

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
simd::float3x3 discardTranslation(const simd::float4x4& m);
}

class Renderer {
public:
    Renderer(MTL::Device* device);
    ~Renderer();
    void buildShaders();
    void buildComputePipeline();
    void buildDepthStencilStates();
    void buildTextures();
    void buildBuffers();
    void generateMandelbrotTexture(MTL::CommandBuffer* pCommandBuffer);
    void draw(MTK::View* view);

private:
    MTL::Device* m_device;
    MTL::CommandQueue* m_commandQueue;
    MTL::Library* m_shaderLibrary;
    MTL::RenderPipelineState* m_renderPipelineState;
    MTL::ComputePipelineState* m_computePipelineState;
    MTL::DepthStencilState* m_depthStencilState;
    MTL::Texture* m_texture;
    MTL::Buffer* m_vertexDataBuffer;
    MTL::Buffer* m_instanceDataBuffer[MAX_FRAMES_IN_FLIGHT];
    MTL::Buffer* m_cameraDataBuffer[MAX_FRAMES_IN_FLIGHT];
    MTL::Buffer* m_indexBuffer;
    MTL::Buffer* m_textureAnimationBuffer;
    float m_angle;
    int m_frame;
    dispatch_semaphore_t m_semaphore;
    static const int MAX_FRAMES_IN_FLIGHT;
    uint _animationIndex;
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
    CGRect frame = (CGRect) { { 100.0, 100.0 }, { 1024.0, 1024.0 } };

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
    m_window->setTitle(NS::String::string("09 - Compute to Render", NS::StringEncoding::UTF8StringEncoding));

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

simd::float3x3 discardTranslation(const simd::float4x4& m)
{
    return simd_matrix(m.columns[0].xyz, m.columns[1].xyz, m.columns[2].xyz);
}

}

#pragma mark - Renderer
#pragma region Renderer {

const int Renderer::MAX_FRAMES_IN_FLIGHT = 3;

Renderer::Renderer(MTL::Device* device)
    : m_device(device->retain())
    , m_angle(0.f)
    , m_frame(0)
    , _animationIndex(0)
{
    m_commandQueue = m_device->newCommandQueue();
    buildShaders();
    buildComputePipeline();
    buildDepthStencilStates();
    buildTextures();
    buildBuffers();

    m_semaphore = dispatch_semaphore_create(Renderer::MAX_FRAMES_IN_FLIGHT);
}

Renderer::~Renderer()
{
    m_textureAnimationBuffer->release();
    m_texture->release();
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
    m_computePipelineState->release();
    m_renderPipelineState->release();
    m_commandQueue->release();
    m_device->release();
}

namespace shader_types {
struct VertexData {
    simd::float3 position;
    simd::float3 normal;
    simd::float2 texcoord;
};

struct InstanceData {
    simd::float4x4 instanceTransform;
    simd::float3x3 instanceNormalTransform;
    simd::float4 instanceColor;
};

struct CameraData {
    simd::float4x4 perspectiveTransform;
    simd::float4x4 worldTransform;
    simd::float3x3 worldNormalTransform;
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
            float3 normal;
            half3 color;
            float2 texcoord;
        };

        struct VertexData
        {
            float3 position;
            float3 normal;
            float2 texcoord;
        };

        struct InstanceData
        {
            float4x4 instanceTransform;
            float3x3 instanceNormalTransform;
            float4 instanceColor;
        };

        struct CameraData
        {
            float4x4 perspectiveTransform;
            float4x4 worldTransform;
            float3x3 worldNormalTransform;
        };

        v2f vertex vertexMain( device const VertexData* vertexData [[buffer(0)]],
                               device const InstanceData* instanceData [[buffer(1)]],
                               device const CameraData& cameraData [[buffer(2)]],
                               uint vertexId [[vertex_id]],
                               uint instanceId [[instance_id]] )
        {
            v2f o;

            const device VertexData& vd = vertexData[ vertexId ];
            float4 pos = float4( vd.position, 1.0 );
            pos = instanceData[ instanceId ].instanceTransform * pos;
            pos = cameraData.perspectiveTransform * cameraData.worldTransform * pos;
            o.position = pos;

            float3 normal = instanceData[ instanceId ].instanceNormalTransform * vd.normal;
            normal = cameraData.worldNormalTransform * normal;
            o.normal = normal;

            o.texcoord = vd.texcoord.xy;

            o.color = half3( instanceData[ instanceId ].instanceColor.rgb );
            return o;
        }

        half4 fragment fragmentMain( v2f in [[stage_in]], texture2d< half, access::sample > tex [[texture(0)]] )
        {
            constexpr sampler s( address::repeat, filter::linear );
            half3 texel = tex.sample( s, in.texcoord ).rgb;

            // assume light coming from (front-top-right)
            float3 l = normalize(float3( 1.0, 1.0, 0.8 ));
            float3 n = normalize( in.normal );

            half ndotl = half( saturate( dot( n, l ) ) );

            half3 illum = (in.color * texel * 0.1) + (in.color * texel * ndotl);
            return half4( illum, 1.0 );
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
    pDesc->setDepthAttachmentPixelFormat(MTL::PixelFormat::PixelFormatDepth16Unorm);

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

void Renderer::buildComputePipeline()
{
    const char* kernelSrc = R"(
        #include <metal_stdlib>
        using namespace metal;

        kernel void mandelbrot_set(texture2d< half, access::write > tex [[texture(0)]],
                                   uint2 index [[thread_position_in_grid]],
                                   uint2 gridSize [[threads_per_grid]],
                                   device const uint* frame [[buffer(0)]])
        {
            constexpr float kAnimationFrequency = 0.01;
            constexpr float kAnimationSpeed = 4;
            constexpr float kAnimationScaleLow = 0.62;
            constexpr float kAnimationScale = 0.38;

            constexpr float2 kMandelbrotPixelOffset = {-0.2, -0.35};
            constexpr float2 kMandelbrotOrigin = {-1.2, -0.32};
            constexpr float2 kMandelbrotScale = {2.2, 2.0};

            // Map time to zoom value in [kAnimationScaleLow, 1]
            float zoom = kAnimationScaleLow + kAnimationScale * cos(kAnimationFrequency * *frame);
            // Speed up zooming
            zoom = pow(zoom, kAnimationSpeed);

            //Scale
            float x0 = zoom * kMandelbrotScale.x * ((float)index.x / gridSize.x + kMandelbrotPixelOffset.x) + kMandelbrotOrigin.x;
            float y0 = zoom * kMandelbrotScale.y * ((float)index.y / gridSize.y + kMandelbrotPixelOffset.y) + kMandelbrotOrigin.y;

            // Implement Mandelbrot set
            float x = 0.0;
            float y = 0.0;
            uint iteration = 0;
            uint max_iteration = 1000;
            float xtmp = 0.0;
            while(x * x + y * y <= 4 && iteration < max_iteration)
            {
                xtmp = x * x - y * y + x0;
                y = 2 * x * y + y0;
                x = xtmp;
                iteration += 1;
            }

            // Convert iteration result to colors
            half color = (0.5 + 0.5 * cos(3.0 + iteration * 0.15));
            tex.write(half4(color, color, color, 1.0), index, 0);
        })";
    NS::Error* pError = nullptr;

    MTL::Library* pComputeLibrary = m_device->newLibrary(NS::String::string(kernelSrc, NS::UTF8StringEncoding), nullptr, &pError);
    if (!pComputeLibrary) {
        __builtin_printf("%s", pError->localizedDescription()->utf8String());
        assert(false);
    }

    MTL::Function* pMandelbrotFn = pComputeLibrary->newFunction(NS::String::string("mandelbrot_set", NS::UTF8StringEncoding));
    m_computePipelineState = m_device->newComputePipelineState(pMandelbrotFn, &pError);
    if (!m_computePipelineState) {
        __builtin_printf("%s", pError->localizedDescription()->utf8String());
        assert(false);
    }

    pMandelbrotFn->release();
    pComputeLibrary->release();
}

void Renderer::buildDepthStencilStates()
{
    MTL::DepthStencilDescriptor* pDsDesc = MTL::DepthStencilDescriptor::alloc()->init();
    pDsDesc->setDepthCompareFunction(MTL::CompareFunction::CompareFunctionLess);
    pDsDesc->setDepthWriteEnabled(true);

    m_depthStencilState = m_device->newDepthStencilState(pDsDesc);

    pDsDesc->release();
}

void Renderer::buildTextures()
{
    MTL::TextureDescriptor* pTextureDesc = MTL::TextureDescriptor::alloc()->init();
    pTextureDesc->setWidth(kTextureWidth);
    pTextureDesc->setHeight(kTextureHeight);
    pTextureDesc->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
    pTextureDesc->setTextureType(MTL::TextureType2D);
    pTextureDesc->setStorageMode(MTL::StorageModeManaged);
    pTextureDesc->setUsage(MTL::ResourceUsageSample | MTL::ResourceUsageRead | MTL::ResourceUsageWrite);

    MTL::Texture* pTexture = m_device->newTexture(pTextureDesc);
    m_texture = pTexture;

    pTextureDesc->release();
}

void Renderer::buildBuffers()
{
    using simd::float2;
    using simd::float3;

    const float s = 0.5f;

    shader_types::VertexData verts[] = {
        //                                         Texture
        //   Positions           Normals         Coordinates
        { { -s, -s, +s }, { 0.f, 0.f, 1.f }, { 0.f, 1.f } },
        { { +s, -s, +s }, { 0.f, 0.f, 1.f }, { 1.f, 1.f } },
        { { +s, +s, +s }, { 0.f, 0.f, 1.f }, { 1.f, 0.f } },
        { { -s, +s, +s }, { 0.f, 0.f, 1.f }, { 0.f, 0.f } },

        { { +s, -s, +s }, { 1.f, 0.f, 0.f }, { 0.f, 1.f } },
        { { +s, -s, -s }, { 1.f, 0.f, 0.f }, { 1.f, 1.f } },
        { { +s, +s, -s }, { 1.f, 0.f, 0.f }, { 1.f, 0.f } },
        { { +s, +s, +s }, { 1.f, 0.f, 0.f }, { 0.f, 0.f } },

        { { +s, -s, -s }, { 0.f, 0.f, -1.f }, { 0.f, 1.f } },
        { { -s, -s, -s }, { 0.f, 0.f, -1.f }, { 1.f, 1.f } },
        { { -s, +s, -s }, { 0.f, 0.f, -1.f }, { 1.f, 0.f } },
        { { +s, +s, -s }, { 0.f, 0.f, -1.f }, { 0.f, 0.f } },

        { { -s, -s, -s }, { -1.f, 0.f, 0.f }, { 0.f, 1.f } },
        { { -s, -s, +s }, { -1.f, 0.f, 0.f }, { 1.f, 1.f } },
        { { -s, +s, +s }, { -1.f, 0.f, 0.f }, { 1.f, 0.f } },
        { { -s, +s, -s }, { -1.f, 0.f, 0.f }, { 0.f, 0.f } },

        { { -s, +s, +s }, { 0.f, 1.f, 0.f }, { 0.f, 1.f } },
        { { +s, +s, +s }, { 0.f, 1.f, 0.f }, { 1.f, 1.f } },
        { { +s, +s, -s }, { 0.f, 1.f, 0.f }, { 1.f, 0.f } },
        { { -s, +s, -s }, { 0.f, 1.f, 0.f }, { 0.f, 0.f } },

        { { -s, -s, -s }, { 0.f, -1.f, 0.f }, { 0.f, 1.f } },
        { { +s, -s, -s }, { 0.f, -1.f, 0.f }, { 1.f, 1.f } },
        { { +s, -s, +s }, { 0.f, -1.f, 0.f }, { 1.f, 0.f } },
        { { -s, -s, +s }, { 0.f, -1.f, 0.f }, { 0.f, 0.f } }
    };

    uint16_t indices[] = {
        0, 1, 2, 2, 3, 0, /* front */
        4, 5, 6, 6, 7, 4, /* right */
        8, 9, 10, 10, 11, 8, /* back */
        12, 13, 14, 14, 15, 12, /* left */
        16, 17, 18, 18, 19, 16, /* top */
        20, 21, 22, 22, 23, 20, /* bottom */
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

    const size_t cameraDataSize = MAX_FRAMES_IN_FLIGHT * sizeof(shader_types::CameraData);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_cameraDataBuffer[i] = m_device->newBuffer(cameraDataSize, MTL::ResourceStorageModeManaged);
    }

    m_textureAnimationBuffer = m_device->newBuffer(sizeof(uint), MTL::ResourceStorageModeManaged);
}

void Renderer::generateMandelbrotTexture(MTL::CommandBuffer* pCommandBuffer)
{
    assert(pCommandBuffer);

    uint* ptr = reinterpret_cast<uint*>(m_textureAnimationBuffer->contents());
    *ptr = (_animationIndex++) % 5000;
    m_textureAnimationBuffer->didModifyRange(NS::Range::Make(0, sizeof(uint)));

    MTL::ComputeCommandEncoder* pComputeEncoder = pCommandBuffer->computeCommandEncoder();

    pComputeEncoder->setComputePipelineState(m_computePipelineState);
    pComputeEncoder->setTexture(m_texture, 0);
    pComputeEncoder->setBuffer(m_textureAnimationBuffer, 0, 0);

    MTL::Size gridSize = MTL::Size(kTextureWidth, kTextureHeight, 1);

    NS::UInteger threadGroupSize = m_computePipelineState->maxTotalThreadsPerThreadgroup();
    MTL::Size threadgroupSize(threadGroupSize, 1, 1);

    pComputeEncoder->dispatchThreads(gridSize, threadgroupSize);

    pComputeEncoder->endEncoding();
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

    m_angle += 0.002f;

    const float scl = 0.2f;
    shader_types::InstanceData* pInstanceData = reinterpret_cast<shader_types::InstanceData*>(pInstanceDataBuffer->contents());

    float3 objectPosition = { 0.f, 0.f, -10.f };

    float4x4 rt = math::makeTranslate(objectPosition);
    float4x4 rr1 = math::makeYRotate(-m_angle);
    float4x4 rr0 = math::makeXRotate(m_angle * 0.5);
    float4x4 rtInv = math::makeTranslate({ -objectPosition.x, -objectPosition.y, -objectPosition.z });
    float4x4 fullObjectRot = rt * rr1 * rr0 * rtInv;

    size_t ix = 0;
    size_t iy = 0;
    size_t iz = 0;
    for (size_t i = 0; i < kNumInstances; ++i) {
        if (ix == kInstanceRows) {
            ix = 0;
            iy += 1;
        }
        if (iy == kInstanceRows) {
            iy = 0;
            iz += 1;
        }

        float4x4 scale = math::makeScale((float3) { scl, scl, scl });
        float4x4 zrot = math::makeZRotate(m_angle * sinf((float)ix));
        float4x4 yrot = math::makeYRotate(m_angle * cosf((float)iy));

        float x = ((float)ix - (float)kInstanceRows / 2.f) * (2.f * scl) + scl;
        float y = ((float)iy - (float)kInstanceColumns / 2.f) * (2.f * scl) + scl;
        float z = ((float)iz - (float)kInstanceDepth / 2.f) * (2.f * scl);
        float4x4 translate = math::makeTranslate(math::add(objectPosition, { x, y, z }));

        pInstanceData[i].instanceTransform = fullObjectRot * translate * yrot * zrot * scale;
        pInstanceData[i].instanceNormalTransform = math::discardTranslation(pInstanceData[i].instanceTransform);

        float iDivNumInstances = i / (float)kNumInstances;
        float r = iDivNumInstances;
        float g = 1.0f - r;
        float b = sinf(M_PI * 2.0f * iDivNumInstances);
        pInstanceData[i].instanceColor = (float4) { r, g, b, 1.0f };

        ix += 1;
    }
    pInstanceDataBuffer->didModifyRange(NS::Range::Make(0, pInstanceDataBuffer->length()));

    // Update camera state:

    MTL::Buffer* pCameraDataBuffer = m_cameraDataBuffer[m_frame];
    shader_types::CameraData* pCameraData = reinterpret_cast<shader_types::CameraData*>(pCameraDataBuffer->contents());
    pCameraData->perspectiveTransform = math::makePerspective(45.f * M_PI / 180.f, 1.f, 0.03f, 500.0f);
    pCameraData->worldTransform = math::makeIdentity();
    pCameraData->worldNormalTransform = math::discardTranslation(pCameraData->worldTransform);
    pCameraDataBuffer->didModifyRange(NS::Range::Make(0, sizeof(shader_types::CameraData)));

    // Update texture:

    generateMandelbrotTexture(commandBuffer);

    // Begin render pass:

    MTL::RenderPassDescriptor* pRpd = view->currentRenderPassDescriptor();
    MTL::RenderCommandEncoder* renderCommandEncoder = commandBuffer->renderCommandEncoder(pRpd);

    renderCommandEncoder->setRenderPipelineState(m_renderPipelineState);
    renderCommandEncoder->setDepthStencilState(m_depthStencilState);

    renderCommandEncoder->setVertexBuffer(m_vertexDataBuffer, /* offset */ 0, /* index */ 0);
    renderCommandEncoder->setVertexBuffer(pInstanceDataBuffer, /* offset */ 0, /* index */ 1);
    renderCommandEncoder->setVertexBuffer(pCameraDataBuffer, /* offset */ 0, /* index */ 2);

    renderCommandEncoder->setFragmentTexture(m_texture, /* index */ 0);

    renderCommandEncoder->setCullMode(MTL::CullModeBack);
    renderCommandEncoder->setFrontFacingWinding(MTL::Winding::WindingCounterClockwise);

    renderCommandEncoder->drawIndexedPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle,
        6 * 6, MTL::IndexType::IndexTypeUInt16,
        m_indexBuffer,
        0,
        kNumInstances);

    renderCommandEncoder->endEncoding();
    commandBuffer->presentDrawable(view->currentDrawable());
    commandBuffer->commit();

    pPool->release();
}

#pragma endregion Renderer }
