# from PIL import Image
import sys
import os
import gradio as gr

pp = os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../build/bin")
print(pp)
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../build/bin"))
import cgl

cross_gate_version_map = {e.name: e for e in cgl.CrossGateVersionList()}
env_palette_type_map = {e.name: e for e in cgl.EnvironmentPaletteTypesList()}

# ------------------------------------------------------------------------------
class GraphicsDataViewerApp:
    def __init__(self):
        self.session = cgl.Session()
        self.palette_list = {}
        self.serial_nums = {}
        self.img_col = 8
        self.img_row = 2
        self.interface = self.build_interface()


    def acquire_available_serial_nums_list(self, version):
        if version not in self.serial_nums:
            self.serial_nums[version] = self.session.acquire_available_serial_nums(version)
            self.serial_nums[version].sort()

        return self.serial_nums[version]


    def acquire_palette(self, palette):
        if palette not in self.palette_list:
            self.palette_list[palette] = self.session.acquire_env_palette(palette)

        return self.palette_list[palette]


    def on_image_change(self, version, palette, serial_num):
        version = cross_gate_version_map[version]
        palette = env_palette_type_map[palette]

        # acquire palette
        palette = self.acquire_palette(palette)

        # acquire available serial num
        available_serial_nums = self.acquire_available_serial_nums_list(version)
        count = self.img_col * self.img_row
        end_serial_num = min(serial_num + count, len(available_serial_nums))
        start_serial_num = min(serial_num, len(available_serial_nums) - count)

        images = []
        for i in range(start_serial_num, end_serial_num):
            serial_num  = available_serial_nums[i]
            gfx_resource = self.session.acquire_graphics_resource(version, serial_num)
            if (gfx_resource is not None) and (palette is not None):
                img      = self.session.apply_palette(gfx_resource, palette)
                img      = img[:, :, [2, 1, 0]]
                img      = img[::-1, :, :]
                images.append((img, f"S/N: {serial_num}"))
        return images


    def on_version_type_change(self, version):
        version = cross_gate_version_map[version]

        indices = self.acquire_available_serial_nums_list(version)
        if not indices:
            return gr.update(visible=False)
        return gr.update(minimum=0, maximum=len(indices), value=0)


    def on_page_load(self):
        default_version = list(cross_gate_version_map.values())[0]
        default_palette = list(env_palette_type_map.values())[0]

        # update sn slider
        serial_nums = self.acquire_available_serial_nums_list(default_version)
        serial_num_min = min(serial_nums)
        serial_num_max = max(serial_nums)
        default_serial_num = serial_num_min

        #  update image
        images = self.on_image_change(default_version.name, default_palette.name, default_serial_num)

        return (
            # update version_type
            gr.update(value=default_version.name),
            # update palette_type,
            gr.update(value=default_palette.name),
            # update serial_num_selector
            gr.update(minimum=serial_num_min, maximum=serial_num_max, value=default_serial_num),
            # update image_gallery
            images
        )

    def build_interface(self):
        with gr.Blocks() as demo:
            with gr.Row():
                ui_version = gr.Dropdown(label="Cross Gate Version", choices=cross_gate_version_map)
                ui_palette = gr.Dropdown(label="Environment Palette Type", choices=env_palette_type_map)

            with gr.Column(scale=3):
                ui_serial_num_selector = gr.Slider(label="Serial Num", minimum=0, maximum=100, step=8, value=0, interactive=True)
                ui_image_gallery = gr.Gallery(label="Images", columns=self.img_col, rows=self.img_row)

            # Bind event
            ui_version.change(
                fn      = self.on_version_type_change,
                inputs  = ui_version,
                outputs = ui_serial_num_selector
            )

            ui_version.change(
                fn      = self.on_image_change,
                inputs  = [ui_version, ui_palette, ui_serial_num_selector],
                outputs = ui_image_gallery
            )

            ui_serial_num_selector.change(
                fn      = self.on_image_change,
                inputs  = [ui_version,
                           ui_palette,
                           ui_serial_num_selector],
                outputs = ui_image_gallery
            )

            demo.load(
                fn      = self.on_page_load,
                inputs  = [],
                outputs = [ui_version,
                           ui_palette,
                           ui_serial_num_selector,
                           ui_image_gallery]
            )
        return demo

    def launch(self, **kwargs):
        self.interface.launch(**kwargs)

# ------------------------------------------------------------------------------
if __name__ == "__main__":
    app = GraphicsDataViewerApp()
    app.launch()