
#include "Rendering.h"

#include <iostream>
#include <chrono>
#include <algorithm>

#include <glm/gtc/random.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <glbinding/gl/gl.h>

#include "common.h"

#include "ArcVertexCloud.h"
//#include "ArcTriangles.h"
//#include "ArcTriangleStrip.h"
//#include "ArcInstancing.h"


using namespace gl;


namespace
{


static const size_t arcCount = 50000;
static const size_t arcTessellationCount = 128;
static const size_t fpsSampleCount = 100;


} // namespace


Rendering::Rendering()
: m_current(nullptr)
, m_query(0)
, m_measure(false)
, m_rasterizerDiscard(false)
, m_fpsSamples(fpsSampleCount+1)
{
    m_implementations[0] = new ArcVertexCloud;//new CuboidTriangles;
    m_implementations[1] = new ArcVertexCloud;//new CuboidTriangleStrip;
    m_implementations[2] = new ArcVertexCloud;//new CuboidInstancing;
    m_implementations[3] = new ArcVertexCloud;

    setTechnique(0);
}

Rendering::~Rendering()
{
    // Flag all aquired resources for deletion (hint: driver decides when to actually delete them; see: shared contexts)
    glDeleteQueries(1, &m_query);
}

void Rendering::initialize()
{
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClearDepth(1.0f);
    glEnable(GL_DEPTH_TEST);

    createGeometry();

    glGenQueries(1, &m_query);

    m_start = std::chrono::high_resolution_clock::now();
}

void Rendering::reloadShaders()
{
    for (auto implementation : m_implementations)
    {
        if (implementation->initialized())
        {
            implementation->loadShader();
        }
    }
}

void Rendering::createGeometry()
{
    for (auto implementation : m_implementations)
    {
        implementation->resize(arcCount);
    }

#pragma omp parallel for
    for (size_t i = 0; i < arcCount; ++i)
    {
        Arc a;
        a.center = glm::vec2(glm::linearRand(-7.0f, 7.0f), glm::linearRand(-7.0f, 7.0f));

        a.heightRange = glm::vec2(glm::linearRand(-4.0f, 4.0f), glm::linearRand(0.1f, 0.3f));
        a.heightRange.y += a.heightRange.x;

        a.angleRange = glm::vec2(glm::linearRand(0.0f * glm::pi<float>(), 2.0f * glm::pi<float>()), glm::linearRand(0.1f * glm::pi<float>(), 0.9f * glm::pi<float>()));
        a.angleRange.y += a.angleRange.x;

        a.radiusRange = glm::vec2(glm::linearRand(0.3f, 1.0f), glm::linearRand(0.2f, 0.8f));
        a.radiusRange.y += a.radiusRange.x;

        a.colorValue = glm::linearRand(0.0f, 1.0f);
        a.gradientIndex = 0;

        a.tessellationCount = glm::round((a.angleRange.y - a.angleRange.x) * a.radiusRange.y * glm::linearRand(4.0f, 64.0f) / (2.0f * glm::pi<float>()));

        for (auto implementation : m_implementations)
        {
            implementation->setArc(i, a);
        }
    }
}

void Rendering::updateUniforms()
{
    static const auto eye = glm::vec3(1.0f, 12.0f, 1.0f);
    static const auto center = glm::vec3(0.0f, 0.0f, 0.0f);
    static const auto up = glm::vec3(0.0f, 1.0f, 0.0f);

    const auto f = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - m_start).count()) / 1000.0f;

    auto eyeRotation = glm::mat4(1.0f);
    eyeRotation = glm::rotate(eyeRotation, glm::sin(0.8342378f * f), glm::vec3(0.0f, 1.0f, 0.0f));
    eyeRotation = glm::rotate(eyeRotation, glm::cos(-0.5423543f * f), glm::vec3(1.0f, 0.0f, 0.0f));
    eyeRotation = glm::rotate(eyeRotation, glm::sin(0.13234823f * f), glm::vec3(0.0f, 0.0f, 1.0f));

    const auto rotatedEye = eyeRotation * glm::vec4(eye, 1.0f);
    //const auto rotatedEye = glm::vec3(12.0f, 0.0f, 0.0f);

    const auto view = glm::lookAt(glm::vec3(rotatedEye), center, up);
    const auto viewProjection = glm::perspectiveFov(glm::radians(45.0f), float(m_width), float(m_height), 1.0f, 30.0f) * view;

    for (auto implementation : m_implementations)
    {
        if (implementation->initialized())
        {
            for (GLuint program : implementation->programs())
            {
                const auto viewProjectionLocation = glGetUniformLocation(program, "viewProjection");
                glUseProgram(program);
                glUniformMatrix4fv(viewProjectionLocation, 1, GL_FALSE, glm::value_ptr(viewProjection));
            }
        }
    }

    glUseProgram(0);
}

