//reference: https://github.com/ShaoqiangSun/15-466-f25-base4
#include "GlyphCache.hpp"
#include <iostream>



GlyphCache::GlyphCache(int t_w, int t_h, int c_w, int c_h) 
: texture_w(t_w), texture_h(t_h), cell_w(c_w), cell_h(c_h) {
  
  create_texture();
}

GlyphCache::~GlyphCache() {
  for (auto& t : textures) {
    if (t.tex_index) glDeleteTextures(1, &t.tex_index);
  }
}

const GlyphEntry& GlyphCache::get(FT_Face ft_face, hb_codepoint_t gid) {
    if (glyph_map.find(gid) != glyph_map.end()) return glyph_map[gid];
    
    //reference :   https://freetype.org/freetype2/docs/tutorial/step1.html
    //              https://freetype.org/freetype2/docs/reference/index.html
    if (FT_Load_Glyph(ft_face, gid, FT_LOAD_DEFAULT)) {
        std::cerr << "  FT_Load_Glyph failed for gid=" << gid << "\n";
    }

    if (FT_Render_Glyph(ft_face->glyph, FT_RENDER_MODE_NORMAL)) {
        std::cerr << "  FT_Render_Glyph failed for gid=" << gid << "\n";
    }

    auto& g = *ft_face->glyph;
    auto& bmp = g.bitmap;
    int g_w = bmp.width;
    int g_h = bmp.rows;

    if (g_w > cell_w || g_h > cell_h) {
        throw std::runtime_error("GlyphCache: glyph bitmap exceeds cell size; increase cellW/cellH.");
    }

    int x_index = 0;
    int y_index = 0;
    int last_texture_index = -1;

    if (g_w > 0 && g_h > 0) {
        last_texture_index = textures.size() - 1;
        if (!find_empty_cell(last_texture_index, x_index, y_index)) {
            last_texture_index = create_texture();
            find_empty_cell(last_texture_index, x_index, y_index);
        }

        write_cell(last_texture_index, x_index, y_index, bmp);
    }
    else {
        last_texture_index = 0;
        x_index = 0;
        y_index = 0;
    }

    GlyphEntry entry;
    entry.texture_id = last_texture_index;
    entry.w = g_w;
    entry.h = g_h;
    entry.left = g.bitmap_left;
    entry.top = g.bitmap_top;
    entry.x_adv = g.advance.x / 64.f;
    entry.y_adv = g.advance.y / 64.f;
    
    if (g_w >0 && g_h > 0) {
        entry.u0 = (float)x_index / (float)texture_w;
        entry.v0 = (float)y_index / (float)texture_h;
        entry.u1 = (float)(x_index + g_w) / (float)texture_w;
        entry.v1 = (float)(y_index + g_h) / (float)texture_h;
    }
    else {
        entry.u0 = 0.0f;
        entry.v0 = 0.0f;
        entry.u1 = 0.0f;
        entry.v1 = 0.0f;
    }

    glyph_map[gid] = entry;
    return glyph_map[gid];
}

int GlyphCache::create_texture() {
    Texture texture;
    texture.w = texture_w;
    texture.h = texture_h;
    texture.row_num = texture_h / cell_h;
    texture.col_num = texture_w / cell_w;
    texture.next_index = 0;

    //reference: https://docs.gl/
    glGenTextures(1, &texture.tex_index);
    glBindTexture(GL_TEXTURE_2D, texture.tex_index);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);

    std::vector<unsigned char> zeros((texture.w * texture.h), 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, texture.w, texture.h, 0, GL_RED, GL_UNSIGNED_BYTE, zeros.data());

    textures.push_back(texture);

    return textures.size() - 1;
}

bool GlyphCache::find_empty_cell(int texture_index, int& x_index, int& y_index) {
    Texture &texture = textures[texture_index];

    if (texture.next_index >= texture.row_num * texture.col_num) return false;

    int cell_x_index = texture.next_index % texture.col_num;
    int cell_y_index = texture.next_index / texture.col_num;

    x_index = cell_x_index * cell_w;
    y_index = cell_y_index * cell_h;

    texture.next_index++;

    return true;
}

void GlyphCache::write_cell(int texture_index, int& x_index, int& y_index, const FT_Bitmap& bmp) {
    glBindTexture(GL_TEXTURE_2D, textures[texture_index].tex_index);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (int i = 0; i < (int)bmp.rows; i++) {
        const GLvoid* src = bmp.buffer + i * bmp.pitch;
        glTexSubImage2D(GL_TEXTURE_2D, 0, x_index, y_index + i, bmp.width, 1, GL_RED, GL_UNSIGNED_BYTE, src);

    }
}

