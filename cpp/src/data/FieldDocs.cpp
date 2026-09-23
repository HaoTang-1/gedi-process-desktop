#include "data/FieldDocs.h"

#include <QHash>
#include <QPair>

namespace data {

QString fieldDoc(const QString& name)
{
    static const QHash<QString, QPair<QString, QString>> table = {
        {"shot_number", {"激光足印唯一编号", "Unique laser shot identifier"}},
        {"delta_time", {"自参考历元起的时间（秒）", "Seconds since reference epoch"}},
        {"beam", {"波束编号 0-11", "Beam number 0-11"}},
        {"latitude_bin0", {"第 0 距离仓纬度", "Latitude of range bin 0"}},
        {"longitude_bin0", {"第 0 距离仓经度", "Longitude of range bin 0"}},
        {"lat_lowestmode", {"最低模式纬度", "Latitude of lowest mode"}},
        {"lon_lowestmode", {"最低模式经度", "Longitude of lowest mode"}},
        {"elevation_bin0", {"第 0 距离仓高程", "Elevation of range bin 0"}},
        {"elevation_lastbin", {"最后距离仓高程", "Elevation of last bin"}},
        {"elev_lowestmode", {"最低模式高程（常作地表）", "Elev lowest mode (ground)"}},
        {"elev_highestreturn", {"最高回波高程（冠层顶）", "Elev highest return (canopy top)"}},
        {"rxwaveform", {"接收回波波形采样", "Received waveform samples"}},
        {"txwaveform", {"发射脉冲波形", "Transmitted waveform"}},
        {"rx_sample_count", {"每 shot 回波采样数", "RX samples per shot"}},
        {"rx_sample_start_index", {"回波起始索引（1-based）", "RX start index (1-based)"}},
        {"rx_energy", {"接收回波能量", "Received energy"}},
        {"energy_total", {"总能量", "Total energy"}},
        {"rh", {"相对高度指标 (N×101, RH0-100)", "Relative height metrics (N×101)"}},
        {"sensitivity", {"探测灵敏度 0-1", "Detection sensitivity"}},
        {"degrade_flag", {"降质标志 0=好", "Degrade flag"}},
        {"l2a_quality_flag_rel3", {"L2A 质量标志 Rel.3", "L2A quality Rel.3"}},
        {"selected_algorithm", {"L2A 反演算法", "Selected L2A algorithm"}},
        {"surface_flag", {"地表标志", "Surface flag"}},
        {"digital_elevation_model", {"外部 DEM", "External DEM"}},
        {"rx_clipflag", {"回波削波标志", "RX clip flag"}},
        {"geolocation", {"定位与几何", "Geolocation group"}},
        {"METADATA", {"产品元数据", "Product metadata"}},
    };
    QString key = name;
    key.remove(0, key.lastIndexOf('/') + 1);
    if (table.contains(key)) {
        auto p = table.value(key);
        return p.first + QStringLiteral(" | ") + p.second;
    }
    return {};
}

} // namespace data
