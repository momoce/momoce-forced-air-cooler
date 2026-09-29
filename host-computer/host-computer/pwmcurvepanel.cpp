#include "pwmcurvepanel.h"
#include "pwmcurvechart.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QDebug>

PwmCurvePanel::PwmCurvePanel(QWidget *parent)
    : QWidget(parent)
{
    // ⭐ 完全透明
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAutoFillBackground(false);
    setStyleSheet("background: transparent;");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(6);

    // ================= 标题（灰白偏白） =================
    m_titleLabel = new QLabel(QStringLiteral("BIOS 风扇曲线"), this);
    QFont tf = m_titleLabel->font();
    tf.setBold(true);
    tf.setPointSize(tf.pointSize() + 1);
    m_titleLabel->setFont(tf);
    m_titleLabel->setStyleSheet(
        "QLabel { color: #E0E0E0; background: transparent; padding-left: 2px; }");
    root->addWidget(m_titleLabel);

    // ================= CPU 曲线 =================
    m_cpuChart = new PwmCurveChart(
        QStringLiteral("CPU 风扇配置文件, PWM/°C"),
        QColor(74, 144, 217),   // 蓝色
        this);
    m_cpuChart->setYRange(0, 7000);
    root->addWidget(m_cpuChart, 1);

    // ================= GPU 曲线 =================
    m_gpuChart = new PwmCurveChart(
        QStringLiteral("GPU 风扇配置文件, PWM/°C"),
        QColor(231, 76, 60),    // 红色
        this);
    m_gpuChart->setYRange(0, 7200);
    root->addWidget(m_gpuChart, 1);

    // ================= 信号 =================
    // ⭐ 拖动小球 → 保存到当前预设 → 通知外部
    connect(m_cpuChart, &PwmCurveChart::pointMoved,
            this, [this](int, QPointF){
                if (m_updating) return;
                saveCurrentCurvesToPreset();          // 保存
                emit curvesChanged(cpuCurve(), gpuCurve());
            });
    connect(m_gpuChart, &PwmCurveChart::pointMoved,
            this, [this](int, QPointF){
                if (m_updating) return;
                saveCurrentCurvesToPreset();          // 保存
                emit curvesChanged(cpuCurve(), gpuCurve());
            });

    // 初始化：默认使用"平衡"
    applyPreset(QStringLiteral("平衡"));
}

QVector<QPointF> PwmCurvePanel::cpuCurve() const
{
    return m_cpuChart->data();
}

QVector<QPointF> PwmCurvePanel::gpuCurve() const
{
    return m_gpuChart->data();
}

// ============================================================
// ⭐ 把当前图表数据写回当前预设
// ============================================================
void PwmCurvePanel::saveCurrentCurvesToPreset()
{
    if (m_currentPresetName.isEmpty()) return;

    m_presetCurves[m_currentPresetName] = {
        m_cpuChart->data(),
        m_gpuChart->data()
    };

    qDebug() << "[预设] 已保存修改到预设：" << m_currentPresetName;

    emit presetModified(m_currentPresetName);
}

// ============================================================
// 应用预设
// ============================================================
void PwmCurvePanel::applyPreset(const QString &name)
{
    const QString n = name.trimmed();
    qDebug() << "[applyPreset] 收到：" << n;

    QVector<QPointF> cpu, gpu;

    // ⭐ 如果这个预设已经存在（用户改过或之前打开过），直接用保存的曲线
    if (m_presetCurves.contains(n)) {
        cpu = m_presetCurves.value(n).first;
        gpu = m_presetCurves.value(n).second;
        qDebug() << "[applyPreset] 使用已保存的曲线，预设：" << n;

    } else {
        // 第一次打开该预设：套用内置模板
        if (n.contains(QStringLiteral("性能"))
            || n.contains(QStringLiteral("Performance"), Qt::CaseInsensitive)) {

            cpu = { {30, 2200}, {50, 2800}, {65, 3200}, {70, 4400},
                   {75, 5400}, {80, 6400}, {90, 7000}, {100, 7000} };
            gpu = { {30, 2200}, {50, 2800}, {65, 3200}, {70, 3400},
                   {75, 4400}, {80, 5400}, {90, 6600}, {100, 7000} };

        } else if (n.contains(QStringLiteral("静音"))
                   || n.contains(QStringLiteral("Silent"), Qt::CaseInsensitive)) {

            cpu = { {30, 0}, {65, 0}, {70, 2200}, {75, 2800},
                   {80, 3200}, {85, 4400}, {90, 5400}, {100, 6400} };
            gpu = { {30, 0}, {65, 0}, {70, 2200}, {75, 2800},
                   {80, 3200}, {85, 3400}, {90, 4400}, {100, 6600} };

        } else if (n.contains(QStringLiteral("平衡"))
                   || n.contains(QStringLiteral("Balanced"), Qt::CaseInsensitive)
                   || n.contains(QStringLiteral("默认"))) {

            cpu = { {30, 0}, {65, 0}, {70, 2200}, {75, 2800},
                   {80, 3200}, {85, 4400}, {90, 5400}, {100, 6400} };
            gpu = { {30, 0}, {65, 0}, {70, 2200}, {75, 2800},
                   {80, 3200}, {85, 3400}, {90, 4400}, {100, 6600} };

        } else {
            // 用户新建的预设名（比如"我的曲线"）：
            // 以当前图表内容作为起点；如果当前没有曲线，用平衡曲线兜底
            qDebug() << "[applyPreset] 新预设，使用当前曲线作为起点：" << n;

            cpu = m_cpuChart->data();
            gpu = m_gpuChart->data();

            if (cpu.isEmpty() || gpu.isEmpty()) {
                cpu = { {30, 0}, {65, 0}, {70, 2200}, {75, 2800},
                       {80, 3200}, {85, 4400}, {90, 5400}, {100, 6400} };
                gpu = { {30, 0}, {65, 0}, {70, 2200}, {75, 2800},
                       {80, 3200}, {85, 3400}, {90, 4400}, {100, 6600} };
            }
        }

        // 缓存起来，下次直接读取
        m_presetCurves[n] = {cpu, gpu};
    }

    // ⭐ 记录当前预设名（拖动时就知道要保存到哪）
    m_currentPresetName = n;

    // 更新图表
    m_updating = true;
    m_cpuChart->setData(cpu);
    m_gpuChart->setData(gpu);
    m_updating = false;

    emit curvesChanged(cpu, gpu);
}