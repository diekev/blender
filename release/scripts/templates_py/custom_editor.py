# TODOs
# - add icon to the Space definition
# - figure out how to properly insert and update the space selector
# - figure out how to set the contexts string for the panels display
# - transform API (TransInfo.spacetype, TransConvertTypeInfo)
# - Grease Pencil API
# - color management

import bpy
from bpy.types import Space, Region, Menu, Panel, Header
from bpy.props import StringProperty, FloatProperty

# Implementation of custom editors ("Space") from Python


# Defines a new editor.
# Derived from the Space base type, similar to Menu, Operator, Panel, etc.
class MyCustomEditor(Space):
    # === Basics ===
    # Optional identifier string. If not explicitly defined, the python class name is used.
    bl_idname = 'MY_CUSTOM_EDITOR_TYPE'
    # Label for nice name display
    bl_label = 'Custom Editor'
    # Icon for the editor in the select menu.
    bl_icon = 'SCRIPT'

    # === Custom Properties ===
    # These work just like custom properties in ID data blocks
    # Extensive information can be found under
    # https://docs.blender.org/api/current/bpy.props.html
    my_string_prop: bpy.props.StringProperty()
    my_float_prop: FloatProperty(default=3.1415926)


# Retrieve or create if none exists the main keymap for the custom editor.
# This keymap will be active the given region_type.
def ensure_editor_keymap(window_manager, region_type):
    kc = window_manager.keyconfigs.addon

    if not kc:
        return None

    return kc.keymaps.new(name = "MyCustomEditor", space_type = "MY_CUSTOM_EDITOR_TYPE", region_type = region_type)


# Add a custom header region for the editor.
# This is optional.
class MyCustomEditorHeaderRegion(Region):
    bl_idname = 'MyCustomEditorHeaderRegion'
    bl_space_type = 'MY_CUSTOM_EDITOR_TYPE'
    bl_region_type = 'HEADER'

    # Initialization function, called when the region is created/displayed/ resized.
    # NOTE: this is not the same as the standard __init__ function in Python, which is
    #       a purely internal Python method and unknown to the node system!
    def init(self, _window_manager):
        # Initialize the region with default settings for headers.
        # This is similar to Region.view2d_init(self, 'HEADER')
        Region.header_init(self)


# Draw class for headers and footers.
# Other region types can have a custom draw function.
class EDITEUR_HT_entete(Header):
    bl_space_type = 'MY_CUSTOM_EDITOR_TYPE'

    def draw(self, context):
        layout = self.layout

        layout.template_header()
        layout.menu("EDITEUR_MT_view")


# Add a custom window region for the editor.
# This is optional.
class MyCustomEditorWindowRegion(Region):
    bl_idname = 'MyCustomEditorWindowRegion'
    bl_space_type = 'MY_CUSTOM_EDITOR_TYPE'
    bl_region_type = 'WINDOW'

    # Initialization function, called when the region is created/displayed/ resized.
    # NOTE: this is not the same as the standard __init__ function in Python, which is
    #       a purely internal Python method and unknown to the node system!
    def init(self, window_manager):
        # Initialize the region with default settings for panels.
        # In this case, a custom draw function is not needed.
        Region.panels_init(window_manager, self)

        keymap = ensure_editor_keymap(window_manager, 'WINDOW')
        if keymap:
            self.add_keymap(keymap)

    # If the editor needs to display other things, a custom draw function can be implemented:
    # def draw(self, context):
    #    pass


# Add a custom UI region for the editor.
# This is optional.
class MyCustomEditorPropertiesRegion(Region):
    bl_idname = 'MyCustomEditorPropertiesRegion'
    bl_space_type = 'MY_CUSTOM_EDITOR_TYPE'
    bl_region_type = 'UI'

    # Initialization function, called when the region is created/displayed/ resized.
    # NOTE: this is not the same as the standard __init__ function in Python, which is
    #       a purely internal Python method and unknown to the node system!
    def init(self, window_manager):
        # Initialize the region with default settings for panels.
        Region.panels_init(window_manager, self)


# Add a custom navigation bar region for the editor.
# This is optional.
class MyCustomEditorNavigationRegion(Region):
    bl_idname = 'MyCustomEditorNavigationRegion'
    bl_space_type = 'MY_CUSTOM_EDITOR_TYPE'
    bl_region_type = 'NAVIGATION_BAR'

    # Initialization function, called when the region is created/displayed/ resized.
    # NOTE: this is not the same as the standard __init__ function in Python, which is
    #       a purely internal Python method and unknown to the node system!
    def init(self, window_manager):
        # Initialize the region with default settings for panels.
        Region.panels_init(window_manager, self)


def draw_editor_properties(layout, context):
    # Access the custom editor from the context.
    editor = context.space_data

    row = layout.row()
    row.label(text="Hello world!", icon='WORLD_DATA')

    row = layout.row()
    row.prop(editor, "my_string_prop")

    row = layout.row()
    row.prop(editor, "my_float_prop")


# Panel for displaying the editor's properties in the UI region.
class UIRegionPanel(Panel):
    bl_label = "My Custom Compositor Panel"
    bl_idname = "COMPOSITEUR_PT_hello"
    bl_space_type = 'MY_CUSTOM_EDITOR_TYPE'
    bl_region_type = 'UI'
    bl_category = "Some Category"

    def draw(self, context):
        layout = self.layout
        draw_editor_properties(layout, context)


# Panel for displaying the editor's properties in the window region.
class WindowRegionPanel(Panel):
    bl_label = "Hello World Panel"
    bl_idname = "COMPOSITEUR_PT_window"
    bl_space_type = 'MY_CUSTOM_EDITOR_TYPE'
    bl_region_type = 'WINDOW'

    def draw(self, context):
        draw_editor_properties(self.layout, context)


# Panel for displaying the editor's properties in the navigation bar region.
class NavBarRegionPanel(Panel):
    """Creates a Panel in the Object properties window"""
    bl_label = "Hello World Navigation Panel"
    bl_idname = "COMPOSITEUR_PT_navbar"
    bl_space_type = 'MY_CUSTOM_EDITOR_TYPE'
    bl_region_type = 'NAVIGATION_BAR'

    def draw(self, context):
        draw_editor_properties(self.layout, context)


# Menu displayed in the Header.
class EDITEUR_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout
        sc = context.space_data

        layout.prop(sc, "show_region_ui")


classes = (
    MyCustomEditor,
    MyCustomEditorHeaderRegion,
    MyCustomEditorWindowRegion,
    MyCustomEditorPropertiesRegion,
    MyCustomEditorNavigationRegion,
    EDITEUR_HT_entete,
    EDITEUR_MT_view,
    UIRegionPanel,
    WindowRegionPanel,
    NavBarRegionPanel,
)


# Initialize the editor's keymap.
# This should be called in the addon's register() method.
def initialize_keymap():
    wm = bpy.context.window_manager

    keymap = ensure_editor_keymap(wm, 'WINDOW')
    if not keymap:
        return

    # Add an item for show the UI region when pressing the 'N' key
    item = keymap.keymap_items.new("wm.context_toggle", type = 'N', value = 'PRESS')
    item.properties.data_path = 'space_data.show_region_ui'


def register():
    for classe in classes:
        bpy.utils.register_class(classe)

    initialize_keymap()


def unregister():
    for classe in reversed(classes):
        bpy.utils.unregister_class(classe)


if __name__ == "__main__":
    register()
