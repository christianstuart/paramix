#include "texturedcolormaterial.h"

#include <QMatrix4x4>
#include <QVector2D>
#include <QVector4D>

#include "rendergraph/materialshader.h"
#include "rendergraph/materialtype.h"

using namespace rendergraph;

TexturedColorMaterial::TexturedColorMaterial()
        : Material(uniforms()) {
}

/* static */ const AttributeSet& TexturedColorMaterial::attributes() {
    static AttributeSet set = makeAttributeSet<QVector2D, QVector2D, QVector4D>(
            {"position", "texcoord", "color"});
    return set;
}

/* static */ const UniformSet& TexturedColorMaterial::uniforms() {
    static UniformSet set = makeUniformSet<QMatrix4x4>({"ubuf.matrix"});
    return set;
}

MaterialType* TexturedColorMaterial::type() const {
    static MaterialType type;
    return &type;
}

std::unique_ptr<MaterialShader> TexturedColorMaterial::createShader() const {
    return std::make_unique<MaterialShader>(
            "texturecolor.vert", "texturecolor.frag", uniforms(), attributes());
}
