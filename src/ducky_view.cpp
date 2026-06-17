#include "ducky_view.hpp"

static inline int rowOffset(int S, int i, int j) {
    return i * (S + 1) - i * (i - 1) / 2 + j;
}

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
    m_edge3D.resize(m_edges.size() * 2 * 3);

    unsigned int numFaces = m_model.indexCount / 3;
    unsigned int numVertsPerFace = (m_subdivisionLevel + 1) * (m_subdivisionLevel + 2) / 2;
    unsigned int numIndicesPerFace = 3 * m_subdivisionLevel * m_subdivisionLevel;
    m_subdivVerts.resize(numFaces * numVertsPerFace * 6);
    m_subdivIndices.resize(numFaces * numIndicesPerFace);
    m_subdivEdgeVerts.resize(m_edges.size() * 2 * m_subdivisionLevel * 3);

    // Pre-build subdivision indices (same every frame)
    {
        unsigned int vertBase = 0;
        unsigned int idxOut = 0;
        for (unsigned int f = 0; f < numFaces; f++) {
            int S = m_subdivisionLevel;
            for (int i = 0; i < S; i++) {
                for (int j = 0; j < S - i; j++) {
                    int v00 = vertBase + rowOffset(S, i, j);
                    int v10 = vertBase + rowOffset(S, i + 1, j);
                    int v01 = vertBase + rowOffset(S, i, j + 1);
                    m_subdivIndices[idxOut++] = v00;
                    m_subdivIndices[idxOut++] = v10;
                    m_subdivIndices[idxOut++] = v01;
                    if (j < S - i - 1) {
                        int v11 = vertBase + rowOffset(S, i + 1, j + 1);
                        m_subdivIndices[idxOut++] = v10;
                        m_subdivIndices[idxOut++] = v11;
                        m_subdivIndices[idxOut++] = v01;
                    }
                }
            }
            vertBase += numVertsPerFace;
        }
    }


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
    glDeleteVertexArrays(1, &m_edgeVAO);
    glDeleteBuffers(1, &m_edgeVBO);
    glDeleteVertexArrays(1, &m_subdivVAO);
    glDeleteBuffers(1, &m_subdivVBO);
    glDeleteBuffers(1, &m_subdivEBO);
    glDeleteVertexArrays(1, &m_subdivEdgeVAO);
    glDeleteBuffers(1, &m_subdivEdgeVBO);
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
    {
        // Initialize with actual data to avoid driver lazy-allocation issues
        std::vector<float> initData(m_edges.size() * 2 * 3, 0.0f);
        glBufferData(GL_ARRAY_BUFFER, initData.size() * sizeof(float), initData.data(), GL_DYNAMIC_DRAW);
    }
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    m_edgeProgram = createShaderProgram("shaders/edge.vert", "shaders/edge.frag");
    if (!m_edgeProgram) return;
    m_edgeUAspect = glGetUniformLocation(m_edgeProgram, "uAspect");
    m_edgeUDist3D = glGetUniformLocation(m_edgeProgram, "uDist3D");
    m_edgeURenderMode = glGetUniformLocation(m_edgeProgram, "uRenderMode");

    // Subdivision buffers for stereographic mode
    glGenVertexArrays(1, &m_subdivVAO);
    glGenBuffers(1, &m_subdivVBO);
    glGenBuffers(1, &m_subdivEBO);
    glBindVertexArray(m_subdivVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_subdivVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_subdivEBO);

    glGenVertexArrays(1, &m_subdivEdgeVAO);
    glGenBuffers(1, &m_subdivEdgeVBO);
    glBindVertexArray(m_subdivEdgeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_subdivEdgeVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

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
            if (m_renderMode == 0)
                projectPerspective(pos, &m_projectedVerts[i * 6], m_dims, m_focalLength);
            else if (m_renderMode == 2)
                projectStereographic(pos, &m_projectedVerts[i * 6], m_dims, m_focalLength);
            else
                projectOrthographic(pos, &m_projectedVerts[i * 6], m_dims);
            m_projectedVerts[i * 6 + 3] = m_model.vertices[i * m_fpv + m_dims];
            m_projectedVerts[i * 6 + 4] = m_model.vertices[i * m_fpv + m_dims + 1];
            m_projectedVerts[i * 6 + 5] = m_model.vertices[i * m_fpv + m_dims + 2];
        }
    }

    if (m_colorScheme == 5) {
        m_vertexDepths.resize(m_model.vertexCount);
        float minDist = INFINITY, maxDist = 0.0f;
        for (unsigned int i = 0; i < m_model.vertexCount; i++) {
            float distSq = 0.0f;
            for (unsigned int d = 0; d < m_dims; d++) {
                float v = m_rotatedND[i * m_dims + d];
                distSq += v * v;
            }
            m_vertexDepths[i] = distSq;
            if (distSq < minDist) minDist = distSq;
            if (distSq > maxDist) maxDist = distSq;
        }
        m_depthMin = minDist;
        m_depthMax = maxDist;
        float range = maxDist - minDist;
        if (range < 1e-8f) range = 1.0f;
        for (unsigned int i = 0; i < m_model.vertexCount; i++) {
            float t = (m_vertexDepths[i] - minDist) / range;
            m_projectedVerts[i * 6 + 3] = 1.0f - t;
            m_projectedVerts[i * 6 + 4] = 0.0f;
            m_projectedVerts[i * 6 + 5] = t;
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

    int shaderMode = m_renderMode == 1 ? 2 : 0;
    bool isStereographic = (m_renderMode == 2);

    if (isStereographic) {
        buildStereographicMesh();
    }

    {
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
            glUniform1i(m_tessURenderMode, shaderMode);
            glUniform1f(m_tessUAspect, aspect);
            glUniform1f(m_tessUDist3D, 3.0f * m_focalLength);
            glUniform1f(m_tessUAlpha, m_transparent ? m_modelAlpha : 1.0f);
            glUniform1f(m_tessULighting, m_lighting ? 1.0f : 0.0f);
            if (isStereographic) {
                glBindBuffer(GL_ARRAY_BUFFER, m_subdivVBO);
                glBufferData(GL_ARRAY_BUFFER, m_subdivVerts.size() * sizeof(float), m_subdivVerts.data(), GL_DYNAMIC_DRAW);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_subdivEBO);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_subdivIndices.size() * sizeof(unsigned int), m_subdivIndices.data(), GL_DYNAMIC_DRAW);
                glBindVertexArray(m_subdivVAO);
                glDrawElements(GL_TRIANGLES, (GLsizei)m_subdivIndices.size(), GL_UNSIGNED_INT, nullptr);
            } else {
                glBindBuffer(GL_ARRAY_BUFFER, m_tessVBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, m_model.vertexCount * 6 * sizeof(float), m_projectedVerts.data());
                glBindVertexArray(m_tessVAO);
                glDrawElements(GL_TRIANGLES, m_model.indexCount, GL_UNSIGNED_INT, nullptr);
            }
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
            if (m_renderMode == 0) {
                projectPerspective(origin, &m_axis3D[d * 12], m_dims, m_focalLength);
                projectPerspective(tip, &m_axis3D[d * 12 + 6], m_dims, m_focalLength);
            } else if (m_renderMode == 2) {
                projectStereographic(origin, &m_axis3D[d * 12], m_dims, m_focalLength);
                projectStereographic(tip, &m_axis3D[d * 12 + 6], m_dims, m_focalLength);
            } else {
                projectOrthographic(origin, &m_axis3D[d * 12], m_dims);
                projectOrthographic(tip, &m_axis3D[d * 12 + 6], m_dims);
            }
            m_axis3D[d * 12 + 3] = m_axisR[d]; m_axis3D[d * 12 + 4] = m_axisG[d]; m_axis3D[d * 12 + 5] = m_axisB[d];
            m_axis3D[d * 12 + 9] = m_axisR[d]; m_axis3D[d * 12 + 10] = m_axisG[d]; m_axis3D[d * 12 + 11] = m_axisB[d];
        }
        glBindBuffer(GL_ARRAY_BUFFER, m_axesVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, m_dims * 2 * 6 * sizeof(float), m_axis3D.data());
        glUseProgram(m_axesProgram);
        glUniform1i(m_axesURenderMode, shaderMode);
        glUniform1f(m_axesUAspect, aspect);
        glUniform1f(m_axesUDist3D, 3.0f * m_focalLength);
        glBindVertexArray(m_axesVAO);
        glDrawArrays(GL_LINES, 0, m_dims * 2);
    }
    glEnable(GL_DEPTH_TEST);

    if (isStereographic) {
        buildStereographicEdges();
        glBindBuffer(GL_ARRAY_BUFFER, m_subdivEdgeVBO);
        glBufferData(GL_ARRAY_BUFFER, m_subdivEdgeVerts.size() * sizeof(float), m_subdivEdgeVerts.data(), GL_DYNAMIC_DRAW);
        glLineWidth(2.0f);
        glDisable(GL_DEPTH_TEST);
        glUseProgram(m_edgeProgram);
        glUniform1i(m_edgeURenderMode, shaderMode);
        glUniform1f(m_edgeUAspect, aspect);
        glUniform1f(m_edgeUDist3D, 3.0f * m_focalLength);
        glBindVertexArray(m_subdivEdgeVAO);
        glDrawArrays(GL_LINES, 0, (GLsizei)(m_subdivEdgeVerts.size() / 3));
        glEnable(GL_DEPTH_TEST);
        glLineWidth(1.0f);
    } else {
        size_t vertCount = 0;
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
        glBindBuffer(GL_ARRAY_BUFFER, m_edgeVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertCount * 3 * sizeof(float), m_edge3D.data());

        glLineWidth(2.0f);
        glDisable(GL_DEPTH_TEST);
        glUseProgram(m_edgeProgram);
        glUniform1i(m_edgeURenderMode, shaderMode);
        glUniform1f(m_edgeUAspect, aspect);
        glUniform1f(m_edgeUDist3D, 3.0f * m_focalLength);
        glBindVertexArray(m_edgeVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_edgeVBO);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_LINES, 0, (GLsizei)vertCount);

        glEnable(GL_DEPTH_TEST);
        glLineWidth(1.0f);
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

