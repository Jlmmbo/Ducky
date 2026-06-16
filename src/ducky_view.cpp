#include "ducky_view.hpp"

DuckyView::DuckyView(const char* modelPath, QWidget* parent)
    : QOpenGLWidget(parent)
{
    m_model = LoadModel(modelPath);
    if (m_model.vertexCount == 0) {
        std::cerr << "Failed to load model: " << modelPath << "\n";
        return;
    }
    std::cout << "Loaded " << m_model.dimensions << "D model: "
              << m_model.vertexCount << " vertices, " << m_model.indexCount << " indices" << std::endl;

    m_dims = m_model.dimensions;
    m_fpv = m_dims + 3;
    m_modelVertsBackup = m_model.vertices;

    m_transform.dims = m_dims;
    m_transform.angles.resize(m_transform.planeCount(), 0.0f);
    m_transform.modelAngles.resize(m_transform.planeCount(), 0.0f);
    m_transform.autoRotate.resize(m_transform.planeCount(), true);
    m_transform.translation.resize(m_dims, 0.0f);
    if (m_dims > 2) m_transform.translation[2] = -4.0f;

    m_edges = generateEdges(m_model.vertices.data(), m_model.vertexCount,
                             m_dims, m_fpv, m_model.indices.data(), m_model.indexCount);
    std::cout << "Generated " << m_edges.size() << " edges" << std::endl;

    m_projectedVerts.resize(m_model.vertexCount * 6);
    m_rotatedND.resize(m_model.vertexCount * m_dims);
    m_axis3D.resize(m_dims * 2 * 6);
    m_edge3D.resize(m_edges.size() * (EDGE_SUBDIV + 1) * 3);


    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &DuckyView::tick);
    m_timer->start(16);

    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

DuckyView::~DuckyView() {
    makeCurrent();
    glDeleteVertexArrays(1, &m_tessVAO);
    glDeleteBuffers(1, &m_tessVBO);
    glDeleteBuffers(1, &m_tessEBO);
    glDeleteVertexArrays(1, &m_axesVAO);
    glDeleteBuffers(1, &m_axesVBO);
    glDeleteVertexArrays(1, &m_subVAO);
    glDeleteBuffers(1, &m_subVBO);
    glDeleteBuffers(1, &m_subEBO);
    glDeleteVertexArrays(1, &m_edgeVAO);
    glDeleteBuffers(1, &m_edgeVBO);
    glDeleteProgram(m_edgeProgram);
    glDeleteProgram(m_tessProgram);
    glDeleteProgram(m_axesProgram);
}