void Rendering::resize(int w, int h)
{
    m_width = w;
    m_height = h;
}

void Rendering::setTechnique(int i)
{
    m_current = m_implementations.at(i);

    switch (i)
    {
    case 0:
        std::cout << "Switch to Triangles implementation" << std::endl;
        break;
    case 1:
        std::cout << "Switch to TriangleStrip implementation" << std::endl;
        break;
    case 2:
        std::cout << "Switch to Instancing implementation" << std::endl;
        break;
    case 3:
        std::cout << "Switch to AttributedVertexCloud implementation" << std::endl;
        break;
    }
}

void Rendering::render()
{
    if (m_fpsSamples == fpsSampleCount)
    {
        const auto end = std::chrono::high_resolution_clock::now();

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - m_fpsMeasurementStart).count() / 1000.0f / fpsSampleCount;

        std::cout << "Measured " << (1.0f / elapsed) << "FPS (" << "(~ " << (elapsed * 1000.0f) << "ms per frame)" << std::endl;

        m_fpsSamples = fpsSampleCount + 1;
    }

    if (m_fpsSamples < fpsSampleCount)
    {
        ++m_fpsSamples;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glViewport(0, 0, m_width, m_height);

    m_current->initialize();

    updateUniforms();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (m_rasterizerDiscard)
    {
        glEnable(GL_RASTERIZER_DISCARD);
    }

    measureGPU("rendering", [this]() {
        m_current->render();
    }, m_measure);

    if (m_rasterizerDiscard)
    {
        glDisable(GL_RASTERIZER_DISCARD);
    }
}

void Rendering::spaceMeasurement()
{
    const auto reference = std::accumulate(m_implementations.begin(), m_implementations.end(),
            std::accumulate(m_implementations.begin(), m_implementations.end(), 0, [](size_t currentSize, const ArcImplementation * technique) {
                return std::max(currentSize, technique->fullByteSize());
            }), [](size_t currentSize, const ArcImplementation * technique) {
        return std::min(currentSize, technique->fullByteSize());
    });

    const auto printSpaceMeasurement = [&reference](const std::string & techniqueName, size_t byteSize)
    {
        std::cout << techniqueName << std::endl << (byteSize / 1024) << "kB (" << (static_cast<float>(byteSize) / reference) << "x)" << std::endl;
    };

    std::cout << "Arc count: " << arcCount << std::endl;
    std::cout << std::endl;

    for (const auto implementation : m_implementations)
    {
        printSpaceMeasurement(implementation->name(), implementation->fullByteSize());
    }
}

void Rendering::measureCPU(const std::string & name, std::function<void()> callback, bool on) const
{
    if (!on)
    {
        return callback();
    }

    const auto start = std::chrono::high_resolution_clock::now();

    callback();

    const auto end = std::chrono::high_resolution_clock::now();

    std::cout << name << ": " << std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() << "ns" << std::endl;
}

void Rendering::measureGPU(const std::string & name, std::function<void()> callback, bool on) const
{
    if (!on)
    {
        return callback();
    }

    glBeginQuery(gl::GL_TIME_ELAPSED, m_query);

    callback();

    glEndQuery(gl::GL_TIME_ELAPSED);

    int available = 0;
    while (!available)
    {
        glGetQueryObjectiv(m_query, gl::GL_QUERY_RESULT_AVAILABLE, &available);
    }

    int value;
    glGetQueryObjectiv(m_query, gl::GL_QUERY_RESULT, &value);

    std::cout << name << ": " << value << "ns" << std::endl;
}

void Rendering::togglePerformanceMeasurements()
{
    m_measure = !m_measure;
}

void Rendering::toggleRasterizerDiscard()
{
    m_rasterizerDiscard = !m_rasterizerDiscard;
}

void Rendering::startFPSMeasuring()
{
    m_fpsSamples = 0;
    m_fpsMeasurementStart = std::chrono::high_resolution_clock::now();
}
