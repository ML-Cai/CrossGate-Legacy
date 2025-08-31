# ======================================================================================================================
#   The MIT License (MIT)
#
#   Copyright (c) 2024-2025 MengLun,Cai
#
#   All rights reserved.
# ======================================================================================================================
import sys
import os
from PIL import Image

sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../build/bin"))
import cgl

cross_gate_version_map = {e.name: e for e in cgl.CrossGateVersionList()}
env_palette_type_map = {e.name: e for e in cgl.EnvironmentPaletteTypesList()}

# ----------------------------------------------------------------------------------------------------------------------
class App:
    def __init__(self):
        self.session = cgl.Session()

    def launch(self):
        palette = cgl.EnvironmentPaletteTypes.Daytime
        palette = self.session.acquire_env_palette(palette)

        version         = cgl.CG_VERSION_Classic.CG_VERSION_Classic
        # anime_seral_num = 101000
        anime_seral_num = 101406
        anime_res_bundle = self.session.acquire_anime_resource(version, anime_seral_num)
        if anime_res_bundle is None:
            print("No anime data found for the given version and serial number.")
            return

        all_keys = anime_res_bundle.keys()
        for key in all_keys:
            dir, motion = key
            # print(f"Key -> {key},  {type(key[0])},  {type(key[1])}")
            anime_data = anime_res_bundle.acquire(dir, motion)
            # print(type(anime_data))
            # print(f"anime_data {anime_data.version}, {anime_data.direction}, {anime_data.motion}, {anime_data.duration}, {anime_data.motionGraphicsSerialNums}")


            # generate gif source
            gif_src = []
            bbox = [0, 0, 0, 0]     # left, right, top, bottom
            for serial_num in anime_data.motionGraphicsSerialNums:
                gfx_data = self.session.acquire_graphics_resource(version, serial_num)

                # generate image
                img = self.session.apply_palette(gfx_data, palette)
                img = img[:, :, [2, 1, 0]]  # Convert RGB to BGR
                img = img[::-1, :, :]       # Flip vertically
                img = Image.fromarray(img)

                # determine max width and height in current animation images
                width, height      = img.size
                offset_x, offset_y = gfx_data.offset()
                bbox[0] = min(bbox[0], offset_x)                     # left
                bbox[1] = max(bbox[1], offset_x + width)
                bbox[2] = min(bbox[2], offset_y)                     # top
                bbox[3] = max(bbox[3], offset_y + height)

                # append to gif source
                gif_src.append((img, gfx_data.offset()))

            aligned_images = []
            gif_width  = bbox[1] - bbox[0]
            gif_height = bbox[3] - bbox[2]
            for img_bundle in gif_src:
                img = Image.new('RGBA', (gif_width, gif_height), (0, 0, 0, 255))

                src_offset_x, src_offset_y = img_bundle[1]

                offset_x = int(src_offset_x + -bbox[0])
                offset_y = int(src_offset_y + -bbox[2])

                # align image to the same top-left corner
                img.paste(img_bundle[0], (offset_x, offset_y))
                aligned_images.append(img)

            # create gif
            if aligned_images:
                gif_path = f"anime_{anime_data.direction.name}_{anime_data.motion.name}.gif"
                aligned_images[0].save(
                    gif_path,
                    save_all=True,
                    append_images=aligned_images[1:],
                    loop=0,
                    # duration=anime_data.duration/len(aligned_images),
                    duration=anime_data.duration/len(aligned_images),
                    optimize=False
                )
                print(f"Saved GIF: {gif_path}")

            break

# ------------------------------------------------------------------------------
if __name__ == "__main__":
    app = App()
    app.launch()