void DuckyView::initializeGL() {
    initializeOpenGLFunctions();

    glGenVertexArrays(1, &m_tessVAO);
    glGenBuffers(1, &m_tessVBO);
    glGenBuffers(1, &m_tessEBO);

    glBindVertexArray(m_tessVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_tessVBO);
    glBufferData(GL_ARRAY_BUFFER, m_model.vertexCount * 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_tessEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_model.indexCount * sizeof(unsigned int), m_model.indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    m_tessProgram = createShaderProgram("shaders/tesseract.vert", "shaders/tesseract.frag");
    if (!m_tessProgram) return;
    m_tessUAspect = glGetUniformLocation(m_tessProgram, "uAspect");
    m_tessUDist3D = glGetUniformLocation(m_tessProgram, "uDist3D");
    m_tessUAlpha = glGetUniformLocation(m_tessProgram, "uAlpha");
    m_tessULighting = glGetUniformLocation(m_tessProgram, "uLighting");
    m_tessURenderMode = glGetUniformLocation(m_tessProgram, "uRenderMode");

    glGenVertexArrays(1, &m_axesVAO);
    glGenBuffers(1, &m_axesVBO);

    {
        std::vector<float> axisData(m_dims * 2 * 6);
        for (unsigned int d = 0; d < m_dims; d++) {
            float h = (float)d / (float)m_dims;
            float r, g, b;
            hslToRgb(h, 0.9f, 0.6f, r, g, b);
            axisData[d * 12 + 0] = 0; axisData[d * 12 + 1] = 0; axisData[d * 12 + 2] = 0;
            axisData[d * 12 + 3] = r; axisData[d * 12 + 4] = g; axisData[d * 12 + 5] = b;
            axisData[d * 12 + 6] = 0; axisData[d * 12 + 7] = 0; axisData[d * 12 + 8] = 0;
            axisData[d * 12 + 9] = r; axisData[d * 12 + 10] = g; axisData[d * 12 + 11] = b;
        }

        glBindVertexArray(m_axesVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_axesVBO);
        glBufferData(GL_ARRAY_BUFFER, axisData.size() * sizeof(float), axisData.data(), GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    }

    m_axisR.resize(m_dims);
    m_axisG.resize(m_dims);
    m_axisB.resize(m_dims);
    for (unsigned int d = 0; d < m_dims; d++) {
        float h = (float)d / (float)m_dims;
        hslToRgb(h, 0.9f, 0.6f, m_axisR[d], m_axisG[d], m_axisB[d]);
    }

    m_axesProgram = createShaderProgram("shaders/axes.vert", "shaders/axes.frag");
    if (!m_axesProgram) return;
    m_axesUAspect = glGetUniformLocation(m_axesProgram, "uAspect");
    m_axesUDist3D = glGetUniformLocation(m_axesProgram, "uDist3D");
    m_axesURenderMode = glGetUniformLocation(m_axesProgram, "uRenderMode");

    glGenVertexArrays(1, &m_edgeVAO);
    glGenBuffers(1, &m_edgeVBO);

    glBindVertexArray(m_edgeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_edgeVBO);
    glBufferData(GL_ARRAY_BUFFER, m_edges.size() * (EDGE_SUBDIV + 1) * 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    m_edgeProgram = createShaderProgram("shaders/edge.vert", "shaders/edge.frag");
    if (!m_edgeProgram) return;
    m_edgeUAspect = glGetUniformLocation(m_edgeProgram, "uAspect");
    m_edgeUDist3D = glGetUniformLocation(m_edgeProgram, "uDist3D");
    m_edgeURenderMode = glGetUniformLocation(m_edgeProgram, "uRenderMode");

    int maxVertsPerTri = (EDGE_SUBDIV + 1) * (EDGE_SUBDIV + 1);
    int maxTrisPerTri = EDGE_SUBDIV * (EDGE_SUBDIV + 1);
    glGenVertexArrays(1, &m_subVAO);
    glGenBuffers(1, &m_subVBO);
    glGenBuffers(1, &m_subEBO);
    glBindVertexArray(m_subVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_subVBO);
    glBufferData(GL_ARRAY_BUFFER, (m_model.indexCount / 3) * maxVertsPerTri * 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_subEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (m_model.indexCount / 3) * maxTrisPerTri * 3 * sizeof(unsigned int), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    m_elapsed.start();
    m_lastTime = 0;
    m_perfLastTime = 0;
}

void DuckyView::resizeGL(int w, int h) {
    glViewport(0, 0, w * devicePixelRatio(), h * devicePixelRatio());
}

void DuckyView::tick() {
    update();
}

void DuckyView::processInput(float dt) {
    if (m_newControls) {
        float spd = MOVE_SPEED * dt;
        if (m_mouse.keys[Qt::Key_A]) m_transform.translation[0] -= spd;
        if (m_mouse.keys[Qt::Key_D]) m_transform.translation[0] += spd;
        if (m_mouse.keys[Qt::Key_Q]) m_transform.translation[1] -= spd;
        if (m_mouse.keys[Qt::Key_E]) m_transform.translation[1] += spd;
        if (m_mouse.keys[Qt::Key_W]) m_transform.translation[2] += spd;
        if (m_mouse.keys[Qt::Key_S]) m_transform.translation[2] -= spd;
        if (m_mouse.keys[Qt::Key_R]) {
            std::fill(m_transform.angles.begin(), m_transform.angles.end(), 0.0f);
            std::fill(m_transform.modelAngles.begin(), m_transform.modelAngles.end(), 0.0f);
            std::fill(m_transform.translation.begin(), m_transform.translation.end(), 0.0f);
            if (m_dims > 2) m_transform.translation[2] = -4.0f;
        }
        return;
    }

    int planeKeysPos[] = {
        Qt::Key_1, Qt::Key_3, Qt::Key_5, Qt::Key_7,
        Qt::Key_9, Qt::Key_Minus
    };
    int planeKeysNeg[] = {
        Qt::Key_2, Qt::Key_4, Qt::Key_6, Qt::Key_8,
        Qt::Key_0, Qt::Key_Equal
    };

    for (int i = 0; i < 6 && i < m_transform.planeCount(); i++) {
        if (m_mouse.keys[planeKeysPos[i]])
            m_transform.angles[i] += ROTATE_SPEED;
        if (m_mouse.keys[planeKeysNeg[i]])
            m_transform.angles[i] -= ROTATE_SPEED;
    }

    int extraKeysPos[] = {
        Qt::Key_Q, Qt::Key_E, Qt::Key_T, Qt::Key_U,
        Qt::Key_O, Qt::Key_BracketLeft
    };
    int extraKeysNeg[] = {
        Qt::Key_W, Qt::Key_R, Qt::Key_Y, Qt::Key_I,
        Qt::Key_P, Qt::Key_BracketRight
    };
    bool ctrl = m_mouse.keys[Qt::Key_Control];

    for (int i = 0; i < 6 && (6 + i) < m_transform.planeCount(); i++) {
        if (ctrl && m_mouse.keys[extraKeysPos[i]])
            m_transform.angles[6 + i] += ROTATE_SPEED;
        if (ctrl && m_mouse.keys[extraKeysNeg[i]])
            m_transform.angles[6 + i] -= ROTATE_SPEED;
    }

    if (m_mouse.keys[Qt::Key_R]) {
        std::fill(m_transform.angles.begin(), m_transform.angles.end(), 0.0f);
        std::fill(m_transform.modelAngles.begin(), m_transform.modelAngles.end(), 0.0f);
        std::fill(m_transform.translation.begin(), m_transform.translation.end(), 0.0f);
        if (m_dims > 2) m_transform.translation[2] = -4.0f;
    }
}

void DuckyView::paintGL() {
    float dt = std::min((float)m_elapsed.elapsed() / 1000.0f - m_lastTime, 0.05f);
    m_lastTime = (float)m_elapsed.elapsed() / 1000.0f;

    processInput(dt);

    for (int i = 0; i < m_transform.planeCount(); i++) {
        if (m_transform.autoRotate[i])
            m_transform.modelAngles[i] += dt * 0.5f * (1 + (i % 3));
        float a = m_transform.modelAngles[i];
        a = fmodf(a + PI, 2.0f * PI);
        if (a < 0) a += 2.0f * PI;
        m_transform.modelAngles[i] = a - PI;
    }

    float aspect = (float)width() / (float)height();

    {
        float* pos = (float*)alloca(m_dims * sizeof(float));
        for (unsigned int i = 0; i < m_model.vertexCount; i++) {
            for (unsigned int d = 0; d < m_dims; d++)
                pos[d] = m_model.vertices[i * m_fpv + d];
            for (int ii = 0; ii < (int)m_dims; ii++)
                for (int jj = ii + 1; jj < (int)m_dims; jj++) {
                    float angle = m_transform.modelAngles[m_transform.planeIndex(ii, jj)];
                    if (fabsf(angle) > 0.0001f)
                        rotatePlane(pos[ii], pos[jj], angle);
                }
            for (unsigned int d = 0; d < m_dims; d++)
                pos[d] += m_transform.translation[d];
            applyRotation(pos, m_transform);
            for (unsigned int d = 0; d < m_dims; d++)
                m_rotatedND[i * m_dims + d] = pos[d];
            switch (m_renderMode) {
                case 0: projectPerspective(pos, &m_projectedVerts[i * 6], m_dims, m_focalLength); break;
                case 1: projectStereographic(pos, &m_projectedVerts[i * 6], m_dims, m_focalLength); break;
                default: projectOrthographic(pos, &m_projectedVerts[i * 6], m_dims); break;
            }
            m_projectedVerts[i * 6 + 3] = m_model.vertices[i * m_fpv + m_dims];
            m_projectedVerts[i * 6 + 4] = m_model.vertices[i * m_fpv + m_dims + 1];
            m_projectedVerts[i * 6 + 5] = m_model.vertices[i * m_fpv + m_dims + 2];
        }
    }

    if (g_debug) {
        m_farthestVertIndex = 0;
        float maxDistSq = 0;
        for (unsigned int i = 0; i < m_model.vertexCount; i++) {
            float distSq = 0;
            for (unsigned int d = 0; d < m_dims; d++) {
                float v = m_rotatedND[i * m_dims + d];
                distSq += v * v;
            }
            if (distSq > maxDistSq) {
                maxDistSq = distSq;
                m_farthestVertIndex = i;
            }
        }
        m_farthestVertCoords.resize(m_dims);
        for (unsigned int d = 0; d < m_dims; d++)
            m_farthestVertCoords[d] = m_rotatedND[m_farthestVertIndex * m_dims + d];
    }

    glViewport(0, 0, width() * devicePixelRatio(), height() * devicePixelRatio());

    unsigned int numTri = m_model.indexCount / 3;

    if (m_renderMode == 1) {
        int n = EDGE_SUBDIV;
        int vertsPerTri = (n + 1) * (n + 1);
        int trisPerTri = n * (n + 1);
        unsigned int totalSubTris = numTri * trisPerTri;
        unsigned int totalSubVerts = numTri * vertsPerTri;

        m_subVerts.resize(totalSubVerts * 6);
        m_subIdx.resize(totalSubTris * 3);

        float* tposA = (float*)alloca(m_dims * sizeof(float));
        float* tposB = (float*)alloca(m_dims * sizeof(float));
        float* tposC = (float*)alloca(m_dims * sizeof(float));
        float* interp = (float*)alloca(m_dims * sizeof(float));

        for (unsigned int t = 0; t < numTri; t++) {
            int ia = m_model.indices[t * 3];
            int ib = m_model.indices[t * 3 + 1];
            int ic = m_model.indices[t * 3 + 2];

            for (unsigned int d = 0; d < m_dims; d++) {
                tposA[d] = m_rotatedND[ia * m_dims + d];
                tposB[d] = m_rotatedND[ib * m_dims + d];
                tposC[d] = m_rotatedND[ic * m_dims + d];
            }

            float rA = m_model.vertices[ia * m_fpv + m_dims];
            float gA = m_model.vertices[ia * m_fpv + m_dims + 1];
            float bA = m_model.vertices[ia * m_fpv + m_dims + 2];
            float rB = m_model.vertices[ib * m_fpv + m_dims];
            float gB = m_model.vertices[ib * m_fpv + m_dims + 1];
            float bB = m_model.vertices[ib * m_fpv + m_dims + 2];
            float rC = m_model.vertices[ic * m_fpv + m_dims];
            float gC = m_model.vertices[ic * m_fpv + m_dims + 1];
            float bC = m_model.vertices[ic * m_fpv + m_dims + 2];

            unsigned int triVertBase = t * vertsPerTri;
            unsigned int triTriBase = t * trisPerTri;

            for (int j = 0; j <= n; j++) {
                for (int i = 0; i <= n; i++) {
                    float u = (float)i / n;
                    float v = (float)j / n;
                    float s = 1.0f - u - v;
                    if (s < 0.0f) {
                        float inv = 1.0f / (u + v);
                        u *= inv;
                        v *= inv;
                        s = 0.0f;
                    }
                    int vidx = triVertBase + j * (n + 1) + i;

                    for (unsigned int d = 0; d < m_dims; d++)
                        interp[d] = tposA[d] * s + tposB[d] * u + tposC[d] * v;
                    projectStereographic(interp, &m_subVerts[vidx * 6], m_dims, m_focalLength);

                    m_subVerts[vidx * 6 + 3] = rA * s + rB * u + rC * v;
                    m_subVerts[vidx * 6 + 4] = gA * s + gB * u + gC * v;
                    m_subVerts[vidx * 6 + 5] = bA * s + bB * u + bC * v;
                }
            }

            unsigned int subTriCount = 0;
            for (int j = 0; j < n; j++) {
                for (int i = 0; i < n; i++) {
                    if (i + j >= n) continue;
                    unsigned int v00 = triVertBase + j * (n + 1) + i;
                    unsigned int v10 = triVertBase + j * (n + 1) + i + 1;
                    unsigned int v01 = triVertBase + (j + 1) * (n + 1) + i;
                    unsigned int v11 = triVertBase + (j + 1) * (n + 1) + i + 1;

                    unsigned int ti = triTriBase + subTriCount * 2;
                    m_subIdx[ti * 3 + 0] = v00;
                    m_subIdx[ti * 3 + 1] = v10;
                    m_subIdx[ti * 3 + 2] = v01;
                    m_subIdx[ti * 3 + 3] = v10;
                    m_subIdx[ti * 3 + 4] = v11;
                    m_subIdx[ti * 3 + 5] = v01;
                    subTriCount++;
                }
            }
        }

        std::vector<float> triDepth(numTri);
        for (unsigned int t = 0; t < numTri; t++) {
            int i0 = m_model.indices[t * 3];
            int i1 = m_model.indices[t * 3 + 1];
            int i2 = m_model.indices[t * 3 + 2];
            triDepth[t] = (m_projectedVerts[i0 * 6 + 2] +
                           m_projectedVerts[i1 * 6 + 2] +
                           m_projectedVerts[i2 * 6 + 2]) / 3.0f;
        }

        std::vector<int> triOrder(numTri);
        for (unsigned int i = 0; i < numTri; i++) triOrder[i] = i;
        std::sort(triOrder.begin(), triOrder.end(), [&](int a, int b) {
            return triDepth[a] < triDepth[b];
        });

        std::vector<unsigned int> sortedIdx(totalSubTris * 3);
        unsigned int idxOff = 0;
        for (unsigned int ti = 0; ti < numTri; ti++) {
            int t = triOrder[ti];
            unsigned int base = t * trisPerTri * 3;
            for (unsigned int j = 0; j < trisPerTri * 3; j++)
                sortedIdx[idxOff + j] = m_subIdx[base + j];
            idxOff += trisPerTri * 3;
        }

        glBindBuffer(GL_ARRAY_BUFFER, m_subVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, totalSubVerts * 6 * sizeof(float), m_subVerts.data());
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_subEBO);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, totalSubTris * 3 * sizeof(unsigned int), sortedIdx.data());

        if (g_debug) {
            std::cout << "stereo: " << totalSubVerts << " verts, " << totalSubTris << " tris"
                      << "  v0=(" << m_subVerts[0] << "," << m_subVerts[1] << "," << m_subVerts[2]
                      << ") c0=(" << m_subVerts[3] << "," << m_subVerts[4] << "," << m_subVerts[5] << ")"
                      << "  idx0=" << sortedIdx[0] << " " << sortedIdx[1] << " " << sortedIdx[2]
                      << std::endl;
        }

        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);

        if (!m_wireframeOnly) {
            if (m_transparent) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDepthMask(GL_FALSE);
            }
            glUseProgram(m_tessProgram);
            glUniform1i(m_tessURenderMode, m_renderMode);
            glUniform1f(m_tessUAspect, aspect);
            glUniform1f(m_tessUDist3D, 3.0f * m_focalLength);
            glUniform1f(m_tessUAlpha, m_transparent ? m_modelAlpha : 1.0f);
            glUniform1f(m_tessULighting, m_lighting ? 1.0f : 0.0f);
            glBindVertexArray(m_subVAO);
            glDrawElements(GL_TRIANGLES, totalSubTris * 3, GL_UNSIGNED_INT, nullptr);
            if (m_transparent) {
                glDepthMask(GL_TRUE);
                glDisable(GL_BLEND);
            }
        }
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, m_tessVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, m_model.vertexCount * 6 * sizeof(float), m_projectedVerts.data());

        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        if (!m_wireframeOnly) {
            if (m_transparent) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDepthMask(GL_FALSE);
            }
            glUseProgram(m_tessProgram);
            glUniform1i(m_tessURenderMode, m_renderMode);
            glUniform1f(m_tessUAspect, aspect);
            glUniform1f(m_tessUDist3D, 3.0f * m_focalLength);
            glUniform1f(m_tessUAlpha, m_transparent ? m_modelAlpha : 1.0f);
            glUniform1f(m_tessULighting, m_lighting ? 1.0f : 0.0f);
            glBindVertexArray(m_tessVAO);
            glDrawElements(GL_TRIANGLES, m_model.indexCount, GL_UNSIGNED_INT, nullptr);
            if (m_transparent) {
                glDepthMask(GL_TRUE);
                glDisable(GL_BLEND);
            }
        }
    }

    glDisable(GL_DEPTH_TEST);
    {
        float* origin = (float*)alloca(m_dims * sizeof(float));
        float* tip = (float*)alloca(m_dims * sizeof(float));
        for (unsigned int d = 0; d < m_dims; d++) {
            for (unsigned int i = 0; i < m_dims; i++) {
                origin[i] = m_transform.translation[i];
                tip[i] = (i == d ? AXIS_LENGTH : 0.0f) + m_transform.translation[i];
            }
            applyRotation(origin, m_transform);
            applyRotation(tip, m_transform);
            switch (m_renderMode) {
                case 0: projectPerspective(origin, &m_axis3D[d * 12], m_dims, m_focalLength); break;
                case 1: projectStereographic(origin, &m_axis3D[d * 12], m_dims, m_focalLength); break;
                default: projectOrthographic(origin, &m_axis3D[d * 12], m_dims); break;
            }
            switch (m_renderMode) {
                case 0: projectPerspective(tip, &m_axis3D[d * 12 + 6], m_dims, m_focalLength); break;
                case 1: projectStereographic(tip, &m_axis3D[d * 12 + 6], m_dims, m_focalLength); break;
                default: projectOrthographic(tip, &m_axis3D[d * 12 + 6], m_dims); break;
            }
            m_axis3D[d * 12 + 3] = m_axisR[d]; m_axis3D[d * 12 + 4] = m_axisG[d]; m_axis3D[d * 12 + 5] = m_axisB[d];
            m_axis3D[d * 12 + 9] = m_axisR[d]; m_axis3D[d * 12 + 10] = m_axisG[d]; m_axis3D[d * 12 + 11] = m_axisB[d];
        }
        glBindBuffer(GL_ARRAY_BUFFER, m_axesVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, m_dims * 2 * 6 * sizeof(float), m_axis3D.data());
        glUseProgram(m_axesProgram);
        glUniform1i(m_axesURenderMode, m_renderMode);
        glUniform1f(m_axesUAspect, aspect);
        glUniform1f(m_axesUDist3D, 3.0f * m_focalLength);
        glBindVertexArray(m_axesVAO);
        glDrawArrays(GL_LINES, 0, m_dims * 2);
    }
    glEnable(GL_DEPTH_TEST);

    {
        size_t vertCount = 0;
        std::vector<size_t> edgeOffsets;
        edgeOffsets.reserve(m_edges.size());
        if (m_renderMode == 1) {
            float* posA = (float*)alloca(m_dims * sizeof(float));
            float* posB = (float*)alloca(m_dims * sizeof(float));
            float* interp = (float*)alloca(m_dims * sizeof(float));
            for (size_t i = 0; i < m_edges.size(); i++) {
                int ia = m_edges[i].a, ib = m_edges[i].b;
                for (unsigned int d = 0; d < m_dims; d++) {
                    posA[d] = m_rotatedND[ia * m_dims + d];
                    posB[d] = m_rotatedND[ib * m_dims + d];
                }
                edgeOffsets.push_back(vertCount);
                for (int s = 0; s <= EDGE_SUBDIV; s++) {
                    float t = (float)s / (float)EDGE_SUBDIV;
                    for (unsigned int d = 0; d < m_dims; d++)
                        interp[d] = posA[d] * (1.0f - t) + posB[d] * t;
                    projectStereographic(interp, &m_edge3D[vertCount * 3], m_dims, m_focalLength);
                    vertCount++;
                }
            }
        } else {
            for (size_t i = 0; i < m_edges.size(); i++) {
                int ia = m_edges[i].a, ib = m_edges[i].b;
                m_edge3D[vertCount * 3 + 0] = m_projectedVerts[ia * 6];
                m_edge3D[vertCount * 3 + 1] = m_projectedVerts[ia * 6 + 1];
                m_edge3D[vertCount * 3 + 2] = m_projectedVerts[ia * 6 + 2];
                vertCount++;
                m_edge3D[vertCount * 3 + 0] = m_projectedVerts[ib * 6];
                m_edge3D[vertCount * 3 + 1] = m_projectedVerts[ib * 6 + 1];
                m_edge3D[vertCount * 3 + 2] = m_projectedVerts[ib * 6 + 2];
                vertCount++;
            }
        }
        glBindBuffer(GL_ARRAY_BUFFER, m_edgeVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertCount * 3 * sizeof(float), m_edge3D.data());

        glDisable(GL_DEPTH_TEST);
        glUseProgram(m_edgeProgram);
        glUniform1i(m_edgeURenderMode, m_renderMode);
        glUniform1f(m_edgeUAspect, aspect);
        glUniform1f(m_edgeUDist3D, 3.0f * m_focalLength);
        glBindVertexArray(m_edgeVAO);
        if (m_renderMode == 1) {
            for (auto offset : edgeOffsets)
                glDrawArrays(GL_LINE_STRIP, (GLint)offset, EDGE_SUBDIV + 1);
        } else {
            glDrawArrays(GL_LINES, 0, (GLsizei)vertCount);
        }
        glEnable(GL_DEPTH_TEST);
    }

    if (m_showPerformance) {
        m_perfFrameCount++;
        double now = m_elapsed.elapsed() / 1000.0;
        if (now - m_perfLastTime >= 0.5) {
            m_perfFps = m_perfFrameCount / (float)(now - m_perfLastTime);
            m_perfFrameCount = 0;
            m_perfLastTime = now;
            emit fpsUpdated(m_perfFps);
        }
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QFont font("monospace", 10);
    painter.setFont(font);

    if (m_showPerformance) {
        QString perfStr = QString("FPS: %1  Verts: %2  Tris: %3")
            .arg(m_perfFps, 0, 'f', 1)
            .arg(m_model.vertexCount)
            .arg(m_model.indexCount / 3);
        painter.setPen(Qt::white);
        painter.drawText(20, height() - 40, perfStr);
    }

    QString hint = QString("%1D  |  E=wireframe V=preset C=color A=autorotate T=transparency L=lighting []=focal M=render R=reset  |  F11=FS F12=shot F1=perf")
        .arg(m_dims);
    painter.setPen(QColor(200, 200, 200));
    painter.drawText(20, height() - 20, hint);

    if (g_debug && m_farthestVertIndex >= 0) {
        QString debugStr = QString("Debug: farthest vertex %1: (").arg(m_farthestVertIndex);
        for (unsigned int d = 0; d < m_dims; d++) {
            debugStr += QString::number(m_farthestVertCoords[d], 'f', 3);
            if (d < m_dims - 1) debugStr += ", ";
        }
        debugStr += ")";
        painter.setPen(Qt::yellow);
        painter.drawText(20, height() - 60, debugStr);
    }

    painter.end();

    emit stateChanged();
}

