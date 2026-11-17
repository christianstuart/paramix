#include "rendergraph/texture.h"

#include "rendergraph/assert.h"
#include "rendergraph/context.h"

using namespace rendergraph;

Texture::Texture(Context* pContext, const QImage& image, Filtering filtering)
        : m_pTexture(pContext->window()->createTextureFromImage(image)) {
    VERIFY_OR_DEBUG_ASSERT(pContext->window() != nullptr) {
        return;
    }
    m_pTexture->setFiltering(
            filtering == Filtering::Nearest ? QSGTexture::Nearest : QSGTexture::Linear);
    m_pTexture->setMipmapFiltering(QSGTexture::None);
    DEBUG_ASSERT(!m_pTexture->textureSize().isNull());
}

qint64 Texture::comparisonKey() const {
    return m_pTexture->comparisonKey();
}
