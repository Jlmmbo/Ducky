#include "ducky_window.hpp"

DuckyWindow::DuckyWindow(const char* modelPath, QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Ducky");
    setAcceptDrops(true);

    m_view = new DuckyView(modelPath, this);

    QWidget* central = new QWidget(this);
    QHBoxLayout* mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    QWidget* leftPanel = createLeftPanel();
    leftPanel->setFixedWidth(290);

    m_view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QWidget* rightPanel = createRightPanel();
    rightPanel->setFixedWidth(260);

    mainLayout->addWidget(leftPanel);
    mainLayout->addWidget(m_view, 1);
    mainLayout->addWidget(rightPanel);

    setCentralWidget(central);

    statusBar()->showMessage(
        QString("%1D | Shift=toggle controls F11=FS F12=shot F1=perf | Right panel has all controls")
            .arg(m_view->dimensions()));

    connect(m_view, &DuckyView::stateChanged, this, &DuckyWindow::syncInfoPanel);
    connect(m_view, &DuckyView::fpsUpdated, this, [this](float fps) {
        statusBar()->showMessage(
            QString("FPS: %1 | %2D | Shift=toggle controls F11=FS F12=shot F1=perf")
                .arg(fps, 0, 'f', 1)
                .arg(m_view->dimensions()));
    });
    connect(m_view, &DuckyView::fullscreenRequested, this, [this]() {
        if (isFullScreen()) showNormal(); else showFullScreen();
    });
    connect(m_view, &DuckyView::controlsModeChanged, this, [this](bool newControls) {
        m_controlStack->setCurrentIndex(newControls ? 0 : 1);
    });

    m_holdTimer = new QTimer(this);
    m_holdTimer->setInterval(50);
    connect(m_holdTimer, &QTimer::timeout, this, [this]() {
        if (m_holdType >= 0) {
            if (m_holdType == 0)
                m_view->transform().translation[m_holdIdx] += m_holdDir * 0.1f;
            else
                m_view->transform().angles[m_holdIdx] += m_holdDir * 0.1f;
        }
    });

    syncInfoPanel();
    resize(1920, 1020);
}

void DuckyWindow::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void DuckyWindow::dropEvent(QDropEvent* e) {
    if (e->mimeData()->hasUrls()) {
        QString path = e->mimeData()->urls().first().toLocalFile();
        if (path.endsWith(".dky")) loadModel(path);
    }
}

void DuckyWindow::loadModel(const QString& path) {
    statusBar()->showMessage("Drop not fully supported - restart with model path");
}

QWidget* DuckyWindow::createLeftPanel() {
    QWidget* panel = new QWidget(this);
    panel->setStyleSheet("background-color: #1a1a2e; color: #ccc;");

    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);

    QPushButton* toggleBtn = new QPushButton("Toggle Controls (Shift)", panel);
    toggleBtn->setStyleSheet(
        "QPushButton { background-color: #2a2a4e; color: #ccc; border: 1px solid #444; padding: 4px; }"
        "QPushButton:hover { background-color: #3a3a5e; }");
    connect(toggleBtn, &QPushButton::clicked, this, [this]() {
        m_newControls = !m_newControls;
        m_controlStack->setCurrentIndex(m_newControls ? 0 : 1);
    });

    layout->addWidget(toggleBtn);

    m_controlStack = new QStackedWidget(panel);
    m_controlStack->addWidget(createHDControls());
    m_controlStack->addWidget(createSliderControls());
    m_controlStack->setCurrentIndex(0);

    layout->addWidget(m_controlStack);
    layout->addStretch();
    return panel;
}