void DuckyView::setWireframe(bool on) { m_wireframeOnly = on; update(); }

void DuckyView::setColorScheme(int scheme) {
    m_colorScheme = scheme;
    if (scheme == 0)
        m_model.vertices = m_modelVertsBackup;
    else
        assignFaceColors(m_model, scheme - 1);
    update();
}

void DuckyView::setRenderMode(int mode) { m_renderMode = mode % 3; update(); }
void DuckyView::setFocalLength(float fl) { m_focalLength = std::max(0.1f, std::min(5.0f, fl)); update(); }
void DuckyView::setLighting(bool on) { m_lighting = on; update(); }
void DuckyView::setTransparent(bool on) { m_transparent = on; update(); }

void DuckyView::resetTransform() {
    std::fill(m_transform.angles.begin(), m_transform.angles.end(), 0.0f);
    std::fill(m_transform.modelAngles.begin(), m_transform.modelAngles.end(), 0.0f);
    std::fill(m_transform.translation.begin(), m_transform.translation.end(), 0.0f);
    if (m_dims > 2) m_transform.translation[2] = -4.0f;
    update();
}

void DuckyView::saveState(const char* path) { ::saveState(path, m_transform); }
void DuckyView::loadState(const char* path) { ::loadState(path, m_transform); update(); }

