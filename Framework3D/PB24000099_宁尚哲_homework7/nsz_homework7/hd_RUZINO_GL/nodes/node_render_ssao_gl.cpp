

#include "../light.h"
#include "nodes/core/def/node_def.hpp"
#include "pxr/imaging/hd/tokens.h"
#include "render_node_base.h"
#include "rich_type_buffer.hpp"
#include "utils/draw_fullscreen.h"
#include <random>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
NODE_DEF_OPEN_SCOPE
NODE_DECLARATION_FUNCTION(ssao)
{
    b.add_input<GLTextureHandle>("Color");
    b.add_input<GLTextureHandle>("Position");
    b.add_input<GLTextureHandle>("Depth");

    // HW6: For HBAO you might need normal texture.

    b.add_input<std::string>("Shader").default_val("shaders/ssao.fs");
    b.add_output<GLTextureHandle>("Color");
}

NODE_EXECUTION_FUNCTION(ssao)
{
    auto color = params.get_input<GLTextureHandle>("Color");
    auto position = params.get_input<GLTextureHandle>("Position");
    auto depth = params.get_input<GLTextureHandle>("Depth");

    auto size = color->desc.size;

    unsigned int VBO, VAO;

    CreateFullScreenVAO(VAO, VBO);

    GLTextureDesc texture_desc;
    texture_desc.size = size;
    texture_desc.format = HdFormatFloat32Vec4;
    auto color_texture = resource_allocator.create(texture_desc);

    auto shaderPath = params.get_input<std::string>("Shader");

    GLShaderDesc shader_desc;
    shader_desc.set_vertex_path(
        std::filesystem::path(RENDER_NODES_FILES_DIR) /
        std::filesystem::path("shaders/fullscreen.vs"));

    shader_desc.set_fragment_path(
        std::filesystem::path(RENDER_NODES_FILES_DIR) /
        std::filesystem::path(shaderPath));
    auto shader = resource_allocator.create(shader_desc);
    GLuint framebuffer;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        color_texture->texture_id,
        0);

    glClearColor(0.f, 0.f, 0.f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    shader->shader.use();
    shader->shader.setVec2("iResolution", size);

    // generate SSAO kernel
    const unsigned int kernelSize = 64;
    std::vector<glm::vec3> ssaoKernel;
    ssaoKernel.reserve(kernelSize);
    std::uniform_real_distribution<float> randomFloats(0.0f, 1.0f);
    std::default_random_engine generator;
    for (unsigned int i = 0; i < kernelSize; ++i) {
        glm::vec3 sample(
            randomFloats(generator) * 2.0f - 1.0f,
            randomFloats(generator) * 2.0f - 1.0f,
            randomFloats(generator)
        );
        sample = glm::normalize(sample);
        sample *= randomFloats(generator);
        float scale = float(i) / float(kernelSize);
        scale = 0.1f + 0.9f * (scale * scale);
        sample *= scale;
        ssaoKernel.push_back(sample);
        shader->shader.setVec3(std::string("samples[") + std::to_string(i) + "]", sample.x, sample.y, sample.z);
    }

    // generate noise texture (4x4)
    std::vector<glm::vec3> ssaoNoise;
    ssaoNoise.reserve(16);
    for (unsigned int i = 0; i < 16; ++i) {
        glm::vec3 noise(
            randomFloats(generator) * 2.0f - 1.0f,
            randomFloats(generator) * 2.0f - 1.0f,
            0.0f
        );
        ssaoNoise.push_back(noise);
    }

    GLTextureDesc noise_desc;
    noise_desc.size = {4, 4};
    noise_desc.format = HdFormatFloat32Vec4;
    auto noise_texture = resource_allocator.create(noise_desc);
    glBindTexture(GL_TEXTURE_2D, noise_texture->texture_id);
    std::vector<float> noise_data(4 * 4 * 4, 0.0f);
    for (int i = 0; i < 16; ++i) {
        noise_data[i * 4 + 0] = ssaoNoise[i].x;
        noise_data[i * 4 + 1] = ssaoNoise[i].y;
        noise_data[i * 4 + 2] = ssaoNoise[i].z;
        noise_data[i * 4 + 3] = 0.0f;
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 4, 4, 0, GL_RGBA, GL_FLOAT, noise_data.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // bind inputs and uniforms
    unsigned id = 0;
    shader->shader.setInt("gColor", id);
    glActiveTexture(GL_TEXTURE0 + id);
    glBindTexture(GL_TEXTURE_2D, color->texture_id);
    id++;

    shader->shader.setInt("gPosition", id);
    glActiveTexture(GL_TEXTURE0 + id);
    glBindTexture(GL_TEXTURE_2D, position->texture_id);
    id++;

    shader->shader.setInt("gDepth", id);
    glActiveTexture(GL_TEXTURE0 + id);
    glBindTexture(GL_TEXTURE_2D, depth->texture_id);
    id++;

    shader->shader.setInt("texNoise", id);
    glActiveTexture(GL_TEXTURE0 + id);
    glBindTexture(GL_TEXTURE_2D, noise_texture->texture_id);
    id++;

    shader->shader.setInt("kernelSize", (int)kernelSize);
    shader->shader.setFloat("radius", 0.5f);
    shader->shader.setFloat("bias", 0.025f);

    // camera matrices
    Hd_RUZINO_Camera* free_camera = get_free_camera(params);
    shader->shader.setMat4("view", GfMatrix4f(free_camera->_viewMatrix));
    shader->shader.setMat4("projection", GfMatrix4f(free_camera->_projMatrix));

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // --- blur pass ---
    GLTextureDesc blur_desc;
    blur_desc.size = size;
    blur_desc.format = HdFormatFloat32Vec4;
    auto blur_texture = resource_allocator.create(blur_desc);

    GLShaderDesc blur_shader_desc;
    blur_shader_desc.set_vertex_path(
        std::filesystem::path(RENDER_NODES_FILES_DIR) /
        std::filesystem::path("shaders/fullscreen.vs"));
    blur_shader_desc.set_fragment_path(
        std::filesystem::path(RENDER_NODES_FILES_DIR) /
        std::filesystem::path("shaders/ssao_blur.fs"));
    auto blur_shader = resource_allocator.create(blur_shader_desc);

    GLuint blur_fbo;
    glGenFramebuffers(1, &blur_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, blur_fbo);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        blur_texture->texture_id,
        0);

    glClearColor(0.f, 0.f, 0.f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    blur_shader->shader.use();
    blur_shader->shader.setVec2("iResolution", size);
    blur_shader->shader.setInt("ssaoInput", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, color_texture->texture_id);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    DestroyFullScreenVAO(VAO, VBO);
    resource_allocator.destroy(shader);
    resource_allocator.destroy(blur_shader);

    params.set_output("Color", blur_texture);
    return true;
}

NODE_DECLARATION_UI(ssao);
NODE_DEF_CLOSE_SCOPE