QWidget* DuckyWindow::createHDControls() {
    QWidget* w = new QWidget(this);
    QVBoxLayout* lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(2);

    QLabel* title = new QLabel("HD Controls", w);
    title->setStyleSheet("font-weight: bold; color: #ddd; padding: 2px;");
    lay->addWidget(title);

    unsigned int dims = m_view->dimensions();

    if (dims >= 4) {
        for (unsigned int d = 3; d < dims; d++) {
            QWidget* row = new QWidget(w);
            QHBoxLayout* rl = new QHBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0);
            rl->setSpacing(4);

            QPushButton* minus = new QPushButton("-", row);
            minus->setFixedSize(20, 20);
            minus->setStyleSheet(
                "QPushButton { background-color: #5a2a2a; color: #ddd; border: 1px solid #666; }"
                "QPushButton:hover { background-color: #7a3a3a; }"
                "QPushButton:pressed { background-color: #9a4a4a; }");
            minus->setAutoRepeat(true);
            minus->setAutoRepeatDelay(300);
            minus->setAutoRepeatInterval(50);
            connect(minus, &QPushButton::pressed, this, [this, d]() {
                m_holdType = 0; m_holdIdx = d; m_holdDir = -1; m_holdTimer->start();
                m_view->transform().translation[d] -= 0.1f;
            });
            connect(minus, &QPushButton::released, this, [this]() {
                m_holdType = -1; m_holdTimer->stop();
            });

            QLabel* label = new QLabel(QString("Move Dim %1").arg(d), row);
            label->setStyleSheet("color: #ccc;");

            QPushButton* plus = new QPushButton("+", row);
            plus->setFixedSize(20, 20);
            plus->setStyleSheet(
                "QPushButton { background-color: #2a5a2a; color: #ddd; border: 1px solid #666; }"
                "QPushButton:hover { background-color: #3a7a3a; }"
                "QPushButton:pressed { background-color: #4a9a4a; }");
            plus->setAutoRepeat(true);
            plus->setAutoRepeatDelay(300);
            plus->setAutoRepeatInterval(50);
            connect(plus, &QPushButton::pressed, this, [this, d]() {
                m_holdType = 0; m_holdIdx = d; m_holdDir = 1; m_holdTimer->start();
                m_view->transform().translation[d] += 0.1f;
            });
            connect(plus, &QPushButton::released, this, [this]() {
                m_holdType = -1; m_holdTimer->stop();
            });

            rl->addWidget(minus);
            rl->addWidget(label, 1);
            rl->addWidget(plus);
            lay->addWidget(row);
        }

        if (dims > 4) {
            QLabel* sep = new QLabel("---", w);
            sep->setStyleSheet("color: #666;");
            lay->addWidget(sep);
        }
    }

    for (int a = 3; a < (int)dims; a++) {
        for (int b = a + 1; b < (int)dims; b++) {
            QWidget* row = new QWidget(w);
            QHBoxLayout* rl = new QHBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0);
            rl->setSpacing(4);

            QPushButton* minus = new QPushButton("-", row);
            minus->setFixedSize(20, 20);
            minus->setStyleSheet(
                "QPushButton { background-color: #5a2a2a; color: #ddd; border: 1px solid #666; }"
                "QPushButton:hover { background-color: #7a3a3a; }"
                "QPushButton:pressed { background-color: #9a4a4a; }");
            minus->setAutoRepeat(true);
            minus->setAutoRepeatDelay(300);
            minus->setAutoRepeatInterval(50);
            int pi = m_view->transform().planeIndex(a, b);
            connect(minus, &QPushButton::pressed, this, [this, pi]() {
                m_holdType = 1; m_holdIdx = pi; m_holdDir = -1; m_holdTimer->start();
                m_view->transform().angles[pi] -= 0.1f;
            });
            connect(minus, &QPushButton::released, this, [this]() {
                m_holdType = -1; m_holdTimer->stop();
            });

            QLabel* label = new QLabel(QString("Rotate (%1,%2)").arg(a).arg(b), row);
            label->setStyleSheet("color: #ccc;");

            QPushButton* plus = new QPushButton("+", row);
            plus->setFixedSize(20, 20);
            plus->setStyleSheet(
                "QPushButton { background-color: #2a5a2a; color: #ddd; border: 1px solid #666; }"
                "QPushButton:hover { background-color: #3a7a3a; }"
                "QPushButton:pressed { background-color: #4a9a4a; }");
            plus->setAutoRepeat(true);
            plus->setAutoRepeatDelay(300);
            plus->setAutoRepeatInterval(50);
            connect(plus, &QPushButton::pressed, this, [this, pi]() {
                m_holdType = 1; m_holdIdx = pi; m_holdDir = 1; m_holdTimer->start();
                m_view->transform().angles[pi] += 0.1f;
            });
            connect(plus, &QPushButton::released, this, [this]() {
                m_holdType = -1; m_holdTimer->stop();
            });

            rl->addWidget(minus);
            rl->addWidget(label, 1);
            rl->addWidget(plus);
            lay->addWidget(row);
        }
    }

    lay->addStretch();
    return w;
}

