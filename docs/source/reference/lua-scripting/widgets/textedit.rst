TextEdit Widget
===============

Properties
----------

.. list-table::
  :header-rows: 1
  :widths: 20 20 60

  * - name
    - type (default value)
    - description
  * - text
    - string ('')
    - The text to display.
  * - textSize
    - number (10)
    - The size of the text.
  * - textColor
    - util.color (0, 0, 0, 1)
    - The color of the text.
  * - multiline
    - boolean (false)
    - Whether to render text on multiple lines.
  * - wordWrap
    - boolean (false)
    - Whether to break text into lines to fit the widget's width.
  * - textAlignH
    - ui.ALIGNMENT (Start)
    - Horizontal alignment of the text.
  * - textAlignV
    - ui.ALIGNMENT (Start)
    - Vertical alignment of the text.
  * - readOnly
    - boolean (false)
    - Whether the text can be edited.

Events
------

.. list-table::
  :header-rows: 1
  :widths: 20 20 60

  * - name
    - first argument type
    - description
  * - textChanged
    - string
    - Displayed text changed (e. g. by user input)