void DuckyView::takeScreenshot() {
    makeCurrent();
    int w = width() * devicePixelRatio();
    int h = height() * devicePixelRatio();
    std::vector<unsigned char> pixels(w * h * 3);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    char screenshotPath[64];
    time_t now = time(nullptr);
    struct tm* tmNow = localtime(&now);
    snprintf(screenshotPath, sizeof(screenshotPath), "screenshot_%04d%02d%02d_%02d%02d%02d.tga",
             tmNow->tm_year + 1900, tmNow->tm_mon + 1, tmNow->tm_mday,
             tmNow->tm_hour, tmNow->tm_min, tmNow->tm_sec);
    writeTGA(screenshotPath, w, h, pixels.data());
    std::cout << "Screenshot saved: " << screenshotPath << std::endl;
}

void DuckyView::toggleFullscreen() {
    m_isFullscreen = !m_isFullscreen;
    emit fullscreenRequested();
}

void DuckyView::mousePressEvent(QMouseEvent* e) {
    m_mouse.lastX = e->position().x();
    m_mouse.lastY = e->position().y();
    if (e->button() == Qt::LeftButton) { m_mouse.left = true; m_mouse.leftPressed = true; }
    if (e->button() == Qt::RightButton) { m_mouse.right = true; m_mouse.rightPressed = true; }
    bool orbitKey = m_newControls ? m_mouse.left : m_mouse.right;
    if (orbitKey) m_orbitMode = true;
}