QWidget* DuckyWindow::createSliderControls() {
    QWidget* w = new QWidget(this);
    QVBoxLayout* lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(2);

    QLabel* title = new QLabel(
        QString("%1D Rotations").arg(m_view->dimensions()), w);
    title->setStyleSheet("font-weight: bold; color: #ddd; padding: 2px;");
    lay->addWidget(title);

    int nPlanes = m_view->transform().planeCount();
    unsigned int dims = m_view->dimensions();

    QScrollArea* scroll = new QScrollArea(w);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; }");

    QWidget* scrollContent = new QWidget(scroll);
    QVBoxLayout* scrollLay = new QVBoxLayout(scrollContent);
    scrollLay->setContentsMargins(0, 0, 0, 0);
    scrollLay->setSpacing(2);

    for (int i = 0; i < nPlanes; i++) {
        int pi = 0;
        int ai = -1, aj = -1;
        for (int a = 0; a < (int)dims && pi <= i; a++)
            for (int b = a + 1; b < (int)dims && pi <= i; b++, pi++)
                if (pi == i) { ai = a; aj = b; }

        QWidget* row = new QWidget(scrollContent);
        QHBoxLayout* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(3);

        QCheckBox* toggle = new QCheckBox("A", row);
        toggle->setFixedWidth(20);
        toggle->setChecked(m_view->transform().autoRotate[i]);
        toggle->setStyleSheet(
            "QCheckBox { color: #4a4; }"
            "QCheckBox::indicator { width: 14px; height: 14px; }"
            "QCheckBox::indicator:checked { background-color: #2a7a2a; }"
            "QCheckBox::indicator:unchecked { background-color: #3a3a3a; }");
        connect(toggle, &QCheckBox::toggled, this, [this, i](bool checked) {
            m_view->transform().autoRotate[i] = checked;
        });

        QLabel* label = new QLabel(QString("(%1,%2)").arg(ai).arg(aj), row);
        label->setFixedWidth(36);
        label->setStyleSheet("color: #ccc;");

        QSlider* slider = new QSlider(Qt::Horizontal, row);
        slider->setRange(-3141, 3141);
        slider->setValue((int)(m_view->transform().modelAngles[i] * 1000.0f));
        slider->setStyleSheet(
            "QSlider::groove:horizontal { background: #333; height: 8px; border-radius: 4px; }"
            "QSlider::handle:horizontal { background: #5588dd; width: 12px; margin: -4px 0; border-radius: 4px; }"
            "QSlider::sub-page:horizontal { background: #5588dd; border-radius: 4px; }");

        connect(slider, &QSlider::valueChanged, this, [this, i](int val) {
            m_view->transform().modelAngles[i] = (float)val / 1000.0f;
        });

        QLabel* value = new QLabel(QString::number(m_view->transform().modelAngles[i], 'f', 2), row);
        value->setFixedWidth(50);
        value->setStyleSheet("color: #aaa;");
        connect(slider, &QSlider::valueChanged, this, [value](int val) {
            value->setText(QString::number((float)val / 1000.0f, 'f', 2));
        });

        rl->addWidget(toggle);
        rl->addWidget(label);
        rl->addWidget(slider, 1);
        rl->addWidget(value);
        scrollLay->addWidget(row);
    }

    scrollLay->addStretch();
    scroll->setWidget(scrollContent);
    lay->addWidget(scroll);
    return w;
}

