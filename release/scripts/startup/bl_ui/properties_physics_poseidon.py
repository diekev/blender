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
from bpy.types import Panel

from bl_ui.properties_physics_common import (
        point_cache_ui,
        )


class PhysicButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"

    @classmethod
    def poll(cls, context):
        ob = context.object
        rd = context.scene.render
        return (ob and ob.type == 'MESH') and (not rd.use_game_engine) and (context.poseidon)


class PHYSICS_PT_poseidon(PhysicButtonsPanel, Panel):
    bl_label = "Smoke"

    def draw(self, context):
        layout = self.layout

        md = context.poseidon
        ob = context.object

        layout.prop(md, "smoke_type", expand=True)


class PHYSICS_PT_smoke_cache(PhysicButtonsPanel, Panel):
    bl_label = "Smoke Cache"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.poseidon
        rd = context.scene.render
        return md and (md.smoke_type == 'DOMAIN') and (not rd.use_game_engine)

    def draw(self, context):
        layout = self.layout

        domain = context.poseidon.domain_settings

        cache = domain.cache
        point_cache_ui(self, context, cache, (cache.is_baked is False), 'POSEIDON')


if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
