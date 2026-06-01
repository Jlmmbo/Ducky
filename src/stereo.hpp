#pragma once

#include <vector>
#include <algorithm>
#include <glad/glad.h>
#include "transform.hpp"
#include "render.hpp"

struct TriDepth {
    int idx;
    float depth;
};

// -------------------------------------------------------------------
// Face rendering: subdivide triangles, project, sort, upload, draw
// -------------------------------------------------------------------
inline void stereoRenderFaces(
    // Input geometry
    const float* rotatedND,
    const float* modelVerts,
    const unsigned int* indices,
    unsigned int numTri,
    unsigned int dims,
    int fpv,
    float focalLength,
    int edgeSubdiv,
    // Render state
    float aspect,
    bool wireframeOnly,
    bool transparent,
    float modelAlpha,
    bool lighting,
    // OpenGL resources
    GLuint subVAO, GLuint subVBO, GLuint subEBO,
    GLuint tessProgram,
    GLuint tessUAspect, GLuint tessUDist3D,
    GLuint tessUNear, GLuint tessUFar,
    GLuint tessUAlpha, GLuint tessULighting,
    // Output z-range
    float& outMinZ, float& outMaxZ,
    // Temp buffers (reused across frames)
    std::vector<float>& subVerts,
    std::vector<unsigned int>& subIdx,
    std::vector<unsigned int>& sortedSubIdx,
    std::vector<TriDepth>& subDepths)
{
    int n = edgeSubdiv;
    int vertsPerTri = (n + 1) * (n + 2) / 2;
    int trisPerTri = n * n;
    unsigned int totalSubTris = numTri * trisPerTri;
    unsigned int totalSubVerts = numTri * vertsPerTri;

    subVerts.resize(totalSubVerts * 6);
    subIdx.resize(totalSubTris * 3);
    subDepths.resize(totalSubTris);

    float* tposA = (float*)alloca(dims * sizeof(float));
    float* tposB = (float*)alloca(dims * sizeof(float));
    float* tposC = (float*)alloca(dims * sizeof(float));
    float* interp = (float*)alloca(dims * sizeof(float));

    outMinZ = INFINITY;
    outMaxZ = -INFINITY;

    for (unsigned int t = 0; t < numTri; t++) {
        int ia = indices[t * 3];
        int ib = indices[t * 3 + 1];
        int ic = indices[t * 3 + 2];

        for (unsigned int d = 0; d < dims; d++) {
            tposA[d] = rotatedND[ia * dims + d];
            tposB[d] = rotatedND[ib * dims + d];
            tposC[d] = rotatedND[ic * dims + d];
        }

        float rA = modelVerts[ia * fpv + dims];
        float gA = modelVerts[ia * fpv + dims + 1];
        float bA = modelVerts[ia * fpv + dims + 2];
        float rB = modelVerts[ib * fpv + dims];
        float gB = modelVerts[ib * fpv + dims + 1];
        float bB = modelVerts[ib * fpv + dims + 2];
        float rC = modelVerts[ic * fpv + dims];
        float gC = modelVerts[ic * fpv + dims + 1];
        float bC = modelVerts[ic * fpv + dims + 2];

        unsigned int triVertBase = t * vertsPerTri;
        unsigned int triTriBase = t * trisPerTri;

        // Barycentric subdivision grid
        for (int j = 0; j <= n; j++) {
            int rowOff = j * (n + 1) - j * (j - 1) / 2;
            for (int i = 0; i <= n - j; i++) {
                float u = (float)i / n;
                float v = (float)j / n;
                float w = 1.0f - u - v;
                int vidx = triVertBase + rowOff + i;

                for (unsigned int d = 0; d < dims; d++)
                    interp[d] = tposA[d] * w + tposB[d] * u + tposC[d] * v;
                projectStereographic(interp, &subVerts[vidx * 6], dims, focalLength);

                float zz = subVerts[vidx * 6 + 2];
                if (zz < outMinZ) outMinZ = zz;
                if (zz > outMaxZ) outMaxZ = zz;

                subVerts[vidx * 6 + 3] = rA * w + rB * u + rC * v;
                subVerts[vidx * 6 + 4] = gA * w + gB * u + gC * v;
                subVerts[vidx * 6 + 5] = bA * w + bB * u + bC * v;
            }
        }

        // Generate sub-triangle indices
        unsigned int subTriCount = 0;
        for (int j = 0; j < n; j++) {
            int rowOffJ = j * (n + 1) - j * (j - 1) / 2;
            int rowOffJ1 = (j + 1) * (n + 1) - (j + 1) * j / 2;
            for (int i = 0; i < n - j; i++) {
                unsigned int v00 = triVertBase + rowOffJ + i;
                unsigned int v10 = triVertBase + rowOffJ + i + 1;
                unsigned int v01 = triVertBase + rowOffJ1 + i;

                unsigned int ti = triTriBase + subTriCount;
                subIdx[ti * 3 + 0] = v00;
                subIdx[ti * 3 + 1] = v10;
                subIdx[ti * 3 + 2] = v01;
                subDepths[ti].idx = (int)ti;
                subDepths[ti].depth = (subVerts[v00 * 6 + 2] + subVerts[v10 * 6 + 2] + subVerts[v01 * 6 + 2]) / 3.0f;
                subTriCount++;

                if (i + j < n - 1) {
                    unsigned int v11 = triVertBase + rowOffJ1 + i + 1;
                    unsigned int ti2 = triTriBase + subTriCount;
                    subIdx[ti2 * 3 + 0] = v10;
                    subIdx[ti2 * 3 + 1] = v11;
                    subIdx[ti2 * 3 + 2] = v01;
                    subDepths[ti2].idx = (int)ti2;
                    subDepths[ti2].depth = (subVerts[v10 * 6 + 2] + subVerts[v11 * 6 + 2] + subVerts[v01 * 6 + 2]) / 3.0f;
                    subTriCount++;
                }
            }
        }
    }

    // Sort back-to-front (with NaN protection)
    for (auto& td : subDepths)
        if (!std::isfinite(td.depth)) td.depth = 0.0f;
    std::sort(subDepths.begin(), subDepths.end(),
              [](auto& a, auto& b) { return a.depth > b.depth; });

    sortedSubIdx.resize(totalSubTris * 3);
    for (unsigned int i = 0; i < totalSubTris; i++) {
        int t = subDepths[i].idx;
        sortedSubIdx[i * 3 + 0] = subIdx[t * 3 + 0];
        sortedSubIdx[i * 3 + 1] = subIdx[t * 3 + 1];
        sortedSubIdx[i * 3 + 2] = subIdx[t * 3 + 2];
    }

    // Upload to GPU
    glBindBuffer(GL_ARRAY_BUFFER, subVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, totalSubVerts * 6 * sizeof(float), subVerts.data());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, subEBO);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, totalSubTris * 3 * sizeof(unsigned int), sortedSubIdx.data());

    if (wireframeOnly) return;

    // Draw
    if (transparent) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    }
    glUseProgram(tessProgram);
    glUniform1f(tessUAspect, aspect);
    glUniform1f(tessUDist3D, 3.0f * focalLength);
    {
        float uDist = 3.0f * focalLength;
        float near = uDist - outMaxZ;
        if (near < 0.01f) near = 0.01f;
        float far = uDist - outMinZ;
        if (far < near + 0.01f) far = near + 0.01f;
        float maxFar = std::max(near * 100.0f, 20.0f);
        if (far > maxFar) far = maxFar;
        glUniform1f(tessUNear, near);
        glUniform1f(tessUFar, far);
    }
    glUniform1f(tessUAlpha, transparent ? modelAlpha : 1.0f);
    glUniform1f(tessULighting, lighting ? 1.0f : 0.0f);
    glBindVertexArray(subVAO);
    glDrawElements(GL_TRIANGLES, totalSubTris * 3, GL_UNSIGNED_INT, nullptr);
    if (transparent) {
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}

