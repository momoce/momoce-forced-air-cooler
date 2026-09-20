#include "backgrounddialog.h"
#include "cropdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QFileDialog>
#include <QPainter>

// ============================================================
// 构造函数
// ============================================================
BackgroundDialog::BackgroundDialog(const QColor &currentColor,
                                   const QPixmap &currentImage,
                                   int currentOpacity,
                                   QWidget *parent)
    : QDialog(parent)
    , m_selectedColor(currentColor)
    , m_selectedImage(currentImage)
    , m_selectedOpacity(currentOpacity)
{
    setWindowTitle(QStringLiteral("背景设置"));
    setMinimumWidth(580);

    setupUI();
    updatePreview();
}

// ============================================================
// 构建 UI
// ============================================================
void BackgroundDialog::setupUI()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(14);

    // ---------------- 1. 纯色背景（底层） ----------------
    QLabel *colorTitle = new QLabel(QStringLiteral("纯色背景（底层）"), this);
    colorTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #222;");
    root->addWidget(colorTitle);
    root->addWidget(createColorRow());

    // ---------------- 2. 背景图片（上层） ----------------
    QLabel *imgTitle = new QLabel(QStringLiteral("背景图片（叠加在纯色上）"), this);
    imgTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #222;");
    root->addWidget(imgTitle);

    QHBoxLayout *imgBtnRow = new QHBoxLayout();
    QPushButton *btnSelect = new QPushButton(QStringLiteral("选择图片"), this);
    QPushButton *btnClear  = new QPushButton(QStringLiteral("清除图片"), this);
    btnSelect->setCursor(Qt::PointingHandCursor);
    btnClear->setCursor(Qt::PointingHandCursor);
    connect(btnSelect, &QPushButton::clicked, this, &BackgroundDialog::onSelectImage);
    connect(btnClear,  &QPushButton::clicked, this, &BackgroundDialog::onClearImage);

    m_imageStatus = new QLabel(this);
    m_imageStatus->setStyleSheet("color: #888; font-size: 12px;");

    imgBtnRow->addWidget(btnSelect);
    imgBtnRow->addWidget(btnClear);
    imgBtnRow->addStretch();
    imgBtnRow->addWidget(m_imageStatus);
    root->addLayout(imgBtnRow);

    // ---------------- 3. 图片透明度 ----------------
    QHBoxLayout *opacityTitleRow = new QHBoxLayout();
    QLabel *opacityTitle = new QLabel(QStringLiteral("图片透明度"), this);
    opacityTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #222;");
    m_opacityValue = new QLabel(this);
    m_opacityValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    opacityTitleRow->addWidget(opacityTitle);
    opacityTitleRow->addWidget(m_opacityValue);
    root->addLayout(opacityTitleRow);

    m_opacitySlider = new QSlider(Qt::Horizontal, this);
    m_opacitySlider->setRange(0, 255);
    m_opacitySlider->setValue(m_selectedOpacity);
    connect(m_opacitySlider, &QSlider::valueChanged,
            this, &BackgroundDialog::onOpacityChanged);
    root->addWidget(m_opacitySlider);

    // ---------------- 4. 预览 ----------------
    QLabel *previewTitle = new QLabel(QStringLiteral("预览"), this);
    previewTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #222;");
    root->addWidget(previewTitle);

    m_previewLabel = new QLabel(this);
    m_previewLabel->setFixedSize(340, 140);
    m_previewLabel->setStyleSheet(
        "QLabel {"
        "    border: 1px solid rgba(0,0,0,40);"
        "    border-radius: 6px;"
        "}");
    root->addWidget(m_previewLabel, 0, Qt::AlignLeft);

    // ---------------- 5. 底部按钮 ----------------
    QHBoxLayout *btnRow = new QHBoxLayout();
    QPushButton *btnApply  = new QPushButton(QStringLiteral("应用"), this);
    QPushButton *btnCancel = new QPushButton(QStringLiteral("取消"), this);
    btnApply->setCursor(Qt::PointingHandCursor);
    btnCancel->setCursor(Qt::PointingHandCursor);

    btnApply->setStyleSheet(
        "QPushButton {"
        "    background-color: #4CAF50; color: white; border: none;"
        "    border-radius: 6px; padding: 8px 24px; font-size: 13px; min-width: 80px;"
        "}"
        "QPushButton:hover  { background-color: #45a049; }"
        "QPushButton:pressed{ background-color: #3d8b40; }");
    btnCancel->setStyleSheet(
        "QPushButton {"
        "    background-color: #e0e0e0; color: #333; border: none;"
        "    border-radius: 6px; padding: 8px 24px; font-size: 13px; min-width: 80px;"
        "}"
        "QPushButton:hover  { background-color: #d0d0d0; }"
        "QPushButton:pressed{ background-color: #c0c0c0; }");

    connect(btnApply,  &QPushButton::clicked, this, &QDialog::accept);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    btnRow->addWidget(btnApply);           // 左下角"应用"
    btnRow->addStretch();
    btnRow->addWidget(btnCancel);          // 右下角"取消"
    root->addLayout(btnRow);

    // 初始化文字
    m_opacityValue->setText(QString::number(m_selectedOpacity * 100 / 255) + "%");
    m_imageStatus->setText(m_selectedImage.isNull()
                               ? QStringLiteral("未选择图片")
                               : QStringLiteral("已选择图片"));
}

