#include "ui/render/draw_list.hpp"

namespace atpl::render {

void DrawList::addText(
    const FloatRect& rect, std::string_view text, const PartStyle& style, Align align, bool wrapped
) {
    if (!style.shown || style.color.a == 0 || text.empty() || rect.width() <= 0.f || rect.height() <= 0.f) {
        return;
    }

    if (m_textCount == m_texts.size()) {
        m_texts.emplace_back();
    }
    TextRun& run = m_texts[m_textCount++];
    ++m_textRevision;
    run.rect = rect;
    run.text.assign(text); // reuses the memory of the entry's earlier text
    run.color = style.color;
    run.size = style.textSize;
    run.font = style.font;
    run.align = align;
    run.wrapped = wrapped;
}

void DrawList::clear() {
    m_shapes.clear();
    m_textCount = 0;
    ++m_textRevision;
}

} // namespace atpl::render