// -------------------------------------------------------------------
// Edge rendering: subdivide edges with visibility culling, draw
// -------------------------------------------------------------------
inline void stereoRenderEdges(
    const float* rotatedND,
    const Edge* edges,
    size_t edgeCount,
    unsigned int dims,
    float focalLength,
    float aspect,
    int edgeSubdiv,
    // OpenGL resources
    GLuint edgeVAO, GLuint edgeVBO,
    GLuint edgeProgram,
    GLuint edgeUAspect, GLuint edgeUDist3D,
    GLuint edgeUNear, GLuint edgeUFar,
    // Near/far planes
    float near, float far,
    // Temp buffers (reused across frames)
    std::vector<float>& edge3D,
    std::vector<size_t>& edgeOffsets)
{
    size_t vertCount = 0;
    edgeOffsets.clear();
    edgeOffsets.reserve(edgeCount);

    for (size_t i = 0; i < edgeCount; i++) {
        int ia = edges[i].a, ib = edges[i].b;

        float* posA = (float*)alloca(dims * sizeof(float));
        float* posB = (float*)alloca(dims * sizeof(float));
        for (unsigned int d = 0; d < dims; d++) {
            posA[d] = rotatedND[ia * dims + d];
            posB[d] = rotatedND[ib * dims + d];
        }

        // Frustum culling: skip edges whose midpoint projects off-screen
        {
            float* midP = (float*)alloca(dims * sizeof(float));
            for (unsigned int d = 0; d < dims; d++)
                midP[d] = (posA[d] + posB[d]) * 0.5f;
            float checkOut[3];
            projectStereographic(midP, checkOut, dims, focalLength);
            if (!std::isfinite(checkOut[0] + checkOut[1] + checkOut[2]))
                continue;
            float uDist = 3.0f * focalLength;
            float zDepth = uDist - checkOut[2];
            float perspDiv = zDepth > 0.1f ? zDepth : 0.1f;
            float ndc_x = checkOut[0] * uDist / aspect / perspDiv;
            float ndc_y = checkOut[1] * uDist / perspDiv;
            if (ndc_x > 1.0f || ndc_x < -1.0f || ndc_y > 1.0f || ndc_y < -1.0f)
                continue;
        }

        edgeOffsets.push_back(vertCount);
        for (int s = 0; s <= edgeSubdiv; s++) {
            float t = (float)s / (float)edgeSubdiv;
            float* interp = (float*)alloca(dims * sizeof(float));
            for (unsigned int d = 0; d < dims; d++)
                interp[d] = posA[d] * (1.0f - t) + posB[d] * t;
            projectStereographic(interp, &edge3D[vertCount * 3], dims, focalLength);
            vertCount++;
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, edgeVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertCount * 3 * sizeof(float), edge3D.data());

    glDisable(GL_DEPTH_TEST);
    glUseProgram(edgeProgram);
    glUniform1f(edgeUAspect, aspect);
    glUniform1f(edgeUDist3D, 3.0f * focalLength);
    glUniform1f(edgeUNear, near);
    glUniform1f(edgeUFar, far);
    glBindVertexArray(edgeVAO);
    for (auto offset : edgeOffsets)
        glDrawArrays(GL_LINE_STRIP, (GLint)offset, edgeSubdiv + 1);
    glEnable(GL_DEPTH_TEST);
}
