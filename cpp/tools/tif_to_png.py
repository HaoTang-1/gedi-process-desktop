# Rasterio helper: convert any GeoTIFF to PNG + WGS84 bounds JSON (for Qt C++ preview)
# Usage: python tif_to_png.py <src.tif> <dst.png>
import json, sys
from pathlib import Path

import numpy as np

def main():
    src, dst = Path(sys.argv[1]), Path(sys.argv[2])
    meta_path = dst.with_suffix(".bounds.json")
    try:
        import rasterio
        from rasterio.enums import Resampling
        from rasterio.crs import CRS
        from rasterio.warp import transform_bounds
    except Exception as e:
        meta_path.write_text(json.dumps({"error": f"rasterio: {e}"}), encoding="utf-8")
        return 1

    with rasterio.open(src) as ds:
        # prefer overview-ish downsample
        max_dim = 4096
        scale = max(ds.width, ds.height) / float(max_dim)
        out_h = max(1, int(ds.height / scale))
        out_w = max(1, int(ds.width / scale))
        count = min(ds.count, 3)
        data = ds.read(list(range(1, count + 1)), out_shape=(count, out_h, out_w),
                       resampling=Resampling.bilinear)
        data = data.astype(np.float64)
        for i, nd in enumerate(ds.nodatavals[:count]):
            if nd is not None:
                data[i][data[i] == nd] = np.nan
        if data.size:
            finite = data[np.isfinite(data)]
            if finite.size:
                vmin = float(np.nanpercentile(finite, 2))
                vmax = float(np.nanpercentile(finite, 98))
                if vmax <= vmin:
                    vmax = vmin + 1.0
                data = (data - vmin) / (vmax - vmin)
        data = np.nan_to_num(np.clip(data, 0, 1), nan=0.45)
        if count == 1:
            rgb = np.dstack([data[0], data[0], data[0]])
        else:
            rgb = np.moveaxis(data, 0, -1)
            if rgb.shape[-1] == 2:
                rgb = np.dstack([rgb[..., 0], rgb[..., 0], rgb[..., 0]])
        from PIL import Image
        Image.fromarray((rgb * 255).astype("uint8")).save(dst)

        left, bottom, right, top = ds.bounds
        try:
            if ds.crs and ds.crs != CRS.from_epsg(4326):
                left, bottom, right, top = transform_bounds(
                    ds.crs, CRS.from_epsg(4326), left, bottom, right, top)
        except Exception:
            pass
        ovr = ds.overviews(1) if ds.count else []
        meta = {
            "west": float(left), "east": float(right),
            "south": float(bottom), "north": float(top),
            "width": ds.width, "height": ds.height,
            "bands": ds.count,
            "crs": str(ds.crs),
            "overviews": list(ovr) if ovr else [],
            "has_pyramid": bool(ovr),
            "driver": ds.driver,
            "size_bytes": Path(src).stat().st_size,
        }
        meta_path.write_text(json.dumps(meta), encoding="utf-8")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
