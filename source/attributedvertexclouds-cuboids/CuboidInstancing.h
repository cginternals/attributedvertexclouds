
#pragma once

#include <vector>

#include <glm/vec3.hpp>

#include <glbinding/gl/types.h>

#include "Cuboid.h"
#include "CuboidImplementation.h"


class CuboidInstancing : public CuboidImplementation
{
public:
    CuboidInstancing();
    ~CuboidInstancing();

    virtual void onInitialize() override;
    virtual void onRender() override;

    virtual bool loadShader() override;

    virtual void setCube(size_t index, const Cuboid & cuboid) override;

    virtual size_t size() const override;
    virtual size_t verticesCount() const override;
    virtual size_t staticByteSize() const override;
    virtual size_t byteSize() const override;
    virtual size_t vertexByteSize() const override;
    virtual size_t componentCount() const override;

    virtual void resize(size_t count) override;

    virtual gl::GLuint program() const override;
public:
    std::vector<glm::vec3> m_position;
    std::vector<glm::vec3> m_scale;
    std::vector<float> m_colorValue;

    gl::GLuint m_vertices;
    gl::GLuint m_attributes;
    gl::GLuint m_vao;

    gl::GLuint m_vertexShader;
    gl::GLuint m_fragmentShader;

    gl::GLuint m_program;

    void initializeVAO();
    size_t verticesPerCuboid() const;
};
