#ifndef CROPDIALOG_H
#define CROPDIALOG_H

#include <QDialog>
#include <QPixmap>

class CropWidget;

// 图片裁剪对话框
class CropDialog : public QDialog
{
    Q_OBJECT

public:
    CropDialog(const QPixmap &source, qreal aspectRatio, QWidget *parent = nullptr);
    QPixmap croppedImage() const;

private:
    CropWidget *m_cropWidget = nullptr;
};

#endif // CROPDIALOG_H