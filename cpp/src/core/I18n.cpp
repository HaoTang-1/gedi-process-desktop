#include "core/I18n.h"

#include <QHash>
#include <QByteArray>
#include <vector>

namespace i18n {

static Lang g_lang = Lang::Zh;
static std::vector<std::function<void(Lang)>> g_listeners;

struct Entry {
    const char* key;
    const char* zh;
    const char* en;
};

static const Entry kTable[] = {
    {"app_title", "GEDI 处理桌面", "GEDI Process Desktop"},
    {"menu_file", "文件(&F)", "&File"},
    {"menu_layers", "图层(&L)", "&Layers"},
    {"menu_view", "视图(&V)", "&View"},
    {"menu_transform", "变换(&T)", "&Transform"},
    {"menu_metric", "计算(&C)", "&Compute"},
    {"menu_calc", "波形计算器", "Waveform Calculator"},
    {"menu_batch", "批处理(&B)", "&Batch"},
    {"menu_help", "帮助(&H)", "&Help"},
    {"menu_language", "语言 / Language", "Language / 语言"},
    {"lang_zh", "中文（默认）", "中文（默认）"},
    {"lang_en", "English", "English"},
    {"act_open", "打开数据文件(&O)…", "Open Data Files(&O)…"},
    {"act_open_dir", "打开文件夹(&D)…", "Open Folder(&D)…"},
    {"act_sample", "加载示例数据(&S)", "Load Sample Data(&S)"},
    {"act_clear", "清空已加载文件", "Clear Loaded Files"},
    {"act_quit", "退出(&Q)", "E&xit"},
    {"act_add_raster", "添加栅格/底图图层(&R)…", "Add Raster/Basemap(&R)…"},
    {"act_wave_calc", "波形计算器(&W)…", "Waveform Calculator(&W)…"},
    {"act_reset_zoom", "重置光斑视图", "Reset Map View"},
    {"act_base_off", "关闭底图", "Basemap Off"},
    {"act_base_esri", "Esri 世界影像", "Esri World Imagery"},
    {"act_grid", "显示经纬网", "Show Graticule"},
    {"basemap_menu", "底图", "Basemap"},
    {"act_batch", "批处理多文件…", "Batch Process Files…"},
    {"act_about", "关于 / 扩展方式", "About"},
    {"act_recompute", "重新计算", "Recompute"},
    {"act_apply_default", "综合指标（当前光斑）", "Comprehensive Metrics"},
    {"act_hdf5_viewer", "查看 HDF5 详情", "HDF5 Viewer"},
    {"act_hdfview", "用 HDFView 打开", "Open in HDFView"},
    {"act_h5dump", "h5dump 详细信息…", "h5dump details…"},
    {"legend_cap", "图例 / 分类着色", "Legend / categories"},
    {"ctx_raster_props", "查看栅格属性", "Raster Properties"},
    {"ctx_zoom_layer", "缩放到图层", "Zoom to Layer"},
    {"ctx_delete", "删除数据", "Remove Layer"},
    {"ctx_check_all", "全部显示", "Show All"},
    {"ctx_uncheck_all", "全部隐藏", "Hide All"},
    {"panel_db", "数据库 / 文件", "Database / Files"},
    {"panel_info", "信息 / 计算", "Info / Compute"},
    {"panel_display", "显示属性", "Properties"},
    {"panel_wave", "波形演示", "Waveform"},
    {"tab_file_info", "文件信息", "File Info"},
    {"tab_metrics", "波形计算", "Metrics"},
    {"layer_cat_raster", "栅格 / 底图", "Raster / Basemap"},
    {"hint_check", "勾选 = 显示图层；取消 = 隐藏", "Checked = show layer"},
    {"hint_right_delete", "右键可删除图层", "Right-click to remove"},
    {"hint_wave", "点击地图光斑后，在右侧查看波形（高度↑ / 强度→）",
     "Click a footprint for waveform (Height↑ / Intensity→)"},
    {"hint_info", "导入数据后显示文件信息", "File info after import"},
    {"hint_map_empty", "暂无光斑\n请导入数据并在左侧勾选文件", "No footprints\nImport data and check layers"},
    {"hint_no_wave", "请选择光斑", "Select a footprint first"},
    {"tb_map_base_on", "光斑位置 · 底图", "Footprint Map · Basemap"},
    {"tb_map_base_off", "光斑位置 · 无底图", "Footprint Map · No Basemap"},
    {"tb_map_sel", "已选光斑", "Selected"},
    {"axis_lon", "经度 (°E) · EPSG:4326", "Longitude (°E) · EPSG:4326"},
    {"axis_lat", "纬度 (°N)", "Latitude (°N)"},
    {"axis_height", "高度 (m)", "Height (m)"},
    {"axis_intensity", "强度 (DN)", "Intensity (DN)"},
    {"msg_pick_footprint", "请先在地图上点击一个光斑。", "Click a footprint first."},
    {"msg_pick_file", "请先导入 GEDI 数据文件。", "Import GEDI files first."},
    {"msg_lang_changed", "界面语言已切换为英文。", "Language set to English."},
    {"status_ready", "就绪", "Ready"},
    {"menu_tools", "工具(&T)", "&Tools"},
    {"act_export_wave", "导出当前波形 CSV…", "Export Waveform CSV…"},
    {"act_export_metrics", "导出当前指标 CSV…", "Export Metrics CSV…"},
    {"act_save_map", "导出地图图片…", "Export Map Image…"},
    {"act_find_shot", "按 Shot 号定位…", "Go to Shot…"},
    {"act_stats", "数据统计", "Data Statistics"},
    {"act_copy_coord", "复制坐标", "Copy Coordinates"},
    {"act_recent", "最近打开", "Recent Files"},
    {"filter_quality", "仅质量=1", "Quality==1 only"},
};

Lang lang() { return g_lang; }

void setLang(Lang l)
{
    g_lang = l;
    for (auto& fn : g_listeners)
        fn(l);
}

void setLangFromName(const QString& name)
{
    setLang(name == QLatin1String("en") ? Lang::En : Lang::Zh);
}

QString name() { return g_lang == Lang::En ? QStringLiteral("en") : QStringLiteral("zh"); }

QString t(const char* key)
{
    const bool en = g_lang == Lang::En;
    for (const auto& e : kTable) {
        if (qstrcmp(e.key, key) == 0)
            return QString::fromUtf8(en ? e.en : e.zh);
    }
    return QString::fromUtf8(key);
}

void addListener(std::function<void(Lang)> fn)
{
    g_listeners.push_back(std::move(fn));
}

} // namespace i18n
