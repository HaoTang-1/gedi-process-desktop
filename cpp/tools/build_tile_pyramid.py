# Fast tile pyramid: one decimated read per zoom level, then split into 256px PNG tiles.
# Usage: python build_tile_pyramid.py <src.tif> <tiles_dir>
import json, math, sys
from pathlib import Path

import numpy as np

def main():
    src, out = Path(sys.argv[1]), Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    import rasterio
    from rasterio.enums import Resampling
    from rasterio.crs import CRS
    from rasterio.warp import transform_bounds
    from PIL import Image

    with rasterio.open(src) as ds:
        left, bottom, right, top = ds.bounds
        try:
            if ds.crs and ds.crs != CRS.from_epsg(4326):
                left, bottom, right, top = transform_bounds(
                    ds.crs, CRS.from_epsg(4326), left, bottom, right, top)
        except Exception:
            pass
        max_dim = max(ds.width, ds.height)
        zmax = min(int(math.ceil(math.log2(max(max_dim / 256.0, 1.0)))), 6)  # z7 too slow
        count = min(ds.count, 3)
        n_tiles = 0
        z_levels = zmax + 1
        print(f"plan zmax={zmax} levels={z_levels} size={ds.width}x{ds.height}", flush=True)
        for z in range(0, zmax + 1):
            decim = 2 ** (zmax - z)
            lw = max(1, ds.width // decim)
            lh = max(1, ds.height // decim)
            # ONE read per level (uses GDAL overviews if present)
            data = ds.read(list(range(1, count + 1)),
                           out_shape=(count, lh, lw),
                           resampling=Resampling.bilinear).astype(np.float64)
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
                rgb = np.moveaxis(data[:3], 0, -1)
                if rgb.shape[-1] == 2:
                    rgb = np.dstack([rgb[..., 0], rgb[..., 0], rgb[..., 0]])
            arr = (rgb * 255).astype("uint8")
            img = Image.fromarray(arr)
            nx = (lw + 255) // 256
            ny = (lh + 255) // 256
            for ty in range(ny):
                for tx in range(nx):
                    tile = img.crop((tx * 256, ty * 256, min((tx + 1) * 256, lw), min((ty + 1) * 256, lh)))
                    # store exact tile size — do NOT pad black (was the big black frame)
                    d = out / str(z) / str(tx)
                    d.mkdir(parents=True, exist_ok=True)
                    tile.save(d / f"{ty}.png")
                    n_tiles += 1
            print("level", z, "tiles", nx * ny, "size", lw, lh, "progress", f"{z+1}/{z_levels}", flush=True)

        (out / "meta.json").write_text(json.dumps({
            "west": float(left), "east": float(right),
            "south": float(bottom), "north": float(top),
            "zmax": zmax, "width": ds.width, "height": ds.height,
            "tiles": n_tiles,
        }), encoding="utf-8")
        print("DONE tiles", n_tiles)

if __name__ == "__main__":
    main()
