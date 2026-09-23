#include "data/H5Dyn.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLibrary>
#include <QVector>
#include <cmath>
#include <cstring>
#include <stdexcept>

// Minimal HDF5 C API bindings (selected) via QLibrary / GetProcAddress.

namespace h5 {

// C API signatures (subset)
using herr_t = int;
using hsize_t = unsigned long long;
using htri_t = int;
using haddr_t = unsigned long long;

#define H5F_ACC_RDONLY 0u
#define H5P_DEFAULT 0
#define H5S_ALL 0
#define H5T_C_S1 0 /* placeholder — resolved at runtime via H5T_C_S1_g if needed */

// Type class
#define H5T_INTEGER 0
#define H5T_FLOAT 1
#define H5T_STRING 3

// Native type symbols are exported as hid_t variables on Windows
static struct Api {
    bool ok = false;
    QString err;

    // types as functions/vars
    hid_t (*H5open)(void) = nullptr;
    herr_t (*H5close)(void) = nullptr;
    hid_t (*H5Fopen)(const char*, unsigned, hid_t) = nullptr;
    herr_t (*H5Fclose)(hid_t) = nullptr;
    hid_t (*H5Dopen2)(hid_t, const char*, hid_t) = nullptr;
    herr_t (*H5Dclose)(hid_t) = nullptr;
    hid_t (*H5Dget_space)(hid_t) = nullptr;
    herr_t (*H5Sclose)(hid_t) = nullptr;
    int (*H5Sget_simple_extent_ndims)(hid_t) = nullptr;
    int (*H5Sget_simple_extent_dims)(hid_t, hsize_t*, hsize_t*) = nullptr;
    hid_t (*H5Dget_type)(hid_t) = nullptr;
    herr_t (*H5Tclose)(hid_t) = nullptr;
    int (*H5Tget_class)(hid_t) = nullptr;
    size_t (*H5Tget_size)(hid_t) = nullptr;
    herr_t (*H5Dread)(hid_t, hid_t, hid_t, hid_t, hid_t, void*) = nullptr;
    hid_t (*H5Tget_native_type)(hid_t, int) = nullptr;
    hid_t (*H5Gopen2)(hid_t, const char*, hid_t) = nullptr;
    herr_t (*H5Gclose)(hid_t) = nullptr;
    herr_t (*H5Literate)(hid_t, int, int, hsize_t*, void*, void*) = nullptr;
    // H5Literate2 in newer
    herr_t (*H5Literate2)(hid_t, int, int, hsize_t*, void*, void*, void*) = nullptr;
    int (*H5Lget_name_by_idx)(hid_t, const char*, int, int, hsize_t, char*, size_t, hid_t) = nullptr;
    herr_t (*H5Oget_info_by_name3)(hid_t, const char*, void*, unsigned, hid_t) = nullptr;
    // attrs
    hid_t (*H5Aopen)(hid_t, const char*, hid_t) = nullptr;
    hid_t (*H5Aget_space)(hid_t) = nullptr;
    hid_t (*H5Aget_type)(hid_t) = nullptr;
    herr_t (*H5Aread)(hid_t, hid_t, void*) = nullptr;
    herr_t (*H5Aclose)(hid_t) = nullptr;
    ssize_t (*H5Aget_name)(hid_t, size_t, char*) = nullptr;
    int (*H5Aget_num_attrs)(hid_t) = nullptr;
    hid_t (*H5Aopen_by_idx)(hid_t, const char*, int, int, hsize_t, hid_t, hid_t) = nullptr;
    // iterate links via H5Literate callback needs type — we use a simpler walk:
    hid_t (*H5Dopen_by_idx) = nullptr; // unused
    // native type global ids loaded via H5Tcopy of predefined
    hid_t nativeDouble = -1;
    hid_t nativeInt64 = -1;
    hid_t nativeFloat = -1;
    hid_t cString = -1;

