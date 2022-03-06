---
-- `openmw.ui` controls user interface.
-- Can be used only by local scripts, that are attached to a player.
-- @module ui
-- @usage
-- local ui = require('openmw.ui')

---
-- Widget types
-- @field [parent=#ui] #TYPE TYPE

---
-- Alignment values (left to right, top to bottom)
-- @field [parent=#ui] #ALIGNMENT ALIGNMENT

---
-- Tools for working with layers
-- @field [parent=#ui] #Layers layers

---
-- All available widget types
-- @type TYPE
-- @field Widget Base widget type
-- @field Text Display text
-- @field TextEdit Accepts user text input
-- @field Window Can be moved and resized by the user

---
-- Alignment values (details depend on the specific property).
-- For horizontal alignment the order is left to right, for vertical alignment the order is top to bottom.
-- @type ALIGNMENT
-- @field Start
-- @field Center
-- @field End

---
-- Shows given message at the bottom of the screen.
-- @function [parent=#ui] showMessage
-- @param #string msg

---
-- Converts a given table of tables into an @{openmw.ui#Content}
-- @function [parent=#ui] content
-- @param #table table
-- @return #Content

---
-- Creates a UI element from the given layout table
-- @function [parent=#ui] create
-- @param #Layout layout
-- @return #Element

---
-- Adds a settings page to main menu setting's Scripts tab.
-- @function [parent=#ui] registerSettingsPage
-- @param #SettingsPageOptions page

---
-- Table with settings page options, passed as an argument to ui.registerSettingsPage
-- @type SettingsPageOptions
-- @field #string name Name of the page, displayed in the list, used for search
-- @field #string searchHints Additional keywords used in search, not displayed anywhere
-- @field #Element element The page's UI, which will be attached to the settings tab. The root widget has to have a fixed size. Set the `size` field in `props`, `relativeSize` is ignored.


---
-- Layout
-- @type Layout
-- @field #string name Optional name of the layout. Allows access by name from Content
-- @field #table props Optional table of widget properties
-- @field #table events Optional table of event callbacks
-- @field #Content content Optional @{openmw.ui#Content} of children layouts

---
-- Layers. Implements [iterables#List](iterables.html#List) of #string.
-- @type Layers
-- @list <#string>
-- @usage
-- ui.layers.insertAfter('HUD', 'NewLayer', { interactive = true })
-- local fourthLayerName = ui.layers[4]
-- local windowsIndex = ui.layers.indexOf('Windows')
-- for i, name in ipairs(ui.layers) do
--   print('layer', i, name)
-- end

---
-- Index of the layer with the givent name. Returns nil if the layer doesn't exist
-- @function [parent=#Layers] indexOf
-- @param #string name Name of the layer
-- @return #number, #nil index

---
-- Creates a layer and inserts it after another layer (shifts indexes of some other layers).
-- @function [parent=#Layers] insertAfter
-- @param #string afterName Name of the layer after which the new layer will be inserted
-- @param #string name Name of the new layer
-- @param #table options Table with a boolean `interactive` field (default is true). Layers with interactive = false will ignore all mouse interactions.

---
-- Content. An array-like container, which allows to reference elements by their name.
-- Implements [iterables#List](iterables.html#List) of #Layout and [iterables#Map](iterables.html#Map) of #string to #Layout.
-- @type Content
-- @list <#Layout>
-- @usage
-- local content = ui.content {
--    { name = 'input' },
-- }
-- -- bad idea!
-- -- content[1].name = 'otherInput'
-- -- do this instead:
-- content.input = { name = 'otherInput' }
-- @usage
-- local content = ui.content {
--    { name = 'display' },
--    { name = 'submit' },
-- }
-- -- allowed, but shifts all the items after it "up" the array
-- content.display = nil
-- -- still no holes after this!
-- @usage
-- -- iterate over a Content
-- for i = 1, #content do
--    print('widget',content[i].name,'at',i)
-- end

---
-- Puts the layout at given index by shifting all the elements after it
-- @function [parent=#Content] insert
-- @param self
-- @param #number index
-- @param #Layout layout

---
-- Adds the layout at the end of the Content
-- (same as calling insert with `last index + 1`)
-- @function [parent=#Content] add
-- @param self
-- @param #Layout layout

---
-- Finds the index of the given layout. If it is not in the container, returns nil
-- @function [parent=#Content] indexOf
-- @param self
-- @param #Layout layout
-- @return #number, #nil index

---
-- Element. An element of the user interface
-- @type Element

---
-- Refreshes the rendered element to match the current layout state
-- @function [parent=#Element] update
-- @param self

---
-- Destroys the element
-- @function [parent=#Element] destroy
-- @param self

---
-- Access or replace the element's layout
-- @field [parent=#Element] #Layout layout

---
-- Mouse event, passed as an argument to relevant UI events
-- @type MouseEvent
-- @field openmw.util#Vector2 position Absolute position of the mouse cursor
-- @field openmw.util#Vector2 offset Position of the mouse cursor relative to the widget
-- @field #number button Mouse button which triggered the event (could be nil)

---
-- Register a new texture resource. Can be used to manually atlas UI textures.
-- @function [parent=#ui] texture #TextureResource
-- @param #TextureResourceOptions options
-- @usage
-- local ui = require('openmw.ui')
-- local vector2 = require('openmw.util').vector2
-- local myAtlas = 'textures/my_atlas.dds' -- a 128x128 atlas
-- local texture1 = ui.texture { -- texture in the top left corner of the atlas
--     path = myAtlas,
--     offset = vector2(0, 0),
--     size = vector2(64, 64),
-- }
-- local texture2 = ui.texture { -- texture in the top right corner of the atlas
--     path = myAtlas,
--     offset = vector2(64, 0),
--     size = vector2(64, 64),
-- }

---
-- A texture ready to be used by UI widgets
-- @type TextureResource

---
-- Table with arguments passed to ui.texture.
-- @type TextureResourceOptions
-- @field #string path Path to the texture file. Required
-- @field openmw.util#Vector2 offset Offset of this resource in the texture. (0, 0) by default
-- @field openmw.util#Vector2 size Size of the resource in the texture. (0, 0) by default. 0 means the whole texture size is used.

return nil