void DuckyView::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) { m_mouse.left = false; m_mouse.leftReleased = true; }
    if (e->button() == Qt::RightButton) { m_mouse.right = false; m_mouse.rightReleased = true; }
    m_orbitMode = false;
}

void DuckyView::mouseMoveEvent(QMouseEvent* e) {
    double mx = e->position().x();
    double my = e->position().y();
    m_mouse.moved = (mx != m_mouse.x || my != m_mouse.y);
    m_mouse.lastX = m_mouse.x;
    m_mouse.lastY = m_mouse.y;
    m_mouse.x = mx;
    m_mouse.y = my;

    if (m_orbitMode && m_mouse.moved) {
        double dx = mx - m_mouse.lastX;
        double dy = my - m_mouse.lastY;
        if (m_dims >= 3) {
            m_transform.angles[1] -= (float)dx * 0.005f;
            m_transform.angles[m_dims - 1] += (float)dy * 0.005f;
        } else if (m_transform.planeCount() >= 1) {
            m_transform.angles[0] += (float)dx * 0.005f;
        }
    }
}

void DuckyView::keyPressEvent(QKeyEvent* e) {
    m_mouse.keys[e->key()] = true;

    switch (e->key()) {
    case Qt::Key_Shift:
        m_newControls = !m_newControls;
        emit controlsModeChanged(m_newControls);
        break;
    case Qt::Key_F11:
        toggleFullscreen();
        break;
    case Qt::Key_F12:
        takeScreenshot();
        break;
    case Qt::Key_F1:
        m_showPerformance = !m_showPerformance;
        break;
    case Qt::Key_E:
        if (!m_newControls && !(e->modifiers() & Qt::ControlModifier))
            m_wireframeOnly = !m_wireframeOnly;
        break;
    case Qt::Key_A:
        if (!m_newControls) {
            bool anyOn = false;
            for (auto v : m_transform.autoRotate) if (v) anyOn = true;
            std::fill(m_transform.autoRotate.begin(), m_transform.autoRotate.end(), !anyOn);
        }
        break;
    case Qt::Key_T:
        if (!(e->modifiers() & Qt::ControlModifier))
            m_transparent = !m_transparent;
        break;
    case Qt::Key_L:
        if (e->modifiers() & Qt::ControlModifier)
            ::loadState("ducky_state.txt", m_transform);
        else
            m_lighting = !m_lighting;
        break;
    case Qt::Key_C:
        if (!(e->modifiers() & Qt::ControlModifier)) {
            m_colorScheme = (m_colorScheme + 1) % 5;
            if (m_colorScheme == 0)
                m_model.vertices = m_modelVertsBackup;
            else
                assignFaceColors(m_model, m_colorScheme - 1);
        }
        break;
    case Qt::Key_M:
        m_renderMode = (m_renderMode + 1) % 3;
        break;
    case Qt::Key_V:
        if (!m_newControls) {
            m_rotPreset = (m_rotPreset + 1) % 4;
            switch (m_rotPreset) {
                case 0: std::fill(m_transform.autoRotate.begin(), m_transform.autoRotate.end(), false); break;
                case 1: std::fill(m_transform.autoRotate.begin(), m_transform.autoRotate.end(), true); break;
                case 2: for (int i = 0; i < m_transform.planeCount(); i++) m_transform.autoRotate[i] = (i % 2 == 0); break;
                case 3: for (int i = 0; i < m_transform.planeCount(); i++) m_transform.autoRotate[i] = (i % 3 == 0); break;
            }
        }
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (g_debug && m_farthestVertIndex >= 0) {
            std::cout << "Debug: farthest vertex " << m_farthestVertIndex << ": (";
            for (unsigned int d = 0; d < m_dims; d++) {
                std::cout << m_farthestVertCoords[d];
                if (d < m_dims - 1) std::cout << ", ";
            }
            std::cout << ")" << std::endl;
        }
        break;
    case Qt::Key_BracketLeft:
        if (!(e->modifiers() & Qt::ControlModifier))
            m_focalLength = std::max(0.1f, m_focalLength - 0.1f);
        break;
    case Qt::Key_BracketRight:
        if (!(e->modifiers() & Qt::ControlModifier))
            m_focalLength = std::min(5.0f, m_focalLength + 0.1f);
        break;
    case Qt::Key_S:
        if (e->modifiers() & Qt::ControlModifier)
            ::saveState("ducky_state.txt", m_transform);
        break;
    case Qt::Key_Z:
        if (e->modifiers() & Qt::ControlModifier) {
            // Undo - no-op for now
        }
        break;
    default:
        break;
    }

    update();
}

void DuckyView::keyReleaseEvent(QKeyEvent* e) {
    m_mouse.keys[e->key()] = false;
}
