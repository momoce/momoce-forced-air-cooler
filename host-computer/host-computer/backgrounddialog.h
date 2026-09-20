#ifndef BACKGROUNDDIALOG_H
#define BACKGROUNDDIALOG_H

#include <QDialog>
#include <QColor>
#include <QPixmap>

class QLabel;
class QSlider;
class QPushButton;

// 背景设置对话框：纯色（底层）+ 图片（上层，可调透明度）
class BackgroundDialog : public QDialog
{
    Q_OBJECT

public:
    BackgroundDialog(const QColor &currentColor,
                     const QPixmap &currentImage,
                     int currentOpacity,
                     QWidget *parent = nullptr);

    QColor  selectedColor()   const { return m_selectedColor; }
    QPixmap selectedImage()   const { return m_selectedImage; }
    int     selectedOpacity() const { return m_selectedOpacity; }

private slots:
    void onColorClicked(const QColor &color);
    void onSelectImage();
    void onClearImage();
    void onOpacityChanged(int value);

private:
    void         setupUI();
    QWidget     *createColorRow();
    QPushButton *createColorCircle(const QColor &color);
    void         updatePreview();

    QColor  m_selectedColor;     // 底层纯色
    QPixmap m_selectedImage;     // 上层图片
    int     m_selectedOpacity;   // 图片 alpha 0~255

    QLabel  *m_previewLabel  = nullptr;
    QSlider *m_opacitySlider = nullptr;
    QLabel  *m_opacityValue  = nullptr;
    QLabel  *m_imageStatus   = nullptr;
};

#endif // BACKGROUNDDIALOG_H