QWidget* DuckyWindow::createRightPanel() {
    QWidget* panel = new QWidget(this);
    panel->setStyleSheet("background-color: #1a1a2e;");
    panel->setFixedWidth(250);

    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    m_infoLabel = new QLabel(panel);
    m_infoLabel->setStyleSheet("color: #ccc; font-family: monospace;");
    m_infoLabel->setWordWrap(true);
    layout->addWidget(m_infoLabel);

    layout->addSpacing(8);

    QGroupBox* actions = new QGroupBox("Actions", panel);
    actions->setStyleSheet(
        "QGroupBox { color: #ddd; border: 1px solid #444; padding: 4px; margin-top: 8px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }");

    QGridLayout* grid = new QGridLayout(actions);
    grid->setSpacing(3);

    auto makeBtn = [&](const QString& text) {
        QPushButton* btn = new QPushButton(text, actions);
        btn->setStyleSheet(
            "QPushButton { background-color: #2a2a4e; color: #ccc; border: 1px solid #444; padding: 4px 0; }"
            "QPushButton:hover { background-color: #3a3a6e; }"
            "QPushButton:pressed { background-color: #4a4a8e; }");
        btn->setMinimumHeight(24);
        return btn;
    };

    struct BtnDef { QString label; std::function<void()> onClick; };
    QVector<BtnDef> btnDefs = {
        {"Reset All", [this]() { m_view->resetTransform(); }},
        {"Wireframe", [this]() { m_view->setWireframe(!m_view->wireframe()); }},
        {"Color Scheme", [this]() {
            int sc = (m_view->colorScheme() + 1) % 4;
            m_view->setColorScheme(sc);
        }},
        {"Rotation", [this]() {
            auto& t = m_view->transform();
            static int rotPreset = 1;
            rotPreset = (rotPreset + 1) % 4;
            switch (rotPreset) {
                case 0: std::fill(t.autoRotate.begin(), t.autoRotate.end(), false); break;
                case 1: std::fill(t.autoRotate.begin(), t.autoRotate.end(), true); break;
                case 2: for (int i = 0; i < t.planeCount(); i++) t.autoRotate[i] = (i % 2 == 0); break;
                case 3: for (int i = 0; i < t.planeCount(); i++) t.autoRotate[i] = (i % 3 == 0); break;
            }
        }},
        {"Focus -", [this]() { m_view->setFocalLength(m_view->focalLength() - 0.1f); }},
        {"Focus +", [this]() { m_view->setFocalLength(m_view->focalLength() + 0.1f); }},
        {"Render Mode", [this]() { m_view->setRenderMode(m_view->renderMode() + 1); m_view->update(); }},
        {"Fullscreen", [this]() {
            if (isFullScreen()) showNormal(); else showFullScreen();
        }},
        {"Save State", [this]() { m_view->saveState("ducky_state.txt"); }},
        {"Load State", [this]() { m_view->loadState("ducky_state.txt"); }},
        {"Screenshot", [this]() { m_view->takeScreenshot(); }},
        {"Lighting", [this]() { m_view->setLighting(!m_view->lighting()); }},
    };

    int cols = 3;
    for (int i = 0; i < btnDefs.size(); i++) {
        int col = i % cols;
        int row = i / cols;
        QPushButton* btn = makeBtn(btnDefs[i].label);
        connect(btn, &QPushButton::clicked, btnDefs[i].onClick);
        grid->addWidget(btn, row, col);
    }

    layout->addWidget(actions);
    layout->addStretch();
    return panel;
}

void DuckyWindow::syncInfoPanel() {
    auto& m = m_view->model();
    unsigned int dims = m_view->dimensions();
    auto& t = m_view->transform();

    const char* colorSchemeNames[4] = {"Model", "Rainbow", "Mono", "Depth"};
    const char* renderModeNames[] = {"Perspective", "Orthographic", "Stereographic"};

    QString info;
    info += QString("%1D Model\n").arg(dims);
    info += QString("Vertices: %1\n").arg(m.vertexCount);
    info += QString("Triangles: %1\n").arg(m.indexCount / 3);
    info += QString("Edges: %1\n").arg(m_view->edges().size());
    info += QString("Planes: %1\n").arg(t.planeCount());
    info += QString("Focal Length: %1\n").arg(m_view->focalLength(), 0, 'f', 1);
    info += QString("Color Scheme: %1\n").arg(colorSchemeNames[m_view->colorScheme()]);
    info += QString("Wireframe: %1\n").arg(m_view->wireframe() ? "ON" : "OFF");
    info += QString("Lighting: %1\n").arg(m_view->lighting() ? "ON" : "OFF");
    info += QString("Mode: %1\n").arg(renderModeNames[m_view->renderMode()]);

    QString camStr = "Camera: (";
    for (unsigned int d = 0; d < dims && d < 3; d++) {
        if (d > 0) camStr += ", ";
        camStr += QString::number(-t.translation[d], 'f', 2);
    }
    camStr += dims > 3 ? ", ...)" : ")";
    info += camStr;

    m_infoLabel->setText(info);
}
