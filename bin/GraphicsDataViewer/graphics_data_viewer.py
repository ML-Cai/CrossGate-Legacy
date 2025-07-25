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
        self.index_value_list = {}
        self.img_col = 8
        self.img_row = 2
        self.interface = self.build_interface()


    def acquire_available_index_value_list(self, version):
        if version not in self.index_value_list:
            self.index_value_list[version] = self.session.acquire_available_index_value(version)
            self.index_value_list[version].sort()

        return self.index_value_list[version]


    def acquire_palette(self, palette):
        if palette not in self.palette_list:
            self.palette_list[palette] = self.session.acquire_env_palette(palette)

        return self.palette_list[palette]


    def on_image_change(self, version, palette, index):
        version = cross_gate_version_map[version]
        palette = env_palette_type_map[palette]

        # acquire palette
        palette = self.acquire_palette(palette)

        # acquire available index value
        available_index_value_list = self.acquire_available_index_value_list(version)
        count = self.img_col * self.img_row
        end_index = min(index + count, len(available_index_value_list))
        start_index = min(index, len(available_index_value_list) - count)

        images = []
        for i in range(start_index, end_index):
            index    = available_index_value_list[i]
            gfx_data = self.session.acquire_graphics_data(version, index)
            if (gfx_data is not None) and (palette is not None):
                img      = self.session.apply_palette(gfx_data, palette)
                img      = img[:, :, [2, 1, 0]]
                img      = img[::-1, :, :]
                images.append((img, f"Index: {index}"))
        return images


    def on_version_type_change(self, version):
        version = cross_gate_version_map[version]

        indices = self.acquire_available_index_value_list(version)
        if not indices:
            return gr.update(visible=False)
        return gr.update(minimum=0, maximum=len(indices), value=0)


    def on_page_load(self):
        default_version = list(cross_gate_version_map.values())[0]
        default_palette = list(env_palette_type_map.values())[0]

        # update index slider
        indices = self.acquire_available_index_value_list(default_version)
        index_min = min(indices)
        index_max = max(indices)
        default_index = index_min

        #  update image
        images = self.on_image_change(default_version.name, default_palette.name, default_index)

        return (
            # update version_type
            gr.update(value=default_version.name),
            # update palette_type,
            gr.update(value=default_palette.name),
            # update index_selector,
            gr.update(minimum=index_min, maximum=index_max, value=default_index),
            # update image_gallery
            images
        )

    def build_interface(self):
        with gr.Blocks() as demo:
            with gr.Row():
                ui_version = gr.Dropdown(label="Cross Gate Version", choices=cross_gate_version_map)
                ui_palette = gr.Dropdown(label="Environment Palette Type", choices=env_palette_type_map)

            with gr.Column(scale=3):
                ui_index_selector = gr.Slider(label="Index", minimum=0, maximum=100, step=8, value=0, interactive=True)
                ui_image_gallery = gr.Gallery(label="Images", columns=self.img_col, rows=self.img_row)

            # Bind event
            ui_version.change(
                fn      = self.on_version_type_change,
                inputs  = ui_version,
                outputs = ui_index_selector
            )

            ui_version.change(
                fn      = self.on_image_change,
                inputs  = [ui_version, ui_palette, ui_index_selector],
                outputs = ui_image_gallery
            )

            ui_index_selector.change(
                fn      = self.on_image_change,
                inputs  = [ui_version,
                           ui_palette,
                           ui_index_selector],
                outputs = ui_image_gallery
            )

            demo.load(
                fn      = self.on_page_load,
                inputs  = [],
                outputs = [ui_version,
                           ui_palette,
                           ui_index_selector,
                           ui_image_gallery]
            )
        return demo

    def launch(self, **kwargs):
        self.interface.launch(**kwargs)

# ------------------------------------------------------------------------------
if __name__ == "__main__":
    app = GraphicsDataViewerApp()
    app.launch()