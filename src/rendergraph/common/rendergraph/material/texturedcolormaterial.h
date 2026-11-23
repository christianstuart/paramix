#pragma once

#include "rendergraph/attributeset.h"
#include "rendergraph/material.h"
#include "rendergraph/texture.h"
#include "rendergraph/uniformset.h"

namespace rendergraph {
class TexturedColorMaterial;
}

/// Texture material with a per-vertex colour. The alpha channel of the vertex
/// colour scales the premultiplied texture, so lane opacity and mute ghosting
/// can change without rebuilding the texture.
class rendergraph::TexturedColorMaterial : public rendergraph::Material {
  public:
    TexturedColorMaterial();

    static const AttributeSet& attributes();

    static const UniformSet& uniforms();

    MaterialType* type() const override;

    std::unique_ptr<MaterialShader> createShader() const override;

    Texture* texture(int /*binding*/) const override {
        return m_pTexture.get();
    }

    void setTexture(std::unique_ptr<Texture> texture) {
        m_pTexture = std::move(texture);
    }

  private:
    std::unique_ptr<Texture> m_pTexture;
};
