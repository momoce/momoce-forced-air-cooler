#include "cropdialog.h"
#include "cropwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QScreen>
#include <QGuiApplication>

CropDialog::CropDialog(const QPixmap &source, qreal aspectRatio, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("调整背景"));

    QScreen *screen = QGuiApplication::primaryScreen();
    QSize avail = screen->availableGeometry().size();

    int maxW = int(avail.width()  * 0.75);
    int maxH = int(avail.height() * 0.65);

    int w = maxW;
    int h = int(w / aspectRatio);
    if (h > maxH) {
        h = maxH;
        w = int(h * aspectRatio);
    }

    m_cropWidget = new CropWidget(source, this);
    m_cropWidget->setFixedSize(w, h);

    QLabel *tip = new QLabel(
        QStringLiteral("滚轮缩放 · 按住鼠标左键拖动 · 框内为最终背景"), this);
    tip->setAlignment(Qt::AlignCenter);

    QPushButton *okBtn     = new QPushButton(QStringLiteral("确定"), this);
    QPushButton *cancelBtn = new QPushButton(QStringLiteral("取消"), this);
    connect(okBtn,     &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->addWidget(m_cropWidget, 0, Qt::AlignCenter);
    root->addWidget(tip);
    root->addLayout(btnRow);

    adjustSize();
}

QPixmap CropDialog::croppedImage() const
{
    return m_cropWidget->croppedImage();
}