    hid_t (*H5Tcopy)(hid_t) = nullptr;
    herr_t (*H5Tset_size)(hid_t, size_t) = nullptr;
    herr_t (*H5Eset_auto2)(hid_t, void*, void*) = nullptr;
    hid_t (*H5Tget_super)(hid_t) = nullptr;
    herr_t (*H5Tset_strpad)(hid_t, int) = nullptr;
} g;

static void setErr(const QString& e) { g.err = e; }

QString errorString() { return g.err; }

static void* sym(QLibrary& lib, const char* n)
{
    return reinterpret_cast<void*>(lib.resolve(n));
}

// Link iteration callback
// H5Literate: herr_t (*H5Literate2)(hid_t grp_id, H5_index_t idx_type, H5_iter_order_t order, hsize_t *idx, H5L_iterate2_t op, void *op_data)
// op: herr_t (*)(hid_t, const char*, const H5L_info2_t*, void*)

struct IterCtx {
    QStringList* names = nullptr;
    QList<int>* isGroup = nullptr;
};

#ifdef Q_OS_WIN
static herr_t __cdecl iterCb(hid_t /*group*/, const char* name, const void* /*info*/, void* op_data)
{
    auto* ctx = static_cast<IterCtx*>(op_data);
    if (ctx && ctx->names) {
        QString n = QString::fromUtf8(name);
        if (n != QLatin1String(".") && n != QLatin1String("..")) {
            ctx->names->push_back(n);
            // is_group filled later via open attempts if needed
            ctx->isGroup->push_back(-1);
        }
    }
    return 0; // continue
}
#else
static herr_t iterCb(hid_t, const char* name, const void*, void* op_data)
{
    return iterCb(reinterpret_cast<hid_t>(0), name, nullptr, op_data);
}
#endif

QStringList searchHintPaths()
{
    QStringList out;
    const QString env = QString::fromLocal8Bit(qgetenv("GEDI_HDF5_DLL"));
    if (!env.isEmpty())
        out << env;
    const QString appDir = QCoreApplication::applicationDirPath();
    out << appDir + QStringLiteral("/hdf5.dll");
    out << appDir + QStringLiteral("/../hdf5/bin/hdf5.dll");
    out << appDir + QStringLiteral("/../Library/bin/hdf5.dll");
    // PATH
    const QByteArray path = qgetenv("PATH");
    for (const QString& p : QString::fromLocal8Bit(path).split(';', Qt::SkipEmptyParts)) {
        out << p + QStringLiteral("/hdf5.dll");
    }
    // optional local installs
    out << QStringLiteral("C:/Program Files/HDF_Group/HDF5/1.14.4/bin/hdf5.dll");
    return out;
}

bool ensureLoaded(const QString& hintPath)
{
    if (g.ok)
        return true;
    QStringList tryPaths;
    if (!hintPath.isEmpty())
        tryPaths << hintPath;
    tryPaths << searchHintPaths();

    QLibrary lib;
    for (const QString& p : tryPaths) {
        if (!QFile::exists(p))
            continue;
        lib.setFileName(p);
        if (lib.load()) {
            g.H5open = (hid_t(*)())sym(lib, "H5open");
            g.H5Fopen = (hid_t(*)(const char*, unsigned, hid_t))sym(lib, "H5Fopen");
            g.H5Fclose = (herr_t(*)(hid_t))sym(lib, "H5Fclose");
            g.H5Dopen2 = (hid_t(*)(hid_t, const char*, hid_t))sym(lib, "H5Dopen2");
            g.H5Dclose = (herr_t(*)(hid_t))sym(lib, "H5Dclose");
            g.H5Dget_space = (hid_t(*)(hid_t))sym(lib, "H5Dget_space");
            g.H5Sclose = (herr_t(*)(hid_t))sym(lib, "H5Sclose");
            g.H5Sget_simple_extent_ndims = (int(*)(hid_t))sym(lib, "H5Sget_simple_extent_ndims");
            g.H5Sget_simple_extent_dims = (int(*)(hid_t, hsize_t*, hsize_t*))sym(lib, "H5Sget_simple_extent_dims");
            g.H5Dget_type = (hid_t(*)(hid_t))sym(lib, "H5Dget_type");
            g.H5Tclose = (herr_t(*)(hid_t))sym(lib, "H5Tclose");
            g.H5Tget_class = (int(*)(hid_t))sym(lib, "H5Tget_class");
            g.H5Tget_size = (size_t(*)(hid_t))sym(lib, "H5Tget_size");
            g.H5Dread = (herr_t(*)(hid_t, hid_t, hid_t, hid_t, hid_t, void*))sym(lib, "H5Dread");
            g.H5Tget_native_type = (hid_t(*)(hid_t, int))sym(lib, "H5Tget_native_type");
            g.H5Gopen2 = (hid_t(*)(hid_t, const char*, hid_t))sym(lib, "H5Gopen2");
            g.H5Gclose = (herr_t(*)(hid_t))sym(lib, "H5Gclose");
            g.H5Literate2 = (herr_t(*)(hid_t, int, int, hsize_t*, void*, void*, void*))sym(lib, "H5Literate2");
            g.H5Literate = (herr_t(*)(hid_t, int, int, hsize_t*, void*, void*))sym(lib, "H5Literate");
            g.H5Lget_name_by_idx = (int(*)(hid_t, const char*, int, int, hsize_t, char*, size_t, hid_t))sym(
                lib, "H5Lget_name_by_idx");
            g.H5Aopen = (hid_t(*)(hid_t, const char*, hid_t))sym(lib, "H5Aopen");
            g.H5Aget_space = (hid_t(*)(hid_t))sym(lib, "H5Aget_space");
            g.H5Aget_type = (hid_t(*)(hid_t))sym(lib, "H5Aget_type");
            g.H5Aread = (herr_t(*)(hid_t, hid_t, void*))sym(lib, "H5Aread");
            g.H5Aclose = (herr_t(*)(hid_t))sym(lib, "H5Aclose");
            g.H5Aget_name = (ssize_t(*)(hid_t, size_t, char*))sym(lib, "H5Aget_name");
            g.H5Tcopy = (hid_t(*)(hid_t))sym(lib, "H5Tcopy");
            g.H5Tset_size = (herr_t(*)(hid_t, size_t))sym(lib, "H5Tset_size");
            g.H5Eset_auto2 = (herr_t(*)(hid_t, void*, void*))sym(lib, "H5Eset_auto2");

            // predefined native types are exported as data symbols
            auto load_hid = [&](const char* n) -> hid_t {
                void* p = sym(lib, n);
                if (!p)
                    return -1;
                return *reinterpret_cast<hid_t*>(p);
            };
            // After H5open() these are valid
            if (g.H5open)
                g.H5open();
            g.nativeDouble = load_hid("H5T_NATIVE_DOUBLE_g");
            g.nativeFloat = load_hid("H5T_NATIVE_FLOAT_g");
            g.nativeInt64 = load_hid("H5T_NATIVE_INT64_g");
            g.cString = load_hid("H5T_C_S1_g");

            if (g.H5Fopen && g.H5Dopen2 && g.H5Dread && g.H5Sget_simple_extent_dims) {
                g.ok = true;
                g.err.clear();
                return true;
            }
            setErr(QStringLiteral("hdf5.dll missing exports: %1").arg(p));
            lib.unload();
        } else {
            setErr(QStringLiteral("load failed: %1").arg(p));
        }
    }
    if (g.err.isEmpty())
        g.err = QStringLiteral("hdf5.dll not found (set path to Anaconda/Library/bin/hdf5.dll)");
    return false;
}

bool available()
{
    return ensureLoaded();
}

bool fileOpen(const QString& path, hid_t& out)
{
    if (!ensureLoaded())
        return false;
    out = g.H5Fopen(path.toUtf8().constData(), H5F_ACC_RDONLY, H5P_DEFAULT);
    return out > 0;
}

void fileClose(hid_t id)
{
    if (g.ok && g.H5Fclose && id > 0)
        g.H5Fclose(id);
}

// Walk using H5Literate2 if present else H5Literate
static herr_t iterateGroup(hid_t grp, IterCtx* ctx)
{
    hsize_t idx = 0;
    if (g.H5Literate2) {
        // H5_INDEX_NAME=0, H5_ITER_NATIVE=0, H5_ITER_INC=1
        return g.H5Literate2(grp, 0, 1, &idx, (void*)iterCb, ctx, nullptr);
    }
    if (g.H5Literate) {
        typedef herr_t (*op_t)(hid_t, const char*, const void*, void*);
        return g.H5Literate(grp, 0, 1, &idx, (void*)iterCb, ctx);
    }
    return -1;
}

bool listNames(hid_t fileId, const QString& groupPath, QStringList& outNames, QList<int>& outIsGroup)
{
    if (!ensureLoaded())
        return false;
    hid_t grp = fileId;
    bool opened = false;
    if (!groupPath.isEmpty() && groupPath != QLatin1String("/")) {
        grp = g.H5Gopen2(fileId, groupPath.toUtf8().constData(), H5P_DEFAULT);
        if (grp <= 0)
            return false;
        opened = true;
    }
    IterCtx ctx;
    ctx.names = &outNames;
    ctx.isGroup = &outIsGroup;
    herr_t r = iterateGroup(grp, &ctx);
    if (opened)
        g.H5Gclose(grp);
    // classify dataset vs group by try-open
    for (int i = 0; i < outNames.size(); ++i) {
        QString full = groupPath.isEmpty() || groupPath == QLatin1String("/")
                           ? outNames[i]
                           : (groupPath + QLatin1Char('/') + outNames[i]);
        hid_t d = g.H5Dopen2(fileId, full.toUtf8().constData(), H5P_DEFAULT);
        if (d > 0) {
            outIsGroup[i] = 0;
            g.H5Dclose(d);
        } else {
            hid_t gg = g.H5Gopen2(fileId, full.toUtf8().constData(), H5P_DEFAULT);
            if (gg > 0) {
                outIsGroup[i] = 1;
                g.H5Gclose(gg);
            }
        }
    }
    return r >= 0 || !outNames.isEmpty();
}

bool datasetInfo(hid_t fileId, const QString& path, QVector<qint64>& dims, int& dtypeClass, int& dtypeSize)
{
    if (!ensureLoaded())
        return false;
    hid_t d = g.H5Dopen2(fileId, path.toUtf8().constData(), H5P_DEFAULT);
    if (d <= 0)
        return false;
    hid_t sp = g.H5Dget_space(d);
    int nd = g.H5Sget_simple_extent_ndims(sp);
    dims.resize(nd);
    std::vector<hsize_t> hd(std::max(nd, 0));
    g.H5Sget_simple_extent_dims(sp, hd.data(), nullptr);
    for (int i = 0; i < nd; ++i)
        dims[i] = (qint64)hd[i];
    hid_t ty = g.H5Dget_type(d);
    dtypeClass = g.H5Tget_class(ty);
    dtypeSize = (int)g.H5Tget_size(ty);
    g.H5Tclose(ty);
    g.H5Sclose(sp);
    g.H5Dclose(d);
    return true;
}

bool readAsDouble(hid_t fileId, const QString& path, std::vector<double>& out, QVector<qint64>& dims)
{
    if (!ensureLoaded())
        return false;
    hid_t d = g.H5Dopen2(fileId, path.toUtf8().constData(), H5P_DEFAULT);
    if (d <= 0)
        return false;
    hid_t sp = g.H5Dget_space(d);
    int nd = g.H5Sget_simple_extent_ndims(sp);
    dims.resize(nd);
    std::vector<hsize_t> hd(std::max(nd, 0));
    g.H5Sget_simple_extent_dims(sp, hd.data(), nullptr);
    qint64 n = 1;
    for (int i = 0; i < nd; ++i) {
        dims[i] = (qint64)hd[i];
        n *= dims[i];
    }
    hid_t ty = g.H5Dget_type(d);
    hid_t native = g.H5Tget_native_type ? g.H5Tget_native_type(ty, 0) : ty;
    out.assign(std::max<qint64>(n, 0), 0.0);
    herr_t ok = g.H5Dread(d, native > 0 ? native : g.nativeDouble, H5S_ALL, H5S_ALL, H5P_DEFAULT, out.data());
    // If native was not double, re-read as double via file type conversion
    if (ok < 0 && g.nativeDouble > 0) {
        ok = g.H5Dread(d, g.nativeDouble, H5S_ALL, H5S_ALL, H5P_DEFAULT, out.data());
    }
    // Handle integer/float that was read as its native size into double buffer incorrectly:
    // Re-read properly by class
    if (ok >= 0) {
        int cls = g.H5Tget_class(ty);
        size_t sz = g.H5Tget_size(ty);
        if (!(cls == H5T_FLOAT && sz == 8)) {
            // redo with correct temp buffer
            if (cls == H5T_FLOAT && sz == 4) {
                std::vector<float> tmp(n);
                if (g.H5Dread(d, g.nativeFloat > 0 ? g.nativeFloat : native, H5S_ALL, H5S_ALL, H5P_DEFAULT, tmp.data())
                    >= 0) {
                    for (qint64 i = 0; i < n; ++i)
                        out[i] = double(tmp[i]);
                }
            } else if (cls == H5T_INTEGER && sz == 8) {
                std::vector<long long> tmp(n);
                if (g.H5Dread(d, g.nativeInt64 > 0 ? g.nativeInt64 : native, H5S_ALL, H5S_ALL, H5P_DEFAULT, tmp.data())
                    >= 0) {
                    for (qint64 i = 0; i < n; ++i)
                        out[i] = double(tmp[i]);
                }
            } else if (cls == H5T_INTEGER && sz == 4) {
                std::vector<int> tmp(n);
                if (g.H5Dread(d, native, H5S_ALL, H5S_ALL, H5P_DEFAULT, tmp.data()) >= 0) {
                    for (qint64 i = 0; i < n; ++i)
                        out[i] = double(tmp[i]);
                }
            } else if (cls == H5T_INTEGER && sz == 2) {
                std::vector<short> tmp(n);
                if (g.H5Dread(d, native, H5S_ALL, H5S_ALL, H5P_DEFAULT, tmp.data()) >= 0) {
                    for (qint64 i = 0; i < n; ++i)
                        out[i] = double(tmp[i]);
                }
            } else if (cls == H5T_INTEGER && sz == 1) {
                std::vector<signed char> tmp(n);
                if (g.H5Dread(d, native, H5S_ALL, H5S_ALL, H5P_DEFAULT, tmp.data()) >= 0) {
                    for (qint64 i = 0; i < n; ++i)
                        out[i] = double(tmp[i]);
                }
            }
        }
    }
    if (native > 0 && g.H5Tclose)
        g.H5Tclose(native);
    g.H5Tclose(ty);
    g.H5Sclose(sp);
    g.H5Dclose(d);
    return ok >= 0;
}

bool readSlice1D(hid_t fileId, const QString& path, long long start0, long long count,
                 std::vector<double>& out)
{
    if (!ensureLoaded() || !g.H5Dopen2 || count <= 0)
        return false;
    hid_t d = g.H5Dopen2(fileId, path.toUtf8().constData(), H5P_DEFAULT);
    if (d <= 0)
        return false;
    hid_t sp = g.H5Dget_space(d);
    int nd = g.H5Sget_simple_extent_ndims(sp);
    std::vector<hsize_t> hd(std::max(nd, 0));
    g.H5Sget_simple_extent_dims(sp, hd.data(), nullptr);
    long long n = 1;
    for (int i = 0; i < nd; ++i)
        n *= (long long)hd[i];
    if (start0 < 0 || start0 + count > n) {
        g.H5Sclose(sp);
        g.H5Dclose(d);
        return false;
    }
    // hyperslab on flattened rank-1 view
    hsize_t start = (hsize_t)start0;
    hsize_t cnt = (hsize_t)count;
    using herr_sel = int (*)(hid_t, int, const hsize_t*, const hsize_t*, const hsize_t*, const hsize_t*);
    using hid_sc = hid_t (*)(int, const hsize_t*, const hsize_t*);
    static herr_sel pSelect = nullptr;
    static hid_sc pCreateSimple = nullptr;
    static bool symsTried = false;
    if (!symsTried) {
        symsTried = true;
        for (const QString& pth : searchHintPaths()) {
            QLibrary lib(pth);
            if (!lib.load())
                continue;
            pSelect = (herr_sel)sym(lib, "H5Sselect_hyperslab");
            pCreateSimple = (hid_sc)sym(lib, "H5Screate_simple");
            if (pSelect && pCreateSimple)
                break;
            lib.unload();
        }
    }
    out.assign(count, 0.0);
    bool ok = false;
    if (pSelect && pCreateSimple && nd >= 1) {
        // Select linear range on 1-D file space (most GEDI 1D arrays)
        hsize_t offset = start, block = cnt;
        if (pSelect(sp, 0, &offset, nullptr, &cnt, nullptr) >= 0) {
            hsize_t dims1 = (hsize_t)count;
            hid_t mspace = pCreateSimple(1, &dims1, nullptr);
            hid_t ty = g.H5Dget_type(d);
            hid_t native = g.H5Tget_native_type ? g.H5Tget_native_type(ty, 0) : ty;
            int cls = g.H5Tget_class(ty);
            size_t sz = g.H5Tget_size(ty);
            if (cls == H5T_FLOAT && sz == 8) {
                ok = g.H5Dread(d, native > 0 ? native : g.nativeDouble, mspace, sp, H5P_DEFAULT, out.data()) >= 0;
            } else if (cls == H5T_FLOAT && sz == 4) {
                std::vector<float> tmp(count);
                ok = g.H5Dread(d, g.nativeFloat > 0 ? g.nativeFloat : native, mspace, sp, H5P_DEFAULT, tmp.data()) >= 0;
                for (long long i = 0; i < count; ++i)
                    out[i] = tmp[i];
            } else if (cls == H5T_INTEGER) {
                std::vector<long long> tmp(count);
                hid_t nt = g.nativeInt64 > 0 ? g.nativeInt64 : native;
                ok = g.H5Dread(d, nt, mspace, sp, H5P_DEFAULT, tmp.data()) >= 0;
                for (long long i = 0; i < count; ++i)
                    out[i] = double(tmp[i]);
            }
            if (mspace > 0 && g.H5Sclose)
                g.H5Sclose(mspace);
            if (native > 0 && g.H5Tclose)
                g.H5Tclose(native);
            if (ty > 0)
                g.H5Tclose(ty);
        }
    }
    if (!ok) {
        // fallback: full read + slice (slow)
        std::vector<double> all;
        QVector<qint64> dims;
        g.H5Sclose(sp);
        g.H5Dclose(d);
        if (!readAsDouble(fileId, path, all, dims))
            return false;
        if (start0 + count > (long long)all.size())
            return false;
        out.assign(all.begin() + start0, all.begin() + start0 + count);
        return true;
    }
    g.H5Sclose(sp);
    g.H5Dclose(d);
    return true;
}

bool readAttrString(hid_t fileId, const QString& objPath, const QString& attrName, QString& value)
{
    if (!ensureLoaded() || !g.H5Aopen)
        return false;
    // open object: dataset or group
    hid_t obj = g.H5Dopen2(fileId, objPath.toUtf8().constData(), H5P_DEFAULT);
    bool isD = obj > 0;
    if (!isD) {
        obj = g.H5Gopen2(fileId, objPath.toUtf8().constData(), H5P_DEFAULT);
        if (obj <= 0) {
            // root attributes: open file itself for "/"
            obj = fileId;
            isD = false;
        }
    }
    hid_t at = g.H5Aopen(obj, attrName.toUtf8().constData(), H5P_DEFAULT);
    if (at <= 0) {
        if (isD)
            g.H5Dclose(obj);
        return false;
    }
    char buf[512] = {0};
    if (g.cString > 0) {
        hid_t t = g.H5Tcopy(g.cString);
        g.H5Tset_size(t, 512);
        g.H5Aread(at, t, buf);
        g.H5Tclose(t);
    }
    value = QString::fromUtf8(buf);
    g.H5Aclose(at);
    if (isD)
        g.H5Dclose(obj);
    return true;
}

} // namespace h5
