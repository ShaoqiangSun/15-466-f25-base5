//reference: https://github.com/ShaoqiangSun/15-466-f25-base4
#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb.h>
#include <hb-ft.h>

#include "GL.hpp"

#include <vector>
#include <unordered_map>

struct GlyphEntry {
    int texture_id;
    float u0, v0, u1, v1;
    int w, h;
    int left, top;
    float x_adv, y_adv;
};

class GlyphCache {
public:
    explicit GlyphCache(int texture_w = 1024, int texture_h = 1024,
                        int cell_w  = 64, int cell_h  = 64);
    ~GlyphCache();

    const GlyphEntry& get(FT_Face ft_face, hb_codepoint_t gid);

    
    struct Texture {
        GLuint tex_index = 0;
        int w = 0;
        int h = 0;
        int row_num = 0;
        int col_num = 0;
        int next_index = 0;
    };

    int create_texture();
    bool find_empty_cell(int texture_index, int& x_index, int& y_index);
    void write_cell(int texture_index, int& x_index, int& y_index, const FT_Bitmap& bmp);
    std::vector<Texture> textures;
    int texture_w, texture_h;
    int cell_w, cell_h;
    std::unordered_map<hb_codepoint_t, GlyphEntry> glyph_map;
};