// ============================================================
// 颜色圆行
// ============================================================
QWidget *BackgroundDialog::createColorRow()
{
    QList<QColor> colors = {
        QColor(45, 45, 48),     // 深灰
        QColor(30, 30, 40),     // 深蓝黑
        QColor(240, 240, 240),  // 浅灰
        QColor(255, 255, 255),  // 白
        QColor(244, 67, 54),    // 红
        QColor(255, 152, 0),    // 橙
        QColor(255, 193, 7),    // 黄
        QColor(76, 175, 80),    // 绿
        QColor(0, 188, 212),    // 青
        QColor(33, 150, 243),   // 蓝
        QColor(156, 39, 176),   // 紫
        QColor(233, 30, 99),    // 粉
    };

    QWidget *container = new QWidget(this);
    QGridLayout *grid = new QGridLayout(container);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(10);

    const int cols = 6;
    for (int i = 0; i < colors.size(); ++i) {
        grid->addWidget(createColorCircle(colors[i]),
                        i / cols, i % cols, Qt::AlignLeft);
    }
    grid->setColumnStretch(cols, 1);

    return container;
}

// ============================================================
// 单个颜色圆
// ============================================================
QPushButton *BackgroundDialog::createColorCircle(const QColor &color)
{
    QPushButton *btn = new QPushButton(this);
    btn->setFixedSize(38, 38);
    btn->setCursor(Qt::PointingHandCursor);

    QString borderColor = (color.lightness() > 200) ? "#bbb" : "rgba(0,0,0,60)";

    btn->setStyleSheet(QString(
                           "QPushButton {"
                           "    background-color: %1;"
                           "    border: 2px solid %2;"
                           "    border-radius: 19px;"
                           "}"
                           "QPushButton:hover { border: 2px solid #4CAF50; }")
                           .arg(color.name()).arg(borderColor));

    connect(btn, &QPushButton::clicked, this, [this, color]{
        onColorClicked(color);
    });

    return btn;
}

// ============================================================
// 点击颜色圆：只改纯色，不影响图片
// ============================================================
void BackgroundDialog::onColorClicked(const QColor &color)
{
    m_selectedColor = color;
    updatePreview();
}

// ============================================================
// 选图片
// ============================================================
void BackgroundDialog::onSelectImage()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择背景图片"),
        QString(),
        QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp *.webp)"));
    if (filePath.isEmpty()) return;

    QPixmap src(filePath);
    if (src.isNull()) return;

    // 宽高比：取父窗口的
    qreal aspect = 16.0 / 9.0;
    QWidget *p = parentWidget();
    if (p && p->height() > 0) {
        aspect = qreal(p->width()) / qreal(p->height());
    }

    CropDialog dlg(src, aspect, this);
    if (dlg.exec() != QDialog::Accepted) return;

    QPixmap result = dlg.croppedImage();
    if (result.isNull()) return;

    m_selectedImage = result;
    m_imageStatus->setText(QStringLiteral("已选择图片"));
    updatePreview();
}

// ============================================================
// 清除图片
// ============================================================
void BackgroundDialog::onClearImage()
{
    m_selectedImage = QPixmap();
    m_imageStatus->setText(QStringLiteral("未选择图片"));
    updatePreview();
}

// ============================================================
// 透明度变化
// ============================================================
void BackgroundDialog::onOpacityChanged(int value)
{
    m_selectedOpacity = value;
    m_opacityValue->setText(QString::number(value * 100 / 255) + "%");
    updatePreview();
}

// ============================================================
// 更新预览：先画纯色，再叠图片（带透明度）
// ============================================================
void BackgroundDialog::updatePreview()
{
    QSize sz = m_previewLabel->size();
    QPixmap preview(sz);
    preview.fill(Qt::transparent);

    QPainter p(&preview);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    // ① 底层：纯色（不透明）
    p.fillRect(QRect(QPoint(0, 0), sz), m_selectedColor);

    // ② 上层：图片（带透明度）
    if (!m_selectedImage.isNull()) {
        p.setOpacity(m_selectedOpacity / 255.0);

        QPixmap scaled = m_selectedImage.scaled(
            sz, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        int x = (scaled.width()  - sz.width())  / 2;
        int y = (scaled.height() - sz.height()) / 2;
        p.drawPixmap(0, 0, scaled, x, y, sz.width(), sz.height());

        p.setOpacity(1.0);
    }

    p.end();
    m_previewLabel->setPixmap(preview);
}