void DuckyView::buildStereographicMesh() {
    int S = m_subdivisionLevel;
    unsigned int numFaces = m_model.indexCount / 3;
    unsigned int numVertsPerFace = (S + 1) * (S + 2) / 2;
    unsigned int numIndicesPerFace = 3 * S * S;

    size_t neededVerts = numFaces * numVertsPerFace * 6;
    size_t neededIdx = numFaces * numIndicesPerFace;
    if (m_subdivVerts.size() != neededVerts) m_subdivVerts.resize(neededVerts);
    if (m_subdivIndices.size() != neededIdx) m_subdivIndices.resize(neededIdx);

    float* ndPos = (float*)alloca(m_dims * sizeof(float));
    float proj[6];
    unsigned int vertBase = 0;

    for (unsigned int f = 0; f < numFaces; f++) {
        unsigned int i0 = m_model.indices[f * 3];
        unsigned int i1 = m_model.indices[f * 3 + 1];
        unsigned int i2 = m_model.indices[f * 3 + 2];

        float* v0 = &m_rotatedND[i0 * m_dims];
        float* v1 = &m_rotatedND[i1 * m_dims];
        float* v2 = &m_rotatedND[i2 * m_dims];

        float c0r = m_model.vertices[i0 * m_fpv + m_dims];
        float c0g = m_model.vertices[i0 * m_fpv + m_dims + 1];
        float c0b = m_model.vertices[i0 * m_fpv + m_dims + 2];
        float c1r = m_model.vertices[i1 * m_fpv + m_dims];
        float c1g = m_model.vertices[i1 * m_fpv + m_dims + 1];
        float c1b = m_model.vertices[i1 * m_fpv + m_dims + 2];
        float c2r = m_model.vertices[i2 * m_fpv + m_dims];
        float c2g = m_model.vertices[i2 * m_fpv + m_dims + 1];
        float c2b = m_model.vertices[i2 * m_fpv + m_dims + 2];

        for (int i = 0; i <= S; i++) {
            for (int j = 0; j <= S - i; j++) {
                float t0 = (float)i / S;
                float t1 = (float)j / S;
                float t2 = 1.0f - t0 - t1;

                for (unsigned int d = 0; d < m_dims; d++)
                    ndPos[d] = t2 * v0[d] + t0 * v1[d] + t1 * v2[d];

                projectStereographic(ndPos, proj, m_dims, m_focalLength);
                if (m_colorScheme == 5) {
                    float distSq = 0;
                    for (unsigned int d = 0; d < m_dims; d++)
                        distSq += ndPos[d] * ndPos[d];
                    float t = (distSq - m_depthMin) / (m_depthMax - m_depthMin);
                    t = std::max(0.0f, std::min(1.0f, t));
                    proj[3] = 1.0f - t;
                    proj[4] = 0.0f;
                    proj[5] = t;
                } else {
                    proj[3] = t2 * c0r + t0 * c1r + t1 * c2r;
                    proj[4] = t2 * c0g + t0 * c1g + t1 * c2g;
                    proj[5] = t2 * c0b + t0 * c1b + t1 * c2b;
                }

                unsigned int vi = vertBase + rowOffset(S, i, j);
                m_subdivVerts[vi * 6 + 0] = proj[0];
                m_subdivVerts[vi * 6 + 1] = proj[1];
                m_subdivVerts[vi * 6 + 2] = proj[2];
                m_subdivVerts[vi * 6 + 3] = proj[3];
                m_subdivVerts[vi * 6 + 4] = proj[4];
                m_subdivVerts[vi * 6 + 5] = proj[5];
            }
        }
        vertBase += numVertsPerFace;
    }
}

