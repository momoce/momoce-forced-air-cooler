#ifndef PWMCURVEPANEL_H
#define PWMCURVEPANEL_H

#include <QWidget>
#include <QVector>
#include <QPointF>
#include <QString>
#include <QHash>
#include <QPair>

class QLabel;
class PwmCurveChart;

class PwmCurvePanel : public QWidget
{
    Q_OBJECT
public:
    explicit PwmCurvePanel(QWidget *parent = nullptr);

    QVector<QPointF> cpuCurve() const;
    QVector<QPointF> gpuCurve() const;

public slots:
    // ⭐ 由标题栏 presetChanged 信号驱动
    void applyPreset(const QString &name);

signals:
    void curvesChanged(const QVector<QPointF> &cpu,
                       const QVector<QPointF> &gpu);

    // ⭐ 当前预设的曲线被用户修改后发出（可用于提示/持久化）
    void presetModified(const QString &name);

private:
    // ⭐ 把当前 CPU/GPU 曲线写回到 m_presetCurves[m_currentPresetName]
    void saveCurrentCurvesToPreset();

    QLabel        *m_titleLabel = nullptr;
    PwmCurveChart *m_cpuChart   = nullptr;
    PwmCurveChart *m_gpuChart   = nullptr;

    bool m_updating = false;

    // ⭐ 当前预设名（由 applyPreset 更新）
    QString m_currentPresetName;

    // ⭐ 每个预设对应的曲线数据
    //    key   = 预设名
    //    value = { cpu 曲线, gpu 曲线 }
    QHash<QString, QPair<QVector<QPointF>, QVector<QPointF>>> m_presetCurves;
};

#endif // PWMCURVEPANEL_H