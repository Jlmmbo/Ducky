#pragma once

#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QSlider>
#include <QCheckBox>
#include <QLabel>
#include <QGroupBox>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTimer>
#include <QApplication>
#include <QStatusBar>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QString>

#include "ducky_view.hpp"

class DuckyWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit DuckyWindow(const char* modelPath, QWidget* parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private:
    QWidget* createLeftPanel();
    QWidget* createRightPanel();
    QWidget* createHDControls();
    QWidget* createSliderControls();
    void syncInfoPanel();
    void loadModel(const QString& path);

    DuckyView* m_view = nullptr;
    QStackedWidget* m_controlStack = nullptr;
    QLabel* m_infoLabel = nullptr;
    bool m_newControls = true;
    QTimer* m_holdTimer = nullptr;
    int m_holdType = -1;
    int m_holdIdx = 0;
    float m_holdDir = 0;
};
