# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>
import bpy
from bpy.types import Panel, Menu, UIList
from rna_prop_ui import PropertyPanel


class VolumeButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        return context.volume


class DATA_UL_volume_fields(UIList):
    def draw_items(self, context, layout, item, icon, active_data, active_propname, index):
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            layout.prop(item, "name", text="", emboss=False, icon_value=icon)
        elif self.layout_type in {'GRID'}:
            layout.alignement = 'CENTER'
            layout.label(text="", icon_value=icon)


class DATA_PT_volume_fields(VolumeButtonsPanel, Panel):
    bl_label = "Volume Fields"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        volume = context.volume

        layout.template_list("DATA_UL_volume_fields", "", volume, "fields", volume, "active_field_index", rows=3)

        field = volume.active_field

        if field:
            layout.prop(field, "show_topology")



if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
