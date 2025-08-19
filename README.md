## GPU Font Renderer

Work in progress.

I can't afford [slug](https://sluglibrary.com/) so I'm trying to implement my GPU font renderer based on glyph outlines instead of using texture atlases or distance functions.

## TODO
- [ ] Barebones OpenType parser
  - [x] [Table Directory](https://learn.microsoft.com/en-us/typography/opentype/spec/otff#table-directory)
  - [x] [Table Record](https://learn.microsoft.com/en-us/typography/opentype/spec/otff)
  - [ ] Relevant Tables
    - [x] [`head`](https://learn.microsoft.com/en-us/typography/opentype/spec/head)
    - [x] [`loca`](https://learn.microsoft.com/en-us/typography/opentype/spec/loca)
    - [x] [`maxp`](https://learn.microsoft.com/en-us/typography/opentype/spec/maxp)
    - [ ] [`glyf`](https://learn.microsoft.com/en-us/typography/opentype/spec/glyf)
      - [x] Glyph Headers
      - [x] Simple Glyph Descriptors
      - [ ] Composite Glyph Descriptors
    - [ ] [`hmtx`](https://learn.microsoft.com/en-us/typography/opentype/spec/hmtx)
    - [ ] [`vmtx`](https://learn.microsoft.com/en-us/typography/opentype/spec/vmtx)
      - Low priority.
    - [ ] [`cmap`](https://learn.microsoft.com/en-us/typography/opentype/spec/cmap)
      - [x] Subtable format 0
      - [x] Subtable format 4
      - [ ] Subtable format 12
      - [ ] Subtable format 14
      - Formats 10 and 13 are considered low priority; formats 2, 6, 8 are not planned to be supported.
- [ ] Extract Bezier control points from font
- [ ] GPU rendering (haven't thought about this part yet)

## References
- Eric Lengyel, [GPU-Centered Font Rendering Directly from Glyph Outlines](https://jcgt.org/published/0006/02/02/). Journal of Computer Graphics Techniques, 2017.
- [Slug Font Rendering Library](https://sluglibrary.com/)