void DuckyView::buildStereographicEdges() {
    int S = m_subdivisionLevel;
    size_t needed = m_edges.size() * 2 * S * 3;
    if (m_subdivEdgeVerts.size() != needed) m_subdivEdgeVerts.resize(needed);

    float* ndA = (float*)alloca(m_dims * sizeof(float));
    float* ndB = (float*)alloca(m_dims * sizeof(float));
    float pos[3];
    unsigned int outIdx = 0;

    for (size_t e = 0; e < m_edges.size(); e++) {
        int ia = m_edges[e].a;
        int ib = m_edges[e].b;
        float* va = &m_rotatedND[ia * m_dims];
        float* vb = &m_rotatedND[ib * m_dims];

        for (int k = 0; k < S; k++) {
            float t0 = (float)k / S;
            float t1 = (float)(k + 1) / S;
            for (unsigned int d = 0; d < m_dims; d++) {
                ndA[d] = va[d] * (1.0f - t0) + vb[d] * t0;
                ndB[d] = va[d] * (1.0f - t1) + vb[d] * t1;
            }
            projectStereographic(ndA, pos, m_dims, m_focalLength);
            m_subdivEdgeVerts[outIdx++] = pos[0];
            m_subdivEdgeVerts[outIdx++] = pos[1];
            m_subdivEdgeVerts[outIdx++] = pos[2];
            projectStereographic(ndB, pos, m_dims, m_focalLength);
            m_subdivEdgeVerts[outIdx++] = pos[0];
            m_subdivEdgeVerts[outIdx++] = pos[1];
            m_subdivEdgeVerts[outIdx++] = pos[2];
        }
    }
}

void DuckyView::setWireframe(bool on) { m_wireframeOnly = on; update(); }

void DuckyView::setColorScheme(int scheme) {
    m_colorScheme = scheme;
    if (scheme == 0 || scheme == 3)
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
            m_colorScheme = (m_colorScheme + 1) % 4;
            if (m_colorScheme == 0 || m_colorScheme == 3)
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
